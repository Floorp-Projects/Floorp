/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TraceLogging.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "jsapi.h"
#include "jsscript.h"

using namespace js;

#ifndef TRACE_LOG_DIR
# if defined(_WIN32)
#  define TRACE_LOG_DIR ""
# else
#  define TRACE_LOG_DIR "/tmp/"
# endif
#endif

#if defined(__i386__)
static __inline__ uint64_t
rdtsc(void)
{
    uint64_t x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}
#elif defined(__x86_64__)
static __inline__ uint64_t
rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
}
#elif defined(__powerpc__)
static __inline__ uint64_t
rdtsc(void)
{
    uint64_t result=0;
    uint32_t upper, lower,tmp;
    __asm__ volatile(
            "0:                  \n"
            "\tmftbu   %0           \n"
            "\tmftb    %1           \n"
            "\tmftbu   %2           \n"
            "\tcmpw    %2,%0        \n"
            "\tbne     0b         \n"
            : "=r"(upper),"=r"(lower),"=r"(tmp)
            );
    result = upper;
    result = result<<32;
    result = result|lower;

    return(result);
}
#endif

const char* const TraceLogging::type_name[] = {
    "start,script",
    "stop,script",
    "start,ion_compile",
    "stop,ion_compile",
    "start,yarr_jit_execute",
    "stop,yarr_jit_execute",
    "start,gc",
    "stop,gc",
    "start,minor_gc",
    "stop,minor_gc",
    "start,parser_script",
    "stop,parser_script",
    "start,parser_lazy",
    "stop,parser_lazy",
    "start,parser_function",
    "stop,parser_function",
    "info,engine,interpreter",
    "info,engine,baseline",
    "info,engine,ionmonkey"
};
TraceLogging* TraceLogging::_defaultLogger = NULL;

TraceLogging::TraceLogging()
  : loggingTime(0),
    entries(NULL),
    curEntry(0),
    numEntries(1000000),
    fileno(0),
    out(NULL)
{
}

TraceLogging::~TraceLogging()
{
    if (out != NULL) {
        fclose(out);
        out = NULL;
    }

    if (entries != NULL) {
        flush();
        free(entries);
        entries = NULL;
    }
}

void
TraceLogging::grow()
{
    Entry* nentries = (Entry*) realloc(entries, numEntries*2*sizeof(Entry));

    // Allocating a bigger array failed.
    // Keep using the current storage, but remove all entries by flushing them.
    if (nentries == NULL) {
        flush();
        return;
    }

    entries = nentries;
    numEntries *= 2;
}

void
TraceLogging::log(Type type, const char* file, unsigned int lineno)
{
    uint64_t now = rdtsc();

    // Create array containing the entries if not existing.
    if (entries == NULL) {
        entries = (Entry*) malloc(numEntries*sizeof(Entry));
        if (entries == NULL)
            return;
    }

    // Copy the logging information,
    // because original could already be freed before writing the log file.
    char *copy = NULL;
    if (file != NULL)
        copy = strdup(file);

    entries[curEntry++] = Entry(now - loggingTime, copy, lineno, type);

    // Increase length when not enough place in the array
    if (curEntry >= numEntries)
        grow();

    // Save the time spend logging the information in order to discard this
    // time from the logged time. Especially needed when increasing the array
    // or flushing the information.
    loggingTime += rdtsc()-now;
}

void
TraceLogging::log(Type type, const JS::CompileOptions &options)
{
    this->log(type, options.filename, options.lineno);
}

void
TraceLogging::log(Type type, JSScript* script)
{
    this->log(type, script->filename(), script->lineno);
}

void
TraceLogging::log(const char* log)
{
    this->log(INFO, log, 0);
}

void
TraceLogging::log(Type type)
{
    this->log(type, NULL, 0);
}

void
TraceLogging::flush()
{
    // Open the logging file, when not opened yet.
    if (out == NULL)
        out = fopen(TRACE_LOG_DIR "tracelogging.log", "w");

    // Print all log entries into the file
    for (unsigned int i = 0; i < curEntry; i++) {
        int written;
        if (entries[i].type() == INFO) {
            written = fprintf(out, "INFO,%s\n", entries[i].file());
        } else {
            if (entries[i].file() == NULL) {
                written = fprintf(out, "%llu,%s\n",
                                  (unsigned long long)entries[i].tick(),
                                  type_name[entries[i].type()]);
            } else {
                written = fprintf(out, "%llu,%s,%s:%d\n",
                                  (unsigned long long)entries[i].tick(),
                                  type_name[entries[i].type()],
                                  entries[i].file(),
                                  entries[i].lineno());
            }
        }

        // A logging file can only be 2GB of length (fwrite limit).
        // When we exceed this limit, the writing will fail.
        // In that case try creating a extra file to write the log entries.
        if (written < 0) {
            fclose(out);
            if (fileno >= 9999)
                exit(-1);

            char filename[21 + sizeof(TRACE_LOG_DIR)];
            sprintf (filename, TRACE_LOG_DIR "tracelogging-%d.log", ++fileno);
            out = fopen(filename, "w");
            i--; // Try to print message again
            continue;
        }

        if (entries[i].file() != NULL) {
            free(entries[i].file());
            entries[i].file_ = NULL;
        }
    }
    curEntry = 0;
}

TraceLogging*
TraceLogging::defaultLogger()
{
    if (_defaultLogger == NULL) {
        _defaultLogger = new TraceLogging();
        atexit (releaseDefaultLogger);
    }
    return _defaultLogger;
}

void
TraceLogging::releaseDefaultLogger()
{
    if (_defaultLogger != NULL) {
        delete _defaultLogger;
        _defaultLogger = NULL;
    }
}

/* Helper functions for asm calls */
void
js::TraceLog(TraceLogging* logger, TraceLogging::Type type, JSScript* script)
{
    logger->log(type, script);
}

void
js::TraceLog(TraceLogging* logger, const char* log)
{
    logger->log(log);
}

void
js::TraceLog(TraceLogging* logger, TraceLogging::Type type)
{
    logger->log(type);
}
