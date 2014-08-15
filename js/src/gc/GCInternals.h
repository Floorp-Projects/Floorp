/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCInternals_h
#define gc_GCInternals_h

#include "jscntxt.h"

#include "gc/Zone.h"
#include "vm/HelperThreads.h"
#include "vm/Runtime.h"

namespace js {
namespace gc {

void
MarkPersistentRootedChains(JSTracer *trc);

#ifdef JSGC_FJGENERATIONAL
class ForkJoinNurseryCollectionTracer;

void
MarkForkJoinStack(ForkJoinNurseryCollectionTracer *trc);
#endif

class AutoCopyFreeListToArenas
{
    JSRuntime *runtime;
    ZoneSelector selector;

  public:
    AutoCopyFreeListToArenas(JSRuntime *rt, ZoneSelector selector);
    ~AutoCopyFreeListToArenas();
};

struct AutoFinishGC
{
    explicit AutoFinishGC(JSRuntime *rt);
};

/*
 * This class should be used by any code that needs to exclusive access to the
 * heap in order to trace through it...
 */
class AutoTraceSession
{
  public:
    explicit AutoTraceSession(JSRuntime *rt, HeapState state = Tracing);
    ~AutoTraceSession();

  protected:
    AutoLockForExclusiveAccess lock;
    JSRuntime *runtime;

  private:
    AutoTraceSession(const AutoTraceSession&) MOZ_DELETE;
    void operator=(const AutoTraceSession&) MOZ_DELETE;

    HeapState prevState;
};

struct AutoPrepareForTracing
{
    AutoFinishGC finish;
    AutoTraceSession session;
    AutoCopyFreeListToArenas copy;

    AutoPrepareForTracing(JSRuntime *rt, ZoneSelector selector);
};

class IncrementalSafety
{
    const char *reason_;

    explicit IncrementalSafety(const char *reason) : reason_(reason) {}

  public:
    static IncrementalSafety Safe() { return IncrementalSafety(nullptr); }
    static IncrementalSafety Unsafe(const char *reason) { return IncrementalSafety(reason); }

    typedef void (IncrementalSafety::* ConvertibleToBool)();
    void nonNull() {}

    operator ConvertibleToBool() const {
        return reason_ == nullptr ? &IncrementalSafety::nonNull : 0;
    }

    const char *reason() {
        JS_ASSERT(reason_);
        return reason_;
    }
};

IncrementalSafety
IsIncrementalGCSafe(JSRuntime *rt);

#ifdef JS_GC_ZEAL

class AutoStopVerifyingBarriers
{
    GCRuntime *gc;
    bool restartPreVerifier;
    bool restartPostVerifier;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoStopVerifyingBarriers(JSRuntime *rt, bool isShutdown
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : gc(&rt->gc)
    {
        restartPreVerifier = gc->endVerifyPreBarriers() && !isShutdown;
        restartPostVerifier = gc->endVerifyPostBarriers() && !isShutdown &&
            JS::IsGenerationalGCEnabled(rt);
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoStopVerifyingBarriers() {
        if (restartPreVerifier)
            gc->startVerifyPreBarriers();
        if (restartPostVerifier)
            gc->startVerifyPostBarriers();
    }
};
#else
struct AutoStopVerifyingBarriers
{
    AutoStopVerifyingBarriers(JSRuntime *, bool) {}
};
#endif /* JS_GC_ZEAL */

#ifdef JSGC_HASH_TABLE_CHECKS
void
CheckHashTablesAfterMovingGC(JSRuntime *rt);
#endif


} /* namespace gc */
} /* namespace js */

#endif /* gc_GCInternals_h */
