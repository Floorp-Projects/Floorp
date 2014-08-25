/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TraceLogging.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Endian.h"

#include <string.h>

#include "jsapi.h"
#include "jsscript.h"

#include "jit/CompileWrappers.h"
#include "vm/Runtime.h"

#include "jit/IonFrames-inl.h"

using namespace js;
using namespace js::jit;
using mozilla::NativeEndian;

#ifndef TRACE_LOG_DIR
# if defined(_WIN32)
#  define TRACE_LOG_DIR ""
# else
#  define TRACE_LOG_DIR "/tmp/"
# endif
#endif

#if defined(_WIN32)
#include <intrin.h>
static __inline uint64_t
rdtsc(void)
{
    return __rdtsc();
}
#elif defined(__i386__)
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

    return result;
}
#endif

TraceLogging traceLoggers;

static const char* const text[] =
{
    "TraceLogger failed to process text",
#define NAME(x) #x,
    TRACELOGGER_TEXT_ID_LIST(NAME)
#undef NAME
};

TraceLogger::TraceLogger()
 : enabled(0),
   failed(false),
   nextTextId(0),
   treeOffset(0),
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

    MOZ_ASSERT(loggerId <= 999);

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
    treeEntry.setStart(start);
    treeEntry.setStop(0);
    treeEntry.setTextId(0);
    treeEntry.setHasChildren(false);
    treeEntry.setNextId(0);

    StackEntry &stackEntry = stack.pushUninitialized();
    stackEntry.setTreeId(0);
    stackEntry.setLastChildId(0);
    stackEntry.setActive(true);

    int written = fprintf(dictFile, "[");
    if (written < 0)
        fprintf(stderr, "TraceLogging: Error while writing.\n");

    // Eagerly create the default textIds, to match their Tracelogger::TextId.
    for (uint32_t i = 0; i < LAST; i++) {
        mozilla::DebugOnly<uint32_t> textId = createTextId(text[i]);
        MOZ_ASSERT(textId == i);
    }

    enabled = 1;
    return true;
}

bool
TraceLogger::enable()
{
    if (enabled > 0) {
        enabled++;
        return true;
    }

    if (failed)
        return false;

    enabled = 1;
    return true;
}

bool
TraceLogger::enable(JSContext *cx)
{
    if (!enable())
        return false;

    if (enabled == 1) {
        // Get the top Activation to log the top script/pc (No inlined frames).
        Activation *act = cx->mainThread().activation();
        while (act && (act->cx() != cx || (act->isJit() && !act->asJit()->isActive())))
            act = act->prev();

        if (!act) {
            failed = true;
            enabled = 0;
            return false;
        }

        JSScript *script = nullptr;
        int32_t engine = 0;

        if (act->isJit()) {
            JSRuntime *rt = cx->runtime();

            JitFrameIterator it(rt->mainThread.jitTop, SequentialExecution);

            JS_ASSERT(it.type() == JitFrame_Exit);
            ++it;

            if (it.type() == JitFrame_Rectifier ||
                it.type() == JitFrame_Unwound_Rectifier)
            {
                ++it;
                JS_ASSERT(it.type() == JitFrame_BaselineStub ||
                          it.type() == JitFrame_BaselineJS ||
                          it.type() == JitFrame_IonJS);
            }
            if (it.type() == JitFrame_BaselineStub ||
                it.type() == JitFrame_Unwound_BaselineStub)
            {
                ++it;
                JS_ASSERT(it.type() == JitFrame_BaselineJS);
            }

            script = it.script();
            engine = it.isIonJS() ? IonMonkey : Baseline;
        } else {
            JS_ASSERT(act->isInterpreter());
            InterpreterFrame *fp = act->asInterpreter()->current();
            JS_ASSERT(!fp->runningInJit());

            script = fp->script();
            engine = Interpreter;
            if (script->compartment() != cx->compartment()) {
                failed = true;
                enabled = 0;
                return false;
            }
        }

        startEvent(createTextId(script));
        startEvent(engine);
    }

    return true;
}

bool
TraceLogger::disable()
{
    if (failed)
        return false;

    if (enabled == 0)
        return true;

    if (enabled > 1) {
        enabled--;
        return true;
    }

    JS_ASSERT(enabled == 1);
    while (stack.size() > 1)
        stopEvent();

    enabled = 0;

    return true;
}

bool
TraceLogger::flush()
{
    MOZ_ASSERT(!failed);

    if (treeFile) {
        // Format data in big endian.
        for (size_t i = 0; i < tree.size(); i++)
            entryToBigEndian(&tree[i]);

        int success = fseek(treeFile, 0, SEEK_END);
        if (success != 0)
            return false;

        size_t bytesWritten = fwrite(tree.data(), sizeof(TreeEntry), tree.size(), treeFile);
        if (bytesWritten < tree.size())
            return false;

        treeOffset += tree.lastEntryId();
        tree.clear();
    }

    if (eventFile) {
        // Format data in big endian
        for (size_t i = 0; i < events.size(); i++) {
            events[i].time = NativeEndian::swapToBigEndian(events[i].time);
            events[i].textId = NativeEndian::swapToBigEndian(events[i].textId);
        }

        size_t bytesWritten = fwrite(events.data(), sizeof(EventEntry), events.size(), eventFile);
        if (bytesWritten < events.size())
            return false;
        events.clear();
    }

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

    if (!failed && treeFile) {
        // Make sure every start entry has a corresponding stop value.
        // We temporary enable logging for this. Stop doesn't need any extra data,
        // so is safe to do, even when we encountered OOM.
        enabled = 1;
        while (stack.size() > 1)
            stopEvent();
        enabled = 0;
    }

    if (!failed && !flush()) {
        fprintf(stderr, "TraceLogging: Couldn't write the data to disk.\n");
        enabled = 0;
        failed = true;
    }

    if (treeFile) {
        fclose(treeFile);
        treeFile = nullptr;
    }

    if (eventFile) {
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
    if (!script->filename())
        return createTextId("");

    assertNoQuotes(script->filename());

    PointerHashMap::AddPtr p = pointerMap.lookupForAdd(script);
    if (p)
        return p->value();

    uint32_t textId = nextTextId++;
    if (!pointerMap.add(p, script, textId))
        return TraceLogger::TL_Error;

    int written;
    if (textId > 0) {
        written = fprintf(dictFile, ",\n\"script %s:%u:%u\"", script->filename(),
                          (unsigned)script->lineno(), (unsigned)script->column());
    } else {
        written = fprintf(dictFile, "\"script %s:%u:%u\"", script->filename(),
                          (unsigned)script->lineno(), (unsigned)script->column());
    }

    if (written < 0)
        return TraceLogger::TL_Error;

    return textId;
}

uint32_t
TraceLogger::createTextId(const JS::ReadOnlyCompileOptions &compileOptions)
{
    if (!compileOptions.filename())
        return createTextId("");

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
    if (enabled == 0)
        return;

    if (!events.ensureSpaceBeforeAdd()) {
        fprintf(stderr, "TraceLogging: Disabled a tracelogger due to OOM.\n");
        enabled = 0;
        return;
    }

    uint64_t time = rdtsc() - traceLoggers.startupTime;

    EventEntry &entry = events.pushUninitialized();
    entry.time = time;
    entry.textId = id;
}

void
TraceLogger::entryToBigEndian(TreeEntry *entry)
{
    entry->start_ = NativeEndian::swapToBigEndian(entry->start_);
    entry->stop_ = NativeEndian::swapToBigEndian(entry->stop_);
    uint32_t data = (entry->u.s.textId_ << 1) + entry->u.s.hasChildren_;
    entry->u.value_ = NativeEndian::swapToBigEndian(data);
    entry->nextId_ = NativeEndian::swapToBigEndian(entry->nextId_);
}

void
TraceLogger::entryToSystemEndian(TreeEntry *entry)
{
    entry->start_ = NativeEndian::swapFromBigEndian(entry->start_);
    entry->stop_ = NativeEndian::swapFromBigEndian(entry->stop_);

    uint32_t data = NativeEndian::swapFromBigEndian(entry->u.value_);
    entry->u.s.textId_ = data >> 1;
    entry->u.s.hasChildren_ = data & 0x1;

    entry->nextId_ = NativeEndian::swapFromBigEndian(entry->nextId_);
}

bool
TraceLogger::getTreeEntry(uint32_t treeId, TreeEntry *entry)
{
    // Entry is still in memory
    if (treeId >= treeOffset) {
        *entry = tree[treeId];
        return true;
    }

    int success = fseek(treeFile, treeId * sizeof(TreeEntry), SEEK_SET);
    if (success != 0)
        return false;

    size_t itemsRead = fread((void *)entry, sizeof(TreeEntry), 1, treeFile);
    if (itemsRead < 1)
        return false;

    entryToSystemEndian(entry);
    return true;
}

bool
TraceLogger::saveTreeEntry(uint32_t treeId, TreeEntry *entry)
{
    int success = fseek(treeFile, treeId * sizeof(TreeEntry), SEEK_SET);
    if (success != 0)
        return false;

    entryToBigEndian(entry);

    size_t itemsWritten = fwrite(entry, sizeof(TreeEntry), 1, treeFile);
    if (itemsWritten < 1)
        return false;

    return true;
}

bool
TraceLogger::updateHasChildren(uint32_t treeId, bool hasChildren)
{
    if (treeId < treeOffset) {
        TreeEntry entry;
        if (!getTreeEntry(treeId, &entry))
            return false;
        entry.setHasChildren(hasChildren);
        if (!saveTreeEntry(treeId, &entry))
            return false;
        return true;
    }

    tree[treeId - treeOffset].setHasChildren(hasChildren);
    return true;
}

bool
TraceLogger::updateNextId(uint32_t treeId, uint32_t nextId)
{
    if (treeId < treeOffset) {
        TreeEntry entry;
        if (!getTreeEntry(treeId, &entry))
            return false;
        entry.setNextId(nextId);
        if (!saveTreeEntry(treeId, &entry))
            return false;
        return true;
    }

    tree[treeId - treeOffset].setNextId(nextId);
    return true;
}

bool
TraceLogger::updateStop(uint32_t treeId, uint64_t timestamp)
{
    if (treeId < treeOffset) {
        TreeEntry entry;
        if (!getTreeEntry(treeId, &entry))
            return false;
        entry.setStop(timestamp);
        if (!saveTreeEntry(treeId, &entry))
            return false;
        return true;
    }

    tree[treeId - treeOffset].setStop(timestamp);
    return true;
}

void
TraceLogger::startEvent(uint32_t id)
{
    if (failed || enabled == 0)
        return;

    if (!tree.hasSpaceForAdd()){
        uint64_t start = rdtsc() - traceLoggers.startupTime;
        if (!tree.ensureSpaceBeforeAdd()) {
            if (!flush()) {
                fprintf(stderr, "TraceLogging: Couldn't write the data to disk.\n");
                enabled = 0;
                failed = true;
                return;
            }
        }

        // Log the time it took to flush the events as being from the
        // Tracelogger.
        if (!startEvent(TraceLogger::TL, start)) {
            fprintf(stderr, "TraceLogging: Failed to start an event.\n");
            enabled = 0;
            failed = true;
            return;
        }
        stopEvent();
    }

    uint64_t start = rdtsc() - traceLoggers.startupTime;
    if (!startEvent(id, start)) {
        fprintf(stderr, "TraceLogging: Failed to start an event.\n");
        enabled = 0;
        failed = true;
        return;
    }
}

TraceLogger::StackEntry &
TraceLogger::getActiveAncestor()
{
    uint32_t parentId = stack.lastEntryId();
    while (!stack[parentId].active())
        parentId--;
    return stack[parentId];
}

bool
TraceLogger::startEvent(uint32_t id, uint64_t timestamp)
{
    if (!stack.ensureSpaceBeforeAdd())
        return false;

    // When a textId is disabled, a stack entry still needs to be pushed,
    // together with an annotation that nothing needs to get done when receiving
    // the stop event.
    if (!traceLoggers.isTextIdEnabled(id)) {
        StackEntry &stackEntry = stack.pushUninitialized();
        stackEntry.setActive(false);
        return true;
    }

    // Patch up the tree to be correct. There are two scenarios:
    // 1) Parent has no children yet. So update parent to include children.
    // 2) Parent has already children. Update last child to link to the new
    //    child.
    StackEntry &parent = getActiveAncestor();
#ifdef DEBUG
    TreeEntry entry;
    if (!getTreeEntry(parent.treeId(), &entry))
        return false;
#endif

    if (parent.lastChildId() == 0) {
        MOZ_ASSERT(!entry.hasChildren());
        MOZ_ASSERT(parent.treeId() == tree.lastEntryId() + treeOffset);

        if (!updateHasChildren(parent.treeId()))
            return false;
    } else {
        MOZ_ASSERT(entry.hasChildren());

        if (!updateNextId(parent.lastChildId(), tree.size() + treeOffset))
            return false;
    }

    // Add a new tree entry.
    TreeEntry &treeEntry = tree.pushUninitialized();
    treeEntry.setStart(timestamp);
    treeEntry.setStop(0);
    treeEntry.setTextId(id);
    treeEntry.setHasChildren(false);
    treeEntry.setNextId(0);

    // Add a new stack entry.
    StackEntry &stackEntry = stack.pushUninitialized();
    stackEntry.setTreeId(tree.lastEntryId() + treeOffset);
    stackEntry.setLastChildId(0);
    stackEntry.setActive(true);

    // Set the last child of the parent to this newly added entry.
    parent.setLastChildId(tree.lastEntryId() + treeOffset);

    return true;
}

void
TraceLogger::stopEvent(uint32_t id)
{
#ifdef DEBUG
    if (stack.size() > 1) {
        TreeEntry entry;
        MOZ_ASSERT_IF(stack.lastEntry().active(), getTreeEntry(stack.lastEntry().treeId(), &entry));
        MOZ_ASSERT_IF(stack.lastEntry().active(), entry.textId() == id);
    }
#endif
    stopEvent();
}

void
TraceLogger::stopEvent()
{
    if (enabled > 0 && stack.lastEntry().active()) {
        uint64_t stop = rdtsc() - traceLoggers.startupTime;
        if (!updateStop(stack.lastEntry().treeId(), stop)) {
            fprintf(stderr, "TraceLogging: Failed to stop an event.\n");
            enabled = 0;
            failed = true;
            return;
        }
    }
    if (stack.size() == 1) {
        if (enabled == 0)
            return;

        // Forcefully disable logging. We have no stack information anymore.
        enabled = 1;
        disable();
        return;
    }
    stack.pop();
}

TraceLogging::TraceLogging()
{
    initialized = false;
    enabled = 0;
    mainThreadEnabled = true;
    offThreadEnabled = true;
    loggerId = 0;

    lock = PR_NewLock();
    if (!lock)
        MOZ_CRASH();
}

TraceLogging::~TraceLogging()
{
    if (out) {
        fprintf(out, "]");
        fclose(out);
        out = nullptr;
    }

    for (size_t i = 0; i < mainThreadLoggers.length(); i++)
        delete mainThreadLoggers[i];

    mainThreadLoggers.clear();

    if (threadLoggers.initialized()) {
        for (ThreadLoggerHashMap::Range r = threadLoggers.all(); !r.empty(); r.popFront())
            delete r.front().value();

        threadLoggers.finish();
    }

    if (lock) {
        PR_DestroyLock(lock);
        lock = nullptr;
    }

    enabled = 0;
}

static bool
ContainsFlag(const char *str, const char *flag)
{
    size_t flaglen = strlen(flag);
    const char *index = strstr(str, flag);
    while (index) {
        if ((index == str || index[-1] == ',') && (index[flaglen] == 0 || index[flaglen] == ','))
            return true;
        index = strstr(index + flaglen, flag);
    }
    return false;
}

bool
TraceLogging::lazyInit()
{
    if (initialized)
        return enabled > 0;

    initialized = true;

    out = fopen(TRACE_LOG_DIR "tl-data.json", "w");
    if (!out)
        return false;
    fprintf(out, "[");

    if (!threadLoggers.init())
        return false;

    const char *env = getenv("TLLOG");
    if (!env)
        env = "";

    if (strstr(env, "help")) {
        fflush(nullptr);
        printf(
            "\n"
            "usage: TLLOG=option,option,option,... where options can be:\n"
            "\n"
            "Collections:\n"
            "  Default        Output all default\n"
            "  IonCompiler    Output all information about compilation\n"
            "\n"
            "Specific log items:\n"
        );
        for (uint32_t i = 1; i < TraceLogger::LAST; i++) {
            printf("  %s\n", text[i]);
        }
        printf("\n");
        exit(0);
        /*NOTREACHED*/
    }

    for (uint32_t i = 1; i < TraceLogger::LAST; i++)
        enabledTextIds[i] = ContainsFlag(env, text[i]);

    enabledTextIds[TraceLogger::TL_Error] = true;
    enabledTextIds[TraceLogger::TL] = true;

    if (ContainsFlag(env, "Default") || strlen(env) == 0) {
        enabledTextIds[TraceLogger::Bailout] = true;
        enabledTextIds[TraceLogger::Baseline] = true;
        enabledTextIds[TraceLogger::BaselineCompilation] = true;
        enabledTextIds[TraceLogger::GC] = true;
        enabledTextIds[TraceLogger::GCAllocation] = true;
        enabledTextIds[TraceLogger::GCSweeping] = true;
        enabledTextIds[TraceLogger::Interpreter] = true;
        enabledTextIds[TraceLogger::IonCompilation] = true;
        enabledTextIds[TraceLogger::IonLinking] = true;
        enabledTextIds[TraceLogger::IonMonkey] = true;
        enabledTextIds[TraceLogger::MinorGC] = true;
        enabledTextIds[TraceLogger::ParserCompileFunction] = true;
        enabledTextIds[TraceLogger::ParserCompileLazy] = true;
        enabledTextIds[TraceLogger::ParserCompileScript] = true;
        enabledTextIds[TraceLogger::IrregexpCompile] = true;
        enabledTextIds[TraceLogger::IrregexpExecute] = true;
    }

    if (ContainsFlag(env, "IonCompiler") || strlen(env) == 0) {
        enabledTextIds[TraceLogger::IonCompilation] = true;
        enabledTextIds[TraceLogger::IonLinking] = true;
        enabledTextIds[TraceLogger::FoldTests] = true;
        enabledTextIds[TraceLogger::SplitCriticalEdges] = true;
        enabledTextIds[TraceLogger::RenumberBlocks] = true;
        enabledTextIds[TraceLogger::DominatorTree] = true;
        enabledTextIds[TraceLogger::PhiAnalysis] = true;
        enabledTextIds[TraceLogger::ApplyTypes] = true;
        enabledTextIds[TraceLogger::ParallelSafetyAnalysis] = true;
        enabledTextIds[TraceLogger::AliasAnalysis] = true;
        enabledTextIds[TraceLogger::GVN] = true;
        enabledTextIds[TraceLogger::UCE] = true;
        enabledTextIds[TraceLogger::LICM] = true;
        enabledTextIds[TraceLogger::RangeAnalysis] = true;
        enabledTextIds[TraceLogger::LoopUnrolling] = true;
        enabledTextIds[TraceLogger::EffectiveAddressAnalysis] = true;
        enabledTextIds[TraceLogger::EliminateDeadCode] = true;
        enabledTextIds[TraceLogger::EdgeCaseAnalysis] = true;
        enabledTextIds[TraceLogger::EliminateRedundantChecks] = true;
        enabledTextIds[TraceLogger::GenerateLIR] = true;
        enabledTextIds[TraceLogger::RegisterAllocation] = true;
        enabledTextIds[TraceLogger::GenerateCode] = true;
    }

    const char *options = getenv("TLOPTIONS");
    if (options) {
        if (strstr(options, "help")) {
            fflush(nullptr);
            printf(
                "\n"
                "usage: TLOPTIONS=option,option,option,... where options can be:\n"
                "\n"
                "  DisableMainThread        Don't start logging the mainThread automatically.\n"
                "  DisableOffThread         Don't start logging the off mainThread automatically.\n"
            );
            printf("\n");
            exit(0);
            /*NOTREACHED*/
        }

        if (strstr(options, "DisableMainThread"))
           mainThreadEnabled = false;
        if (strstr(options, "DisableOffThread"))
           offThreadEnabled = false;
    }

    startupTime = rdtsc();
    enabled = 1;
    return true;
}

TraceLogger *
js::TraceLoggerForMainThread(CompileRuntime *runtime)
{
    return traceLoggers.forMainThread(runtime);
}

TraceLogger *
TraceLogging::forMainThread(CompileRuntime *runtime)
{
    return forMainThread(runtime->mainThread());
}

TraceLogger *
js::TraceLoggerForMainThread(JSRuntime *runtime)
{
    return traceLoggers.forMainThread(runtime);
}

TraceLogger *
TraceLogging::forMainThread(JSRuntime *runtime)
{
    return forMainThread(&runtime->mainThread);
}

TraceLogger *
TraceLogging::forMainThread(PerThreadData *mainThread)
{
    if (!mainThread->traceLogger) {
        AutoTraceLoggingLock lock(this);

        if (!lazyInit())
            return nullptr;

        TraceLogger *logger = create();
        mainThread->traceLogger = logger;

        if (!mainThreadLoggers.append(logger))
            return nullptr;

        if (!mainThreadEnabled)
            logger->disable();
    }

    return mainThread->traceLogger;
}

TraceLogger *
js::TraceLoggerForCurrentThread()
{
    PRThread *thread = PR_GetCurrentThread();
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

    if (!offThreadEnabled)
        logger->disable();

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

    loggerId++;

    int written = fprintf(out, "{\"tree\":\"tl-tree.%d.tl\", \"events\":\"tl-event.%d.tl\", \"dict\":\"tl-dict.%d.json\", \"treeFormat\":\"64,64,31,1,32\"}",
                          loggerId, loggerId, loggerId);
    if (written < 0)
        fprintf(stderr, "TraceLogging: Error while writing.\n");


    TraceLogger *logger = new TraceLogger();
    if (!logger)
        return nullptr;

    if (!logger->init(loggerId)) {
        delete logger;
        return nullptr;
    }

    return logger;
}

bool
js::TraceLogTextIdEnabled(uint32_t textId)
{
    return traceLoggers.isTextIdEnabled(textId);
}
