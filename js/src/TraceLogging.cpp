/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TraceLogging.h"

#include <string.h>

#include "jsapi.h"
#include "jsscript.h"

#include "vm/Runtime.h"

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

TraceLogging traceLoggers;

// The text that will get logged for eagerly created logged text.
// When adding/removing something here, you must update the enum
// Tracelogger::TextId in TraceLogging.h too.
const char* const text[] = {
    "TraceLogger failed to process text",
    "Bailout",
    "Baseline",
    "GC",
    "GCAllocating",
    "GCSweeping",
    "Interpreter",
    "Invalidation",
    "IonCompile",
    "IonLink",
    "IonMonkey",
    "MinorGC",
    "ParserCompileFunction",
    "ParserCompileLazy",
    "ParserCompileScript",
    "TraceLogger",
    "YarrCompile",
    "YarrInterpret",
    "YarrJIT"
};

TraceLogger::TraceLogger()
 : enabled(false),
   nextTextId(0),
   top(nullptr)
{ }

bool
TraceLogger::init(uint32_t loggerId)
{
    if (!pointerMap.init())
        return false;
    if (!tree.init())
        return false;
    if (!stack.init())
        return false;
    if (!events.init())
        return false;

    JS_ASSERT(loggerId <= 999);

    char dictFilename[sizeof TRACE_LOG_DIR "tl-dict.100.json"];
    sprintf(dictFilename, TRACE_LOG_DIR "tl-dict.%d.json", loggerId);
    dictFile = fopen(dictFilename, "w");
    if (!dictFile)
        return false;

    char treeFilename[sizeof TRACE_LOG_DIR "tl-tree.100.tl"];
    sprintf(treeFilename, TRACE_LOG_DIR "tl-tree.%d.tl", loggerId);
    treeFile = fopen(treeFilename, "wb");
    if (!treeFile) {
        fclose(dictFile);
        dictFile = nullptr;
        return false;
    }

    char eventFilename[sizeof TRACE_LOG_DIR "tl-event.100.tl"];
    sprintf(eventFilename, TRACE_LOG_DIR "tl-event.%d.tl", loggerId);
    eventFile = fopen(eventFilename, "wb");
    if (!eventFile) {
        fclose(dictFile);
        fclose(treeFile);
        dictFile = nullptr;
        treeFile = nullptr;
        return false;
    }

    uint64_t start = rdtsc() - traceLoggers.startupTime;

    TreeEntry &treeEntry = tree.pushUninitialized();
    treeEntry.start = start;
    treeEntry.stop = 0;
    treeEntry.u.s.textId = 0;
    treeEntry.u.s.hasChildren = false;
    treeEntry.nextId = 0;

    StackEntry &stackEntry = stack.pushUninitialized();
    stackEntry.treeId = 0;
    stackEntry.lastChildId = 0;

    int written = fprintf(dictFile, "[");
    if (written < 0)
        fprintf(stderr, "TraceLogging: Error while writing.\n");

    // Eagerly create the default textIds, to match their Tracelogger::TextId.
    for (uint32_t i = 0; i < LAST; i++) {
        uint32_t textId = createTextId(text[i]);
        JS_ASSERT(textId == i);
    }

    enabled = true;
    return true;
}

TraceLogger::~TraceLogger()
{
    // Write dictionary to disk
    if (dictFile) {
        int written = fprintf(dictFile, "]");
        if (written < 0)
            fprintf(stderr, "TraceLogging: Error while writing.\n");
        fclose(dictFile);

        dictFile = nullptr;
    }

    // Write tree of logged events to disk.
    if (treeFile) {
        // Make sure every start entry has a corresponding stop value.
        // We temporary enable logging for this. Stop doesn't need any extra data,
        // so is safe to do, even when we encountered OOM.
        enabled = true;
        while (stack.size() > 0)
            stopEvent();
        enabled = false;

        // Format data in big endian.
        for (uint32_t i = 0; i < tree.size(); i++) {
            tree[i].start = htobe64(tree[i].start);
            tree[i].stop = htobe64(tree[i].stop);
            tree[i].u.value = htobe32((tree[i].u.s.textId << 1) + tree[i].u.s.hasChildren);
            tree[i].nextId = htobe32(tree[i].nextId);
        }

        size_t bytesWritten = fwrite(tree.data(), sizeof(TreeEntry), tree.size(), treeFile);
        if (bytesWritten < tree.size())
            fprintf(stderr, "TraceLogging: Couldn't write the full tree to disk.\n");
        tree.clear();
        fclose(treeFile);

        treeFile = nullptr;
    }

    // Write details for all log entries to disk.
    if (eventFile) {
        // Format data in big endian
        for (uint32_t i = 0; i < events.size(); i++) {
            events[i].time = htobe64(events[i].time);
            events[i].textId = htobe64(events[i].textId);
        }

        size_t bytesWritten = fwrite(events.data(), sizeof(EventEntry), events.size(), eventFile);
        if (bytesWritten < events.size())
            fprintf(stderr, "TraceLogging: Couldn't write all event entries to disk.\n");
        events.clear();
        fclose(eventFile);

        eventFile = nullptr;
    }
}

uint32_t
TraceLogger::createTextId(const char *text)
{
    assertNoQuotes(text);

    PointerHashMap::AddPtr p = pointerMap.lookupForAdd((const void *)text);
    if (p)
        return p->value();

    uint32_t textId = nextTextId++;
    if (!pointerMap.add(p, text, textId))
        return TraceLogger::TL_Error;

    int written;
    if (textId > 0)
        written = fprintf(dictFile, ",\n\"%s\"", text);
    else
        written = fprintf(dictFile, "\"%s\"", text);

    if (written < 0)
        return TraceLogger::TL_Error;

    return textId;
}

uint32_t
TraceLogger::createTextId(JSScript *script)
{
    assertNoQuotes(script->filename());

    PointerHashMap::AddPtr p = pointerMap.lookupForAdd(script);
    if (p)
        return p->value();

    uint32_t textId = nextTextId++;
    if (!pointerMap.add(p, script, textId))
        return TraceLogger::TL_Error;

    int written;
    if (textId > 0) {
        written = fprintf(dictFile, ",\n\"script %s:%d:%d\"", script->filename(),
                          script->lineno(), script->column());
    } else {
        written = fprintf(dictFile, "\"script %s:%d:%d\"", script->filename(),
                          script->lineno(), script->column());
    }

    if (written < 0)
        return TraceLogger::TL_Error;

    return textId;
}

uint32_t
TraceLogger::createTextId(const JS::ReadOnlyCompileOptions &compileOptions)
{
    assertNoQuotes(compileOptions.filename());

    PointerHashMap::AddPtr p = pointerMap.lookupForAdd(&compileOptions);
    if (p)
        return p->value();

    uint32_t textId = nextTextId++;
    if (!pointerMap.add(p, &compileOptions, textId))
        return TraceLogger::TL_Error;

    int written;
    if (textId > 0) {
        written = fprintf(dictFile, ",\n\"script %s:%d:%d\"", compileOptions.filename(),
                          compileOptions.lineno, compileOptions.column);
    } else {
        written = fprintf(dictFile, "\"script %s:%d:%d\"", compileOptions.filename(),
                          compileOptions.lineno, compileOptions.column);
    }

    if (written < 0)
        return TraceLogger::TL_Error;

    return textId;
}

void
TraceLogger::logTimestamp(uint32_t id)
{
    if (!enabled)
        return;

    if (!events.ensureSpaceBeforeAdd()) {
        fprintf(stderr, "TraceLogging: Disabled a tracelogger due to OOM.\n");
        enabled = false;
        return;
    }

    uint64_t time = rdtsc() - traceLoggers.startupTime;

    EventEntry &entry = events.pushUninitialized();
    entry.time = time;
    entry.textId = id;
}

void
TraceLogger::startEvent(uint32_t id)
{
    if (!enabled)
        return;

    if (!tree.ensureSpaceBeforeAdd() || !stack.ensureSpaceBeforeAdd()) {
        fprintf(stderr, "TraceLogging: Disabled a tracelogger due to OOM.\n");
        enabled = false;
        return;
    }

    uint64_t start = rdtsc() - traceLoggers.startupTime;

    // Patch up the tree to be correct. There are two scenarios:
    // 1) Parent has no children yet. So update parent to include children.
    // 2) Parent has already children. Update last child to link to the new
    //    child.
    StackEntry &parent = stack.current();
    if (parent.lastChildId == 0) {
        JS_ASSERT(tree[parent.treeId].u.s.hasChildren == 0);
        JS_ASSERT(parent.treeId == tree.currentId());

        tree[parent.treeId].u.s.hasChildren = 1;
    } else {
        JS_ASSERT(tree[parent.treeId].u.s.hasChildren == 1);

        tree[parent.lastChildId].nextId = tree.nextId();
    }

    // Add a new tree entry.
    TreeEntry &treeEntry = tree.pushUninitialized();
    treeEntry.start = start;
    treeEntry.stop = 0;
    treeEntry.u.s.textId = id;
    treeEntry.u.s.hasChildren = false;
    treeEntry.nextId = 0;

    // Add a new stack entry.
    StackEntry &stackEntry = stack.pushUninitialized();
    stackEntry.treeId = tree.currentId();
    stackEntry.lastChildId = 0;

    // Set the last child of the parent to this newly added entry.
    parent.lastChildId = tree.currentId();
}

void
TraceLogger::stopEvent(uint32_t id)
{
    MOZ_ASSERT_IF(enabled, tree[stack.current().treeId].u.s.textId == id);
    stopEvent();
}

void
TraceLogger::stopEvent()
{
    if (!enabled)
        return;

    uint64_t stop = rdtsc() - traceLoggers.startupTime;
    tree[stack.current().treeId].stop = stop;
    stack.pop();
}

TraceLogging::TraceLogging()
{
    initialized = false;
    enabled = false;
    loggerId = 0;

#ifdef JS_THREADSAFE
    lock = PR_NewLock();
    if (!lock)
        MOZ_CRASH();
#endif
}

TraceLogging::~TraceLogging()
{
    if (out) {
        fprintf(out, "]");
        fclose(out);
        out = nullptr;
    }

    if (threadLoggers.initialized()) {
        for (ThreadLoggerHashMap::Range r = threadLoggers.all(); !r.empty(); r.popFront()) {
            delete r.front().value();
        }

        threadLoggers.finish();
    }

    if (lock) {
        PR_DestroyLock(lock);
        lock = nullptr;
    }

    enabled = false;
}

bool
TraceLogging::lazyInit()
{
    if (initialized)
        return enabled;

    initialized = true;

    out = fopen(TRACE_LOG_DIR "tl-data.json", "w");
    if (!out)
        return false;
    fprintf(out, "[");

    if (!threadLoggers.init())
        return false;

    startupTime = rdtsc();
    enabled = true;
    return true;
}

TraceLogger *
js::TraceLoggerForMainThread(JSRuntime *runtime)
{
    return traceLoggers.forMainThread(runtime);
}

TraceLogger *
TraceLogging::forMainThread(JSRuntime *runtime)
{
    if (!runtime->mainThread.traceLogger) {
        AutoTraceLoggingLock lock(this);

        if (!lazyInit())
            return nullptr;

        runtime->mainThread.traceLogger = create();
    }

    return runtime->mainThread.traceLogger;
}

TraceLogger *
js::TraceLoggerForThread(PRThread *thread)
{
    return traceLoggers.forThread(thread);
}

TraceLogger *
TraceLogging::forThread(PRThread *thread)
{
    AutoTraceLoggingLock lock(this);

    if (!lazyInit())
        return nullptr;

    ThreadLoggerHashMap::AddPtr p = threadLoggers.lookupForAdd(thread);
    if (p)
        return p->value();

    TraceLogger *logger = create();
    if (!logger)
        return nullptr;

    if (!threadLoggers.add(p, thread, logger)) {
        delete logger;
        return nullptr;
    }

    return logger;
}

TraceLogger *
TraceLogging::create()
{
    if (loggerId > 999) {
        fprintf(stderr, "TraceLogging: Can't create more than 999 different loggers.");
        return nullptr;
    }

    if (loggerId > 0) {
        int written = fprintf(out, ",\n");
        if (written < 0)
            fprintf(stderr, "TraceLogging: Error while writing.\n");
    }


    fprintf(out, "{\"tree\":\"tl-tree.%d.tl\", \"events\":\"tl-event.%d.tl\", \"dict\":\"tl-dict.%d.json\", \"treeFormat\":\"64,64,31,1,32\"}",
            loggerId, loggerId, loggerId);

    loggerId++;

    TraceLogger *logger = new TraceLogger();
    if (!logger)
        return nullptr;

    if (!logger->init(loggerId)) {
        delete logger;
        return nullptr;
    }

    return logger;
}
