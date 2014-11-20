/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TraceLogging_h
#define TraceLogging_h

#include "mozilla/GuardObjects.h"
#include "mozilla/UniquePtr.h"

#include "jsalloc.h"
#include "jslock.h"

#include "js/HashTable.h"
#include "js/TypeDecls.h"
#include "js/Vector.h"
#include "vm/TraceLoggingGraph.h"
#include "vm/TraceLoggingTypes.h"

struct JSRuntime;

namespace JS {
    class ReadOnlyCompileOptions;
}

namespace js {
class PerThreadData;

namespace jit {
    class CompileRuntime;
}

/*
 * Tracelogging overview.
 *
 * Tracelogging makes it possible to trace the timestamp of a single event and/or
 * the duration of an event. This is implemented to give an as low overhead as
 * possible so it doesn't interfere with running.
 *
 *
 * Logging something is done in 3 stages.
 * 1) Get the tracelogger of the current thread.
 *     - TraceLoggerForMainThread(JSRuntime *)
 *     - TraceLoggerForCurrentThread(); // Should NOT be used for the mainthread.
 * 2) Optionally create a textId for the text that needs to get logged. This
 *    step takes some time, so try to do this beforehand, outside the hot
 *    path and don't do unnecessary repetitions, since it will criple
 *    performance.
 *     - TraceLogCreateTextId(logger, ...);
 *
 *    There are also some text IDs created beforehand. They are located in
 *    Tracelogger::TextId.
 * 3) Log the timestamp of an event:
 *    - TraceLogTimestamp(logger, textId);
 *
 *    or the duration:
 *    - TraceLogStartEvent(logger, textId);
 *    - TraceLogStopEvent(logger, textId);
 *
 *    or the duration with a RAII class:
 *    - AutoTraceLog logger(logger, textId);
 */


class AutoTraceLog;

class TraceLoggerThread
{
#ifdef JS_TRACE_LOGGING
  private:
    typedef HashMap<const void *,
                    uint32_t,
                    PointerHasher<const void *, 3>,
                    SystemAllocPolicy> PointerHashMap;

    uint32_t enabled;
    bool failed;

    mozilla::UniquePtr<TraceLoggerGraph> graph;

    PointerHashMap pointerMap;
    Vector<char *, 0, js::SystemAllocPolicy> extraTextId;

    ContinuousSpace<EventEntry> events;

  public:
    AutoTraceLog *top;

    TraceLoggerThread();
    bool init();
    ~TraceLoggerThread();

    bool init(uint32_t loggerId);
    void initGraph();

    bool enable();
    bool enable(JSContext *cx);
    bool disable();

    const char *eventText(uint32_t id);

    // The createTextId functions map a unique input to a logger ID.
    // This can be used to give start and stop events. Calls to these functions should be
    // limited if possible, because of the overhead.
    // Note: it is not allowed to use them in logTimestamp.
    uint32_t createTextId(const char *text);
    uint32_t createTextId(JSScript *script);
    uint32_t createTextId(const JS::ReadOnlyCompileOptions &script);
  private:
    uint32_t createTextId(const char *filename, size_t lineno, size_t colno, const void *p);

  public:
    // Log an event (no start/stop, only the timestamp is recorded).
    void logTimestamp(uint32_t id);

    // Record timestamps for start and stop of an event.
    void startEvent(uint32_t id);
    void stopEvent(uint32_t id);

  private:
    void stopEvent();

  public:
    static unsigned offsetOfEnabled() {
        return offsetof(TraceLoggerThread, enabled);
    }

  private:
    void assertNoQuotes(const char *text) {
#ifdef DEBUG
        const char *quote = strchr(text, '"');
        MOZ_ASSERT(!quote);
#endif
    }
#endif
};

class TraceLoggerThreadState
{
#ifdef JS_TRACE_LOGGING
    typedef HashMap<PRThread *,
                    TraceLoggerThread *,
                    PointerHasher<PRThread *, 3>,
                    SystemAllocPolicy> ThreadLoggerHashMap;
    typedef Vector<TraceLoggerThread *, 1, js::SystemAllocPolicy > MainThreadLoggers;

    bool initialized;
    bool enabled;
    bool enabledTextIds[TraceLogger_Last];
    bool mainThreadEnabled;
    bool offThreadEnabled;
    bool graphSpewingEnabled;
    ThreadLoggerHashMap threadLoggers;
    MainThreadLoggers mainThreadLoggers;

  public:
    uint64_t startupTime;
    PRLock *lock;

    TraceLoggerThreadState();
    ~TraceLoggerThreadState();

    TraceLoggerThread *forMainThread(JSRuntime *runtime);
    TraceLoggerThread *forMainThread(jit::CompileRuntime *runtime);
    TraceLoggerThread *forThread(PRThread *thread);

    bool isTextIdEnabled(uint32_t textId) {
        if (textId < TraceLogger_Last)
            return enabledTextIds[textId];
        return true;
    }
    void enableTextId(JSContext *cx, uint32_t textId);
    void disableTextId(JSContext *cx, uint32_t textId);

  private:
    TraceLoggerThread *forMainThread(PerThreadData *mainThread);
    TraceLoggerThread *create();
    bool lazyInit();
#endif
};

#ifdef JS_TRACE_LOGGING
TraceLoggerThread *TraceLoggerForMainThread(JSRuntime *runtime);
TraceLoggerThread *TraceLoggerForMainThread(jit::CompileRuntime *runtime);
TraceLoggerThread *TraceLoggerForCurrentThread();
#else
inline TraceLoggerThread *TraceLoggerForMainThread(JSRuntime *runtime) {
    return nullptr;
};
inline TraceLoggerThread *TraceLoggerForMainThread(jit::CompileRuntime *runtime) {
    return nullptr;
};
inline TraceLoggerThread *TraceLoggerForCurrentThread() {
    return nullptr;
};
#endif

inline bool TraceLoggerEnable(TraceLoggerThread *logger) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->enable();
#endif
    return false;
}
inline bool TraceLoggerEnable(TraceLoggerThread *logger, JSContext *cx) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->enable(cx);
#endif
    return false;
}
inline bool TraceLoggerDisable(TraceLoggerThread *logger) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->disable();
#endif
    return false;
}

inline uint32_t TraceLogCreateTextId(TraceLoggerThread *logger, JSScript *script) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->createTextId(script);
#endif
    return TraceLogger_Error;
}
inline uint32_t TraceLogCreateTextId(TraceLoggerThread *logger,
                                     const JS::ReadOnlyCompileOptions &compileOptions)
{
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->createTextId(compileOptions);
#endif
    return TraceLogger_Error;
}
inline uint32_t TraceLogCreateTextId(TraceLoggerThread *logger, const char *text) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->createTextId(text);
#endif
    return TraceLogger_Error;
}
#ifdef JS_TRACE_LOGGING
bool TraceLogTextIdEnabled(uint32_t textId);
void TraceLogEnableTextId(JSContext *cx, uint32_t textId);
void TraceLogDisableTextId(JSContext *cx, uint32_t textId);
#else
inline bool TraceLogTextIdEnabled(uint32_t textId) {
    return false;
}
inline void TraceLogEnableTextId(JSContext *cx, uint32_t textId) {}
inline void TraceLogDisableTextId(JSContext *cx, uint32_t textId) {}
#endif
inline void TraceLogTimestamp(TraceLoggerThread *logger, uint32_t textId) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        logger->logTimestamp(textId);
#endif
}
inline void TraceLogStartEvent(TraceLoggerThread *logger, uint32_t textId) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        logger->startEvent(textId);
#endif
}
inline void TraceLogStopEvent(TraceLoggerThread *logger, uint32_t textId) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        logger->stopEvent(textId);
#endif
}

// Automatic logging at the start and end of function call.
class AutoTraceLog {
#ifdef JS_TRACE_LOGGING
    TraceLoggerThread *logger;
    uint32_t textId;
    bool executed;
    AutoTraceLog *prev;

  public:
    AutoTraceLog(TraceLoggerThread *logger, uint32_t textId MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : logger(logger),
        textId(textId),
        executed(false)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        if (logger) {
            TraceLogStartEvent(logger, textId);

            prev = logger->top;
            logger->top = this;
        }
    }

    ~AutoTraceLog()
    {
        if (logger) {
            while (this != logger->top)
                logger->top->stop();
            stop();
        }
    }
  private:
    void stop() {
        if (!executed) {
            executed = true;
            TraceLogStopEvent(logger, textId);
        }

        if (logger->top == this)
            logger->top = prev;
    }
#else
  public:
    AutoTraceLog(TraceLoggerThread *logger, uint32_t textId MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
#endif

  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

}  /* namedata js */

#endif /* TraceLogging_h */
