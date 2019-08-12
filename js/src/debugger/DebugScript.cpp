/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/DebugScript.h"

#include "mozilla/Assertions.h"  // for AssertionConditionType
#include "mozilla/HashTable.h"   // for HashMapEntry, HashTable<>::Ptr, HashMap
#include "mozilla/Move.h"        // for std::move
#include "mozilla/UniquePtr.h"   // for UniquePtr

#include "jsapi.h"

#include "debugger/DebugAPI.h"  // for DebugAPI
#include "debugger/Debugger.h"  // for BreakpointSite, Breakpoint
#include "gc/Barrier.h"         // for GCPtrNativeObject, WriteBarriered
#include "gc/Cell.h"            // for TenuredCell
#include "gc/FreeOp.h"          // for FreeOp
#include "gc/GCEnum.h"          // for MemoryUse, MemoryUse::BreakpointSite
#include "gc/Marking.h"         // for IsAboutToBeFinalized
#include "gc/Zone.h"            // for Zone
#include "gc/ZoneAllocator.h"   // for AddCellMemory
#include "jit/BaselineJIT.h"    // for BaselineScript
#include "vm/JSContext.h"       // for JSContext
#include "vm/JSScript.h"        // for JSScript, DebugScriptMap
#include "vm/NativeObject.h"    // for NativeObject
#include "vm/Realm.h"           // for Realm, AutoRealm
#include "vm/Runtime.h"         // for ReportOutOfMemory
#include "vm/Stack.h"           // for ActivationIterator, Activation

#include "gc/FreeOp-inl.h"     // for FreeOp::free_
#include "gc/GC-inl.h"         // for ZoneCellIter
#include "gc/Marking-inl.h"    // for CheckGCThingAfterMovingGC
#include "vm/JSContext-inl.h"  // for JSContext::check
#include "vm/Realm-inl.h"      // for AutoRealm::AutoRealm

namespace js {

/* static */
DebugScript* DebugScript::get(JSScript* script) {
  MOZ_ASSERT(script->hasDebugScript());
  DebugScriptMap* map = script->realm()->debugScriptMap.get();
  MOZ_ASSERT(map);
  DebugScriptMap::Ptr p = map->lookup(script);
  MOZ_ASSERT(p);
  return p->value().get();
}

/* static */
DebugScript* DebugScript::getOrCreate(JSContext* cx, JSScript* script) {
  if (script->hasDebugScript()) {
    return get(script);
  }

  size_t nbytes = allocSize(script->length());
  UniqueDebugScript debug(
      reinterpret_cast<DebugScript*>(cx->pod_calloc<uint8_t>(nbytes)));
  if (!debug) {
    return nullptr;
  }

  /* Create realm's debugScriptMap if necessary. */
  if (!script->realm()->debugScriptMap) {
    auto map = cx->make_unique<DebugScriptMap>();
    if (!map) {
      return nullptr;
    }

    script->realm()->debugScriptMap = std::move(map);
  }

  DebugScript* borrowed = debug.get();
  if (!script->realm()->debugScriptMap->putNew(script, std::move(debug))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  // It is safe to set this: we can't fail after this point.
  script->setHasDebugScript(true);
  AddCellMemory(script, nbytes, MemoryUse::ScriptDebugScript);

  /*
   * Ensure that any Interpret() instances running on this script have
   * interrupts enabled. The interrupts must stay enabled until the
   * debug state is destroyed.
   */
  for (ActivationIterator iter(cx); !iter.done(); ++iter) {
    if (iter->isInterpreter()) {
      iter->asInterpreter()->enableInterruptsIfRunning(script);
    }
  }

  return borrowed;
}

/* static */
BreakpointSite* DebugScript::getBreakpointSite(JSScript* script,
                                               jsbytecode* pc) {
  uint32_t offset = script->pcToOffset(pc);
  return script->hasDebugScript() ? get(script)->breakpoints[offset] : nullptr;
}

/* static */
BreakpointSite* DebugScript::getOrCreateBreakpointSite(JSContext* cx,
                                                       JSScript* script,
                                                       jsbytecode* pc) {
  AutoRealm ar(cx, script);

  DebugScript* debug = getOrCreate(cx, script);
  if (!debug) {
    return nullptr;
  }

  BreakpointSite*& site = debug->breakpoints[script->pcToOffset(pc)];

  if (!site) {
    site = cx->new_<JSBreakpointSite>(script, pc);
    if (!site) {
      return nullptr;
    }
    debug->numSites++;
    AddCellMemory(script, sizeof(JSBreakpointSite), MemoryUse::BreakpointSite);
  }

  return site;
}

/* static */
void DebugScript::destroyBreakpointSite(JSFreeOp* fop, JSScript* script,
                                        jsbytecode* pc) {
  DebugScript* debug = get(script);
  BreakpointSite*& site = debug->breakpoints[script->pcToOffset(pc)];
  MOZ_ASSERT(site);

  size_t size = site->type() == BreakpointSite::Type::JS
                    ? sizeof(JSBreakpointSite)
                    : sizeof(WasmBreakpointSite);
  fop->delete_(script, site, size, MemoryUse::BreakpointSite);
  site = nullptr;

  debug->numSites--;
  if (!debug->needed()) {
    DebugAPI::destroyDebugScript(fop, script);
  }
}

/* static */
void DebugScript::clearBreakpointsIn(JSFreeOp* fop, Realm* realm, Debugger* dbg,
                                     JSObject* handler) {
  for (auto script = realm->zone()->cellIter<JSScript>(); !script.done();
       script.next()) {
    if (script->realm() == realm && script->hasDebugScript()) {
      clearBreakpointsIn(fop, script, dbg, handler);
    }
  }
}

/* static */
void DebugScript::clearBreakpointsIn(JSFreeOp* fop, JSScript* script,
                                     Debugger* dbg, JSObject* handler) {
  if (!script->hasDebugScript()) {
    return;
  }

  for (jsbytecode* pc = script->code(); pc < script->codeEnd(); pc++) {
    BreakpointSite* site = getBreakpointSite(script, pc);
    if (site) {
      Breakpoint* nextbp;
      for (Breakpoint* bp = site->firstBreakpoint(); bp; bp = nextbp) {
        nextbp = bp->nextInSite();
        if ((!dbg || bp->debugger == dbg) &&
            (!handler || bp->getHandler() == handler)) {
          bp->destroy(fop);
        }
      }
    }
  }
}

#ifdef DEBUG
/* static */
uint32_t DebugScript::getStepperCount(JSScript* script) {
  return script->hasDebugScript() ? get(script)->stepperCount : 0;
}
#endif  // DEBUG

/* static */
bool DebugScript::incrementStepperCount(JSContext* cx, JSScript* script) {
  cx->check(script);
  MOZ_ASSERT(cx->realm()->isDebuggee());

  AutoRealm ar(cx, script);

  DebugScript* debug = getOrCreate(cx, script);
  if (!debug) {
    return false;
  }

  debug->stepperCount++;

  if (debug->stepperCount == 1) {
    if (script->hasBaselineScript()) {
      script->baselineScript()->toggleDebugTraps(script, nullptr);
    }
  }

  return true;
}

/* static */
void DebugScript::decrementStepperCount(JSFreeOp* fop, JSScript* script) {
  DebugScript* debug = get(script);
  MOZ_ASSERT(debug);
  MOZ_ASSERT(debug->stepperCount > 0);

  debug->stepperCount--;

  if (debug->stepperCount == 0) {
    if (script->hasBaselineScript()) {
      script->baselineScript()->toggleDebugTraps(script, nullptr);
    }

    if (!debug->needed()) {
      DebugAPI::destroyDebugScript(fop, script);
    }
  }
}

/* static */
bool DebugScript::incrementGeneratorObserverCount(JSContext* cx,
                                                  JSScript* script) {
  cx->check(script);
  MOZ_ASSERT(cx->realm()->isDebuggee());

  AutoRealm ar(cx, script);

  DebugScript* debug = getOrCreate(cx, script);
  if (!debug) {
    return false;
  }

  debug->generatorObserverCount++;

  // It is our caller's responsibility, before bumping the generator observer
  // count, to make sure that the baseline code includes the necessary
  // JS_AFTERYIELD instrumentation by calling
  // {ensure,update}ExecutionObservabilityOfScript.
  MOZ_ASSERT_IF(script->hasBaselineScript(),
                script->baselineScript()->hasDebugInstrumentation());

  return true;
}

/* static */
void DebugScript::decrementGeneratorObserverCount(JSFreeOp* fop,
                                                  JSScript* script) {
  DebugScript* debug = get(script);
  MOZ_ASSERT(debug);
  MOZ_ASSERT(debug->generatorObserverCount > 0);

  debug->generatorObserverCount--;

  if (!debug->needed()) {
    DebugAPI::destroyDebugScript(fop, script);
  }
}

/* static */
void DebugAPI::destroyDebugScript(JSFreeOp* fop, JSScript* script) {
  if (script->hasDebugScript()) {
    DebugScriptMap* map = script->realm()->debugScriptMap.get();
    MOZ_ASSERT(map);
    DebugScriptMap::Ptr p = map->lookup(script);
    MOZ_ASSERT(p);
    DebugScript* debug = p->value().release();
    map->remove(p);
    script->setHasDebugScript(false);

    fop->free_(script, debug, DebugScript::allocSize(script->length()),
               MemoryUse::ScriptDebugScript);
  }
}

#ifdef JSGC_HASH_TABLE_CHECKS
/* static */
void DebugAPI::checkDebugScriptAfterMovingGC(DebugScript* ds) {
  for (uint32_t i = 0; i < ds->numSites; i++) {
    BreakpointSite* site = ds->breakpoints[i];
    if (site && site->type() == BreakpointSite::Type::JS) {
      CheckGCThingAfterMovingGC(site->asJS()->script);
    }
  }
}
#endif  // JSGC_HASH_TABLE_CHECKS

/* static */
void DebugAPI::sweepBreakpointsSlow(JSFreeOp* fop, JSScript* script) {
  bool scriptGone = IsAboutToBeFinalizedUnbarriered(&script);
  for (unsigned i = 0; i < script->length(); i++) {
    BreakpointSite* site =
        DebugScript::getBreakpointSite(script, script->offsetToPC(i));
    if (!site) {
      continue;
    }

    Breakpoint* nextbp;
    for (Breakpoint* bp = site->firstBreakpoint(); bp; bp = nextbp) {
      nextbp = bp->nextInSite();
      GCPtrNativeObject& dbgobj = bp->debugger->toJSObjectRef();

      // If we are sweeping, then we expect the script and the
      // debugger object to be swept in the same sweep group, except
      // if the breakpoint was added after we computed the sweep
      // groups. In this case both script and debugger object must be
      // live.
      MOZ_ASSERT_IF(
          script->zone()->isGCSweeping() && dbgobj->zone()->isCollecting(),
          dbgobj->zone()->isGCSweeping() ||
              (!scriptGone && dbgobj->asTenured().isMarkedAny()));

      bool dying = scriptGone || IsAboutToBeFinalized(&dbgobj);
      MOZ_ASSERT_IF(!dying, !IsAboutToBeFinalized(&bp->getHandlerRef()));
      if (dying) {
        bp->destroy(fop);
      }
    }
  }
}

/* static */
bool DebugAPI::stepModeEnabledSlow(JSScript* script) {
  return DebugScript::get(script)->stepperCount > 0;
}

/* static */
bool DebugAPI::hasBreakpointsAtSlow(JSScript* script, jsbytecode* pc) {
  BreakpointSite* site = DebugScript::getBreakpointSite(script, pc);
  return site && site->enabledCount > 0;
}

}  // namespace js
