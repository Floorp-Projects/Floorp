/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(TraceLogging_h__)
#define TraceLogging_h__

#include "jsscript.h"

namespace js {

class TraceLogging
{
  public:
    enum Type {
        ION_COMPILE_START,
        ION_COMPILE_STOP,
        ION_CANNON_START,
        ION_CANNON_STOP,
        ION_CANNON_BAIL,
        ION_SIDE_CANNON_START,
        ION_SIDE_CANNON_STOP,
        ION_SIDE_CANNON_BAIL,
        YARR_JIT_START,
        YARR_JIT_STOP,
        JM_SAFEPOINT_START,
        JM_SAFEPOINT_STOP,
        JM_START,
        JM_STOP,
        JM_COMPILE_START,
        JM_COMPILE_STOP,
        GC_START,
        GC_STOP,
        INTERPRETER_START,
        INTERPRETER_STOP,
        INFO
    };

  private:
    struct Entry {
        uint64_t tick_;
        char* file_;
        uint32_t lineno_;
        uint8_t type_;

        Entry(uint64_t tick, char* file, uint32_t lineno, Type type)
            : tick_(tick), file_(file), lineno_(lineno), type_((uint8_t)type) {}

        uint64_t tick() const { return tick_; }
        char *file() const { return file_; }
        uint32_t lineno() const { return lineno_; }
        Type type() const { return (Type) type_; }
    };

    uint64_t loggingTime;
    Entry *entries;
    unsigned int curEntry;
    unsigned int numEntries;
    int fileno;
    FILE *out;

    const static char *type_name[];
    static TraceLogging* _defaultLogger;
  public:
    TraceLogging();
    ~TraceLogging();

    void log(Type type, const char* filename, unsigned int line);
    void log(Type type, JSScript* script);
    void log(const char* log);
    void log(Type type);
    void flush();

    static TraceLogging* defaultLogger();
    static void releaseDefaultLogger();

  private:
    void grow();
};

/* Helpers functions for asm calls */
void TraceLog(TraceLogging* logger, TraceLogging::Type type, JSScript* script);
void TraceLog(TraceLogging* logger, const char* log);
void TraceLog(TraceLogging* logger, TraceLogging::Type type);

/* Automatic logging at the start and end of function call */
class AutoTraceLog {
    TraceLogging* logger;
    TraceLogging::Type stop;

  public:
    AutoTraceLog(TraceLogging* logger, TraceLogging::Type start, TraceLogging::Type stop, JSScript* script)
      : logger(logger),
        stop(stop)
    {
        logger->log(start, script);
    }

    AutoTraceLog(TraceLogging* logger, TraceLogging::Type start, TraceLogging::Type stop)
      : logger(logger),
        stop(stop)
    {
        logger->log(start);
    }

    ~AutoTraceLog()
    {
        logger->log(stop);
    }
};

}  /* namespace js */

#endif // TraceLogging_h__

