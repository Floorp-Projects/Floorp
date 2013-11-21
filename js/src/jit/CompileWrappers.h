/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CompileWrappers_h
#define jit_CompileWrappers_h

#ifdef JS_ION

#include "jscntxt.h"

namespace js {
namespace jit {

class JitRuntime;

// During Ion compilation we need access to various bits of the current
// compartment, runtime and so forth. However, since compilation can run off
// thread while the main thread is actively mutating the VM, this access needs
// to be restricted. The classes below give the compiler an interface to access
// all necessary information in a threadsafe fashion.

class CompileRuntime
{
    JSRuntime *runtime();

  public:
    static CompileRuntime *get(JSRuntime *rt);

    bool onMainThread();

    // &mainThread.ionTop
    const void *addressOfIonTop();

    // rt->mainThread.ionStackLimit;
    const void *addressOfIonStackLimit();

    // &mainThread.ionJSContext
    const void *addressOfJSContext();

    // &mainThread.activation_
    const void *addressOfActivation();

    // &GetIonContext()->runtime->nativeIterCache.last
    const void *addressOfLastCachedNativeIterator();

#ifdef JS_GC_ZEAL
    const void *addressOfGCZeal();
#endif

    const void *addressOfInterrupt();

    const JitRuntime *jitRuntime();

    // Compilation does not occur off thread when the SPS profiler is enabled.
    SPSProfiler &spsProfiler();

    bool signalHandlersInstalled();
    bool jitSupportsFloatingPoint();
    bool hadOutOfMemory();

    const JSAtomState &names();
    const StaticStrings &staticStrings();
    const Value &NaNValue();
    const Value &positiveInfinityValue();

    bool isInsideNursery(gc::Cell *cell);

    // DOM callbacks must be threadsafe (and will hopefully be removed soon).
    const DOMCallbacks *DOMcallbacks();

    const MathCache *maybeGetMathCache();

#ifdef JSGC_GENERATIONAL
    const Nursery &gcNursery();
#endif
};

class CompileZone
{
    Zone *zone();

  public:
    static CompileZone *get(Zone *zone);

    const void *addressOfNeedsBarrier();

    // allocator.arenas.getFreeList(allocKind)
    const void *addressOfFreeListFirst(gc::AllocKind allocKind);
    const void *addressOfFreeListLast(gc::AllocKind allocKind);
};

class CompileCompartment
{
    JSCompartment *compartment();

  public:
    static CompileCompartment *get(JSCompartment *comp);

    CompileZone *zone();
    CompileRuntime *runtime();

    const void *addressOfEnumerators();

    const CallsiteCloneTable &callsiteClones();

    const JitCompartment *jitCompartment();

    bool hasObjectMetadataCallback();
};

} // namespace jit
} // namespace js

#endif // JS_ION

#endif // jit_CompileWrappers_h
