/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CompileWrappers.h"

#include "gc/GC.h"
#include "jit/Ion.h"
#include "jit/JitRealm.h"

#include "vm/Realm-inl.h"

using namespace js;
using namespace js::jit;

JSRuntime*
CompileRuntime::runtime()
{
    return reinterpret_cast<JSRuntime*>(this);
}

/* static */ CompileRuntime*
CompileRuntime::get(JSRuntime* rt)
{
    return reinterpret_cast<CompileRuntime*>(rt);
}

#ifdef JS_GC_ZEAL
const void*
CompileRuntime::addressOfGCZealModeBits()
{
    return runtime()->gc.addressOfZealModeBits();
}
#endif

const JitRuntime*
CompileRuntime::jitRuntime()
{
    return runtime()->jitRuntime();
}

GeckoProfilerRuntime&
CompileRuntime::geckoProfiler()
{
    return runtime()->geckoProfiler();
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

bool
CompileRuntime::profilingScripts()
{
    return runtime()->profilingScripts;
}

const JSAtomState&
CompileRuntime::names()
{
    return *runtime()->commonNames;
}

const PropertyName*
CompileRuntime::emptyString()
{
    return runtime()->emptyString;
}

const StaticStrings&
CompileRuntime::staticStrings()
{
    return *runtime()->staticStrings;
}

const Value&
CompileRuntime::NaNValue()
{
    return runtime()->NaNValue;
}

const Value&
CompileRuntime::positiveInfinityValue()
{
    return runtime()->positiveInfinityValue;
}

const WellKnownSymbols&
CompileRuntime::wellKnownSymbols()
{
    return *runtime()->wellKnownSymbols;
}

const void*
CompileRuntime::mainContextPtr()
{
    return runtime()->mainContextFromAnyThread();
}

const void*
CompileRuntime::addressOfJitStackLimit()
{
    return runtime()->mainContextFromAnyThread()->addressOfJitStackLimit();
}

const void*
CompileRuntime::addressOfInterruptBits()
{
    return runtime()->mainContextFromAnyThread()->addressOfInterruptBits();
}

#ifdef DEBUG
bool
CompileRuntime::isInsideNursery(gc::Cell* cell)
{
    return UninlinedIsInsideNursery(cell);
}
#endif

const DOMCallbacks*
CompileRuntime::DOMcallbacks()
{
    return runtime()->DOMcallbacks;
}

bool
CompileRuntime::runtimeMatches(JSRuntime* rt)
{
    return rt == runtime();
}

Zone*
CompileZone::zone()
{
    return reinterpret_cast<Zone*>(this);
}

/* static */ CompileZone*
CompileZone::get(Zone* zone)
{
    return reinterpret_cast<CompileZone*>(zone);
}

CompileRuntime*
CompileZone::runtime()
{
    return CompileRuntime::get(zone()->runtimeFromAnyThread());
}

bool
CompileZone::isAtomsZone()
{
    return zone()->isAtomsZone();
}

#ifdef DEBUG
const void*
CompileZone::addressOfIonBailAfter()
{
    return zone()->runtimeFromAnyThread()->jitRuntime()->addressOfIonBailAfter();
}
#endif

const void*
CompileZone::addressOfNeedsIncrementalBarrier()
{
    return zone()->addressOfNeedsIncrementalBarrier();
}

const void*
CompileZone::addressOfFreeList(gc::AllocKind allocKind)
{
    return zone()->arenas.addressOfFreeList(allocKind);
}

const void*
CompileZone::addressOfNurseryPosition()
{
    return zone()->runtimeFromAnyThread()->gc.addressOfNurseryPosition();
}

const void*
CompileZone::addressOfStringNurseryPosition()
{
    // Objects and strings share a nursery, for now at least.
    return zone()->runtimeFromAnyThread()->gc.addressOfNurseryPosition();
}

const void*
CompileZone::addressOfNurseryCurrentEnd()
{
    return zone()->runtimeFromAnyThread()->gc.addressOfNurseryCurrentEnd();
}

const void*
CompileZone::addressOfStringNurseryCurrentEnd()
{
    return zone()->runtimeFromAnyThread()->gc.addressOfStringNurseryCurrentEnd();
}

bool
CompileZone::canNurseryAllocateStrings()
{
    return nurseryExists() &&
        zone()->runtimeFromAnyThread()->gc.nursery().canAllocateStrings() &&
        zone()->allocNurseryStrings;
}

bool
CompileZone::nurseryExists()
{
    return zone()->runtimeFromAnyThread()->gc.nursery().exists();
}

void
CompileZone::setMinorGCShouldCancelIonCompilations()
{
    MOZ_ASSERT(CurrentThreadCanAccessZone(zone()));
    JSRuntime* rt = zone()->runtimeFromMainThread();
    rt->gc.storeBuffer().setShouldCancelIonCompilations();
}

JS::Realm*
CompileRealm::realm()
{
    return reinterpret_cast<JS::Realm*>(this);
}

/* static */ CompileRealm*
CompileRealm::get(JS::Realm* realm)
{
    return reinterpret_cast<CompileRealm*>(realm);
}

CompileZone*
CompileRealm::zone()
{
    return CompileZone::get(realm()->zone());
}

CompileRuntime*
CompileRealm::runtime()
{
    return CompileRuntime::get(realm()->runtimeFromAnyThread());
}

const void*
CompileRealm::addressOfRandomNumberGenerator()
{
    return realm()->addressOfRandomNumberGenerator();
}

const JitRealm*
CompileRealm::jitRealm()
{
    return realm()->jitRealm();
}

const GlobalObject*
CompileRealm::maybeGlobal()
{
    // This uses unsafeUnbarrieredMaybeGlobal() so as not to trigger the read
    // barrier on the global from off thread.  This is safe because we
    // abort Ion compilation when we GC.
    return realm()->unsafeUnbarrieredMaybeGlobal();
}

const uint32_t*
CompileRealm::addressOfGlobalWriteBarriered()
{
    return &realm()->globalWriteBarriered;
}

bool
CompileRealm::hasAllocationMetadataBuilder()
{
    return realm()->hasAllocationMetadataBuilder();
}

// Note: This function is thread-safe because setSingletonAsValue sets a boolean
// variable to false, and this boolean variable has no way to be resetted to
// true. So even if there is a concurrent write, this concurrent write will
// always have the same value.  If there is a concurrent read, then we will
// clone a singleton instead of using the value which is baked in the JSScript,
// and this would be an unfortunate allocation, but this will not change the
// semantics of the JavaScript code which is executed.
void
CompileRealm::setSingletonsAsValues()
{
    realm()->behaviors().setSingletonsAsValues();
}

JitCompileOptions::JitCompileOptions()
  : cloneSingletons_(false),
    profilerSlowAssertionsEnabled_(false),
    offThreadCompilationAvailable_(false)
{
}

JitCompileOptions::JitCompileOptions(JSContext* cx)
{
    cloneSingletons_ = cx->realm()->creationOptions().cloneSingletons();
    profilerSlowAssertionsEnabled_ = cx->runtime()->geckoProfiler().enabled() &&
                                     cx->runtime()->geckoProfiler().slowAssertionsEnabled();
    offThreadCompilationAvailable_ = OffThreadCompilationAvailable(cx);
}
