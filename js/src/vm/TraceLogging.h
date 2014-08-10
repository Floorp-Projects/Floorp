/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TraceLogging_h
#define TraceLogging_h

#include "mozilla/GuardObjects.h"

#include "jsalloc.h"
#include "jslock.h"

#include "js/HashTable.h"
#include "js/TypeDecls.h"
#include "js/Vector.h"

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
 * The output of a tracelogging session is saved in /tmp/tl-data.json.
 * The format of that file is a JS array per tracelogger (=thread), with a map
 * containing:
 *  - dict:   Name of the file containing a json table with the log text.
 *            All other files only contain a index to this table when logging.
 *  - events: Name of the file containing a flat list of log events saved
 *            in binary format.
 *            (64bit: Time Stamp Counter, 32bit index to dict)
 *  - tree:   Name of the file containing the events with duration. The content
 *            is already in a tree data structure. This is also saved in a
 *            binary file.
 *  - treeFormat: The format used to encode the tree. By default "64,64,31,1,32".
 *                There are currently no other formats to save the tree.
 *     - 64,64,31,1,31 signifies how many bytes are used for the different
 *       parts of the tree.
 *       => 64 bits: Time Stamp Counter of start of event.
 *       => 64 bits: Time Stamp Counter of end of event.
 *       => 31 bits: Index to dict file containing the log text.
 *       =>  1 bit:  Boolean signifying if this entry has children.
 *                   When true, the child can be found just behind this entry.
 *       => 32 bits: Containing the ID of the next event on the same depth
 *                   or 0 if there isn't an event on the same depth anymore.
 *
 *        /-> The position in the file. Id is this divided by size of entry.
 *        |   So in this case this would be 1 (192bits per entry).
 *        |                              /-> Indicates there are children. The
 *        |                              |   first child is located at current
 *        |                              |   ID + 1. So 1 + 1 in this case: 2.
 *        |                              |   Or 0x00180 in the tree file.
 *        |                              | /-> Next event on the same depth is
 *        |                              | |    located at 4. So 0x00300 in the
 *        |                              | |    tree file.
 *       0x0000C0: [start, end, dictId, 1, 4]
 *
 *
 *       Example:
 *                          0x0: [start, end, dictId, 1, 0]
 *                                        |
 *                      /----------------------------------\
 *                      |                                  |
 *       0xC0: [start, end, dictId, 0, 2]      0x180 [start, end, dictId, 1, 0]
 *                                                      |
 *                                  /----------------------------------\
 *                                  |                                  |
 *         0x240: [start, end, dictId, 0, 4]    0x300 [start, end, dictId, 0, 0]
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

#define TRACELOGGER_TEXT_ID_LIST(_)                   \
    _(Bailout)                                        \
    _(Baseline)                                       \
    _(BaselineCompilation)                            \
    _(GC)                                             \
    _(GCAllocation)                                   \
    _(GCSweeping)                                     \
    _(Interpreter)                                    \
    _(Invalidation)                                   \
    _(IonCompilation)                                 \
    _(IonCompilationPaused)                           \
    _(IonLinking)                                     \
    _(IonMonkey)                                      \
    _(MinorGC)                                        \
    _(ParserCompileFunction)                          \
    _(ParserCompileLazy)                              \
    _(ParserCompileScript)                            \
    _(TL)                                             \
    _(IrregexpCompile)                                \
    _(IrregexpExecute)                                \
    _(VM)                                             \
                                                      \
    /* Specific passes during ion compilation */      \
    _(FoldTests)                                      \
    _(SplitCriticalEdges)                             \
    _(RenumberBlocks)                                 \
    _(ScalarReplacement)                              \
    _(DominatorTree)                                  \
    _(PhiAnalysis)                                    \
    _(MakeLoopsContiguous)                            \
    _(ApplyTypes)                                     \
    _(ParallelSafetyAnalysis)                         \
    _(AliasAnalysis)                                  \
    _(GVN)                                            \
    _(UCE)                                            \
    _(LICM)                                           \
    _(RangeAnalysis)                                  \
    _(LoopUnrolling)                                  \
    _(EffectiveAddressAnalysis)                       \
    _(EliminateDeadCode)                              \
    _(EdgeCaseAnalysis)                               \
    _(EliminateRedundantChecks)                       \
    _(GenerateLIR)                                    \
    _(RegisterAllocation)                             \
    _(GenerateCode)                                   \

class AutoTraceLog;

template <class T>
class ContinuousSpace {
    T *data_;
    uint32_t next_;
    uint32_t capacity_;

  public:
    ContinuousSpace ()
     : data_(nullptr)
    { }

    bool init() {
        capacity_ = 64;
        next_ = 0;
        data_ = (T *) js_malloc(capacity_ * sizeof(T));
        if (!data_)
            return false;

        return true;
    }

    T *data() {
        return data_;
    }

    uint32_t capacity() {
        return capacity_;
    }

    uint32_t size() {
        return next_;
    }

    uint32_t nextId() {
        return next_;
    }

    T &next() {
        return data()[next_];
    }

    uint32_t currentId() {
        MOZ_ASSERT(next_ > 0);
        return next_ - 1;
    }

    T &current() {
        return data()[currentId()];
    }

    bool hasSpaceForAdd(uint32_t count = 1) {
        if (next_ + count <= capacity_)
            return true;
        return false;
    }

    bool ensureSpaceBeforeAdd(uint32_t count = 1) {
        if (hasSpaceForAdd(count))
            return true;

        uint32_t nCapacity = capacity_ * 2;
        if (next_ + count > nCapacity)
            nCapacity = next_ + count;
        T *entries = (T *) js_realloc(data_, nCapacity * sizeof(T));

        if (!entries)
            return false;

        data_ = entries;
        capacity_ = nCapacity;

        return true;
    }

    T &operator[](size_t i) {
        MOZ_ASSERT(i < next_);
        return data()[i];
    }

    void push(T &data) {
        MOZ_ASSERT(next_ < capacity_);
        data()[next_++] = data;
    }

    T &pushUninitialized() {
        MOZ_ASSERT(next_ < capacity_);
        return data()[next_++];
    }

    void pop() {
        MOZ_ASSERT(next_ > 0);
        next_--;
    }

    void clear() {
        next_ = 0;
    }
};

class TraceLogger
{
  public:
    // Predefined IDs for common operations. These IDs can be used
    // without using TraceLogCreateTextId, because there are already created.
    enum TextId {
        TL_Error = 0,
#   define DEFINE_TEXT_ID(textId) textId,
        TRACELOGGER_TEXT_ID_LIST(DEFINE_TEXT_ID)
#   undef DEFINE_TEXT_ID
        LAST
    };

#ifdef JS_TRACE_LOGGING
  private:
    typedef HashMap<const void *,
                    uint32_t,
                    PointerHasher<const void *, 3>,
                    SystemAllocPolicy> PointerHashMap;

    // The layout of the tree in memory and in the log file. Readable by JS
    // using TypedArrays.
    struct TreeEntry {
        uint64_t start_;
        uint64_t stop_;
        union {
            struct {
                uint32_t textId_: 31;
                uint32_t hasChildren_: 1;
            } s;
            uint32_t value_;
        } u;
        uint32_t nextId_;

        TreeEntry(uint64_t start, uint64_t stop, uint32_t textId, bool hasChildren,
                  uint32_t nextId)
        {
            start_ = start;
            stop_ = stop;
            u.s.textId_ = textId;
            u.s.hasChildren_ = hasChildren;
            nextId_ = nextId;
        }
        TreeEntry()
        { }
        uint64_t start() {
            return start_;
        }
        uint64_t stop() {
            return stop_;
        }
        uint32_t textId() {
            return u.s.textId_;
        }
        bool hasChildren() {
            return u.s.hasChildren_;
        }
        uint32_t nextId() {
            return nextId_;
        }
        void setStart(uint64_t start) {
            start_ = start;
        }
        void setStop(uint64_t stop) {
            stop_ = stop;
        }
        void setTextId(uint32_t textId) {
            MOZ_ASSERT(textId < uint32_t(1<<31) );
            u.s.textId_ = textId;
        }
        void setHasChildren(bool hasChildren) {
            u.s.hasChildren_ = hasChildren;
        }
        void setNextId(uint32_t nextId) {
            nextId_ = nextId;
        }
    };

    // Helper structure for keeping track of the current entries in
    // the tree. Pushed by `start(id)`, popped by `stop(id)`. The active flag
    // is used to know if a subtree doesn't need to get logged.
    struct StackEntry {
        uint32_t treeId_;
        uint32_t lastChildId_;
        struct {
            uint32_t textId_: 31;
            uint32_t active_: 1;
        } s;
        StackEntry(uint32_t treeId, uint32_t lastChildId, bool active = true)
          : treeId_(treeId), lastChildId_(lastChildId)
        {
            s.textId_ = 0;
            s.active_ = active;
        }
        uint32_t treeId() {
            return treeId_;
        }
        uint32_t lastChildId() {
            return lastChildId_;
        }
        uint32_t textId() {
            return s.textId_;
        }
        bool active() {
            return s.active_;
        }
        void setTreeId(uint32_t treeId) {
            treeId_ = treeId;
        }
        void setLastChildId(uint32_t lastChildId) {
            lastChildId_ = lastChildId;
        }
        void setTextId(uint32_t textId) {
            MOZ_ASSERT(textId < uint32_t(1<<31) );
            s.textId_ = textId;
        }
        void setActive(bool active) {
            s.active_ = active;
        }
    };

    // The layout of the event log in memory and in the log file.
    // Readable by JS using TypedArrays.
    struct EventEntry {
        uint64_t time;
        uint32_t textId;
        EventEntry(uint64_t time, uint32_t textId)
          : time(time), textId(textId)
        { }
    };

    FILE *dictFile;
    FILE *treeFile;
    FILE *eventFile;

    uint32_t enabled;
    bool failed;
    uint32_t nextTextId;

    PointerHashMap pointerMap;

    ContinuousSpace<TreeEntry> tree;
    ContinuousSpace<StackEntry> stack;
    ContinuousSpace<EventEntry> events;

    uint32_t treeOffset;

  public:
    AutoTraceLog *top;

  private:
    // Helper functions that convert a TreeEntry in different endianness
    // in place.
    void entryToBigEndian(TreeEntry *entry);
    void entryToSystemEndian(TreeEntry *entry);

    // Helper functions to get/save a tree from file.
    bool getTreeEntry(uint32_t treeId, TreeEntry *entry);
    bool saveTreeEntry(uint32_t treeId, TreeEntry *entry);

    // Return the first StackEntry that is active.
    StackEntry &getActiveAncestor();

    // This contains the meat of startEvent, except the test for enough space,
    // the test if tracelogger is enabled and the timestamp computation.
    bool startEvent(uint32_t id, uint64_t timestamp);

    // Update functions that can adjust the items in the tree,
    // both in memory or already written to disk.
    bool updateHasChildren(uint32_t treeId, bool hasChildren = true);
    bool updateNextId(uint32_t treeId, uint32_t nextId);
    bool updateStop(uint32_t treeId, uint64_t timestamp);

    // Flush the tree and events.
    bool flush();

  public:
    TraceLogger();
    ~TraceLogger();

    bool init(uint32_t loggerId);

    bool enable();
    bool enable(JSContext *cx);
    bool disable();

    // The createTextId functions map a unique input to a logger ID.
    // This ID can be used to log something. Calls to these functions should be
    // limited if possible, because of the overhead.
    uint32_t createTextId(const char *text);
    uint32_t createTextId(JSScript *script);
    uint32_t createTextId(const JS::ReadOnlyCompileOptions &script);

    // Log an event (no start/stop, only the timestamp is recorded).
    void logTimestamp(uint32_t id);

    // Record timestamps for start and stop of an event.
    // In the stop method, the ID is only used in debug builds to test
    // correctness.
    void startEvent(uint32_t id);
    void stopEvent(uint32_t id);
    void stopEvent();

    static unsigned offsetOfEnabled() {
        return offsetof(TraceLogger, enabled);
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

class TraceLogging
{
#ifdef JS_TRACE_LOGGING
    typedef HashMap<PRThread *,
                    TraceLogger *,
                    PointerHasher<PRThread *, 3>,
                    SystemAllocPolicy> ThreadLoggerHashMap;
    typedef Vector<TraceLogger *, 1, js::SystemAllocPolicy > MainThreadLoggers;

    bool initialized;
    bool enabled;
    bool enabledTextIds[TraceLogger::LAST];
    bool mainThreadEnabled;
    bool offThreadEnabled;
    ThreadLoggerHashMap threadLoggers;
    MainThreadLoggers mainThreadLoggers;
    uint32_t loggerId;
    FILE *out;

  public:
    uint64_t startupTime;
    PRLock *lock;

    TraceLogging();
    ~TraceLogging();

    TraceLogger *forMainThread(JSRuntime *runtime);
    TraceLogger *forMainThread(jit::CompileRuntime *runtime);
    TraceLogger *forThread(PRThread *thread);

    bool isTextIdEnabled(uint32_t textId) {
        if (textId < TraceLogger::LAST)
            return enabledTextIds[textId];
        return true;
    }

  private:
    TraceLogger *forMainThread(PerThreadData *mainThread);
    TraceLogger *create();
    bool lazyInit();
#endif
};

#ifdef JS_TRACE_LOGGING
TraceLogger *TraceLoggerForMainThread(JSRuntime *runtime);
TraceLogger *TraceLoggerForMainThread(jit::CompileRuntime *runtime);
TraceLogger *TraceLoggerForCurrentThread();
#else
inline TraceLogger *TraceLoggerForMainThread(JSRuntime *runtime) {
    return nullptr;
};
inline TraceLogger *TraceLoggerForMainThread(jit::CompileRuntime *runtime) {
    return nullptr;
};
inline TraceLogger *TraceLoggerForCurrentThread() {
    return nullptr;
};
#endif

inline bool TraceLoggerEnable(TraceLogger *logger) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->enable();
#endif
    return false;
}
inline bool TraceLoggerEnable(TraceLogger *logger, JSContext *cx) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->enable(cx);
#endif
    return false;
}
inline bool TraceLoggerDisable(TraceLogger *logger) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->disable();
#endif
    return false;
}

inline uint32_t TraceLogCreateTextId(TraceLogger *logger, JSScript *script) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->createTextId(script);
#endif
    return TraceLogger::TL_Error;
}
inline uint32_t TraceLogCreateTextId(TraceLogger *logger,
                                     const JS::ReadOnlyCompileOptions &compileOptions)
{
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->createTextId(compileOptions);
#endif
    return TraceLogger::TL_Error;
}
inline uint32_t TraceLogCreateTextId(TraceLogger *logger, const char *text) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        return logger->createTextId(text);
#endif
    return TraceLogger::TL_Error;
}
#ifdef JS_TRACE_LOGGING
bool TraceLogTextIdEnabled(uint32_t textId);
#else
inline bool TraceLogTextIdEnabled(uint32_t textId) {
    return false;
}
#endif
inline void TraceLogTimestamp(TraceLogger *logger, uint32_t textId) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        logger->logTimestamp(textId);
#endif
}
inline void TraceLogStartEvent(TraceLogger *logger, uint32_t textId) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        logger->startEvent(textId);
#endif
}
inline void TraceLogStopEvent(TraceLogger *logger, uint32_t textId) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        logger->stopEvent(textId);
#endif
}
inline void TraceLogStopEvent(TraceLogger *logger) {
#ifdef JS_TRACE_LOGGING
    if (logger)
        logger->stopEvent();
#endif
}

// Automatic logging at the start and end of function call.
class AutoTraceLog {
#ifdef JS_TRACE_LOGGING
    TraceLogger *logger;
    uint32_t textId;
    bool executed;
    AutoTraceLog *prev;

  public:
    AutoTraceLog(TraceLogger *logger, uint32_t textId MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
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
    AutoTraceLog(TraceLogger *logger, uint32_t textId MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
#endif

  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#ifdef JS_TRACE_LOGGING
class AutoTraceLoggingLock
{
  TraceLogging *logging;

  public:
    AutoTraceLoggingLock(TraceLogging *logging MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : logging(logging)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        PR_Lock(logging->lock);
    }
    ~AutoTraceLoggingLock() {
        PR_Unlock(logging->lock);
    }
  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};
#endif

}  /* namedata js */

#endif /* TraceLogging_h */
