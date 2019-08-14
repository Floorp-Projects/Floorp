/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Definitions related to javascript type inference. */

#ifndef vm_TypeInference_h
#define vm_TypeInference_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"

#include "jsfriendapi.h"
#include "jstypes.h"

#include "ds/LifoAlloc.h"
#include "gc/Barrier.h"
#include "jit/IonTypes.h"
#include "js/AllocPolicy.h"
#include "js/HeapAPI.h"  // js::CurrentThreadCanAccessZone
#include "js/UbiNode.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "threading/ProtectedData.h"  // js::ZoneData
#include "vm/Shape.h"
#include "vm/TypeSet.h"

namespace js {

class TypeConstraint;
class TypeZone;
class CompilerConstraintList;
class HeapTypeSetKey;

namespace jit {

struct IonScript;
class JitScript;
class TempAllocator;

}  // namespace jit

// If there is an OOM while sweeping types, the type information is deoptimized
// so that it stays correct (i.e. overapproximates the possible types in the
// zone), but constraints might not have been triggered on the deoptimization
// or even copied over completely. In this case, destroy all JIT code and new
// script information in the zone, the only things whose correctness depends on
// the type constraints.
class AutoClearTypeInferenceStateOnOOM {
  Zone* zone;

  AutoClearTypeInferenceStateOnOOM(const AutoClearTypeInferenceStateOnOOM&) =
      delete;
  void operator=(const AutoClearTypeInferenceStateOnOOM&) = delete;

 public:
  explicit AutoClearTypeInferenceStateOnOOM(Zone* zone);
  ~AutoClearTypeInferenceStateOnOOM();
};

class MOZ_RAII AutoSweepBase {
  // Make sure we don't GC while this class is live since GC might trigger
  // (incremental) sweeping.
  JS::AutoCheckCannotGC nogc;
};

// Sweep an ObjectGroup. Functions that expect a swept group should take a
// reference to this class.
class MOZ_RAII AutoSweepObjectGroup : public AutoSweepBase {
#ifdef DEBUG
  ObjectGroup* group_;
#endif

 public:
  inline explicit AutoSweepObjectGroup(ObjectGroup* group);
#ifdef DEBUG
  inline ~AutoSweepObjectGroup();

  ObjectGroup* group() const { return group_; }
#endif
};

// Sweep the type inference data in a JitScript. Functions that expect a swept
// script should take a reference to this class.
class MOZ_RAII AutoSweepJitScript : public AutoSweepBase {
#ifdef DEBUG
  Zone* zone_;
  jit::JitScript* jitScript_;
#endif

 public:
  inline explicit AutoSweepJitScript(JSScript* script);
#ifdef DEBUG
  inline ~AutoSweepJitScript();

  jit::JitScript* jitScript() const { return jitScript_; }
  Zone* zone() const { return zone_; }
#endif
};

CompilerConstraintList* NewCompilerConstraintList(jit::TempAllocator& alloc);

// Stack class to record information about constraints that need to be added
// after finishing the Definite Properties Analysis. When the analysis succeeds
// the |finishConstraints| method must be called to add the constraints to the
// TypeSets.
//
// There are two constraint types managed here:
//
//   1. Proto constraints for HeapTypeSets, to guard against things like getters
//      and setters on the proto chain.
//
//   2. Inlining constraints for StackTypeSets, to invalidate when additional
//      functions could be called at call sites where we inlined a function.
//
// This class uses bare GC-thing pointers because GC is suppressed when the
// analysis runs.
class MOZ_RAII DPAConstraintInfo {
  struct ProtoConstraint {
    JSObject* proto;
    jsid id;
    ProtoConstraint(JSObject* proto, jsid id) : proto(proto), id(id) {}
  };
  struct InliningConstraint {
    JSScript* caller;
    JSScript* callee;
    InliningConstraint(JSScript* caller, JSScript* callee)
        : caller(caller), callee(callee) {}
  };

  JS::AutoCheckCannotGC nogc_;
  Vector<ProtoConstraint, 8> protoConstraints_;
  Vector<InliningConstraint, 4> inliningConstraints_;

 public:
  explicit DPAConstraintInfo(JSContext* cx)
      : nogc_(cx), protoConstraints_(cx), inliningConstraints_(cx) {}

  DPAConstraintInfo(const DPAConstraintInfo&) = delete;
  void operator=(const DPAConstraintInfo&) = delete;

  MOZ_MUST_USE bool addProtoConstraint(JSObject* proto, jsid id) {
    return protoConstraints_.emplaceBack(proto, id);
  }
  MOZ_MUST_USE bool addInliningConstraint(JSScript* caller, JSScript* callee) {
    return inliningConstraints_.emplaceBack(caller, callee);
  }

  MOZ_MUST_USE bool finishConstraints(JSContext* cx, ObjectGroup* group);
};

bool AddClearDefiniteGetterSetterForPrototypeChain(
    JSContext* cx, DPAConstraintInfo& constraintInfo, ObjectGroup* group,
    HandleId id, bool* added);

bool AddClearDefiniteFunctionUsesInScript(JSContext* cx, ObjectGroup* group,
                                          JSScript* script,
                                          JSScript* calleeScript);

// For groups where only a small number of objects have been allocated, this
// structure keeps track of all objects in the group. Once COUNT objects have
// been allocated, this structure is cleared and the objects are analyzed, to
// perform the new script properties analyses or determine if an unboxed
// representation can be used.
class PreliminaryObjectArray {
 public:
  static const uint32_t COUNT = 20;

 private:
  // All objects with the type which have been allocated. The pointers in
  // this array are weak.
  JSObject* objects[COUNT] = {};  // zeroes

 public:
  PreliminaryObjectArray() = default;

  void registerNewObject(PlainObject* res);
  void unregisterObject(PlainObject* obj);

  JSObject* get(size_t i) const {
    MOZ_ASSERT(i < COUNT);
    return objects[i];
  }

  bool full() const;
  bool empty() const;
  void sweep();
};

class PreliminaryObjectArrayWithTemplate : public PreliminaryObjectArray {
  HeapPtr<Shape*> shape_;

 public:
  explicit PreliminaryObjectArrayWithTemplate(Shape* shape) : shape_(shape) {}

  void clear() { shape_.init(nullptr); }

  Shape* shape() { return shape_; }

  void maybeAnalyze(JSContext* cx, ObjectGroup* group, bool force = false);

  void trace(JSTracer* trc);

  static void writeBarrierPre(
      PreliminaryObjectArrayWithTemplate* preliminaryObjects);
};

/**
 * A type representing the initializer of a property within a script being
 * 'new'd.
 */
class TypeNewScriptInitializer {
 public:
  enum Kind { SETPROP, SETPROP_FRAME } kind;
  uint32_t offset;

  TypeNewScriptInitializer(Kind kind, uint32_t offset)
      : kind(kind), offset(offset) {}
};

/* Is this a reasonable PC to be doing inlining on? */
inline bool isInlinableCall(jsbytecode* pc);

bool ClassCanHaveExtraProperties(const Class* clasp);

class RecompileInfo {
  JSScript* script_;
  IonCompilationId id_;

 public:
  RecompileInfo(JSScript* script, IonCompilationId id)
      : script_(script), id_(id) {}

  JSScript* script() const { return script_; }

  inline jit::IonScript* maybeIonScriptToInvalidate(const TypeZone& zone) const;

  inline bool shouldSweep(const TypeZone& zone);

  bool operator==(const RecompileInfo& other) const {
    return script_ == other.script_ && id_ == other.id_;
  }
};

// The RecompileInfoVector has a MinInlineCapacity of one so that invalidating a
// single IonScript doesn't require an allocation.
typedef Vector<RecompileInfo, 1, SystemAllocPolicy> RecompileInfoVector;

// Generate the type constraints for the compilation. Sets |isValidOut| based on
// whether the type constraints still hold.
bool FinishCompilation(JSContext* cx, HandleScript script,
                       CompilerConstraintList* constraints,
                       IonCompilationId compilationId, bool* isValidOut);

// Update the actual types in any scripts queried by constraints with any
// speculative types added during the definite properties analysis.
void FinishDefinitePropertiesAnalysis(JSContext* cx,
                                      CompilerConstraintList* constraints);

struct AutoEnterAnalysis;

class TypeZone {
  JS::Zone* const zone_;

  /* Pool for type information in this zone. */
  static const size_t TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 8 * 1024;
  ZoneData<LifoAlloc> typeLifoAlloc_;

  // Under CodeGenerator::link, the id of the current compilation.
  ZoneData<mozilla::Maybe<IonCompilationId>> currentCompilationId_;

  TypeZone(const TypeZone&) = delete;
  void operator=(const TypeZone&) = delete;

 public:
  // Current generation for sweeping.
  ZoneOrGCTaskOrIonCompileData<uint32_t> generation;

  // During incremental sweeping, allocator holding the old type information
  // for the zone.
  ZoneData<LifoAlloc> sweepTypeLifoAlloc;

  ZoneData<bool> sweepingTypes;
  ZoneData<bool> oomSweepingTypes;

  ZoneData<bool> keepJitScripts;

  // The topmost AutoEnterAnalysis on the stack, if there is one.
  ZoneData<AutoEnterAnalysis*> activeAnalysis;

  explicit TypeZone(JS::Zone* zone);
  ~TypeZone();

  JS::Zone* zone() const { return zone_; }

  LifoAlloc& typeLifoAlloc() {
#ifdef JS_CRASH_DIAGNOSTICS
    MOZ_RELEASE_ASSERT(CurrentThreadCanAccessZone(zone_));
#endif
    return typeLifoAlloc_.ref();
  }

  void beginSweep();
  void endSweep(JSRuntime* rt);
  void clearAllNewScriptsOnOOM();

  /* Mark a script as needing recompilation once inference has finished. */
  void addPendingRecompile(JSContext* cx, const RecompileInfo& info);
  void addPendingRecompile(JSContext* cx, JSScript* script);

  void processPendingRecompiles(FreeOp* fop, RecompileInfoVector& recompiles);

  bool isSweepingTypes() const { return sweepingTypes; }
  void setSweepingTypes(bool sweeping) {
    MOZ_RELEASE_ASSERT(sweepingTypes != sweeping);
    MOZ_ASSERT_IF(sweeping, !oomSweepingTypes);
    sweepingTypes = sweeping;
    oomSweepingTypes = false;
  }
  void setOOMSweepingTypes() {
    MOZ_ASSERT(sweepingTypes);
    oomSweepingTypes = true;
  }
  bool hadOOMSweepingTypes() {
    MOZ_ASSERT(sweepingTypes);
    return oomSweepingTypes;
  }

  mozilla::Maybe<IonCompilationId> currentCompilationId() const {
    return currentCompilationId_.ref();
  }
  mozilla::Maybe<IonCompilationId>& currentCompilationIdRef() {
    return currentCompilationId_.ref();
  }
};

enum TypeSpewChannel {
  ISpewOps,    /* ops: New constraints and types. */
  ISpewResult, /* result: Final type sets. */
  SPEW_COUNT
};

#ifdef DEBUG

bool InferSpewActive(TypeSpewChannel channel);
const char* InferSpewColorReset();
const char* InferSpewColor(TypeConstraint* constraint);
const char* InferSpewColor(TypeSet* types);

#  define InferSpew(channel, ...)   \
    if (InferSpewActive(channel)) { \
      InferSpewImpl(__VA_ARGS__);   \
    } else {                        \
    }
void InferSpewImpl(const char* fmt, ...) MOZ_FORMAT_PRINTF(1, 2);

/* Check that the type property for id in group contains value. */
bool ObjectGroupHasProperty(JSContext* cx, ObjectGroup* group, jsid id,
                            const Value& value);

#else

inline const char* InferSpewColorReset() { return nullptr; }
inline const char* InferSpewColor(TypeConstraint* constraint) {
  return nullptr;
}
inline const char* InferSpewColor(TypeSet* types) { return nullptr; }

#  define InferSpew(channel, ...) \
    do {                          \
    } while (0)

#endif

// Prints type information for a context if spew is enabled or force is set.
void PrintTypes(JSContext* cx, JS::Compartment* comp, bool force);

} /* namespace js */

// JS::ubi::Nodes can point to object groups; they're js::gc::Cell instances
// with no associated compartment.
namespace JS {
namespace ubi {

template <>
class Concrete<js::ObjectGroup> : TracerConcrete<js::ObjectGroup> {
 protected:
  explicit Concrete(js::ObjectGroup* ptr)
      : TracerConcrete<js::ObjectGroup>(ptr) {}

 public:
  static void construct(void* storage, js::ObjectGroup* ptr) {
    new (storage) Concrete(ptr);
  }

  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};

}  // namespace ubi
}  // namespace JS

#endif /* vm_TypeInference_h */
