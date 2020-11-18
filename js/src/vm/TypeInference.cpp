/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TypeInference-inl.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"

#include <algorithm>
#include <new>
#include <utility>

#include "jsapi.h"

#include "gc/HashUtil.h"
#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/IonAnalysis.h"
#include "js/MemoryMetrics.h"
#include "js/ScalarType.h"  // js::Scalar::Type
#include "js/UniquePtr.h"
#include "util/DiagnosticAssertions.h"
#include "util/Poison.h"
#include "vm/FrameIter.h"  // js::AllScriptFramesIter
#include "vm/HelperThreads.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/Opcodes.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/Printer.h"
#include "vm/Shape.h"
#include "vm/Time.h"

#include "gc/GC-inl.h"
#include "gc/Marking-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectGroup-inl.h"  // JSObject::setSingleton

using namespace js;

using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::PodArrayZero;
using mozilla::PodCopy;

using js::jit::JitScript;

/////////////////////////////////////////////////////////////////////
// Logging
/////////////////////////////////////////////////////////////////////

#ifdef DEBUG

bool js::InferSpewActive(TypeSpewChannel channel) {
  static bool active[SPEW_COUNT];
  static bool checked = false;
  if (!checked) {
    checked = true;
    PodArrayZero(active);
    const char* env = getenv("INFERFLAGS");
    if (!env) {
      return false;
    }
    if (strstr(env, "ops")) {
      active[ISpewOps] = true;
    }
    if (strstr(env, "result")) {
      active[ISpewResult] = true;
    }
    if (strstr(env, "full")) {
      for (unsigned i = 0; i < SPEW_COUNT; i++) {
        active[i] = true;
      }
    }
  }
  return active[channel];
}

static bool InferSpewColorable() {
  /* Only spew colors on xterm-color to not screw up emacs. */
  static bool colorable = false;
  static bool checked = false;
  if (!checked) {
    checked = true;
    const char* env = getenv("TERM");
    if (!env) {
      return false;
    }
    if (strcmp(env, "xterm-color") == 0 || strcmp(env, "xterm-256color") == 0) {
      colorable = true;
    }
  }
  return colorable;
}

const char* js::InferSpewColorReset() {
  if (!InferSpewColorable()) {
    return "";
  }
  return "\x1b[0m";
}

void js::InferSpewImpl(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[infer] ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}
#endif

void js::EnsureTrackPropertyTypes(JSContext* cx, JSObject* obj, jsid id) {
  if (!IsTypeInferenceEnabled()) {
    return;
  }
  MOZ_CRASH("TODO(no-TI): remove");
}

static void ObjectStateChange(const AutoSweepObjectGroup& sweep, JSContext* cx,
                              ObjectGroup* group, bool markingUnknown) {
  if (group->unknownProperties(sweep)) {
    return;
  }

  MOZ_CRASH("TODO(no-TI): remove");
}

bool js::ClassCanHaveExtraProperties(const JSClass* clasp) {
  return clasp->getResolve() || clasp->getOpsLookupProperty() ||
         clasp->getOpsGetProperty() || IsTypedArrayClass(clasp);
}

void TypeZone::processPendingRecompiles(JSFreeOp* fop,
                                        RecompileInfoVector& recompiles) {
  MOZ_ASSERT(!recompiles.empty());

  // Steal the list of scripts to recompile, to make sure we don't try to
  // recursively recompile them. Note: the move constructor will not reset the
  // length if the Vector is using inline storage, so we also use clear().
  RecompileInfoVector pending(std::move(recompiles));
  recompiles.clear();

  jit::Invalidate(*this, fop, pending);

  MOZ_ASSERT(recompiles.empty());
}

void TypeZone::addPendingRecompile(JSContext* cx, const RecompileInfo& info) {
  InferSpew(ISpewOps, "addPendingRecompile: %p:%s:%u", info.script(),
            info.script()->filename(), info.script()->lineno());

  AutoEnterOOMUnsafeRegion oomUnsafe;
  RecompileInfoVector& vector =
      cx->zone()->types.activeAnalysis->pendingRecompiles;
  if (!vector.append(info)) {
    // BUG 1536159: For diagnostics, compute the size of the failed allocation.
    // This presumes the vector growth strategy is to double. This is only used
    // for crash reporting so not a problem if we get it wrong.
    size_t allocSize = 2 * sizeof(RecompileInfo) * vector.capacity();
    oomUnsafe.crash(allocSize, "Could not update pendingRecompiles");
  }
}

void TypeZone::addPendingRecompile(JSContext* cx, JSScript* script) {
  MOZ_ASSERT(script);

  CancelOffThreadIonCompile(script);

  // Let the script warm up again before attempting another compile.
  script->resetWarmUpCounterToDelayIonCompilation();

  if (JitScript* jitScript = script->maybeJitScript()) {
    // Trigger recompilation of the IonScript.
    if (jitScript->hasIonScript()) {
      addPendingRecompile(
          cx, RecompileInfo(script, jitScript->ionScript()->compilationId()));
    }

    // Trigger recompilation of any callers inlining this script.
    AutoSweepJitScript sweep(script);
    RecompileInfoVector* inlinedCompilations =
        jitScript->maybeInlinedCompilations(sweep);
    if (inlinedCompilations) {
      for (const RecompileInfo& info : *inlinedCompilations) {
        addPendingRecompile(cx, info);
      }
      inlinedCompilations->clearAndFree();
    }
  }
}

void js::PrintTypes(JSContext* cx, Compartment* comp, bool force) {
#ifdef DEBUG
  gc::AutoSuppressGC suppressGC(cx);

  Zone* zone = comp->zone();
  AutoEnterAnalysis enter(nullptr, zone);

  if (!force && !InferSpewActive(ISpewResult)) {
    return;
  }

  for (auto group = zone->cellIter<ObjectGroup>(); !group.done();
       group.next()) {
    AutoSweepObjectGroup sweep(group);
    group->print(sweep);
  }
#endif
}

/////////////////////////////////////////////////////////////////////
// ObjectGroup
/////////////////////////////////////////////////////////////////////

void js::AddTypePropertyId(JSContext* cx, ObjectGroup* group, JSObject* obj,
                           jsid id, const Value& value) {
  MOZ_CRASH("TODO(no-TI): remove");
}

void ObjectGroup::markStateChange(const AutoSweepObjectGroup& sweep,
                                  JSContext* cx) {
  MOZ_ASSERT(cx->compartment() == compartment());

  if (unknownProperties(sweep)) {
    return;
  }

  MOZ_CRASH("TODO(no-TI): remove");
}

void ObjectGroup::setFlags(const AutoSweepObjectGroup& sweep, JSContext* cx,
                           ObjectGroupFlags flags) {
  MOZ_ASSERT(!(flags & OBJECT_FLAG_UNKNOWN_PROPERTIES),
             "Should use markUnknown to set unknownProperties");

  if (hasAllFlags(sweep, flags)) {
    return;
  }

  AutoEnterAnalysis enter(cx);

  addFlags(sweep, flags);

  ObjectStateChange(sweep, cx, this, false);
}

void ObjectGroup::markUnknown(const AutoSweepObjectGroup& sweep,
                              JSContext* cx) {
  AutoEnterAnalysis enter(cx);

  MOZ_ASSERT(cx->zone()->types.activeAnalysis);
  MOZ_ASSERT(!unknownProperties(sweep));

  ObjectStateChange(sweep, cx, this, true);
}

void ObjectGroup::print(const AutoSweepObjectGroup& sweep) {
  MOZ_CRASH("TODO(no-TI): remove");
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

static inline void AssertGCStateForSweep(Zone* zone) {
  MOZ_ASSERT(zone->isGCSweepingOrCompacting());

  // IsAboutToBeFinalized doesn't work right on tenured objects when called
  // during a minor collection.
  MOZ_ASSERT(!JS::RuntimeHeapIsMinorCollecting());
}

/*
 * Before sweeping the arenas themselves, scan all groups in a compartment to
 * fixup weak references: property type sets referencing dead JS and type
 * objects, and singleton JS objects whose type is not referenced elsewhere.
 * This is done either incrementally as part of the sweep, or on demand as type
 * objects are accessed before their contents have been swept.
 */
void ObjectGroup::sweep(const AutoSweepObjectGroup& sweep) {
  MOZ_ASSERT(generation() != zoneFromAnyThread()->types.generation);
  setGeneration(zone()->types.generation);

  AssertGCStateForSweep(zone());
}

/* static */
void JitScript::sweepTypes(const js::AutoSweepJitScript& sweep, Zone* zone) {
  MOZ_ASSERT(typesGeneration() != zone->types.generation);
  setTypesGeneration(zone->types.generation);

  AssertGCStateForSweep(zone);

  Maybe<AutoClearTypeInferenceStateOnOOM> clearStateOnOOM;
  if (!zone->types.isSweepingTypes()) {
    clearStateOnOOM.emplace(zone);
  }

  TypeZone& types = zone->types;

  // Sweep the inlinedCompilations Vector.
  if (maybeInlinedCompilations(sweep)) {
    RecompileInfoVector& inlinedCompilations = *maybeInlinedCompilations(sweep);
    size_t dest = 0;
    for (size_t i = 0; i < inlinedCompilations.length(); i++) {
      if (inlinedCompilations[i].shouldSweep(types)) {
        continue;
      }
      inlinedCompilations[dest] = inlinedCompilations[i];
      dest++;
    }
    inlinedCompilations.shrinkTo(dest);
  }

  if (types.hadOOMSweepingTypes()) {
    // It's possible we OOM'd while copying freeze constraints, so they
    // need to be regenerated.
    flags_.hasFreezeConstraints = false;
  }
}

TypeZone::TypeZone(Zone* zone)
    : zone_(zone),
      typeLifoAlloc_(zone, (size_t)TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
      currentCompilationId_(zone),
      generation(zone, 0),
      sweepTypeLifoAlloc(zone, (size_t)TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
      sweepingTypes(zone, false),
      oomSweepingTypes(zone, false),
      keepJitScripts(zone, false),
      activeAnalysis(zone, nullptr) {}

TypeZone::~TypeZone() {
  MOZ_RELEASE_ASSERT(!sweepingTypes);
  MOZ_ASSERT(!keepJitScripts);
}

void TypeZone::beginSweep() {
  MOZ_ASSERT(zone()->isGCSweepingOrCompacting());
  MOZ_ASSERT(
      !zone()->runtimeFromMainThread()->gc.storeBuffer().hasTypeSetPointers());

  // Clear the analysis pool, but don't release its data yet. While sweeping
  // types any live data will be allocated into the pool.
  sweepTypeLifoAlloc.ref().steal(&typeLifoAlloc());

  generation = !generation;
}

void TypeZone::endSweep(JSRuntime* rt) {
  rt->gc.queueAllLifoBlocksForFree(&sweepTypeLifoAlloc.ref());
}

AutoClearTypeInferenceStateOnOOM::AutoClearTypeInferenceStateOnOOM(Zone* zone)
    : zone(zone) {
  MOZ_RELEASE_ASSERT(CurrentThreadCanAccessZone(zone));
  MOZ_ASSERT(!TlsContext.get()->inUnsafeCallWithABI);
  zone->types.setSweepingTypes(true);
}

AutoClearTypeInferenceStateOnOOM::~AutoClearTypeInferenceStateOnOOM() {
  if (zone->types.hadOOMSweepingTypes()) {
    gc::AutoSetThreadIsSweeping threadIsSweeping(zone);
    JSRuntime* rt = zone->runtimeFromMainThread();
    JSFreeOp fop(rt);
    js::CancelOffThreadIonCompile(rt);
    zone->setPreservingCode(false);
    zone->discardJitCode(&fop, Zone::KeepBaselineCode);
  }

  zone->types.setSweepingTypes(false);
}

JS::ubi::Node::Size JS::ubi::Concrete<js::ObjectGroup>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  Size size = js::gc::Arena::thingSize(get().asTenured().getAllocKind());
  return size;
}
