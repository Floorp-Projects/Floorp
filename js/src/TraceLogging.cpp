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

const char* const TraceLogging::typeName[] = {
    "1,s",  // start script
    "0,s",  // stop script
    "1,c",  // start ion compilation
    "0,c",  // stop ion compilation
    "1,r",  // start regexp JIT execution
    "0,r",  // stop regexp JIT execution
    "1,G",  // start major GC
    "0,G",  // stop major GC
    "1,g",  // start minor GC
    "0,g",  // stop minor GC
    "1,gS", // start GC sweeping
    "0,gS", // stop GC sweeping
    "1,gA", // start GC allocating
    "0,gA", // stop GC allocating
    "1,ps", // start script parsing
    "0,ps", // stop script parsing
    "1,pl", // start lazy parsing
    "0,pl", // stop lazy parsing
    "1,pf", // start Function parsing
    "0,pf", // stop Function parsing
    "e,i",  // engine interpreter
    "e,b",  // engine baseline
    "e,o"   // engine ionmonkey
};
TraceLogging* TraceLogging::loggers[] = {nullptr, nullptr, nullptr};
bool TraceLogging::atexitSet = false;
uint64_t TraceLogging::startupTime = 0;

TraceLogging::TraceLogging(Logger id)
  : nextTextId(1),
    entries(nullptr),
    curEntry(0),
    numEntries(1000000),
    fileno(0),
    out(nullptr),
    id(id)
{
    textMap.init();
}

TraceLogging::~TraceLogging()
{
    if (entries) {
        flush();
        js_free(entries);
        entries = nullptr;
    }

    if (out) {
        fclose(out);
        out = nullptr;
    }
}

void
TraceLogging::grow()
{
    Entry* nentries = (Entry*) js_realloc(entries, numEntries*2*sizeof(Entry));

    // Allocating a bigger array failed.
    // Keep using the current storage, but remove all entries by flushing them.
    if (!nentries) {
        flush();
        return;
    }

    entries = nentries;
    numEntries *= 2;
}

void
TraceLogging::log(Type type, const char* text /* = nullptr */, unsigned int number /* = 0 */)
{
    uint64_t now = rdtsc() - startupTime;

    // Create array containing the entries if not existing.
    if (!entries) {
        entries = (Entry*) js_malloc(numEntries*sizeof(Entry));
        if (!entries)
            return;
    }

    uint32_t textId = 0;
    char *text_ = nullptr;

    if (text) {
        TextHashMap::AddPtr p = textMap.lookupForAdd(text);
        if (!p) {
            // Copy the text, because original could already be freed before writing the log file.
            text_ = strdup(text);
            if (!text_)
                return;
            textId = nextTextId++;
            if (!textMap.add(p, text, textId))
                return;
        } else {
            textId = p->value();
        }
    }

    entries[curEntry++] = Entry(now, text_, textId, number, type);

    // Increase length when not enough place in the array
    if (curEntry >= numEntries)
        grow();
}

void
TraceLogging::log(Type type, const JS::ReadOnlyCompileOptions &options)
{
    this->log(type, options.filename(), options.lineno);
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
TraceLogging::flush()
{
    // Open the logging file, when not opened yet.
    if (!out) {
        switch(id) {
          case DEFAULT:
            out = fopen(TRACE_LOG_DIR "tracelogging.log", "w");
            break;
          case ION_BACKGROUND_COMPILER:
            out = fopen(TRACE_LOG_DIR "tracelogging-compile.log", "w");
            break;
          case GC_BACKGROUND:
            out = fopen(TRACE_LOG_DIR "tracelogging-gc.log", "w");
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Bad trigger");
            return;
        }
    }

    // Print all log entries into the file
    for (unsigned int i = 0; i < curEntry; i++) {
        Entry entry = entries[i];
        int written;
        if (entry.type() == INFO) {
            written = fprintf(out, "I,%s\n", entry.text());
        } else {
            if (entry.textId() > 0) {
                if (entry.text()) {
                    written = fprintf(out, "%llu,%s,%s,%d\n",
                                      (unsigned long long)entry.tick(),
                                      typeName[entry.type()],
                                      entry.text(),
                                      entry.lineno());
                } else {
                    written = fprintf(out, "%llu,%s,%d,%d\n",
                                      (unsigned long long)entry.tick(),
                                      typeName[entry.type()],
                                      entry.textId(),
                                      entry.lineno());
                }
            } else {
                written = fprintf(out, "%llu,%s\n",
                                  (unsigned long long)entry.tick(),
                                  typeName[entry.type()]);
            }
        }

        // A logging file can only be 2GB of length (fwrite limit).
        if (written < 0) {
            fprintf(stderr, "Writing tracelog to disk failed,");
            fprintf(stderr, "probably because the file would've exceeded the maximum size of 2GB");
            fclose(out);
            exit(-1);
        }

        if (entries[i].text() != nullptr) {
            js_free(entries[i].text());
            entries[i].text_ = nullptr;
        }
    }
    curEntry = 0;
}

TraceLogging*
TraceLogging::getLogger(Logger id)
{
    if (!loggers[id]) {
        loggers[id] = new TraceLogging(id);
        if (!atexitSet) {
            startupTime = rdtsc();
            atexit (releaseLoggers);
            atexitSet = true;
        }
    }

    return loggers[id];
}

void
TraceLogging::releaseLoggers()
{
    for (size_t i = 0; i < LAST_LOGGER; i++) {
        if (!loggers[i])
            continue;

        delete loggers[i];
        loggers[i] = nullptr;
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
