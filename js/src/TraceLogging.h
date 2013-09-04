/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TraceLogging_h
#define TraceLogging_h

#include <stdint.h>
#include <stdio.h>

namespace JS {
class CompileOptions;
}

namespace js {

class TraceLogging
{
  public:
    enum Type {
        SCRIPT_START,
        SCRIPT_STOP,
        ION_COMPILE_START,
        ION_COMPILE_STOP,
        YARR_JIT_START,
        YARR_JIT_STOP,
        GC_START,
        GC_STOP,
        MINOR_GC_START,
        MINOR_GC_STOP,
        PARSER_COMPILE_SCRIPT_START,
        PARSER_COMPILE_SCRIPT_STOP,
        PARSER_COMPILE_LAZY_START,
        PARSER_COMPILE_LAZY_STOP,
        PARSER_COMPILE_FUNCTION_START,
        PARSER_COMPILE_FUNCTION_STOP,
        INFO_ENGINE_INTERPRETER,
        INFO_ENGINE_BASELINE,
        INFO_ENGINE_IONMONKEY,
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

    uint64_t startupTime;
    uint64_t loggingTime;
    Entry *entries;
    unsigned int curEntry;
    unsigned int numEntries;
    int fileno;
    FILE *out;

    static const char * const type_name[];
    static TraceLogging* _defaultLogger;
  public:
    TraceLogging();
    ~TraceLogging();

    void log(Type type, const char* filename, unsigned int line);
    void log(Type type, const JS::CompileOptions &options);
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
    AutoTraceLog(TraceLogging* logger, TraceLogging::Type start, TraceLogging::Type stop,
                 const JS::CompileOptions &options)
      : logger(logger),
        stop(stop)
    {
        logger->log(start, options);
    }

    AutoTraceLog(TraceLogging* logger, TraceLogging::Type start, TraceLogging::Type stop,
                 JSScript* script)
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

#endif /* TraceLogging_h */
