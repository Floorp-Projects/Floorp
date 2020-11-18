/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Inline members for javascript type inference. */

#ifndef vm_TypeInference_inl_h
#define vm_TypeInference_inl_h

#include "vm/TypeInference.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/Casting.h"
#include "mozilla/PodOperations.h"

#include <utility>  // for ::std::swap

#include "builtin/Symbol.h"
#include "gc/GC.h"
#include "jit/BaselineJIT.h"
#include "jit/IonScript.h"
#include "jit/JitScript.h"
#include "js/HeapAPI.h"
#include "util/DiagnosticAssertions.h"
#include "vm/ArrayObject.h"
#include "vm/BooleanObject.h"
#include "vm/JSFunction.h"
#include "vm/NativeObject.h"
#include "vm/NumberObject.h"
#include "vm/ObjectGroup.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/StringObject.h"
#include "vm/TypedArrayObject.h"

#include "jit/JitScript-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/ObjectGroup-inl.h"

namespace js {

/////////////////////////////////////////////////////////////////////
// RecompileInfo
/////////////////////////////////////////////////////////////////////

jit::IonScript* RecompileInfo::maybeIonScriptToInvalidate(
    const TypeZone& zone) const {
  MOZ_ASSERT(script_->zone() == zone.zone());

  // Make sure this is not called under CodeGenerator::link (before the
  // IonScript is created).
  MOZ_ASSERT_IF(zone.currentCompilationId(),
                zone.currentCompilationId().ref() != id_);

  if (!script_->hasIonScript() ||
      script_->ionScript()->compilationId() != id_) {
    return nullptr;
  }

  return script_->ionScript();
}

inline bool RecompileInfo::shouldSweep(const TypeZone& zone) {
  if (IsAboutToBeFinalizedUnbarriered(&script_)) {
    return true;
  }

  MOZ_ASSERT(script_->zone() == zone.zone());

  // Don't sweep if we're called under CodeGenerator::link, before the
  // IonScript is created.
  if (zone.currentCompilationId() && zone.currentCompilationId().ref() == id_) {
    return false;
  }

  return maybeIonScriptToInvalidate(zone) == nullptr;
}

class MOZ_RAII AutoSuppressAllocationMetadataBuilder {
  JS::Zone* zone;
  bool saved;

 public:
  explicit AutoSuppressAllocationMetadataBuilder(JSContext* cx)
      : AutoSuppressAllocationMetadataBuilder(cx->realm()->zone()) {}

  explicit AutoSuppressAllocationMetadataBuilder(JS::Zone* zone)
      : zone(zone), saved(zone->suppressAllocationMetadataBuilder) {
    zone->suppressAllocationMetadataBuilder = true;
  }

  ~AutoSuppressAllocationMetadataBuilder() {
    zone->suppressAllocationMetadataBuilder = saved;
  }
};

/*
 * Structure for type inference entry point functions. All functions which can
 * change type information must use this, and functions which depend on
 * intermediate types (i.e. JITs) can use this to ensure that intermediate
 * information is not collected and does not change.
 *
 * Ensures that GC cannot occur. Does additional sanity checking that inference
 * is not reentrant and that recompilations occur properly.
 */
struct MOZ_RAII AutoEnterAnalysis {
  // Prevent GC activity in the middle of analysis.
  gc::AutoSuppressGC suppressGC;

  // Allow clearing inference info on OOM during incremental sweeping. This is
  // constructed for the outermost AutoEnterAnalysis on the stack.
  mozilla::Maybe<AutoClearTypeInferenceStateOnOOM> oom;

  // Pending recompilations to perform before execution of JIT code can resume.
  RecompileInfoVector pendingRecompiles;

  // Prevent us from calling the objectMetadataCallback.
  js::AutoSuppressAllocationMetadataBuilder suppressMetadata;

  JSFreeOp* freeOp;
  Zone* zone;

  explicit AutoEnterAnalysis(JSContext* cx)
      : suppressGC(cx), suppressMetadata(cx) {
    init(cx->defaultFreeOp(), cx->zone());
  }

  AutoEnterAnalysis(JSFreeOp* fop, Zone* zone)
      : suppressGC(TlsContext.get()), suppressMetadata(zone) {
    init(fop, zone);
  }

  ~AutoEnterAnalysis() {
    if (this != zone->types.activeAnalysis) {
      return;
    }

    zone->types.activeAnalysis = nullptr;

    if (!pendingRecompiles.empty()) {
      zone->types.processPendingRecompiles(freeOp, pendingRecompiles);
    }
  }

 private:
  void init(JSFreeOp* fop, Zone* zone) {
#ifdef JS_CRASH_DIAGNOSTICS
    MOZ_RELEASE_ASSERT(CurrentThreadCanAccessZone(zone));
#endif
    this->freeOp = fop;
    this->zone = zone;

    if (!zone->types.activeAnalysis) {
      oom.emplace(zone);
      zone->types.activeAnalysis = this;
    }
  }
};

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

MOZ_ALWAYS_INLINE bool TrackPropertyTypes(JSObject* obj, jsid id) {
  return false;
}

void EnsureTrackPropertyTypes(JSContext* cx, JSObject* obj, jsid id);

inline bool CanHaveEmptyPropertyTypesForOwnProperty(JSObject* obj) {
  // Per the comment on TypeSet::propertySet, property type sets for global
  // objects may be empty for 'own' properties if the global property still
  // has its initial undefined value.
  return obj->is<GlobalObject>();
}

MOZ_ALWAYS_INLINE bool HasTypePropertyId(JSObject* obj, jsid id,
                                         const Value& value) {
  MOZ_CRASH("TODO(no-TI): remove");
}

void AddTypePropertyId(JSContext* cx, ObjectGroup* group, JSObject* obj,
                       jsid id, const Value& value);

MOZ_ALWAYS_INLINE void AddTypePropertyId(JSContext* cx, JSObject* obj, jsid id,
                                         const Value& value) {
  if (!IsTypeInferenceEnabled()) {
    return;
  }
  MOZ_CRASH("TODO(no-TI): remove");
}

inline void MarkTypePropertyNonData(JSContext* cx, JSObject* obj, jsid id) {
  if (!IsTypeInferenceEnabled()) {
    return;
  }
  MOZ_CRASH("TODO(no-TI): remove");
}

inline void MarkTypePropertyNonWritable(JSContext* cx, JSObject* obj, jsid id) {
  if (!IsTypeInferenceEnabled()) {
    return;
  }
  MOZ_CRASH("TODO(no-TI): remove");
}

/////////////////////////////////////////////////////////////////////
// ObjectGroup
/////////////////////////////////////////////////////////////////////

inline AutoSweepObjectGroup::AutoSweepObjectGroup(ObjectGroup* group)
#ifdef DEBUG
    : group_(group)
#endif
{
  if (group->needsSweep()) {
    group->sweep(*this);
  }
}

#ifdef DEBUG
inline AutoSweepObjectGroup::~AutoSweepObjectGroup() {
  // This should still hold.
  MOZ_ASSERT(!group_->needsSweep());
}
#endif

inline AutoSweepJitScript::AutoSweepJitScript(BaseScript* script)
#ifdef DEBUG
    : zone_(script->zone()),
      jitScript_(script->maybeJitScript())
#endif
{
  if (jit::JitScript* jitScript = script->maybeJitScript()) {
    Zone* zone = script->zone();
    if (jitScript->typesNeedsSweep(zone)) {
      jitScript->sweepTypes(*this, zone);
    }
  }
}

#ifdef DEBUG
inline AutoSweepJitScript::~AutoSweepJitScript() {
  // This should still hold.
  MOZ_ASSERT_IF(jitScript_, !jitScript_->typesNeedsSweep(zone_));
}
#endif

}  // namespace js

#endif /* vm_TypeInference_inl_h */
