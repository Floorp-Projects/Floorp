/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TraceLogging.h"

#include "mozilla/DebugOnly.h"

#include <string.h>

#include "jsapi.h"
#include "jsprf.h"
#include "jsscript.h"

#include "jit/CompileWrappers.h"
#include "vm/Runtime.h"
#include "vm/TraceLoggingGraph.h"

#include "jit/IonFrames-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::NativeEndian;

TraceLoggerThreadState traceLoggers;

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

class AutoTraceLoggerThreadStateLock
{
  TraceLoggerThreadState *logging;

  public:
    AutoTraceLoggerThreadStateLock(TraceLoggerThreadState *logging MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : logging(logging)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        PR_Lock(logging->lock);
    }
    ~AutoTraceLoggerThreadStateLock() {
        PR_Unlock(logging->lock);
    }
  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

TraceLoggerThread::TraceLoggerThread()
  : enabled(0),
    failed(false),
    graph(),
    top(nullptr)
{ }

bool
TraceLoggerThread::init()
{
    if (!pointerMap.init())
        return false;
    if (!events.init())
        return false;

    uint64_t start = rdtsc() - traceLoggers.startupTime;
    if (!graph.init(start))
        return false;

    // Minimum amount of capacity needed for operation to allow flushing.
    // Flushing requires space for the actual event and two spaces to log the
    // start and stop of flushing.
    if (!events.ensureSpaceBeforeAdd(3))
        return false;

    // Report the textIds to the graph.
    for (uint32_t i = 0; i < TraceLogger_LastTreeItem; i++) {
        TraceLoggerTextId id = TraceLoggerTextId(i);
        graph.addTextId(i, TLTextIdString(id));
    }

    graph.addTextId(TraceLogger_LastTreeItem, "TraceLogger internal");
    for (uint32_t i = TraceLogger_LastTreeItem + 1; i < TraceLogger_Last; i++) {
        TraceLoggerTextId id = TraceLoggerTextId(i);
        graph.addTextId(i, TLTextIdString(id));
    }

    enabled = 1;
    graph.enable();

    return true;
}

TraceLoggerThread::~TraceLoggerThread()
{
    if (!failed)
        graph.log(events);

    for (uint32_t i = 0; i < extraTextId.length(); i++) {
        js_free(extraTextId[i]);
    }
    extraTextId.clear();
}

bool
TraceLoggerThread::enable()
{
    if (enabled > 0) {
        enabled++;
        return true;
    }

    if (failed)
        return false;

    // TODO: Remove this. This is so the refactor works with mimimal changes,
    // It is the intention to remove this by logging TraceLogger_Enable/TraceLogger_Disable.
    events.clear();

    enabled = 1;
    graph.enable();

    return true;
}

bool
TraceLoggerThread::enable(JSContext *cx)
{
    if (!enable())
        return false;

    if (enabled == 1) {
        // Get the top Activation to log the top script/pc (No inlined frames).
        ActivationIterator iter(cx->runtime());
        Activation *act = iter.activation();

        if (!act) {
            failed = true;
            enabled = 0;
            return false;
        }

        JSScript *script = nullptr;
        int32_t engine = 0;

        if (act->isJit()) {
            JitFrameIterator it(iter);

            while (!it.isScripted() && !it.done())
                ++it;

            MOZ_ASSERT(!it.done());
            MOZ_ASSERT(it.isIonJS() || it.isBaselineJS());

            script = it.script();
            engine = it.isIonJS() ? TraceLogger_IonMonkey : TraceLogger_Baseline;
        } else {
            MOZ_ASSERT(act->isInterpreter());
            InterpreterFrame *fp = act->asInterpreter()->current();
            MOZ_ASSERT(!fp->runningInJit());

            script = fp->script();
            engine = TraceLogger_Interpreter;
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
TraceLoggerThread::disable()
{
    if (failed)
        return false;

    if (enabled == 0)
        return true;

    if (enabled > 1) {
        enabled--;
        return true;
    }

    graph.log(events);
    events.clear();

    uint64_t time = rdtsc() - traceLoggers.startupTime;
    graph.disable(time);

    enabled = 0;

    return true;
}

const char *
TraceLoggerThread::eventText(uint32_t id) {
    if (id < TraceLogger_Last)
        return TLTextIdString(static_cast<TraceLoggerTextId>(id));
    return extraTextId[id - TraceLogger_Last];
}

uint32_t
TraceLoggerThread::createTextId(const char *text)
{
    assertNoQuotes(text);

    PointerHashMap::AddPtr p = pointerMap.lookupForAdd((const void *)text);
    if (p)
        return p->value();

    size_t len = strlen(text);
    char *str = js_pod_malloc<char>(len + 1);
    if (!str)
        return TraceLogger_Error;

    DebugOnly<size_t> ret = JS_snprintf(str, len + 1, "%s", text);
    MOZ_ASSERT(ret == len);

    if (!extraTextId.append(str)) {
        js_free(str);
        return TraceLogger_Error;
    }

    uint32_t textId = extraTextId.length() - 1 + TraceLogger_Last;
    if (!pointerMap.add(p, text, textId))
        return TraceLogger_Error;

    graph.addTextId(textId, text);
    return textId;
}

uint32_t
TraceLoggerThread::createTextId(const char *filename, size_t lineno, size_t colno, const void *ptr)
{
    if (!filename)
        filename = "<unknown>";

    assertNoQuotes(filename);

    // Only log scripts when enabled otherwise return the global Scripts textId,
    // which will get filtered out.
    if (!traceLoggers.isTextIdEnabled(TraceLogger_Scripts))
        return TraceLogger_Scripts;

    PointerHashMap::AddPtr p = pointerMap.lookupForAdd(ptr);
    if (p)
        return p->value();

    // Compute the length of the string to create.
    size_t lenFilename = strlen(filename);
    size_t lenLineno = 1;
    for (size_t i = lineno; i /= 10; lenLineno++);
    size_t lenColno = 1;
    for (size_t i = colno; i /= 10; lenColno++);

    size_t len = 7 + lenFilename + 1 + lenLineno + 1 + lenColno;
    char *str = js_pod_malloc<char>(len + 1);
    if (!str)
        return TraceLogger_Error;

    DebugOnly<size_t> ret = JS_snprintf(str, len + 1, "script %s:%u:%u", filename, lineno, colno);
    MOZ_ASSERT(ret == len);

    if (!extraTextId.append(str)) {
        js_free(str);
        return TraceLogger_Error;
    }

    uint32_t textId = extraTextId.length() - 1 + TraceLogger_Last;
    if (!pointerMap.add(p, ptr, textId))
        return TraceLogger_Error;

    graph.addTextId(textId, str);

    return textId;
}

uint32_t
TraceLoggerThread::createTextId(JSScript *script)
{
    return createTextId(script->filename(), script->lineno(), script->column(), script);
}

uint32_t
TraceLoggerThread::createTextId(const JS::ReadOnlyCompileOptions &script)
{
    return createTextId(script.filename(), script.lineno, script.column, &script);
}

void
TraceLoggerThread::startEvent(uint32_t id)
{
    MOZ_ASSERT(TLTextIdIsTreeEvent(id));
    if (!traceLoggers.isTextIdEnabled(id))
       return;

    logTimestamp(id);
}

void
TraceLoggerThread::stopEvent(uint32_t id)
{
    MOZ_ASSERT(TLTextIdIsTreeEvent(id));
    if (!traceLoggers.isTextIdEnabled(id))
        return;

    logTimestamp(TraceLogger_Stop);
}

void
TraceLoggerThread::logTimestamp(uint32_t id)
{
    if (enabled == 0)
        return;

    if (!events.ensureSpaceBeforeAdd()) {
        uint64_t start = rdtsc() - traceLoggers.startupTime;

        graph.log(events);
        events.clear();

        // Log the time it took to flush the events as being from the
        // Tracelogger.
        MOZ_ASSERT(events.capacity() > 2);
        EventEntry &entryStart = events.pushUninitialized();
        entryStart.time = start;
        entryStart.textId = TraceLogger_Internal;

        EventEntry &entryStop = events.pushUninitialized();
        entryStop.time = rdtsc() - traceLoggers.startupTime;
        entryStop.textId = TraceLogger_Stop;
    }

    uint64_t time = rdtsc() - traceLoggers.startupTime;

    EventEntry &entry = events.pushUninitialized();
    entry.time = time;
    entry.textId = id;
}

TraceLoggerThreadState::TraceLoggerThreadState()
{
    initialized = false;
    enabled = 0;
    mainThreadEnabled = false;
    offThreadEnabled = false;

    lock = PR_NewLock();
    if (!lock)
        MOZ_CRASH();
}

TraceLoggerThreadState::~TraceLoggerThreadState()
{
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
TraceLoggerThreadState::lazyInit()
{
    if (initialized)
        return enabled > 0;

    initialized = true;

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
        for (uint32_t i = 1; i < TraceLogger_Last; i++) {
            TraceLoggerTextId id = TraceLoggerTextId(i);
            if (!TLTextIdIsToggable(id))
                continue;
            printf("  %s\n", TLTextIdString(id));
        }
        printf("\n");
        exit(0);
        /*NOTREACHED*/
    }

    for (uint32_t i = 1; i < TraceLogger_Last; i++) {
        TraceLoggerTextId id = TraceLoggerTextId(i);
        if (TLTextIdIsToggable(id))
            enabledTextIds[i] = ContainsFlag(env, TLTextIdString(id));
        else
            enabledTextIds[i] = true;
    }

    if (ContainsFlag(env, "Default")) {
        enabledTextIds[TraceLogger_Bailout] = true;
        enabledTextIds[TraceLogger_Baseline] = true;
        enabledTextIds[TraceLogger_BaselineCompilation] = true;
        enabledTextIds[TraceLogger_GC] = true;
        enabledTextIds[TraceLogger_GCAllocation] = true;
        enabledTextIds[TraceLogger_GCSweeping] = true;
        enabledTextIds[TraceLogger_Interpreter] = true;
        enabledTextIds[TraceLogger_IonCompilation] = true;
        enabledTextIds[TraceLogger_IonLinking] = true;
        enabledTextIds[TraceLogger_IonMonkey] = true;
        enabledTextIds[TraceLogger_MinorGC] = true;
        enabledTextIds[TraceLogger_ParserCompileFunction] = true;
        enabledTextIds[TraceLogger_ParserCompileLazy] = true;
        enabledTextIds[TraceLogger_ParserCompileScript] = true;
        enabledTextIds[TraceLogger_IrregexpCompile] = true;
        enabledTextIds[TraceLogger_IrregexpExecute] = true;
        enabledTextIds[TraceLogger_Scripts] = true;
        enabledTextIds[TraceLogger_Engine] = true;
    }

    if (ContainsFlag(env, "IonCompiler")) {
        enabledTextIds[TraceLogger_IonCompilation] = true;
        enabledTextIds[TraceLogger_IonLinking] = true;
        enabledTextIds[TraceLogger_FoldTests] = true;
        enabledTextIds[TraceLogger_SplitCriticalEdges] = true;
        enabledTextIds[TraceLogger_RenumberBlocks] = true;
        enabledTextIds[TraceLogger_DominatorTree] = true;
        enabledTextIds[TraceLogger_PhiAnalysis] = true;
        enabledTextIds[TraceLogger_ApplyTypes] = true;
        enabledTextIds[TraceLogger_ParallelSafetyAnalysis] = true;
        enabledTextIds[TraceLogger_AliasAnalysis] = true;
        enabledTextIds[TraceLogger_GVN] = true;
        enabledTextIds[TraceLogger_LICM] = true;
        enabledTextIds[TraceLogger_RangeAnalysis] = true;
        enabledTextIds[TraceLogger_LoopUnrolling] = true;
        enabledTextIds[TraceLogger_EffectiveAddressAnalysis] = true;
        enabledTextIds[TraceLogger_EliminateDeadCode] = true;
        enabledTextIds[TraceLogger_EdgeCaseAnalysis] = true;
        enabledTextIds[TraceLogger_EliminateRedundantChecks] = true;
        enabledTextIds[TraceLogger_GenerateLIR] = true;
        enabledTextIds[TraceLogger_RegisterAllocation] = true;
        enabledTextIds[TraceLogger_GenerateCode] = true;
        enabledTextIds[TraceLogger_Scripts] = true;
    }

    enabledTextIds[TraceLogger_Interpreter] = enabledTextIds[TraceLogger_Engine];
    enabledTextIds[TraceLogger_Baseline] = enabledTextIds[TraceLogger_Engine];
    enabledTextIds[TraceLogger_IonMonkey] = enabledTextIds[TraceLogger_Engine];

    const char *options = getenv("TLOPTIONS");
    if (options) {
        if (strstr(options, "help")) {
            fflush(nullptr);
            printf(
                "\n"
                "usage: TLOPTIONS=option,option,option,... where options can be:\n"
                "\n"
                "  EnableMainThread        Start logging the main thread immediately.\n"
                "  EnableOffThread         Start logging helper threads immediately.\n"
            );
            printf("\n");
            exit(0);
            /*NOTREACHED*/
        }

        if (strstr(options, "EnableMainThread"))
           mainThreadEnabled = true;
        if (strstr(options, "EnableOffThread"))
           offThreadEnabled = true;
    }

    startupTime = rdtsc();
    enabled = 1;
    return true;
}

TraceLoggerThread *
js::TraceLoggerForMainThread(CompileRuntime *runtime)
{
    return traceLoggers.forMainThread(runtime);
}

TraceLoggerThread *
TraceLoggerThreadState::forMainThread(CompileRuntime *runtime)
{
    return forMainThread(runtime->mainThread());
}

TraceLoggerThread *
js::TraceLoggerForMainThread(JSRuntime *runtime)
{
    return traceLoggers.forMainThread(runtime);
}

TraceLoggerThread *
TraceLoggerThreadState::forMainThread(JSRuntime *runtime)
{
    return forMainThread(&runtime->mainThread);
}

TraceLoggerThread *
TraceLoggerThreadState::forMainThread(PerThreadData *mainThread)
{
    if (!mainThread->traceLogger) {
        AutoTraceLoggerThreadStateLock lock(this);

        if (!lazyInit())
            return nullptr;

        TraceLoggerThread *logger = create();
        if (!logger)
            return nullptr;

        if (!mainThreadLoggers.append(logger)) {
            delete logger;
            return nullptr;
        }

        mainThread->traceLogger = logger;
        if (!mainThreadEnabled)
            logger->disable();
    }

    return mainThread->traceLogger;
}

TraceLoggerThread *
js::TraceLoggerForCurrentThread()
{
    PRThread *thread = PR_GetCurrentThread();
    return traceLoggers.forThread(thread);
}

TraceLoggerThread *
TraceLoggerThreadState::forThread(PRThread *thread)
{
    AutoTraceLoggerThreadStateLock lock(this);

    if (!lazyInit())
        return nullptr;

    ThreadLoggerHashMap::AddPtr p = threadLoggers.lookupForAdd(thread);
    if (p)
        return p->value();

    TraceLoggerThread *logger = create();
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

TraceLoggerThread *
TraceLoggerThreadState::create()
{
    TraceLoggerThread *logger = new TraceLoggerThread();
    if (!logger)
        return nullptr;

    if (!logger->init()) {
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
