/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "gc/GCInternals.h"
#include "gc/GCLock.h"
#include "js/HashTable.h"
#include "vm/Realm.h"
#include "vm/Runtime.h"

#include "gc/PrivateIterators-inl.h"
#include "vm/JSContext-inl.h"

using namespace js;
using namespace js::gc;

static void IterateRealmsArenasCellsUnbarriered(
    JSContext* cx, Zone* zone, void* data,
    JS::IterateRealmCallback realmCallback, IterateArenaCallback arenaCallback,
    IterateCellCallback cellCallback) {
  {
    Rooted<Realm*> realm(cx);
    for (RealmsInZoneIter r(zone); !r.done(); r.next()) {
      realm = r;
      (*realmCallback)(cx, data, realm);
    }
  }

  for (auto thingKind : AllAllocKinds()) {
    JS::TraceKind traceKind = MapAllocToTraceKind(thingKind);
    size_t thingSize = Arena::thingSize(thingKind);

    for (ArenaIter aiter(zone, thingKind); !aiter.done(); aiter.next()) {
      Arena* arena = aiter.get();
      (*arenaCallback)(cx->runtime(), data, arena, traceKind, thingSize);
      for (ArenaCellIter iter(arena); !iter.done(); iter.next()) {
        (*cellCallback)(cx->runtime(), data, iter.getCell(), traceKind,
                        thingSize);
      }
    }
  }
}

void js::IterateHeapUnbarriered(JSContext* cx, void* data,
                                IterateZoneCallback zoneCallback,
                                JS::IterateRealmCallback realmCallback,
                                IterateArenaCallback arenaCallback,
                                IterateCellCallback cellCallback) {
  AutoPrepareForTracing prep(cx);

  for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next()) {
    (*zoneCallback)(cx->runtime(), data, zone);
    IterateRealmsArenasCellsUnbarriered(cx, zone, data, realmCallback,
                                        arenaCallback, cellCallback);
  }
}

void js::IterateHeapUnbarrieredForZone(JSContext* cx, Zone* zone, void* data,
                                       IterateZoneCallback zoneCallback,
                                       JS::IterateRealmCallback realmCallback,
                                       IterateArenaCallback arenaCallback,
                                       IterateCellCallback cellCallback) {
  AutoPrepareForTracing prep(cx);

  (*zoneCallback)(cx->runtime(), data, zone);
  IterateRealmsArenasCellsUnbarriered(cx, zone, data, realmCallback,
                                      arenaCallback, cellCallback);
}

void js::IterateChunks(JSContext* cx, void* data,
                       IterateChunkCallback chunkCallback) {
  AutoPrepareForTracing prep(cx);
  AutoLockGC lock(cx->runtime());

  for (auto chunk = cx->runtime()->gc.allNonEmptyChunks(lock); !chunk.done();
       chunk.next()) {
    chunkCallback(cx->runtime(), data, chunk);
  }
}

static void TraverseInnerLazyScriptsForLazyScript(
    JSContext* cx, void* data, LazyScript* enclosingLazyScript,
    IterateLazyScriptCallback lazyScriptCallback,
    const JS::AutoRequireNoGC& nogc) {
  for (JSFunction* fun : enclosingLazyScript->innerFunctions()) {
    // LazyScript::CreateForXDR temporarily initializes innerFunctions with
    // its own function, but it should be overwritten with correct
    // inner functions before getting inserted into parent's innerFunctions.
    MOZ_ASSERT(fun != enclosingLazyScript->functionNonDelazifying());

    if (!fun->isInterpretedLazy()) {
      return;
    }

    LazyScript* lazyScript = fun->lazyScript();
    MOZ_ASSERT(lazyScript->hasEnclosingScope() ||
               lazyScript->hasEnclosingLazyScript());
    MOZ_ASSERT_IF(lazyScript->hasEnclosingLazyScript(),
                  lazyScript->enclosingLazyScript() == enclosingLazyScript);

    lazyScriptCallback(cx->runtime(), data, lazyScript, nogc);

    TraverseInnerLazyScriptsForLazyScript(cx, data, lazyScript,
                                          lazyScriptCallback, nogc);
  }
}

static inline void DoScriptCallback(
    JSContext* cx, void* data, LazyScript* lazyScript,
    IterateLazyScriptCallback lazyScriptCallback,
    const JS::AutoRequireNoGC& nogc) {
  // We call the callback only for the LazyScript that:
  //   (a) its enclosing script has ever been fully compiled and
  //       itself is delazifyable (handled in this function)
  //   (b) it is contained in the (a)'s inner function tree
  //       (handled in TraverseInnerLazyScriptsForLazyScript)
  if (!lazyScript->enclosingScriptHasEverBeenCompiled()) {
    return;
  }

  lazyScriptCallback(cx->runtime(), data, lazyScript, nogc);

  TraverseInnerLazyScriptsForLazyScript(cx, data, lazyScript,
                                        lazyScriptCallback, nogc);
}

static inline void DoScriptCallback(JSContext* cx, void* data, JSScript* script,
                                    IterateScriptCallback scriptCallback,
                                    const JS::AutoRequireNoGC& nogc) {
  // We check for presence of script->isUncompleted() because it is
  // possible that the script was created and thus exposed to GC, but *not*
  // fully initialized from fullyInit{FromEmitter,Trivial} due to errors.
  if (script->isUncompleted()) {
    return;
  }

  scriptCallback(cx->runtime(), data, script, nogc);
}

template <typename T, typename Callback>
static void IterateScriptsImpl(JSContext* cx, Realm* realm, void* data,
                               Callback scriptCallback) {
  MOZ_ASSERT(!cx->suppressGC);
  AutoEmptyNursery empty(cx);
  AutoPrepareForTracing prep(cx);
  JS::AutoSuppressGCAnalysis nogc;

  if (realm) {
    Zone* zone = realm->zone();
    for (auto iter = zone->cellIter<T>(empty); !iter.done(); iter.next()) {
      T* script = iter;
      if (script->realm() != realm) {
        continue;
      }
      DoScriptCallback(cx, data, script, scriptCallback, nogc);
    }
  } else {
    for (ZonesIter zone(cx->runtime(), SkipAtoms); !zone.done(); zone.next()) {
      for (auto iter = zone->cellIter<T>(empty); !iter.done(); iter.next()) {
        T* script = iter;
        DoScriptCallback(cx, data, script, scriptCallback, nogc);
      }
    }
  }
}

void js::IterateScripts(JSContext* cx, Realm* realm, void* data,
                        IterateScriptCallback scriptCallback) {
  IterateScriptsImpl<JSScript>(cx, realm, data, scriptCallback);
}

void js::IterateLazyScripts(JSContext* cx, Realm* realm, void* data,
                            IterateLazyScriptCallback scriptCallback) {
  IterateScriptsImpl<LazyScript>(cx, realm, data, scriptCallback);
}

static void IterateGrayObjects(Zone* zone, GCThingCallback cellCallback,
                               void* data) {
  for (auto kind : ObjectAllocKinds()) {
    for (GrayObjectIter obj(zone, kind); !obj.done(); obj.next()) {
      if (obj->asTenured().isMarkedGray()) {
        cellCallback(data, JS::GCCellPtr(obj.get()));
      }
    }
  }
}

void js::IterateGrayObjects(Zone* zone, GCThingCallback cellCallback,
                            void* data) {
  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());
  AutoPrepareForTracing prep(TlsContext.get());
  ::IterateGrayObjects(zone, cellCallback, data);
}

void js::IterateGrayObjectsUnderCC(Zone* zone, GCThingCallback cellCallback,
                                   void* data) {
  mozilla::DebugOnly<JSRuntime*> rt = zone->runtimeFromMainThread();
  MOZ_ASSERT(JS::RuntimeHeapIsCycleCollecting());
  MOZ_ASSERT(!rt->gc.isIncrementalGCInProgress());
  ::IterateGrayObjects(zone, cellCallback, data);
}

JS_PUBLIC_API void JS_IterateCompartments(
    JSContext* cx, void* data,
    JSIterateCompartmentCallback compartmentCallback) {
  AutoTraceSession session(cx->runtime());

  for (CompartmentsIter c(cx->runtime()); !c.done(); c.next()) {
    if ((*compartmentCallback)(cx, data, c) ==
        JS::CompartmentIterResult::Stop) {
      break;
    }
  }
}

JS_PUBLIC_API void JS_IterateCompartmentsInZone(
    JSContext* cx, JS::Zone* zone, void* data,
    JSIterateCompartmentCallback compartmentCallback) {
  AutoTraceSession session(cx->runtime());

  for (CompartmentsInZoneIter c(zone); !c.done(); c.next()) {
    if ((*compartmentCallback)(cx, data, c) ==
        JS::CompartmentIterResult::Stop) {
      break;
    }
  }
}

JS_PUBLIC_API void JS::IterateRealms(JSContext* cx, void* data,
                                     JS::IterateRealmCallback realmCallback) {
  AutoTraceSession session(cx->runtime());

  Rooted<Realm*> realm(cx);
  for (RealmsIter r(cx->runtime()); !r.done(); r.next()) {
    realm = r;
    (*realmCallback)(cx, data, realm);
  }
}

JS_PUBLIC_API void JS::IterateRealmsWithPrincipals(
    JSContext* cx, JSPrincipals* principals, void* data,
    JS::IterateRealmCallback realmCallback) {
  MOZ_ASSERT(principals);

  AutoTraceSession session(cx->runtime());

  Rooted<Realm*> realm(cx);
  for (RealmsIter r(cx->runtime()); !r.done(); r.next()) {
    if (r->principals() != principals) {
      continue;
    }
    realm = r;
    (*realmCallback)(cx, data, realm);
  }
}

JS_PUBLIC_API void JS::IterateRealmsInCompartment(
    JSContext* cx, JS::Compartment* compartment, void* data,
    JS::IterateRealmCallback realmCallback) {
  AutoTraceSession session(cx->runtime());

  Rooted<Realm*> realm(cx);
  for (RealmsInCompartmentIter r(compartment); !r.done(); r.next()) {
    realm = r;
    (*realmCallback)(cx, data, realm);
  }
}
