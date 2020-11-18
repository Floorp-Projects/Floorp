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

#ifdef DEBUG
const char* js::TypeIdStringImpl(jsid id) {
  if (JSID_IS_VOID(id)) {
    return "(index)";
  }
  if (JSID_IS_EMPTY(id)) {
    return "(new)";
  }
  if (JSID_IS_SYMBOL(id)) {
    return "(symbol)";
  }
  static char bufs[4][100];
  static unsigned which = 0;
  which = (which + 1) & 3;
  PutEscapedString(bufs[which], 100, JSID_TO_LINEAR_STRING(id), 0);
  return bufs[which];
}
#endif

/////////////////////////////////////////////////////////////////////
// Logging
/////////////////////////////////////////////////////////////////////

/* static */ const char* TypeSet::NonObjectTypeString(TypeSet::Type type) {
  if (type.isPrimitive()) {
    switch (type.primitive()) {
      case ValueType::Undefined:
        return "void";
      case ValueType::Null:
        return "null";
      case ValueType::Boolean:
        return "bool";
      case ValueType::Int32:
        return "int";
      case ValueType::Double:
        return "float";
      case ValueType::String:
        return "string";
      case ValueType::Symbol:
        return "symbol";
      case ValueType::BigInt:
        return "BigInt";
      case ValueType::Magic:
        return "lazyargs";
      case ValueType::PrivateGCThing:
      case ValueType::Object:
        MOZ_CRASH("Bad type");
    }
  }
  if (type.isUnknown()) {
    return "unknown";
  }

  MOZ_ASSERT(type.isAnyObject());
  return "object";
}

static UniqueChars MakeStringCopy(const char* s) {
  AutoEnterOOMUnsafeRegion oomUnsafe;
  char* copy = strdup(s);
  if (!copy) {
    oomUnsafe.crash("Could not copy string");
  }
  return UniqueChars(copy);
}

/* static */
UniqueChars TypeSet::TypeString(const TypeSet::Type type) {
  if (type.isPrimitive() || type.isUnknown() || type.isAnyObject()) {
    return MakeStringCopy(NonObjectTypeString(type));
  }

  char buf[100];
  if (type.isSingleton()) {
    JSObject* singleton = type.singletonNoBarrier();
    SprintfLiteral(buf, "<%s %#" PRIxPTR ">", singleton->getClass()->name,
                   uintptr_t(singleton));
  } else {
    SprintfLiteral(buf, "[%s * %#" PRIxPTR "]",
                   type.groupNoBarrier()->clasp()->name,
                   uintptr_t(type.groupNoBarrier()));
  }

  return MakeStringCopy(buf);
}

/* static */
UniqueChars TypeSet::ObjectGroupString(const ObjectGroup* group) {
  return TypeString(TypeSet::ObjectType(group));
}

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

const char* js::InferSpewColor(TypeSet* types) {
  /* Type sets are printed out using bold colors. */
  static const char* const colors[] = {"\x1b[1;31m", "\x1b[1;32m", "\x1b[1;33m",
                                       "\x1b[1;34m", "\x1b[1;35m", "\x1b[1;36m",
                                       "\x1b[1;37m"};
  if (!InferSpewColorable()) {
    return "";
  }
  return colors[DefaultHasher<TypeSet*>::hash(types) % 7];
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

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

static TypeFlags MIRTypeToTypeFlags(jit::MIRType type) {
  switch (type) {
    case jit::MIRType::Undefined:
      return TYPE_FLAG_UNDEFINED;
    case jit::MIRType::Null:
      return TYPE_FLAG_NULL;
    case jit::MIRType::Boolean:
      return TYPE_FLAG_BOOLEAN;
    case jit::MIRType::Int32:
      return TYPE_FLAG_INT32;
    case jit::MIRType::Float32:  // Fall through, there's no JSVAL for Float32.
    case jit::MIRType::Double:
      return TYPE_FLAG_DOUBLE;
    case jit::MIRType::String:
      return TYPE_FLAG_STRING;
    case jit::MIRType::Symbol:
      return TYPE_FLAG_SYMBOL;
    case jit::MIRType::BigInt:
      return TYPE_FLAG_BIGINT;
    case jit::MIRType::Object:
      return TYPE_FLAG_ANYOBJECT;
    case jit::MIRType::MagicOptimizedArguments:
      return TYPE_FLAG_LAZYARGS;
    default:
      MOZ_CRASH("Bad MIR type");
  }
}

bool TypeSet::mightBeMIRType(jit::MIRType type) const {
  if (unknown()) {
    return true;
  }

  TypeFlags baseFlags = this->baseFlags();
  if (baseObjectCount() != 0) {
    baseFlags |= TYPE_FLAG_ANYOBJECT;
  }
  return baseFlags & MIRTypeToTypeFlags(type);
}

bool TypeSet::isSubset(std::initializer_list<jit::MIRType> types) const {
  TypeFlags flags = 0;
  for (auto type : types) {
    flags |= MIRTypeToTypeFlags(type);
  }

  TypeFlags baseFlags = this->baseFlags();
  if (baseObjectCount() != 0) {
    baseFlags |= TYPE_FLAG_ANYOBJECT;
  }
  return (baseFlags & flags) == baseFlags;
}

bool TypeSet::objectsAreSubset(TypeSet* other) {
  if (other->unknownObject()) {
    return true;
  }

  if (unknownObject()) {
    return false;
  }

  for (unsigned i = 0; i < getObjectCount(); i++) {
    ObjectKey* key = getObject(i);
    if (!key) {
      continue;
    }
    if (!other->hasType(ObjectType(key))) {
      return false;
    }
  }

  return true;
}

bool TypeSet::isSubset(const TypeSet* other) const {
  if ((baseFlags() & other->baseFlags()) != baseFlags()) {
    return false;
  }

  if (unknownObject()) {
    MOZ_ASSERT(other->unknownObject());
  } else {
    for (unsigned i = 0; i < getObjectCount(); i++) {
      ObjectKey* key = getObject(i);
      if (!key) {
        continue;
      }
      if (!other->hasType(ObjectType(key))) {
        return false;
      }
    }
  }

  return true;
}

bool TypeSet::objectsIntersect(const TypeSet* other) const {
  if (unknownObject() || other->unknownObject()) {
    return true;
  }

  for (unsigned i = 0; i < getObjectCount(); i++) {
    ObjectKey* key = getObject(i);
    if (!key) {
      continue;
    }
    if (other->hasType(ObjectType(key))) {
      return true;
    }
  }

  return false;
}

bool TypeSet::enumerateTypes(TypeList* list) const {
  /* If any type is possible, there's no need to worry about specifics. */
  if (flags & TYPE_FLAG_UNKNOWN) {
    return list->append(UnknownType());
  }

  /* Enqueue type set members stored as bits. */
  for (TypeFlags flag = 1; flag < TYPE_FLAG_ANYOBJECT; flag <<= 1) {
    if (flags & flag) {
      Type type = PrimitiveTypeFromTypeFlag(flag);
      if (!list->append(type)) {
        return false;
      }
    }
  }

  /* If any object is possible, skip specifics. */
  if (flags & TYPE_FLAG_ANYOBJECT) {
    return list->append(AnyObjectType());
  }

  /* Enqueue specific object types. */
  unsigned count = getObjectCount();
  for (unsigned i = 0; i < count; i++) {
    ObjectKey* key = getObject(i);
    if (key) {
      if (!list->append(ObjectType(key))) {
        return false;
      }
    }
  }

  return true;
}

#ifdef DEBUG
static inline bool CompartmentsMatch(Compartment* a, Compartment* b) {
  return !a || !b || a == b;
}
#endif

void TypeSet::clearObjects() {
  setBaseObjectCount(0);
  objectSet = nullptr;
}

Compartment* TypeSet::maybeCompartment() {
  if (unknownObject()) {
    return nullptr;
  }

  unsigned objectCount = getObjectCount();
  for (unsigned i = 0; i < objectCount; i++) {
    ObjectKey* key = getObject(i);
    if (!key) {
      continue;
    }

    Compartment* comp = key->maybeCompartment();
    if (comp) {
      return comp;
    }
  }

  return nullptr;
}

void TypeSet::addType(Type type, LifoAlloc* alloc) {
  MOZ_ASSERT(CompartmentsMatch(maybeCompartment(), type.maybeCompartment()));

  if (unknown()) {
    return;
  }

  if (type.isUnknown()) {
    flags |= TYPE_FLAG_BASE_MASK;
    clearObjects();
    MOZ_ASSERT(unknown());
    return;
  }

  if (type.isPrimitive()) {
    TypeFlags flag = PrimitiveTypeFlag(type);
    if (flags & flag) {
      return;
    }

    /* If we add float to a type set it is also considered to contain int. */
    if (flag == TYPE_FLAG_DOUBLE) {
      flag |= TYPE_FLAG_INT32;
    }

    flags |= flag;
    return;
  }

  if (flags & TYPE_FLAG_ANYOBJECT) {
    return;
  }
  if (type.isAnyObject()) {
    goto unknownObject;
  }

  {
    uint32_t objectCount = baseObjectCount();
    ObjectKey* key = type.objectKey();
    ObjectKey** pentry = TypeHashSet::Insert<ObjectKey*, ObjectKey, ObjectKey>(
        *alloc, objectSet, objectCount, key);
    if (!pentry) {
      goto unknownObject;
    }
    if (*pentry) {
      return;
    }
    *pentry = key;

    setBaseObjectCount(objectCount);

    // Limit the number of objects we track. There is a different limit
    // depending on whether the set only contains DOM objects, which can
    // have many different classes and prototypes but are still optimizable
    // by IonMonkey.
    if (objectCount >= TYPE_FLAG_OBJECT_COUNT_LIMIT) {
      static_assert(TYPE_FLAG_DOMOBJECT_COUNT_LIMIT >=
                    TYPE_FLAG_OBJECT_COUNT_LIMIT);
      // Examining the entire type set is only required when we first hit
      // the normal object limit.
      if (objectCount == TYPE_FLAG_OBJECT_COUNT_LIMIT) {
        for (unsigned i = 0; i < objectCount; i++) {
          const JSClass* clasp = getObjectClass(i);
          if (clasp && !clasp->isDOMClass()) {
            goto unknownObject;
          }
        }
      }

      // Make sure the newly added object is also a DOM object.
      if (!key->clasp()->isDOMClass()) {
        goto unknownObject;
      }

      // Limit the number of DOM objects.
      if (objectCount == TYPE_FLAG_DOMOBJECT_COUNT_LIMIT) {
        goto unknownObject;
      }
    }
  }

  if (type.isGroup()) {
    ObjectGroup* ngroup = type.group();
    MOZ_ASSERT(!ngroup->singleton());
    AutoSweepObjectGroup sweep(ngroup);
    if (ngroup->unknownProperties(sweep)) {
      goto unknownObject;
    }
  }

  if (false) {
  unknownObject:
    flags |= TYPE_FLAG_ANYOBJECT;
    clearObjects();
  }
}

// This class is used for post barriers on type set contents. The only times
// when type sets contain nursery references is when a nursery object has its
// group dynamically changed to a singleton. In such cases the type set will
// need to be traced at the next minor GC.
//
// There is no barrier used for TemporaryTypeSets. These type sets are only
// used during Ion compilation, and if some ConstraintTypeSet contains nursery
// pointers then any number of TemporaryTypeSets might as well. Thus, if there
// are any such ConstraintTypeSets in existence, all off thread Ion
// compilations are canceled by the next minor GC.
class TypeSetRef : public gc::BufferableRef {
  Zone* zone;
  ConstraintTypeSet* types;
#ifdef DEBUG
  uint64_t minorGCNumberAtCreation;
#endif

 public:
  TypeSetRef(Zone* zone, ConstraintTypeSet* types)
      : zone(zone),
        types(types)
#ifdef DEBUG
        ,
        minorGCNumberAtCreation(
            zone->runtimeFromMainThread()->gc.minorGCCount())
#endif
  {
  }

  void trace(JSTracer* trc) override {
    MOZ_ASSERT(trc->runtime()->gc.minorGCCount() == minorGCNumberAtCreation);
    types->trace(zone, trc);
  }
};

void ConstraintTypeSet::postWriteBarrier(JSContext* cx, Type type) {
  if (type.isSingletonUnchecked()) {
    if (gc::StoreBuffer* sb = type.singletonNoBarrier()->storeBuffer()) {
      sb->putGeneric(TypeSetRef(cx->zone(), this));
      sb->setShouldCancelIonCompilations();
      sb->setHasTypeSetPointers();
    }
  }
}

void ConstraintTypeSet::addType(const AutoSweepBase& sweep, JSContext* cx,
                                Type type) {
  checkMagic();

  MOZ_RELEASE_ASSERT(cx->zone()->types.activeAnalysis);

  if (hasType(type)) {
    return;
  }

  TypeSet::addType(type, &cx->typeLifoAlloc());

  if (type.isObjectUnchecked() && unknownObject()) {
    type = AnyObjectType();
  }

  postWriteBarrier(cx, type);

  MOZ_CRASH("TODO(no-TI): remove");
}

void TypeSet::print(FILE* fp) {
  bool fromDebugger = !fp;
  if (!fp) {
    fp = stderr;
  }

  if (flags & TYPE_FLAG_NON_DATA_PROPERTY) {
    fprintf(fp, " [non-data]");
  }

  if (flags & TYPE_FLAG_NON_WRITABLE_PROPERTY) {
    fprintf(fp, " [non-writable]");
  }

  if (definiteProperty()) {
    fprintf(fp, " [definite:%u]", definiteSlot());
  }

  if (baseFlags() == 0 && !baseObjectCount()) {
    fprintf(fp, " missing");
    return;
  }

  if (flags & TYPE_FLAG_UNKNOWN) {
    fprintf(fp, " unknown");
  }
  if (flags & TYPE_FLAG_ANYOBJECT) {
    fprintf(fp, " object");
  }

  if (flags & TYPE_FLAG_UNDEFINED) {
    fprintf(fp, " void");
  }
  if (flags & TYPE_FLAG_NULL) {
    fprintf(fp, " null");
  }
  if (flags & TYPE_FLAG_BOOLEAN) {
    fprintf(fp, " bool");
  }
  if (flags & TYPE_FLAG_INT32) {
    fprintf(fp, " int");
  }
  if (flags & TYPE_FLAG_DOUBLE) {
    fprintf(fp, " float");
  }
  if (flags & TYPE_FLAG_STRING) {
    fprintf(fp, " string");
  }
  if (flags & TYPE_FLAG_SYMBOL) {
    fprintf(fp, " symbol");
  }
  if (flags & TYPE_FLAG_BIGINT) {
    fprintf(fp, " BigInt");
  }
  if (flags & TYPE_FLAG_LAZYARGS) {
    fprintf(fp, " lazyargs");
  }

  uint32_t objectCount = baseObjectCount();
  if (objectCount) {
    fprintf(fp, " object[%u]", objectCount);

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
      ObjectKey* key = getObject(i);
      if (key) {
        fprintf(fp, " %s", TypeString(ObjectType(key)).get());
      }
    }
  }

  if (fromDebugger) {
    fprintf(fp, "\n");
  }
}

/* static */
void TypeSet::readBarrier(const TypeSet* types) {
  if (types->unknownObject()) {
    return;
  }

  for (unsigned i = 0; i < types->getObjectCount(); i++) {
    if (ObjectKey* key = types->getObject(i)) {
      if (key->isSingleton()) {
        (void)key->singleton();
      } else {
        (void)key->group();
      }
    }
  }
}

/* static */
bool TypeSet::IsTypeMarked(JSRuntime* rt, TypeSet::Type* v) {
  bool rv;
  if (v->isSingletonUnchecked()) {
    JSObject* obj = v->singletonNoBarrier();
    rv = IsMarkedUnbarriered(rt, &obj);
    *v = TypeSet::ObjectType(obj);
  } else if (v->isGroupUnchecked()) {
    ObjectGroup* group = v->groupNoBarrier();
    rv = IsMarkedUnbarriered(rt, &group);
    *v = TypeSet::ObjectType(group);
  } else {
    rv = true;
  }
  return rv;
}

static inline bool IsObjectKeyAboutToBeFinalized(TypeSet::ObjectKey** keyp) {
  TypeSet::ObjectKey* key = *keyp;
  bool isAboutToBeFinalized;
  if (key->isGroup()) {
    ObjectGroup* group = key->groupNoBarrier();
    isAboutToBeFinalized = IsAboutToBeFinalizedUnbarriered(&group);
    if (!isAboutToBeFinalized) {
      *keyp = TypeSet::ObjectKey::get(group);
    }
  } else {
    MOZ_ASSERT(key->isSingleton());
    JSObject* singleton = key->singletonNoBarrier();
    isAboutToBeFinalized = IsAboutToBeFinalizedUnbarriered(&singleton);
    if (!isAboutToBeFinalized) {
      *keyp = TypeSet::ObjectKey::get(singleton);
    }
  }
  return isAboutToBeFinalized;
}

bool TypeSet::IsTypeAboutToBeFinalized(TypeSet::Type* v) {
  bool isAboutToBeFinalized;
  if (v->isObjectUnchecked()) {
    TypeSet::ObjectKey* key = v->objectKey();
    isAboutToBeFinalized = IsObjectKeyAboutToBeFinalized(&key);
    if (!isAboutToBeFinalized) {
      *v = TypeSet::ObjectType(key);
    }
  } else {
    isAboutToBeFinalized = false;
  }
  return isAboutToBeFinalized;
}

const JSClass* TypeSet::ObjectKey::clasp() {
  return isGroup() ? group()->clasp() : singleton()->getClass();
}

TaggedProto TypeSet::ObjectKey::proto() {
  return isGroup() ? group()->proto() : singleton()->taggedProto();
}

ObjectGroup* TypeSet::ObjectKey::maybeGroup() {
  if (isGroup()) {
    return group();
  }
  if (!singleton()->hasLazyGroup()) {
    return singleton()->group();
  }
  return nullptr;
}

bool TypeSet::ObjectKey::unknownProperties() {
  if (ObjectGroup* group = maybeGroup()) {
    AutoSweepObjectGroup sweep(group);
    return group->unknownProperties(sweep);
  }
  return false;
}

void TypeSet::ObjectKey::ensureTrackedProperty(JSContext* cx, jsid id) {
  // If we are accessing a lazily defined property which actually exists in
  // the VM and has not been instantiated yet, instantiate it now if we are
  // on the main thread and able to do so.
  if (!JSID_IS_VOID(id) && !JSID_IS_EMPTY(id)) {
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
    if (isSingleton()) {
      JSObject* obj = singleton();
      if (obj->isNative() && obj->as<NativeObject>().containsPure(id)) {
        EnsureTrackPropertyTypes(cx, obj, id);
      }
    }
  }
}

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

#ifdef JS_CRASH_DIAGNOSTICS
void js::ReportMagicWordFailure(uintptr_t actual, uintptr_t expected) {
  MOZ_CRASH_UNSAFE_PRINTF("Got 0x%" PRIxPTR " expected magic word 0x%" PRIxPTR,
                          actual, expected);
}

void js::ReportMagicWordFailure(uintptr_t actual, uintptr_t expected,
                                uintptr_t flags, uintptr_t objectSet) {
  MOZ_CRASH_UNSAFE_PRINTF("Got 0x%" PRIxPTR " expected magic word 0x%" PRIxPTR
                          " flags 0x%" PRIxPTR " objectSet 0x%" PRIxPTR,
                          actual, expected, flags, objectSet);
}
#endif

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

static inline void UpdatePropertyType(const AutoSweepObjectGroup& sweep,
                                      JSContext* cx, HeapTypeSet* types,
                                      NativeObject* obj, Shape* shape,
                                      bool indexed) {
  MOZ_ASSERT(obj->isSingleton() && !obj->hasLazyGroup());

  if (!shape->writable()) {
    types->setNonWritableProperty(sweep, cx);
  }

  if (shape->hasGetterValue() || shape->hasSetterValue()) {
    types->setNonDataProperty(sweep, cx);
    types->TypeSet::addType(TypeSet::UnknownType(), &cx->typeLifoAlloc());
  } else if (shape->isDataProperty()) {
    if (!indexed && types->canSetDefinite(shape->slot())) {
      types->setDefinite(shape->slot());
    }

    const Value& value = obj->getSlot(shape->slot());

    /*
     * Don't add initial undefined types for properties of global objects
     * that are not collated into the JSID_VOID property (see propertySet
     * comment).
     *
     * Also don't add untracked values (initial uninitialized lexical magic
     * values and optimized out values) as appearing in CallObjects, module
     * environments or the global lexical scope.
     */
    MOZ_ASSERT_IF(TypeSet::IsUntrackedValue(value),
                  obj->is<CallObject>() || obj->is<ModuleEnvironmentObject>() ||
                      IsExtensibleLexicalEnvironment(obj));
    if ((indexed || !value.isUndefined() ||
         !CanHaveEmptyPropertyTypesForOwnProperty(obj)) &&
        !TypeSet::IsUntrackedValue(value)) {
      TypeSet::Type type = TypeSet::GetValueType(value);
      types->TypeSet::addType(type, &cx->typeLifoAlloc());
      types->postWriteBarrier(cx, type);
    }

    if (indexed || shape->hadOverwrite()) {
      types->setNonConstantProperty(sweep, cx);
    } else {
      InferSpew(ISpewOps, "typeSet: %sT%p%s property %s %s - setConstant",
                InferSpewColor(types), types, InferSpewColorReset(),
                TypeSet::ObjectGroupString(obj->group()).get(),
                TypeIdString(shape->propid()));
    }
  }
}

void js::AddTypePropertyId(JSContext* cx, ObjectGroup* group, JSObject* obj,
                           jsid id, TypeSet::Type type) {
  MOZ_ASSERT(id == IdToTypeId(id));

  AutoSweepObjectGroup sweep(group);
  if (group->unknownProperties(sweep)) {
    return;
  }

  MOZ_CRASH("TODO(no-TI): remove");
}

void js::AddTypePropertyId(JSContext* cx, ObjectGroup* group, JSObject* obj,
                           jsid id, const Value& value) {
  AddTypePropertyId(cx, group, obj, id, TypeSet::GetValueType(value));
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

  InferSpew(ISpewOps, "%s: setFlags 0x%x",
            TypeSet::ObjectGroupString(this).get(), flags);

  ObjectStateChange(sweep, cx, this, false);
}

void ObjectGroup::markUnknown(const AutoSweepObjectGroup& sweep,
                              JSContext* cx) {
  AutoEnterAnalysis enter(cx);

  MOZ_ASSERT(cx->zone()->types.activeAnalysis);
  MOZ_ASSERT(!unknownProperties(sweep));

  InferSpew(ISpewOps, "UnknownProperties: %s",
            TypeSet::ObjectGroupString(this).get());

  ObjectStateChange(sweep, cx, this, true);
}

void ObjectGroup::print(const AutoSweepObjectGroup& sweep) {
  TaggedProto tagged(proto());
  fprintf(
      stderr, "%s : %s", TypeSet::ObjectGroupString(this).get(),
      tagged.isObject()
          ? TypeSet::TypeString(TypeSet::ObjectType(tagged.toObject())).get()
      : tagged.isDynamic() ? "(dynamic)"
                           : "(null)");

  if (unknownProperties(sweep)) {
    fprintf(stderr, " unknown");
  } else {
    if (!hasAnyFlags(sweep, OBJECT_FLAG_SPARSE_INDEXES)) {
      fprintf(stderr, " dense");
    }
    if (!hasAnyFlags(sweep, OBJECT_FLAG_NON_PACKED)) {
      fprintf(stderr, " packed");
    }
    if (!hasAnyFlags(sweep, OBJECT_FLAG_LENGTH_OVERFLOW)) {
      fprintf(stderr, " noLengthOverflow");
    }
    if (hasAnyFlags(sweep, OBJECT_FLAG_ITERATED)) {
      fprintf(stderr, " iterated");
    }
    if (maybeInterpretedFunction()) {
      fprintf(stderr, " ifun");
    }
  }

  fprintf(stderr, "\n");
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

static inline void TraceObjectKey(JSTracer* trc, TypeSet::ObjectKey** keyp) {
  TypeSet::ObjectKey* key = *keyp;
  if (key->isGroup()) {
    ObjectGroup* group = key->groupNoBarrier();
    TraceManuallyBarrieredEdge(trc, &group, "objectKey_group");
    *keyp = TypeSet::ObjectKey::get(group);
  } else {
    JSObject* singleton = key->singletonNoBarrier();
    TraceManuallyBarrieredEdge(trc, &singleton, "objectKey_singleton");
    *keyp = TypeSet::ObjectKey::get(singleton);
  }
}

void ConstraintTypeSet::trace(Zone* zone, JSTracer* trc) {
  checkMagic();

  // ConstraintTypeSets only hold strong references during minor collections.
  MOZ_ASSERT(JS::RuntimeHeapIsMinorCollecting());

  unsigned objectCount = baseObjectCount();
  TypeHashSet::MapEntries<ObjectKey*, ObjectKey, ObjectKey>(
      objectSet, objectCount, [&](ObjectKey* key) -> ObjectKey* {
        TraceObjectKey(trc, &key);
        return key;
      });

#ifdef DEBUG
  MOZ_ASSERT(objectCount == baseObjectCount());
  if (objectCount >= 2) {
    unsigned capacity = TypeHashSet::Capacity(objectCount);
    MOZ_ASSERT(uintptr_t(objectSet[-1]) == capacity);
    for (unsigned i = 0; i < capacity; i++) {
      ObjectKey* key = objectSet[i];
      if (!key) {
        continue;
      }
      if (key->isGroup()) {
        CheckGCThingAfterMovingGC(key->groupNoBarrier());
      } else {
        CheckGCThingAfterMovingGC(key->singletonNoBarrier());
      }
      Compartment* compartment = key->maybeCompartment();
      MOZ_ASSERT_IF(compartment, compartment->zone() == zone);
    }
  } else if (objectCount == 1) {
    ObjectKey* key = (ObjectKey*)objectSet;
    if (key->isGroup()) {
      CheckGCThingAfterMovingGC(key->groupNoBarrier());
    } else {
      CheckGCThingAfterMovingGC(key->singletonNoBarrier());
    }
    Compartment* compartment = key->maybeCompartment();
    MOZ_ASSERT_IF(compartment, compartment->zone() == zone);
  }
#endif
}

static inline void AssertGCStateForSweep(Zone* zone) {
  MOZ_ASSERT(zone->isGCSweepingOrCompacting());

  // IsAboutToBeFinalized doesn't work right on tenured objects when called
  // during a minor collection.
  MOZ_ASSERT(!JS::RuntimeHeapIsMinorCollecting());
}

void ConstraintTypeSet::sweep(const AutoSweepBase& sweep, Zone* zone) {
  AssertGCStateForSweep(zone);

  checkMagic();

  /*
   * Purge references to objects that are no longer live. Type sets hold
   * only weak references. For type sets containing more than one object,
   * live entries in the object hash need to be copied to the zone's
   * new arena.
   */
  unsigned objectCount = baseObjectCount();
  if (objectCount >= 2) {
    unsigned oldCapacity = TypeHashSet::Capacity(objectCount);
    ObjectKey** oldArray = objectSet;

    MOZ_RELEASE_ASSERT(uintptr_t(oldArray[-1]) == oldCapacity);

    clearObjects();
    objectCount = 0;
    for (unsigned i = 0; i < oldCapacity; i++) {
      ObjectKey* key = oldArray[i];
      if (!key) {
        continue;
      }
      if (!IsObjectKeyAboutToBeFinalized(&key)) {
        ObjectKey** pentry =
            TypeHashSet::Insert<ObjectKey*, ObjectKey, ObjectKey>(
                zone->types.typeLifoAlloc(), objectSet, objectCount, key);
        if (pentry) {
          *pentry = key;
        } else {
          zone->types.setOOMSweepingTypes();
          flags |= TYPE_FLAG_ANYOBJECT;
          clearObjects();
          objectCount = 0;
          break;
        }
      } else if (key->isGroup() &&
                 key->groupNoBarrier()
                     ->unknownPropertiesDontCheckGeneration()) {
        // Object sets containing objects with unknown properties might
        // not be complete. Mark the type set as unknown, which it will
        // be treated as during Ion compilation.
        flags |= TYPE_FLAG_ANYOBJECT;
        clearObjects();
        objectCount = 0;
        break;
      }
    }
    setBaseObjectCount(objectCount);
    // Note: -1/+1 to also poison the capacity field.
    AlwaysPoison(oldArray - 1, JS_SWEPT_TI_PATTERN,
                 (oldCapacity + 1) * sizeof(oldArray[0]),
                 MemCheckKind::MakeUndefined);
  } else if (objectCount == 1) {
    ObjectKey* key = (ObjectKey*)objectSet;
    if (!IsObjectKeyAboutToBeFinalized(&key)) {
      objectSet = reinterpret_cast<ObjectKey**>(key);
    } else {
      // As above, mark type sets containing objects with unknown
      // properties as unknown.
      if (key->isGroup() &&
          key->groupNoBarrier()->unknownPropertiesDontCheckGeneration()) {
        flags |= TYPE_FLAG_ANYOBJECT;
      }
      objectSet = nullptr;
      setBaseObjectCount(0);
    }
  }
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
