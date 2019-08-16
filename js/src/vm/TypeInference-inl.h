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
#include "jit/JitScript.h"
#include "js/HeapAPI.h"
#include "vm/ArrayObject.h"
#include "vm/BooleanObject.h"
#include "vm/JSFunction.h"
#include "vm/NativeObject.h"
#include "vm/NumberObject.h"
#include "vm/ObjectGroup.h"
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

/////////////////////////////////////////////////////////////////////
// Types
/////////////////////////////////////////////////////////////////////

/* static */ inline TypeSet::ObjectKey* TypeSet::ObjectKey::get(JSObject* obj) {
  MOZ_ASSERT(obj);
  if (obj->isSingleton()) {
    return (ObjectKey*)(uintptr_t(obj) | 1);
  }
  return (ObjectKey*)obj->group();
}

/* static */ inline TypeSet::ObjectKey* TypeSet::ObjectKey::get(
    ObjectGroup* group) {
  MOZ_ASSERT(group);
  if (group->singleton()) {
    return (ObjectKey*)(uintptr_t(group->singleton()) | 1);
  }
  return (ObjectKey*)group;
}

inline ObjectGroup* TypeSet::ObjectKey::groupNoBarrier() {
  MOZ_ASSERT(isGroup());
  return (ObjectGroup*)this;
}

inline JSObject* TypeSet::ObjectKey::singletonNoBarrier() {
  MOZ_ASSERT(isSingleton());
  return (JSObject*)(uintptr_t(this) & ~1);
}

inline ObjectGroup* TypeSet::ObjectKey::group() {
  ObjectGroup* res = groupNoBarrier();
  ObjectGroup::readBarrier(res);
  return res;
}

inline JSObject* TypeSet::ObjectKey::singleton() {
  JSObject* res = singletonNoBarrier();
  JSObject::readBarrier(res);
  return res;
}

inline JS::Compartment* TypeSet::ObjectKey::maybeCompartment() {
  if (isSingleton()) {
    return singletonNoBarrier()->compartment();
  }

  return groupNoBarrier()->compartment();
}

/* static */ inline TypeSet::Type TypeSet::ObjectType(const JSObject* obj) {
  if (obj->isSingleton()) {
    return Type(uintptr_t(obj) | 1);
  }
  return Type(uintptr_t(obj->group()));
}

/* static */ inline TypeSet::Type TypeSet::ObjectType(
    const ObjectGroup* group) {
  if (group->singleton()) {
    return Type(uintptr_t(group->singleton()) | 1);
  }
  return Type(uintptr_t(group));
}

/* static */ inline TypeSet::Type TypeSet::ObjectType(const ObjectKey* obj) {
  return Type(uintptr_t(obj));
}

inline TypeSet::Type TypeSet::GetValueType(const Value& val) {
  if (val.isDouble()) {
    return TypeSet::DoubleType();
  }
  if (val.isObject()) {
    return TypeSet::ObjectType(&val.toObject());
  }
  return TypeSet::PrimitiveType(val.extractNonDoubleType());
}

inline bool TypeSet::IsUntrackedValue(const Value& val) {
  return val.isMagic() && (val.whyMagic() == JS_OPTIMIZED_OUT ||
                           val.whyMagic() == JS_UNINITIALIZED_LEXICAL);
}

inline TypeSet::Type TypeSet::GetMaybeUntrackedValueType(const Value& val) {
  return IsUntrackedValue(val) ? UnknownType() : GetValueType(val);
}

inline TypeFlags PrimitiveTypeFlag(ValueType type) {
  switch (type) {
    case ValueType::Undefined:
      return TYPE_FLAG_UNDEFINED;
    case ValueType::Null:
      return TYPE_FLAG_NULL;
    case ValueType::Boolean:
      return TYPE_FLAG_BOOLEAN;
    case ValueType::Int32:
      return TYPE_FLAG_INT32;
    case ValueType::Double:
      return TYPE_FLAG_DOUBLE;
    case ValueType::String:
      return TYPE_FLAG_STRING;
    case ValueType::Symbol:
      return TYPE_FLAG_SYMBOL;
    case ValueType::BigInt:
      return TYPE_FLAG_BIGINT;
    case ValueType::Magic:
      return TYPE_FLAG_LAZYARGS;
    case ValueType::PrivateGCThing:
    case ValueType::Object:
      break;
  }

  MOZ_CRASH("Bad ValueType");
}

inline JSValueType TypeFlagPrimitive(TypeFlags flags) {
  switch (flags) {
    case TYPE_FLAG_UNDEFINED:
      return JSVAL_TYPE_UNDEFINED;
    case TYPE_FLAG_NULL:
      return JSVAL_TYPE_NULL;
    case TYPE_FLAG_BOOLEAN:
      return JSVAL_TYPE_BOOLEAN;
    case TYPE_FLAG_INT32:
      return JSVAL_TYPE_INT32;
    case TYPE_FLAG_DOUBLE:
      return JSVAL_TYPE_DOUBLE;
    case TYPE_FLAG_STRING:
      return JSVAL_TYPE_STRING;
    case TYPE_FLAG_SYMBOL:
      return JSVAL_TYPE_SYMBOL;
    case TYPE_FLAG_BIGINT:
      return JSVAL_TYPE_BIGINT;
    case TYPE_FLAG_LAZYARGS:
      return JSVAL_TYPE_MAGIC;
    default:
      MOZ_CRASH("Bad TypeFlags");
  }
}

/*
 * Get the canonical representation of an id to use when doing inference.  This
 * maintains the constraint that if two different jsids map to the same property
 * in JS (e.g. 3 and "3"), they have the same type representation.
 */
inline jsid IdToTypeId(jsid id) {
  MOZ_ASSERT(!JSID_IS_EMPTY(id));

  // All properties which can be stored in an object's dense elements must
  // map to the aggregate property for index types.
  return JSID_IS_INT(id) ? JSID_VOID : id;
}

const char* TypeIdStringImpl(jsid id);

/* Convert an id for printing during debug. */
static inline const char* TypeIdString(jsid id) {
#ifdef DEBUG
  return TypeIdStringImpl(id);
#else
  return "(missing)";
#endif
}

// New script properties analyses overview.
//
// When constructing objects using 'new' on a script, we attempt to determine
// the properties which that object will eventually have. This is done via two
// analyses. One of these, the definite properties analysis, is static, and the
// other, the acquired properties analysis, is dynamic. As objects are
// constructed using 'new' on some script to create objects of group G, our
// analysis strategy is as follows:
//
// - When the first objects are created, no analysis is immediately performed.
//   Instead, all objects of group G are accumulated in an array.
//
// - After a certain number of such objects have been created, the definite
//   properties analysis is performed. This analyzes the body of the
//   constructor script and any other functions it calls to look for properties
//   which will definitely be added by the constructor in a particular order,
//   creating an object with shape S.
//
// - The properties in S are compared with the greatest common prefix P of the
//   shapes of the objects that have been created. If P has more properties
//   than S, the acquired properties analysis is performed.
//
// - The acquired properties analysis marks all properties in P as definite
//   in G, and creates a new group IG for objects which are partially
//   initialized. Objects of group IG are initially created with shape S, and if
//   they are later given shape P, their group can be changed to G.
//
// For objects which are rarely created, the definite properties analysis can
// be triggered after only one or a few objects have been allocated, when code
// being Ion compiled might access them. In this case type information in the
// constructor might not be good enough for the definite properties analysis to
// compute useful information, but the acquired properties analysis will still
// be able to identify definite properties in this case.
//
// This layered approach is designed to maximize performance on easily
// analyzable code, while still allowing us to determine definite properties
// robustly when code consistently adds the same properties to objects, but in
// complex ways which can't be understood statically.
class TypeNewScript {
 private:
  // Variable-length list of TypeNewScriptInitializer pointers.
  struct InitializerList {
    size_t length;
    TypeNewScriptInitializer entries[1];
  };

  // Scripted function which this information was computed for.
  HeapPtr<JSFunction*> function_ = {};

  // Any preliminary objects with the type. The analyses are not performed
  // until this array is cleared.
  PreliminaryObjectArray* preliminaryObjects = nullptr;

  // After the new script properties analyses have been performed, a template
  // object to use for newly constructed objects. The shape of this object
  // reflects all definite properties the object will have, and the
  // allocation kind to use.
  HeapPtr<PlainObject*> templateObject_ = {};

  // Order in which definite properties become initialized. We need this in
  // case the definite properties are invalidated (such as by adding a setter
  // to an object on the prototype chain) while an object is in the middle of
  // being initialized, so we can walk the stack and fixup any objects which
  // look for in-progress objects which were prematurely set with an incorrect
  // shape. Property assignments in inner frames are preceded by a series of
  // SETPROP_FRAME entries specifying the stack down to the frame containing
  // the write.
  InitializerList* initializerList = nullptr;

  // If there are additional properties found by the acquired properties
  // analysis which were not found by the definite properties analysis, this
  // shape contains all such additional properties (plus the definite
  // properties). When an object of this group acquires this shape, it is
  // fully initialized and its group can be changed to initializedGroup.
  HeapPtr<Shape*> initializedShape_ = {};

  // Group with definite properties set for all properties found by
  // both the definite and acquired properties analyses.
  HeapPtr<ObjectGroup*> initializedGroup_ = {};

 public:
  TypeNewScript() = default;

  ~TypeNewScript() {
    js_delete(preliminaryObjects);
    js_free(initializerList);
  }

  void clear() {
    function_ = nullptr;
    templateObject_ = nullptr;
    initializedShape_ = nullptr;
    initializedGroup_ = nullptr;
  }

  static void writeBarrierPre(TypeNewScript* newScript);

  bool analyzed() const { return preliminaryObjects == nullptr; }

  PlainObject* templateObject() const { return templateObject_; }

  Shape* initializedShape() const { return initializedShape_; }

  ObjectGroup* initializedGroup() const { return initializedGroup_; }

  JSFunction* function() const { return function_; }

  void trace(JSTracer* trc);
  void sweep();

  void registerNewObject(PlainObject* res);
  bool maybeAnalyze(JSContext* cx, ObjectGroup* group, bool* regenerate,
                    bool force = false);

  bool rollbackPartiallyInitializedObjects(JSContext* cx, ObjectGroup* group);

  static bool make(JSContext* cx, ObjectGroup* group, JSFunction* fun);

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  size_t gcMallocBytes() const;

  static size_t offsetOfPreliminaryObjects() {
    return offsetof(TypeNewScript, preliminaryObjects);
  }

  static size_t sizeOfInitializerList(size_t length);
};

inline bool ObjectGroup::hasUnanalyzedPreliminaryObjects() {
  return (newScriptDontCheckGeneration() &&
          !newScriptDontCheckGeneration()->analyzed()) ||
         maybePreliminaryObjectsDontCheckGeneration();
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

void TypeMonitorCallSlow(JSContext* cx, JSObject* callee, const CallArgs& args,
                         bool constructing);

/*
 * Monitor a javascript call, either on entry to the interpreter or made
 * from within the interpreter.
 */
inline void TypeMonitorCall(JSContext* cx, const js::CallArgs& args,
                            bool constructing) {
  if (args.callee().is<JSFunction>()) {
    JSFunction* fun = &args.callee().as<JSFunction>();
    if (fun->isInterpreted() && fun->nonLazyScript()->hasJitScript()) {
      TypeMonitorCallSlow(cx, &args.callee(), args, constructing);
    }
  }
}

MOZ_ALWAYS_INLINE bool TrackPropertyTypes(JSObject* obj, jsid id) {
  if (obj->hasLazyGroup() ||
      obj->group()->unknownPropertiesDontCheckGeneration()) {
    return false;
  }

  if (obj->isSingleton() &&
      !obj->group()->maybeGetPropertyDontCheckGeneration(id)) {
    return false;
  }

  return true;
}

void EnsureTrackPropertyTypes(JSContext* cx, JSObject* obj, jsid id);

inline bool CanHaveEmptyPropertyTypesForOwnProperty(JSObject* obj) {
  // Per the comment on TypeSet::propertySet, property type sets for global
  // objects may be empty for 'own' properties if the global property still
  // has its initial undefined value.
  return obj->is<GlobalObject>();
}

inline bool PropertyHasBeenMarkedNonConstant(JSObject* obj, jsid id) {
  // Non-constant properties are only relevant for singleton objects.
  if (!obj->isSingleton()) {
    return true;
  }

  // EnsureTrackPropertyTypes must have been called on this object.
  AutoSweepObjectGroup sweep(obj->group());
  if (obj->group()->unknownProperties(sweep)) {
    return true;
  }
  HeapTypeSet* types = obj->group()->maybeGetProperty(sweep, IdToTypeId(id));
  return types->nonConstantProperty();
}

MOZ_ALWAYS_INLINE bool HasTrackedPropertyType(JSObject* obj, jsid id,
                                              TypeSet::Type type) {
  MOZ_ASSERT(id == IdToTypeId(id));
  MOZ_ASSERT(TrackPropertyTypes(obj, id));

  if (HeapTypeSet* types =
          obj->group()->maybeGetPropertyDontCheckGeneration(id)) {
    if (!types->hasType(type)) {
      return false;
    }
    // Non-constant properties are only relevant for singleton objects.
    if (obj->isSingleton() && !types->nonConstantProperty()) {
      return false;
    }
    return true;
  }

  return false;
}

MOZ_ALWAYS_INLINE bool HasTypePropertyId(JSObject* obj, jsid id,
                                         TypeSet::Type type) {
  id = IdToTypeId(id);
  if (!TrackPropertyTypes(obj, id)) {
    return true;
  }

  return HasTrackedPropertyType(obj, id, type);
}

MOZ_ALWAYS_INLINE bool HasTypePropertyId(JSObject* obj, jsid id,
                                         const Value& value) {
  return HasTypePropertyId(obj, id, TypeSet::GetValueType(value));
}

void AddTypePropertyId(JSContext* cx, ObjectGroup* group, JSObject* obj,
                       jsid id, TypeSet::Type type);
void AddTypePropertyId(JSContext* cx, ObjectGroup* group, JSObject* obj,
                       jsid id, const Value& value);

/* Add a possible type for a property of obj. */
MOZ_ALWAYS_INLINE void AddTypePropertyId(JSContext* cx, JSObject* obj, jsid id,
                                         TypeSet::Type type) {
  id = IdToTypeId(id);
  if (TrackPropertyTypes(obj, id) && !HasTrackedPropertyType(obj, id, type)) {
    AddTypePropertyId(cx, obj->group(), obj, id, type);
  }
}

MOZ_ALWAYS_INLINE void AddTypePropertyId(JSContext* cx, JSObject* obj, jsid id,
                                         const Value& value) {
  return AddTypePropertyId(cx, obj, id, TypeSet::GetValueType(value));
}

inline void MarkObjectGroupFlags(JSContext* cx, JSObject* obj,
                                 ObjectGroupFlags flags) {
  if (obj->hasLazyGroup()) {
    return;
  }

  AutoSweepObjectGroup sweep(obj->group());
  if (!obj->group()->hasAllFlags(sweep, flags)) {
    obj->group()->setFlags(sweep, cx, flags);
  }
}

inline void MarkObjectGroupUnknownProperties(JSContext* cx, ObjectGroup* obj) {
  AutoSweepObjectGroup sweep(obj);
  if (!obj->unknownProperties(sweep)) {
    obj->markUnknown(sweep, cx);
  }
}

inline void MarkTypePropertyNonData(JSContext* cx, JSObject* obj, jsid id) {
  id = IdToTypeId(id);
  if (TrackPropertyTypes(obj, id)) {
    obj->group()->markPropertyNonData(cx, obj, id);
  }
}

inline void MarkTypePropertyNonWritable(JSContext* cx, JSObject* obj, jsid id) {
  id = IdToTypeId(id);
  if (TrackPropertyTypes(obj, id)) {
    obj->group()->markPropertyNonWritable(cx, obj, id);
  }
}

/* Mark a state change on a particular object. */
inline void MarkObjectStateChange(JSContext* cx, JSObject* obj) {
  if (obj->hasLazyGroup()) {
    return;
  }

  AutoSweepObjectGroup sweep(obj->group());
  if (!obj->group()->unknownProperties(sweep)) {
    obj->group()->markStateChange(sweep, cx);
  }
}

/* static */ inline void jit::JitScript::MonitorBytecodeType(
    JSContext* cx, JSScript* script, jsbytecode* pc, StackTypeSet* types,
    const js::Value& rval) {
  TypeSet::Type type = TypeSet::GetValueType(rval);
  if (!types->hasType(type)) {
    MonitorBytecodeTypeSlow(cx, script, pc, types, type);
  }
}

/* static */ inline void jit::JitScript::MonitorAssign(JSContext* cx,
                                                       HandleObject obj,
                                                       jsid id) {
  if (!obj->isSingleton()) {
    /*
     * Mark as unknown any object which has had dynamic assignments to
     * non-integer properties at SETELEM opcodes. This avoids making large
     * numbers of type properties for hashmap-style objects. We don't need
     * to do this for objects with singleton type, because type properties
     * are only constructed for them when analyzed scripts depend on those
     * specific properties.
     */
    uint32_t i;
    if (IdIsIndex(id, &i)) {
      return;
    }

    // But if we don't have too many properties yet, don't do anything.  The
    // idea here is that normal object initialization should not trigger
    // deoptimization in most cases, while actual usage as a hashmap should.
    ObjectGroup* group = obj->group();
    if (group->basePropertyCountDontCheckGeneration() < 128) {
      return;
    }
    MarkObjectGroupUnknownProperties(cx, group);
  }
}

/* static */ inline void jit::JitScript::MonitorThisType(JSContext* cx,
                                                         JSScript* script,
                                                         TypeSet::Type type) {
  cx->check(script, type);

  JitScript* jitScript = script->maybeJitScript();
  if (!jitScript) {
    return;
  }

  AutoSweepJitScript sweep(script);
  StackTypeSet* types = jitScript->thisTypes(sweep, script);

  if (!types->hasType(type)) {
    AutoEnterAnalysis enter(cx);

    InferSpew(ISpewOps, "externalType: setThis %p: %s", script,
              TypeSet::TypeString(type).get());
    types->addType(sweep, cx, type);
  }
}

/* static */ inline void jit::JitScript::MonitorThisType(
    JSContext* cx, JSScript* script, const js::Value& value) {
  MonitorThisType(cx, script, TypeSet::GetValueType(value));
}

/* static */ inline void jit::JitScript::MonitorArgType(JSContext* cx,
                                                        JSScript* script,
                                                        unsigned arg,
                                                        TypeSet::Type type) {
  cx->check(script->compartment(), type);

  JitScript* jitScript = script->maybeJitScript();
  if (!jitScript) {
    return;
  }

  AutoSweepJitScript sweep(script);
  StackTypeSet* types = jitScript->argTypes(sweep, script, arg);

  if (!types->hasType(type)) {
    AutoEnterAnalysis enter(cx);

    InferSpew(ISpewOps, "externalType: setArg %p %u: %s", script, arg,
              TypeSet::TypeString(type).get());
    types->addType(sweep, cx, type);
  }
}

/* static */ inline void jit::JitScript::MonitorArgType(
    JSContext* cx, JSScript* script, unsigned arg, const js::Value& value) {
  MonitorArgType(cx, script, arg, TypeSet::GetValueType(value));
}

/////////////////////////////////////////////////////////////////////
// TypeHashSet
/////////////////////////////////////////////////////////////////////

// Hashing code shared by objects in TypeSets and properties in ObjectGroups.
struct TypeHashSet {
  // The sets of objects in a type set grow monotonically, are usually empty,
  // almost always small, and sometimes big. For empty or singleton sets, the
  // the pointer refers directly to the value.  For sets fitting into
  // SET_ARRAY_SIZE, an array of this length is used to store the elements.
  // For larger sets, a hash table filled to 25%-50% of capacity is used,
  // with collisions resolved by linear probing.
  static const unsigned SET_ARRAY_SIZE = 8;
  static const unsigned SET_CAPACITY_OVERFLOW = 1u << 30;

  // Get the capacity of a set with the given element count.
  static inline unsigned Capacity(unsigned count) {
    MOZ_ASSERT(count >= 2);
    MOZ_ASSERT(count < SET_CAPACITY_OVERFLOW);

    if (count <= SET_ARRAY_SIZE) {
      return SET_ARRAY_SIZE;
    }

    return 1u << (mozilla::FloorLog2(count) + 2);
  }

  // Compute the FNV hash for the low 32 bits of v.
  template <class T, class KEY>
  static inline uint32_t HashKey(T v) {
    uint32_t nv = KEY::keyBits(v);

    uint32_t hash = 84696351 ^ (nv & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 8) & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 16) & 0xff);
    return (hash * 16777619) ^ ((nv >> 24) & 0xff);
  }

  // Insert space for an element into the specified set and grow its capacity
  // if needed. returned value is an existing or new entry (nullptr if new).
  template <class T, class U, class KEY>
  static U** InsertTry(LifoAlloc& alloc, U**& values, unsigned& count, T key) {
    unsigned capacity = Capacity(count);
    unsigned insertpos = HashKey<T, KEY>(key) & (capacity - 1);

    MOZ_RELEASE_ASSERT(uintptr_t(values[-1]) == capacity);

    // Whether we are converting from a fixed array to hashtable.
    bool converting = (count == SET_ARRAY_SIZE);

    if (!converting) {
      while (values[insertpos] != nullptr) {
        if (KEY::getKey(values[insertpos]) == key) {
          return &values[insertpos];
        }
        insertpos = (insertpos + 1) & (capacity - 1);
      }
    }

    if (count >= SET_CAPACITY_OVERFLOW) {
      return nullptr;
    }

    count++;
    unsigned newCapacity = Capacity(count);

    if (newCapacity == capacity) {
      MOZ_ASSERT(!converting);
      return &values[insertpos];
    }

    // Allocate an extra word right before the array storing the capacity,
    // for sanity checks.
    U** newValues = alloc.newArray<U*>(newCapacity + 1);
    if (!newValues) {
      return nullptr;
    }
    mozilla::PodZero(newValues, newCapacity + 1);

    newValues[0] = (U*)uintptr_t(newCapacity);
    newValues++;

    for (unsigned i = 0; i < capacity; i++) {
      if (values[i]) {
        unsigned pos =
            HashKey<T, KEY>(KEY::getKey(values[i])) & (newCapacity - 1);
        while (newValues[pos] != nullptr) {
          pos = (pos + 1) & (newCapacity - 1);
        }
        newValues[pos] = values[i];
      }
    }

    values = newValues;

    insertpos = HashKey<T, KEY>(key) & (newCapacity - 1);
    while (values[insertpos] != nullptr) {
      insertpos = (insertpos + 1) & (newCapacity - 1);
    }
    return &values[insertpos];
  }

  // Insert an element into the specified set if it is not already there,
  // returning an entry which is nullptr if the element was not there.
  template <class T, class U, class KEY>
  static inline U** Insert(LifoAlloc& alloc, U**& values, unsigned& count,
                           T key) {
    if (count == 0) {
      MOZ_ASSERT(values == nullptr);
      count++;
      return (U**)&values;
    }

    if (count == 1) {
      U* oldData = (U*)values;
      if (KEY::getKey(oldData) == key) {
        return (U**)&values;
      }

      // Allocate an extra word right before the array storing the
      // capacity, for sanity checks.
      values = alloc.newArray<U*>(SET_ARRAY_SIZE + 1);
      if (!values) {
        values = (U**)oldData;
        return nullptr;
      }
      mozilla::PodZero(values, SET_ARRAY_SIZE + 1);

      values[0] = (U*)uintptr_t(SET_ARRAY_SIZE);
      values++;

      count++;

      values[0] = oldData;
      return &values[1];
    }

    if (count <= SET_ARRAY_SIZE) {
      MOZ_RELEASE_ASSERT(uintptr_t(values[-1]) == SET_ARRAY_SIZE);

      for (unsigned i = 0; i < count; i++) {
        if (KEY::getKey(values[i]) == key) {
          return &values[i];
        }
      }

      if (count < SET_ARRAY_SIZE) {
        count++;
        return &values[count - 1];
      }
    }

    return InsertTry<T, U, KEY>(alloc, values, count, key);
  }

  // Lookup an entry in a hash set, return nullptr if it does not exist.
  template <class T, class U, class KEY>
  static MOZ_ALWAYS_INLINE U* Lookup(U** values, unsigned count, T key) {
    if (count == 0) {
      return nullptr;
    }

    if (count == 1) {
      return (KEY::getKey((U*)values) == key) ? (U*)values : nullptr;
    }

    if (count <= SET_ARRAY_SIZE) {
      MOZ_RELEASE_ASSERT(uintptr_t(values[-1]) == SET_ARRAY_SIZE);
      for (unsigned i = 0; i < count; i++) {
        if (KEY::getKey(values[i]) == key) {
          return values[i];
        }
      }
      return nullptr;
    }

    unsigned capacity = Capacity(count);
    unsigned pos = HashKey<T, KEY>(key) & (capacity - 1);

    MOZ_RELEASE_ASSERT(uintptr_t(values[-1]) == capacity);

    while (values[pos] != nullptr) {
      if (KEY::getKey(values[pos]) == key) {
        return values[pos];
      }
      pos = (pos + 1) & (capacity - 1);
    }

    return nullptr;
  }

  template <class T, class U, class Key, typename Fun>
  static void MapEntries(U**& values, unsigned count, Fun f) {
    // No element.
    if (count == 0) {
      MOZ_RELEASE_ASSERT(!values);
      return;
    }

    // Simple functions to read and mutate the mark bit of pointers.
    auto markBit = [](U* elem) -> bool {
      return bool(reinterpret_cast<uintptr_t>(elem) & U::TypeHashSetMarkBit);
    };
    auto toggleMarkBit = [](U* elem) -> U* {
      return reinterpret_cast<U*>(reinterpret_cast<uintptr_t>(elem) ^
                                  U::TypeHashSetMarkBit);
    };

    // When we have a single element it is stored in-place of the function
    // array pointer.
    if (count == 1) {
      U* elem = f(reinterpret_cast<U*>(values));
      MOZ_ASSERT(!markBit(elem));
      values = reinterpret_cast<U**>(elem);
      return;
    }

    // When we have SET_ARRAY_SIZE or fewer elements, the values is an
    // unorderred array.
    if (count <= SET_ARRAY_SIZE) {
      for (unsigned i = 0; i < count; i++) {
        U* elem = f(values[i]);
        MOZ_ASSERT(!markBit(elem));
        values[i] = elem;
      }
      return;
    }

    // This code applies the function f and relocates the values based on
    // the new pointers.
    //
    // To avoid allocations, we reuse the same structure but distinguish the
    // elements to be rellocated from the rellocated elements with the
    // mark bit.
    unsigned capacity = Capacity(count);
    MOZ_RELEASE_ASSERT(uintptr_t(values[-1]) == capacity);
    unsigned found = 0;
    for (unsigned i = 0; i < capacity; i++) {
      if (!values[i]) {
        continue;
      }
      MOZ_ASSERT(found <= count);
      U* elem = f(values[i]);
      values[i] = nullptr;
      MOZ_ASSERT(!markBit(elem));
      values[found++] = toggleMarkBit(elem);
    }
    MOZ_ASSERT(found == count);

    // Follow the same rule as InsertTry, except that for each cell we identify
    // empty cell content with either a nullptr or the value of the mark bit:
    //
    //   nullptr    empty cell.
    //   0b...0.    inserted element.
    //   0b...1.    empty cell - element to be inserted.
    unsigned mask = capacity - 1;
    for (unsigned i = 0; i < count; i++) {
      U* elem = values[i];
      if (!markBit(elem)) {
        // If this is a newly inserted element, this implies that one of
        // the previous objects was moved to this position.
        continue;
      }
      values[i] = nullptr;
      while (elem) {
        MOZ_ASSERT(markBit(elem));
        elem = toggleMarkBit(elem);
        unsigned pos = HashKey<T, Key>(Key::getKey(elem)) & mask;
        while (values[pos] != nullptr && !markBit(values[pos])) {
          pos = (pos + 1) & mask;
        }
        // The replaced element is either a nullptr, which stops this
        // loop, or an element to be inserted, which would be inserted
        // by this loop.
        std::swap(values[pos], elem);
      }
    }
#ifdef DEBUG
    unsigned inserted = 0;
    for (unsigned i = 0; i < capacity; i++) {
      if (!values[i]) {
        continue;
      }
      inserted++;
    }
    MOZ_ASSERT(inserted == count);
#endif
  }
};

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

inline TypeSet::ObjectKey* TypeSet::Type::objectKey() const {
  MOZ_ASSERT(isObject());
  return (ObjectKey*)data;
}

inline JSObject* TypeSet::Type::singleton() const {
  return objectKey()->singleton();
}

inline ObjectGroup* TypeSet::Type::group() const {
  return objectKey()->group();
}

inline JSObject* TypeSet::Type::singletonNoBarrier() const {
  return objectKey()->singletonNoBarrier();
}

inline ObjectGroup* TypeSet::Type::groupNoBarrier() const {
  return objectKey()->groupNoBarrier();
}

inline void TypeSet::Type::trace(JSTracer* trc) {
  if (isSingletonUnchecked()) {
    JSObject* obj = singletonNoBarrier();
    TraceManuallyBarrieredEdge(trc, &obj, "TypeSet::Object");
    *this = TypeSet::ObjectType(obj);
  } else if (isGroupUnchecked()) {
    ObjectGroup* group = groupNoBarrier();
    TraceManuallyBarrieredEdge(trc, &group, "TypeSet::Group");
    *this = TypeSet::ObjectType(group);
  }
}

inline JS::Compartment* TypeSet::Type::maybeCompartment() {
  if (isSingletonUnchecked()) {
    return singletonNoBarrier()->compartment();
  }

  if (isGroupUnchecked()) {
    return groupNoBarrier()->compartment();
  }

  return nullptr;
}

MOZ_ALWAYS_INLINE bool TypeSet::hasType(Type type) const {
  if (unknown()) {
    return true;
  }

  if (type.isUnknown()) {
    return false;
  } else if (type.isPrimitive()) {
    return !!(flags & PrimitiveTypeFlag(type.primitive()));
  } else if (type.isAnyObject()) {
    return !!(flags & TYPE_FLAG_ANYOBJECT);
  } else {
    return !!(flags & TYPE_FLAG_ANYOBJECT) ||
           TypeHashSet::Lookup<ObjectKey*, ObjectKey, ObjectKey>(
               objectSet, baseObjectCount(), type.objectKey()) != nullptr;
  }
}

inline void TypeSet::setBaseObjectCount(uint32_t count) {
  MOZ_ASSERT(count <= TYPE_FLAG_DOMOBJECT_COUNT_LIMIT);
  flags = (flags & ~TYPE_FLAG_OBJECT_COUNT_MASK) |
          (count << TYPE_FLAG_OBJECT_COUNT_SHIFT);
}

inline void HeapTypeSet::newPropertyState(const AutoSweepObjectGroup& sweep,
                                          JSContext* cx) {
  checkMagic();

  /* Propagate the change to all constraints. */
  if (!cx->isHelperThreadContext()) {
    TypeConstraint* constraint = constraintList(sweep);
    while (constraint) {
      constraint->newPropertyState(cx, this);
      constraint = constraint->next();
    }
  } else {
    MOZ_ASSERT(!constraintList(sweep));
  }
}

inline void HeapTypeSet::setNonDataProperty(const AutoSweepObjectGroup& sweep,
                                            JSContext* cx) {
  checkMagic();

  if (flags & TYPE_FLAG_NON_DATA_PROPERTY) {
    return;
  }

  flags |= TYPE_FLAG_NON_DATA_PROPERTY;
  newPropertyState(sweep, cx);
}

inline void HeapTypeSet::setNonWritableProperty(
    const AutoSweepObjectGroup& sweep, JSContext* cx) {
  checkMagic();

  if (flags & TYPE_FLAG_NON_WRITABLE_PROPERTY) {
    return;
  }

  flags |= TYPE_FLAG_NON_WRITABLE_PROPERTY;
  newPropertyState(sweep, cx);
}

inline void HeapTypeSet::setNonConstantProperty(
    const AutoSweepObjectGroup& sweep, JSContext* cx) {
  checkMagic();

  if (flags & TYPE_FLAG_NON_CONSTANT_PROPERTY) {
    return;
  }

  flags |= TYPE_FLAG_NON_CONSTANT_PROPERTY;
  newPropertyState(sweep, cx);
}

inline unsigned TypeSet::getObjectCount() const {
  MOZ_ASSERT(!unknownObject());
  uint32_t count = baseObjectCount();
  if (count > TypeHashSet::SET_ARRAY_SIZE) {
    return TypeHashSet::Capacity(count);
  }
  return count;
}

inline TypeSet::ObjectKey* TypeSet::getObject(unsigned i) const {
  MOZ_ASSERT(i < getObjectCount());
  if (baseObjectCount() == 1) {
    MOZ_ASSERT(i == 0);
    return (ObjectKey*)objectSet;
  }
  return objectSet[i];
}

inline JSObject* TypeSet::getSingleton(unsigned i) const {
  ObjectKey* key = getObject(i);
  return (key && key->isSingleton()) ? key->singleton() : nullptr;
}

inline ObjectGroup* TypeSet::getGroup(unsigned i) const {
  ObjectKey* key = getObject(i);
  return (key && key->isGroup()) ? key->group() : nullptr;
}

inline JSObject* TypeSet::getSingletonNoBarrier(unsigned i) const {
  ObjectKey* key = getObject(i);
  return (key && key->isSingleton()) ? key->singletonNoBarrier() : nullptr;
}

inline ObjectGroup* TypeSet::getGroupNoBarrier(unsigned i) const {
  ObjectKey* key = getObject(i);
  return (key && key->isGroup()) ? key->groupNoBarrier() : nullptr;
}

inline bool TypeSet::hasGroup(unsigned i) const { return getGroupNoBarrier(i); }

inline bool TypeSet::hasSingleton(unsigned i) const {
  return getSingletonNoBarrier(i);
}

inline const JSClass* TypeSet::getObjectClass(unsigned i) const {
  if (JSObject* object = getSingleton(i)) {
    return object->getClass();
  }
  if (ObjectGroup* group = getGroup(i)) {
    return group->clasp();
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////
// ObjectGroup
/////////////////////////////////////////////////////////////////////

inline uint32_t ObjectGroup::basePropertyCountDontCheckGeneration() {
  uint32_t flags = flagsDontCheckGeneration();
  return (flags & OBJECT_FLAG_PROPERTY_COUNT_MASK) >>
         OBJECT_FLAG_PROPERTY_COUNT_SHIFT;
}

inline uint32_t ObjectGroup::basePropertyCount(
    const AutoSweepObjectGroup& sweep) {
  MOZ_ASSERT(sweep.group() == this);
  return basePropertyCountDontCheckGeneration();
}

inline void ObjectGroup::setBasePropertyCount(const AutoSweepObjectGroup& sweep,
                                              uint32_t count) {
  // Note: Callers must ensure they are performing threadsafe operations.
  MOZ_ASSERT(count <= OBJECT_FLAG_PROPERTY_COUNT_LIMIT);
  flags_ = (flags(sweep) & ~OBJECT_FLAG_PROPERTY_COUNT_MASK) |
           (count << OBJECT_FLAG_PROPERTY_COUNT_SHIFT);
}

inline HeapTypeSet* ObjectGroup::getProperty(const AutoSweepObjectGroup& sweep,
                                             JSContext* cx, JSObject* obj,
                                             jsid id) {
  MOZ_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id) ||
             JSID_IS_SYMBOL(id));
  MOZ_ASSERT_IF(!JSID_IS_EMPTY(id), id == IdToTypeId(id));
  MOZ_ASSERT(!unknownProperties(sweep));
  MOZ_ASSERT_IF(obj, obj->group() == this);
  MOZ_ASSERT_IF(singleton(), obj);
  MOZ_ASSERT(cx->compartment() == compartment());

  if (HeapTypeSet* types = maybeGetProperty(sweep, id)) {
    return types;
  }

  Property* base = cx->typeLifoAlloc().new_<Property>(id);
  if (!base) {
    markUnknown(sweep, cx);
    return nullptr;
  }

  uint32_t propertyCount = basePropertyCount(sweep);
  Property** pprop = TypeHashSet::Insert<jsid, Property, Property>(
      cx->typeLifoAlloc(), propertySet, propertyCount, id);
  if (!pprop) {
    markUnknown(sweep, cx);
    return nullptr;
  }

  MOZ_ASSERT(!*pprop);

  setBasePropertyCount(sweep, propertyCount);
  *pprop = base;

  updateNewPropertyTypes(sweep, cx, obj, id, &base->types);

  if (propertyCount == OBJECT_FLAG_PROPERTY_COUNT_LIMIT) {
    // We hit the maximum number of properties the object can have, mark
    // the object unknown so that new properties will not be added in the
    // future.
    markUnknown(sweep, cx);
  }

  base->types.checkMagic();
  return &base->types;
}

MOZ_ALWAYS_INLINE HeapTypeSet* ObjectGroup::maybeGetPropertyDontCheckGeneration(
    jsid id) {
  MOZ_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id) ||
             JSID_IS_SYMBOL(id));
  MOZ_ASSERT_IF(!JSID_IS_EMPTY(id), id == IdToTypeId(id));
  MOZ_ASSERT(!unknownPropertiesDontCheckGeneration());

  Property* prop = TypeHashSet::Lookup<jsid, Property, Property>(
      propertySet, basePropertyCountDontCheckGeneration(), id);

  if (!prop) {
    return nullptr;
  }

  prop->types.checkMagic();
  return &prop->types;
}

MOZ_ALWAYS_INLINE HeapTypeSet* ObjectGroup::maybeGetProperty(
    const AutoSweepObjectGroup& sweep, jsid id) {
  MOZ_ASSERT(sweep.group() == this);
  return maybeGetPropertyDontCheckGeneration(id);
}

inline unsigned ObjectGroup::getPropertyCount(
    const AutoSweepObjectGroup& sweep) {
  uint32_t count = basePropertyCount(sweep);
  if (count > TypeHashSet::SET_ARRAY_SIZE) {
    return TypeHashSet::Capacity(count);
  }
  return count;
}

inline ObjectGroup::Property* ObjectGroup::getProperty(
    const AutoSweepObjectGroup& sweep, unsigned i) {
  MOZ_ASSERT(i < getPropertyCount(sweep));
  Property* result;
  if (basePropertyCount(sweep) == 1) {
    MOZ_ASSERT(i == 0);
    result = (Property*)propertySet;
  } else {
    result = propertySet[i];
  }
  if (result) {
    result->types.checkMagic();
  }
  return result;
}

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

inline AutoSweepJitScript::AutoSweepJitScript(JSScript* script)
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
