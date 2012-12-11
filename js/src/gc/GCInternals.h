/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgc_internal_h___
#define jsgc_internal_h___

#include "jsapi.h"

namespace js {
namespace gc {

void
MarkRuntime(JSTracer *trc, bool useSavedRoots = false);

class AutoCopyFreeListToArenas {
    JSRuntime *runtime;

  public:
    AutoCopyFreeListToArenas(JSRuntime *rt);
    ~AutoCopyFreeListToArenas();
};

struct AutoFinishGC
{
    AutoFinishGC(JSRuntime *rt);
};

/*
 * This class should be used by any code that needs to exclusive access to the
 * heap in order to trace through it...
 */
class AutoTraceSession {
  public:
    AutoTraceSession(JSRuntime *rt, HeapState state = Tracing);
    ~AutoTraceSession();

  protected:
    JSRuntime *runtime;

  private:
    AutoTraceSession(const AutoTraceSession&) MOZ_DELETE;
    void operator=(const AutoTraceSession&) MOZ_DELETE;

    js::HeapState prevState;
};

struct AutoPrepareForTracing
{
    AutoFinishGC finish;
    AutoTraceSession session;
    AutoCopyFreeListToArenas copy;

    AutoPrepareForTracing(JSRuntime *rt);
};

class IncrementalSafety
{
    const char *reason_;

    IncrementalSafety(const char *reason) : reason_(reason) {}

  public:
    static IncrementalSafety Safe() { return IncrementalSafety(NULL); }
    static IncrementalSafety Unsafe(const char *reason) { return IncrementalSafety(reason); }

    typedef void (IncrementalSafety::* ConvertibleToBool)();
    void nonNull() {}

    operator ConvertibleToBool() const {
        return reason_ == NULL ? &IncrementalSafety::nonNull : 0;
    }

    const char *reason() {
        JS_ASSERT(reason_);
        return reason_;
    }
};

IncrementalSafety
IsIncrementalGCSafe(JSRuntime *rt);

#ifdef JSGC_ROOT_ANALYSIS
bool
IsAddressableGCThing(JSRuntime *rt, uintptr_t w);
#endif

#ifdef JS_GC_ZEAL
void
StartVerifyPreBarriers(JSRuntime *rt);

void
EndVerifyPreBarriers(JSRuntime *rt);

void
StartVerifyPostBarriers(JSRuntime *rt);

void
EndVerifyPostBarriers(JSRuntime *rt);

void
FinishVerifier(JSRuntime *rt);
#endif /* JS_GC_ZEAL */

} /* namespace gc */
} /* namespace js */

#endif /* jsgc_internal_h___ */
