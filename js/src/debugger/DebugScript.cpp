/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/DebugScript.h"

#include "mozilla/Assertions.h"  // for AssertionConditionType
#include "mozilla/HashTable.h"   // for HashMapEntry, HashTable<>::Ptr, HashMap
#include "mozilla/UniquePtr.h"   // for UniquePtr

#include <utility>  // for std::move

#include "jsapi.h"

#include "debugger/DebugAPI.h"  // for DebugAPI
#include "debugger/Debugger.h"  // for JSBreakpointSite, Breakpoint
#include "gc/Barrier.h"         // for GCPtrNativeObject, WriteBarriered
#include "gc/Cell.h"            // for TenuredCell
#include "gc/FreeOp.h"          // for JSFreeOp
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

#include "gc/FreeOp-inl.h"     // for JSFreeOp::free_
#include "gc/GC-inl.h"         // for ZoneCellIter
#include "gc/Marking-inl.h"    // for CheckGCThingAfterMovingGC
#include "gc/WeakMap-inl.h"    // for WeakMap::remove
#include "vm/JSContext-inl.h"  // for JSContext::check
#include "vm/JSScript-inl.h"   // for JSScript::hasBaselineScript
#include "vm/Realm-inl.h"      // for AutoRealm::AutoRealm

namespace js {

/* static */
DebugScript* DebugScript::get(JSScript* script) {
  MOZ_ASSERT(script->hasDebugScript());
  DebugScriptMap* map = script->zone()->debugScriptMap.get();
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

  /* Create zone's debugScriptMap if necessary. */
  if (!script->zone()->debugScriptMap) {
    auto map = cx->make_unique<DebugScriptMap>();
    if (!map) {
      return nullptr;
    }

    script->zone()->debugScriptMap = std::move(map);
  }

  MOZ_ASSERT(script->hasBytecode());

  DebugScript* borrowed = debug.get();
  if (!script->zone()->debugScriptMap->putNew(script, std::move(debug))) {
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
JSBreakpointSite* DebugScript::getBreakpointSite(JSScript* script,
                                                 jsbytecode* pc) {
  uint32_t offset = script->pcToOffset(pc);
  return script->hasDebugScript() ? get(script)->breakpoints[offset] : nullptr;
}

/* static */
JSBreakpointSite* DebugScript::getOrCreateBreakpointSite(JSContext* cx,
                                                         JSScript* script,
                                                         jsbytecode* pc) {
  AutoRealm ar(cx, script);

  DebugScript* debug = getOrCreate(cx, script);
  if (!debug) {
    return nullptr;
  }

  JSBreakpointSite*& site = debug->breakpoints[script->pcToOffset(pc)];

  if (!site) {
    site = cx->new_<JSBreakpointSite>(script, pc);
    if (!site) {
      return nullptr;
    }
    debug->numSites++;
    AddCellMemory(script, sizeof(JSBreakpointSite), MemoryUse::BreakpointSite);

    if (script->hasBaselineScript()) {
      script->baselineScript()->toggleDebugTraps(script, pc);
    }
  }

  return site;
}

/* static */
void DebugScript::destroyBreakpointSite(JSFreeOp* fop, JSScript* script,
                                        jsbytecode* pc) {
  DebugScript* debug = get(script);
  JSBreakpointSite*& site = debug->breakpoints[script->pcToOffset(pc)];
  MOZ_ASSERT(site);
  MOZ_ASSERT(site->isEmpty());

  site->delete_(fop);
  site = nullptr;

  debug->numSites--;
  if (!debug->needed()) {
    DebugAPI::destroyDebugScript(fop, script);
  }

  if (script->hasBaselineScript()) {
    script->baselineScript()->toggleDebugTraps(script, pc);
  }
}

/* static */
void DebugScript::clearBreakpointsIn(JSFreeOp* fop, Realm* realm, Debugger* dbg,
                                     JSObject* handler) {
  for (auto base = realm->zone()->cellIter<BaseScript>(); !base.done();
       base.next()) {
    MOZ_ASSERT_IF(base->hasDebugScript(), base->hasBytecode());
    if (base->realm() == realm && base->hasDebugScript()) {
      clearBreakpointsIn(fop, base->asJSScript(), dbg, handler);
    }
  }
}

/* static */
void DebugScript::clearBreakpointsIn(JSFreeOp* fop, JSScript* script,
                                     Debugger* dbg, JSObject* handler) {
  // Breakpoints hold wrappers in the script's compartment for the handler. Make
  // sure we don't try to search for the unwrapped handler.
  MOZ_ASSERT_IF(script && handler,
                script->compartment() == handler->compartment());

  if (!script->hasDebugScript()) {
    return;
  }

  for (jsbytecode* pc = script->code(); pc < script->codeEnd(); pc++) {
    JSBreakpointSite* site = getBreakpointSite(script, pc);
    if (site) {
      Breakpoint* nextbp;
      for (Breakpoint* bp = site->firstBreakpoint(); bp; bp = nextbp) {
        nextbp = bp->nextInSite();
        if ((!dbg || bp->debugger == dbg) &&
            (!handler || bp->getHandler() == handler)) {
          bp->remove(fop);
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
  // JSOp::AfterYield instrumentation by calling
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
void DebugAPI::traceDebugScript(JSTracer* trc, JSScript* script) {
  MOZ_ASSERT(script->hasDebugScript());
  DebugScript::get(script)->trace(trc, script);
}

void DebugScript::trace(JSTracer* trc, JSScript* owner) {
  size_t length = owner->length();
  for (size_t i = 0; i < length; i++) {
    JSBreakpointSite* site = breakpoints[i];
    if (site) {
      site->trace(trc);
    }
  }
}

/* static */
void DebugAPI::destroyDebugScript(JSFreeOp* fop, JSScript* script) {
  if (script->hasDebugScript()) {
    DebugScriptMap* map = script->zone()->debugScriptMap.get();
    MOZ_ASSERT(map);
    DebugScriptMap::Ptr p = map->lookup(script);
    MOZ_ASSERT(p);
    DebugScript* debug = p->value().release();
    map->remove(p);
    script->setHasDebugScript(false);

    debug->delete_(fop, script);
  }
}

void DebugScript::delete_(JSFreeOp* fop, JSScript* owner) {
  size_t length = owner->length();
  for (size_t i = 0; i < length; i++) {
    JSBreakpointSite* site = breakpoints[i];
    if (site) {
      site->delete_(fop);
    }
  }

  fop->free_(owner, this, allocSize(owner->length()),
             MemoryUse::ScriptDebugScript);
}

#ifdef JSGC_HASH_TABLE_CHECKS
/* static */
void DebugAPI::checkDebugScriptAfterMovingGC(DebugScript* ds) {
  for (uint32_t i = 0; i < ds->numSites; i++) {
    JSBreakpointSite* site = ds->breakpoints[i];
    if (site) {
      CheckGCThingAfterMovingGC(site->script.get());
    }
  }
}
#endif  // JSGC_HASH_TABLE_CHECKS

/* static */
bool DebugAPI::stepModeEnabledSlow(JSScript* script) {
  return DebugScript::get(script)->stepperCount > 0;
}

/* static */
bool DebugAPI::hasBreakpointsAtSlow(JSScript* script, jsbytecode* pc) {
  JSBreakpointSite* site = DebugScript::getBreakpointSite(script, pc);
  return !!site;
}

}  // namespace js
