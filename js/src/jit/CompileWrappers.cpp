/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Ion.h"

using namespace js;
using namespace js::jit;

JSRuntime *
CompileRuntime::runtime()
{
    return reinterpret_cast<JSRuntime *>(this);
}

/* static */ CompileRuntime *
CompileRuntime::get(JSRuntime *rt)
{
    return reinterpret_cast<CompileRuntime *>(rt);
}

bool
CompileRuntime::onMainThread()
{
    return js::CurrentThreadCanAccessRuntime(runtime());
}

const void *
CompileRuntime::addressOfIonTop()
{
    return &runtime()->mainThread.ionTop;
}

const void *
CompileRuntime::addressOfIonStackLimit()
{
    return &runtime()->mainThread.ionStackLimit;
}

const void *
CompileRuntime::addressOfJSContext()
{
    return &runtime()->mainThread.ionJSContext;
}

const void *
CompileRuntime::addressOfActivation()
{
    return runtime()->mainThread.addressOfActivation();
}

const void *
CompileRuntime::addressOfLastCachedNativeIterator()
{
    return &runtime()->nativeIterCache.last;
}

#ifdef JS_GC_ZEAL
const void *
CompileRuntime::addressOfGCZeal()
{
    return &runtime()->gcZeal_;
}
#endif

const void *
CompileRuntime::addressOfInterrupt()
{
    return &runtime()->interrupt;
}

const JitRuntime *
CompileRuntime::jitRuntime()
{
    return runtime()->jitRuntime();
}

SPSProfiler &
CompileRuntime::spsProfiler()
{
    return runtime()->spsProfiler;
}

bool
CompileRuntime::signalHandlersInstalled()
{
    return runtime()->signalHandlersInstalled();
}

bool
CompileRuntime::jitSupportsFloatingPoint()
{
    return runtime()->jitSupportsFloatingPoint;
}

bool
CompileRuntime::hadOutOfMemory()
{
    return runtime()->hadOutOfMemory;
}

const JSAtomState &
CompileRuntime::names()
{
    return runtime()->atomState;
}

const StaticStrings &
CompileRuntime::staticStrings()
{
    return runtime()->staticStrings;
}

const Value &
CompileRuntime::NaNValue()
{
    return runtime()->NaNValue;
}

const Value &
CompileRuntime::positiveInfinityValue()
{
    return runtime()->positiveInfinityValue;
}

bool
CompileRuntime::isInsideNursery(gc::Cell *cell)
{
    return UninlinedIsInsideNursery(runtime(), cell);
}

const DOMCallbacks *
CompileRuntime::DOMcallbacks()
{
    return GetDOMCallbacks(runtime());
}

const MathCache *
CompileRuntime::maybeGetMathCache()
{
    return runtime()->maybeGetMathCache();
}

#ifdef JSGC_GENERATIONAL
const Nursery &
CompileRuntime::gcNursery()
{
    return runtime()->gcNursery;
}
#endif

Zone *
CompileZone::zone()
{
    return reinterpret_cast<Zone *>(this);
}

/* static */ CompileZone *
CompileZone::get(Zone *zone)
{
    return reinterpret_cast<CompileZone *>(zone);
}

const void *
CompileZone::addressOfNeedsBarrier()
{
    return zone()->addressOfNeedsBarrier();
}

const void *
CompileZone::addressOfFreeListFirst(gc::AllocKind allocKind)
{
    return &zone()->allocator.arenas.getFreeList(allocKind)->first;
}

const void *
CompileZone::addressOfFreeListLast(gc::AllocKind allocKind)
{
    return &zone()->allocator.arenas.getFreeList(allocKind)->last;
}

JSCompartment *
CompileCompartment::compartment()
{
    return reinterpret_cast<JSCompartment *>(this);
}

/* static */ CompileCompartment *
CompileCompartment::get(JSCompartment *comp)
{
    return reinterpret_cast<CompileCompartment *>(comp);
}

CompileZone *
CompileCompartment::zone()
{
    return CompileZone::get(compartment()->zone());
}

CompileRuntime *
CompileCompartment::runtime()
{
    return CompileRuntime::get(compartment()->runtimeFromAnyThread());
}

const void *
CompileCompartment::addressOfEnumerators()
{
    return &compartment()->enumerators;
}

const CallsiteCloneTable &
CompileCompartment::callsiteClones()
{
    return compartment()->callsiteClones;
}

const JitCompartment *
CompileCompartment::jitCompartment()
{
    return compartment()->jitCompartment();
}

bool
CompileCompartment::hasObjectMetadataCallback()
{
    return compartment()->hasObjectMetadataCallback();
}
