/* Copyright (c) 2010-2013, AOYAMA Kazuharu
 * All rights reserved.
 *
 * This software may be used and distributed according to the terms of
 * the New BSD License, which is incorporated herein by reference.
 */

#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <TWebApplication>
#include <TLogger>
#include <TLog>
#include <TAccessLog>
#include "tsystemglobal.h"
#include "taccesslogstream.h"
#include "tfileaiowriter.h"

static TAccessLogStream *accesslogstrm = 0;
static TAccessLogStream *sqllogstrm = 0;
static TFileAioWriter systemLog;
static QByteArray syslogLayout;
static QByteArray syslogDateTimeFormat;
static QByteArray accessLogLayout;
static QByteArray accessLogDateTimeFormat;


void writeAccessLog(const TAccessLog &log)
{
    if (accesslogstrm) {
        accesslogstrm->writeLog(log.toByteArray(accessLogLayout, accessLogDateTimeFormat));

        if (Tf::app()->multiProcessingModule() == TWebApplication::Prefork) {
            accesslogstrm->flush();
        }
    }
}


void tSetupSystemLogger()
{
    // Log directory
    QDir logdir(Tf::app()->logPath());
    if (!logdir.exists()) {
        logdir.mkpath(".");
    }

    // system log
    systemLog.setFileName(Tf::app()->systemLogFilePath());
    systemLog.open();

    syslogLayout = Tf::app()->appSettings().value("SystemLog.Layout", "%d %5P %m%n").toByteArray();
    syslogDateTimeFormat = Tf::app()->appSettings().value("SystemLog.DateTimeFormat", "yyyy-MM-ddThh:mm:ss").toByteArray();
}


void tSetupAccessLogger()
{
    // access log
    QString accesslogpath = Tf::app()->accessLogFilePath();
    if (!accesslogstrm && !accesslogpath.isEmpty()) {
        accesslogstrm = new TAccessLogStream(accesslogpath);
    }

    accessLogLayout = Tf::app()->appSettings().value("AccessLog.Layout", "%h %d \"%r\" %s %O%n").toByteArray();
    accessLogDateTimeFormat = Tf::app()->appSettings().value("AccessLog.DateTimeFormat", "yyyy-MM-ddThh:mm:ss").toByteArray();
}


void tSetupQueryLogger()
{
    // sql query log
    QString querylogpath = Tf::app()->sqlQueryLogFilePath();
    if (!sqllogstrm && !querylogpath.isEmpty()) {
        sqllogstrm = new TAccessLogStream(querylogpath);
    }
}


static void tSystemMessage(int priority, const char *msg, va_list ap)
{
    TLog log(priority, QString().vsprintf(msg, ap).toLocal8Bit());
    QByteArray buf = TLogger::logToByteArray(log, syslogLayout, syslogDateTimeFormat);
    systemLog.write(buf.data(), buf.length());

    if (Tf::app()->multiProcessingModule() == TWebApplication::Prefork) {
        systemLog.flush();
    }
}


void tSystemError(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    tSystemMessage(TLogger::Error, msg, ap);
    va_end(ap);
}


void tSystemWarn(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    tSystemMessage(TLogger::Warn, msg, ap);
    va_end(ap);
}


void tSystemInfo(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    tSystemMessage(TLogger::Info, msg, ap);
    va_end(ap);
}

#ifndef TF_NO_DEBUG

void tSystemDebug(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    tSystemMessage(TLogger::Debug, msg, ap);
    va_end(ap);
}


void tSystemTrace(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    tSystemMessage(TLogger::Trace, msg, ap);
    va_end(ap);
}

#else

void tSystemDebug(const char *, ...) { }
void tSystemTrace(const char *, ...) { }

#endif


void tQueryLog(const char *msg, ...)
{
    if (sqllogstrm) {
        va_list ap;
        va_start(ap, msg);
        TLog log(-1, QString().vsprintf(msg, ap).toLocal8Bit());
        QByteArray buf = TLogger::logToByteArray(log, syslogLayout, syslogDateTimeFormat);
        sqllogstrm->writeLog(buf);
        va_end(ap);

        if (Tf::app()->multiProcessingModule() == TWebApplication::Prefork) {
            sqllogstrm->flush();
        }
    }
}


void tWriteQueryLog(const QString &query, bool success, const QSqlError &error)
{
    QString q = query;

    if (!success) {
        QString err = (!error.databaseText().isEmpty()) ? error.databaseText() : error.text().trimmed();
        if (!err.isEmpty()) {
            err = QLatin1Char('[') + err + QLatin1String("] ");
        }
        q = QLatin1String("(Query failed) ") + err + query;
    }
    tQueryLog("%s", qPrintable(q));
}
