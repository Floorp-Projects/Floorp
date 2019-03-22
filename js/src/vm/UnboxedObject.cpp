/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/UnboxedObject-inl.h"

#include "mozilla/Maybe.h"
#include "mozilla/MemoryChecking.h"

#include "jit/BaselineIC.h"
#include "jit/ExecutableAllocator.h"
#include "jit/JitCommon.h"
#include "jit/Linker.h"

#include "gc/Nursery-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Shape-inl.h"
#include "vm/TypeInference-inl.h"

using mozilla::ArrayLength;
using mozilla::PodCopy;

using namespace js;

/////////////////////////////////////////////////////////////////////
// UnboxedLayout
/////////////////////////////////////////////////////////////////////

void UnboxedLayout::trace(JSTracer* trc) {
  for (size_t i = 0; i < properties_.length(); i++) {
    TraceManuallyBarrieredEdge(trc, &properties_[i].name,
                               "unboxed_layout_name");
  }

  if (newScript()) {
    newScript()->trace(trc);
  }

  TraceNullableEdge(trc, &nativeGroup_, "unboxed_layout_nativeGroup");
  TraceNullableEdge(trc, &nativeShape_, "unboxed_layout_nativeShape");
  TraceNullableEdge(trc, &allocationScript_, "unboxed_layout_allocationScript");
  TraceNullableEdge(trc, &replacementGroup_, "unboxed_layout_replacementGroup");
  TraceNullableEdge(trc, &constructorCode_, "unboxed_layout_constructorCode");
}

size_t UnboxedLayout::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  return mallocSizeOf(this) + properties_.sizeOfExcludingThis(mallocSizeOf) +
         (newScript() ? newScript()->sizeOfIncludingThis(mallocSizeOf) : 0) +
         mallocSizeOf(traceList());
}

void UnboxedLayout::setNewScript(TypeNewScript* newScript,
                                 bool writeBarrier /* = true */) {
  if (newScript_ && writeBarrier) {
    TypeNewScript::writeBarrierPre(newScript_);
  }
  newScript_ = newScript;
}

// Constructor code returns a 0x1 value to indicate the constructor code should
// be cleared.
static const uintptr_t CLEAR_CONSTRUCTOR_CODE_TOKEN = 0x1;

/* static */
bool UnboxedLayout::makeConstructorCode(JSContext* cx,
                                        HandleObjectGroup group) {
    return false;
}

void UnboxedLayout::detachFromRealm() {
  if (isInList()) {
    remove();
  }
}

static Value GetUnboxedValue(uint8_t* p, JSValueType type,
                             bool maybeUninitialized) {
  switch (type) {
    case JSVAL_TYPE_BOOLEAN:
      if (maybeUninitialized) {
        // Squelch Valgrind/MSan errors.
        MOZ_MAKE_MEM_DEFINED(p, 1);
      }
      return BooleanValue(*p != 0);

    case JSVAL_TYPE_INT32:
      if (maybeUninitialized) {
        MOZ_MAKE_MEM_DEFINED(p, sizeof(int32_t));
      }
      return Int32Value(*reinterpret_cast<int32_t*>(p));

    case JSVAL_TYPE_DOUBLE: {
      // During unboxed plain object creation, non-GC thing properties are
      // left uninitialized. This is normally fine, since the properties will
      // be filled in shortly, but if they are read before that happens we
      // need to make sure that doubles are canonical.
      if (maybeUninitialized) {
        MOZ_MAKE_MEM_DEFINED(p, sizeof(double));
      }
      double d = *reinterpret_cast<double*>(p);
      if (maybeUninitialized) {
        return DoubleValue(JS::CanonicalizeNaN(d));
      }
      return DoubleValue(d);
    }

    case JSVAL_TYPE_STRING:
      return StringValue(*reinterpret_cast<JSString**>(p));

    case JSVAL_TYPE_OBJECT:
      return ObjectOrNullValue(*reinterpret_cast<JSObject**>(p));

    default:
      MOZ_CRASH("Invalid type for unboxed value");
  }
}

static bool SetUnboxedValue(JSContext* cx, JSObject* unboxedObject, jsid id,
                            uint8_t* p, JSValueType type, const Value& v,
                            bool preBarrier) {
  switch (type) {
    case JSVAL_TYPE_BOOLEAN:
      if (v.isBoolean()) {
        *p = v.toBoolean();
        return true;
      }
      return false;

    case JSVAL_TYPE_INT32:
      if (v.isInt32()) {
        *reinterpret_cast<int32_t*>(p) = v.toInt32();
        return true;
      }
      return false;

    case JSVAL_TYPE_DOUBLE:
      if (v.isNumber()) {
        *reinterpret_cast<double*>(p) = v.toNumber();
        return true;
      }
      return false;

    case JSVAL_TYPE_STRING:
      if (v.isString()) {
        JSString** np = reinterpret_cast<JSString**>(p);
        if (IsInsideNursery(v.toString()) && !IsInsideNursery(unboxedObject)) {
          v.toString()->storeBuffer()->putWholeCell(unboxedObject);
        }

        if (preBarrier) {
          JSString::writeBarrierPre(*np);
        }
        *np = v.toString();
        return true;
      }
      return false;

    case JSVAL_TYPE_OBJECT:
      if (v.isObjectOrNull()) {
        JSObject** np = reinterpret_cast<JSObject**>(p);

        // Update property types when writing object properties. Types for
        // other properties were captured when the unboxed layout was
        // created.
        AddTypePropertyId(cx, unboxedObject, id, v);

        // As above, trigger post barriers on the whole object.
        JSObject* obj = v.toObjectOrNull();
        if (IsInsideNursery(obj) && !IsInsideNursery(unboxedObject)) {
          obj->storeBuffer()->putWholeCell(unboxedObject);
        }

        if (preBarrier) {
          JSObject::writeBarrierPre(*np);
        }
        *np = obj;
        return true;
      }
      return false;

    default:
      MOZ_CRASH("Invalid type for unboxed value");
  }
}

/////////////////////////////////////////////////////////////////////
// UnboxedPlainObject
/////////////////////////////////////////////////////////////////////

bool UnboxedPlainObject::setValue(JSContext* cx,
                                  const UnboxedLayout::Property& property,
                                  const Value& v) {
  uint8_t* p = &data_[property.offset];
  return SetUnboxedValue(cx, this, NameToId(property.name), p, property.type, v,
                         /* preBarrier = */ true);
}

Value UnboxedPlainObject::getValue(const UnboxedLayout::Property& property,
                                   bool maybeUninitialized /* = false */) {
  uint8_t* p = &data_[property.offset];
  return GetUnboxedValue(p, property.type, maybeUninitialized);
}

void UnboxedPlainObject::trace(JSTracer* trc, JSObject* obj) {
  UnboxedPlainObject* uobj = &obj->as<UnboxedPlainObject>();

  if (uobj->maybeExpando()) {
    TraceManuallyBarrieredEdge(trc, uobj->addressOfExpando(),
                               "unboxed_expando");
  }

  const UnboxedLayout& layout = uobj->layoutDontCheckGeneration();
  const int32_t* list = layout.traceList();
  if (!list) {
    return;
  }

  uint8_t* data = uobj->data();
  while (*list != -1) {
    GCPtrString* heap = reinterpret_cast<GCPtrString*>(data + *list);
    TraceEdge(trc, heap, "unboxed_string");
    list++;
  }
  list++;
  while (*list != -1) {
    GCPtrObject* heap = reinterpret_cast<GCPtrObject*>(data + *list);
    TraceNullableEdge(trc, heap, "unboxed_object");
    list++;
  }

  // Unboxed objects don't have Values to trace.
  MOZ_ASSERT(*(list + 1) == -1);
}

/* static */
UnboxedExpandoObject* UnboxedPlainObject::ensureExpando(
    JSContext* cx, Handle<UnboxedPlainObject*> obj) {
  if (obj->maybeExpando()) {
    return obj->maybeExpando();
  }

  UnboxedExpandoObject* expando = NewObjectWithGivenProto<UnboxedExpandoObject>(
      cx, nullptr, gc::AllocKind::OBJECT4);
  if (!expando) {
    return nullptr;
  }

  // Don't track property types for expando objects. This allows Baseline
  // and Ion AddSlot ICs to guard on the unboxed group without guarding on
  // the expando group.
  MarkObjectGroupUnknownProperties(cx, expando->group());

  // If the expando is tenured then the original object must also be tenured.
  // Otherwise barriers triggered on the original object for writes to the
  // expando (as can happen in the JIT) won't see the tenured->nursery edge.
  // See WholeCellEdges::mark.
  MOZ_ASSERT_IF(!IsInsideNursery(expando), !IsInsideNursery(obj));

  // As with setValue(), we need to manually trigger post barriers on the
  // whole object. If we treat the field as a GCPtrObject and later
  // convert the object to its native representation, we will end up with a
  // corrupted store buffer entry.
  if (IsInsideNursery(expando) && !IsInsideNursery(obj)) {
    expando->storeBuffer()->putWholeCell(obj);
  }

  obj->setExpandoUnsafe(expando);
  return expando;
}

bool UnboxedPlainObject::containsUnboxedOrExpandoProperty(JSContext* cx,
                                                          jsid id) const {
  if (layoutDontCheckGeneration().lookup(id)) {
    return true;
  }

  if (maybeExpando() && maybeExpando()->containsShapeOrElement(cx, id)) {
    return true;
  }

  return false;
}

static bool PropagatePropertyTypes(JSContext* cx, jsid id,
                                   ObjectGroup* oldGroup,
                                   ObjectGroup* newGroup) {
  AutoSweepObjectGroup sweepOld(oldGroup);
  HeapTypeSet* typeProperty = oldGroup->maybeGetProperty(sweepOld, id);
  TypeSet::TypeList types;
  if (!typeProperty->enumerateTypes(&types)) {
    ReportOutOfMemory(cx);
    return false;
  }
  for (size_t j = 0; j < types.length(); j++) {
    AddTypePropertyId(cx, newGroup, nullptr, id, types[j]);
  }
  return true;
}

static PlainObject* MakeReplacementTemplateObject(JSContext* cx,
                                                  HandleObjectGroup group,
                                                  const UnboxedLayout& layout) {
  Rooted<PlainObject*> obj(
      cx, NewObjectWithGroup<PlainObject>(cx, group, layout.getAllocKind(),
                                          TenuredObject));
  if (!obj) {
    return nullptr;
  }

  RootedId id(cx);
  for (size_t i = 0; i < layout.properties().length(); i++) {
    const UnboxedLayout::Property& property = layout.properties()[i];
    id = NameToId(property.name);
    Shape* shape = NativeObject::addDataProperty(
        cx, obj, id, SHAPE_INVALID_SLOT, JSPROP_ENUMERATE);
    if (!shape) {
      return nullptr;
    }
    MOZ_ASSERT(shape->slot() == i);
    MOZ_ASSERT(obj->slotSpan() == i + 1);
    MOZ_ASSERT(!obj->inDictionaryMode());
  }

  return obj;
}

/* static */
bool UnboxedLayout::makeNativeGroup(JSContext* cx, ObjectGroup* group) {
  MOZ_ASSERT(cx->realm() == group->realm());

  AutoEnterAnalysis enter(cx);

  AutoSweepObjectGroup sweep(group);
  UnboxedLayout& layout = group->unboxedLayout(sweep);
  Rooted<TaggedProto> proto(cx, group->proto());

  MOZ_ASSERT(!layout.nativeGroup());

  RootedObjectGroup replacementGroup(cx);

  // Immediately clear any new script on the group. This is done by replacing
  // the existing new script with one for a replacement default new group.
  // This is done so that the size of the replacment group's objects is the
  // same as that for the unboxed group, so that we do not see polymorphic
  // slot accesses later on for sites that see converted objects from this
  // group and objects that were allocated using the replacement new group.
  if (layout.newScript()) {
    replacementGroup = ObjectGroupRealm::makeGroup(cx, group->realm(),
                                                   &PlainObject::class_, proto);
    if (!replacementGroup) {
      return false;
    }

    PlainObject* templateObject =
        MakeReplacementTemplateObject(cx, replacementGroup, layout);
    if (!templateObject) {
      return false;
    }

    TypeNewScript* replacementNewScript = TypeNewScript::makeNativeVersion(
        cx, layout.newScript(), templateObject);
    if (!replacementNewScript) {
      return false;
    }

    replacementGroup->setNewScript(replacementNewScript);
    gc::gcTracer.traceTypeNewScript(replacementGroup);

    group->clearNewScript(cx, replacementGroup);
  }

  // Similarly, if this group is keyed to an allocation site, replace its
  // entry with a new group that has no unboxed layout.
  if (layout.allocationScript()) {
    RootedScript script(cx, layout.allocationScript());
    jsbytecode* pc = layout.allocationPc();

    replacementGroup = ObjectGroupRealm::makeGroup(cx, group->realm(),
                                                   &PlainObject::class_, proto);
    if (!replacementGroup) {
      return false;
    }

    PlainObject* templateObject = &script->getObject(pc)->as<PlainObject>();
    replacementGroup->addDefiniteProperties(cx, templateObject->lastProperty());

    ObjectGroupRealm& realm = ObjectGroupRealm::get(group);
    realm.replaceAllocationSiteGroup(script, pc, JSProto_Object,
                                     replacementGroup);

    // Clear any baseline information at this opcode which might use the old
    // group.
    if (script->hasICScript()) {
      jit::ICEntry& entry =
          script->icScript()->icEntryFromPCOffset(script->pcToOffset(pc));
      jit::ICFallbackStub* fallback = entry.fallbackStub();
      for (jit::ICStubIterator iter = fallback->beginChain(); !iter.atEnd();
           iter++) {
        iter.unlink(cx);
      }
      if (fallback->isNewObject_Fallback()) {
        fallback->toNewObject_Fallback()->setTemplateObject(nullptr);
      } else if (fallback->isNewArray_Fallback()) {
        fallback->toNewArray_Fallback()->setTemplateGroup(replacementGroup);
      }
    }
  }

  size_t nfixed = gc::GetGCKindSlots(layout.getAllocKind());

  RootedShape shape(cx, EmptyShape::getInitialShape(cx, &PlainObject::class_,
                                                    proto, nfixed, 0));
  if (!shape) {
    return false;
  }

  // Add shapes for each property, if this is for a plain object.
  for (size_t i = 0; i < layout.properties().length(); i++) {
    const UnboxedLayout::Property& property = layout.properties()[i];

    Rooted<StackShape> child(
        cx, StackShape(shape->base()->unowned(), NameToId(property.name), i,
                       JSPROP_ENUMERATE));
    shape = cx->zone()->propertyTree().getChild(cx, shape, child);
    if (!shape) {
      return false;
    }
  }

  ObjectGroup* nativeGroup = ObjectGroupRealm::makeGroup(
      cx, group->realm(), &PlainObject::class_, proto,
      group->flags(sweep) & OBJECT_FLAG_DYNAMIC_MASK);
  if (!nativeGroup) {
    return false;
  }

  // No sense propagating if we don't know what we started with.
  AutoSweepObjectGroup sweepNative(nativeGroup);
  if (!group->unknownProperties(sweep)) {
    for (size_t i = 0; i < layout.properties().length(); i++) {
      const UnboxedLayout::Property& property = layout.properties()[i];
      jsid id = NameToId(property.name);
      if (!PropagatePropertyTypes(cx, id, group, nativeGroup)) {
        return false;
      }

      // If we are OOM we may not be able to propagate properties.
      if (nativeGroup->unknownProperties(sweepNative)) {
        break;
      }
    }
  } else {
    // If we skip, though, the new group had better agree.
    MOZ_ASSERT(nativeGroup->unknownProperties(sweepNative));
  }

  layout.nativeGroup_ = nativeGroup;
  layout.nativeShape_ = shape;
  layout.replacementGroup_ = replacementGroup;

  nativeGroup->setOriginalUnboxedGroup(group);

  group->markStateChange(sweep, cx);

  return true;
}

/* static */
NativeObject* UnboxedPlainObject::convertToNative(JSContext* cx,
                                                  JSObject* obj) {
  // This function returns the original object (instead of bool) to make sure
  // Ion's LConvertUnboxedObjectToNative works correctly. If we return bool
  // and use defineReuseInput, the object register is not preserved across the
  // call.

  const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();
  UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();

  // Ensure we're working in the object's realm, so we don't have to worry about
  // creating groups or templates in the wrong realm.
  AutoRealm ar(cx, obj);

  if (!layout.nativeGroup()) {
    if (!UnboxedLayout::makeNativeGroup(cx, obj->group())) {
      return nullptr;
    }

    // makeNativeGroup can reentrantly invoke this method.
    if (obj->is<PlainObject>()) {
      return &obj->as<PlainObject>();
    }
  }

  AutoValueVector values(cx);
  for (size_t i = 0; i < layout.properties().length(); i++) {
    // We might be reading properties off the object which have not been
    // initialized yet. Make sure any double values we read here are
    // canonicalized.
    if (!values.append(obj->as<UnboxedPlainObject>().getValue(
            layout.properties()[i], true))) {
      return nullptr;
    }
  }

  // We are eliminating the expando edge with the conversion, so trigger a
  // pre barrier.
  JSObject::writeBarrierPre(expando);

  // Additionally trigger a post barrier on the expando itself. Whole cell
  // store buffer entries can be added on the original unboxed object for
  // writes to the expando (see WholeCellEdges::trace), so after conversion
  // we need to make sure the expando itself will still be traced.
  if (expando && !IsInsideNursery(expando)) {
    cx->runtime()->gc.storeBuffer().putWholeCell(expando);
  }

  obj->setGroup(layout.nativeGroup());
  obj->as<PlainObject>().setLastPropertyMakeNative(cx, layout.nativeShape());

  for (size_t i = 0; i < values.length(); i++) {
    obj->as<PlainObject>().initSlotUnchecked(i, values[i]);
  }

  if (!expando) {
    return &obj->as<PlainObject>();
  }

  // Add properties from the expando object to the object, in order.
  // Suppress GC here, so that callers don't need to worry about this
  // method collecting. The stuff below can only fail due to OOM, in
  // which case the object will not have been completely filled back in.
  gc::AutoSuppressGC suppress(cx);

  Vector<jsid> ids(cx);
  for (Shape::Range<NoGC> r(expando->lastProperty()); !r.empty();
       r.popFront()) {
    if (!ids.append(r.front().propid())) {
      return nullptr;
    }
  }
  for (size_t i = 0; i < expando->getDenseInitializedLength(); i++) {
    if (!expando->getDenseElement(i).isMagic(JS_ELEMENTS_HOLE)) {
      if (!ids.append(INT_TO_JSID(i))) {
        return nullptr;
      }
    }
  }
  ::Reverse(ids.begin(), ids.end());

  RootedPlainObject nobj(cx, &obj->as<PlainObject>());
  Rooted<UnboxedExpandoObject*> nexpando(cx, expando);
  RootedId id(cx);
  Rooted<PropertyDescriptor> desc(cx);
  for (size_t i = 0; i < ids.length(); i++) {
    id = ids[i];
    if (!GetOwnPropertyDescriptor(cx, nexpando, id, &desc)) {
      return nullptr;
    }
    ObjectOpResult result;
    if (!DefineProperty(cx, nobj, id, desc, result)) {
      return nullptr;
    }
    MOZ_ASSERT(result.ok());
  }

  return nobj;
}

/* static */ JS::Result<UnboxedObject*, JS::OOM&> UnboxedObject::createInternal(
    JSContext* cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
    js::HandleObjectGroup group) {
  const js::Class* clasp = group->clasp();
  MOZ_ASSERT(clasp == &UnboxedPlainObject::class_);

  MOZ_ASSERT(CanBeFinalizedInBackground(kind, clasp));
  kind = GetBackgroundAllocKind(kind);

  debugCheckNewObject(group, /* shape = */ nullptr, kind, heap);

  JSObject* obj =
      js::AllocateObject(cx, kind, /* nDynamicSlots = */ 0, heap, clasp);
  if (!obj) {
    return cx->alreadyReportedOOM();
  }

  UnboxedObject* uobj = static_cast<UnboxedObject*>(obj);
  uobj->initGroup(group);

  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  cx->realm()->setObjectPendingMetadata(cx, uobj);

  js::gc::gcTracer.traceCreateObject(uobj);

  return uobj;
}

/* static */
UnboxedPlainObject* UnboxedPlainObject::create(JSContext* cx,
                                               HandleObjectGroup group,
                                               NewObjectKind newKind) {
  AutoSetNewObjectMetadata metadata(cx);

  MOZ_ASSERT(group->clasp() == &class_);

  gc::AllocKind allocKind;
  {
    AutoSweepObjectGroup sweep(group);
    allocKind = group->unboxedLayout(sweep).getAllocKind();
  }
  gc::InitialHeap heap = GetInitialHeap(newKind, &class_);

  MOZ_ASSERT(newKind != SingletonObject);

  JSObject* obj;
  JS_TRY_VAR_OR_RETURN_NULL(cx, obj,
                            createInternal(cx, allocKind, heap, group));

  UnboxedPlainObject* uobj = static_cast<UnboxedPlainObject*>(obj);
  uobj->initExpando();

  // Initialize reference fields of the object. All fields in the object will
  // be overwritten shortly, but references need to be safe for the GC.
  const int32_t* list = uobj->layout().traceList();
  if (list) {
    uint8_t* data = uobj->data();
    while (*list != -1) {
      GCPtrString* heap = reinterpret_cast<GCPtrString*>(data + *list);
      heap->init(cx->names().empty);
      list++;
    }
    list++;
    while (*list != -1) {
      GCPtrObject* heap = reinterpret_cast<GCPtrObject*>(data + *list);
      heap->init(nullptr);
      list++;
    }
    // Unboxed objects don't have Values to initialize.
    MOZ_ASSERT(*(list + 1) == -1);
  }

  return uobj;
}

/* static */
JSObject* UnboxedPlainObject::createWithProperties(JSContext* cx,
                                                   HandleObjectGroup group,
                                                   NewObjectKind newKind,
                                                   IdValuePair* properties) {
  MOZ_ASSERT(newKind == GenericObject || newKind == TenuredObject);

  {
    AutoSweepObjectGroup sweep(group);
    UnboxedLayout& layout = group->unboxedLayout(sweep);

    if (layout.constructorCode()) {
      MOZ_ASSERT(!cx->helperThread());

      typedef JSObject* (*ConstructorCodeSignature)(IdValuePair*,
                                                    NewObjectKind);
      ConstructorCodeSignature function =
          reinterpret_cast<ConstructorCodeSignature>(
              layout.constructorCode()->raw());

      JSObject* obj;
      {
        JS::AutoSuppressGCAnalysis nogc;
        obj = reinterpret_cast<JSObject*>(
            CALL_GENERATED_2(function, properties, newKind));
      }
      if (obj > reinterpret_cast<JSObject*>(CLEAR_CONSTRUCTOR_CODE_TOKEN)) {
        return obj;
      }

      if (obj == reinterpret_cast<JSObject*>(CLEAR_CONSTRUCTOR_CODE_TOKEN)) {
        layout.setConstructorCode(nullptr);
      }
    }
  }

  UnboxedPlainObject* obj = UnboxedPlainObject::create(cx, group, newKind);
  if (!obj) {
    return nullptr;
  }

  // AutoSweepObjectGroup can't be live across a GC call, so we reset() it
  // before calling NewPlainObjectWithProperties.
  mozilla::Maybe<AutoSweepObjectGroup> sweep;
  sweep.emplace(group);
  UnboxedLayout& layout = group->unboxedLayout(*sweep);

  for (size_t i = 0; i < layout.properties().length(); i++) {
    if (!obj->setValue(cx, layout.properties()[i], properties[i].value)) {
      sweep.reset();
      return NewPlainObjectWithProperties(
          cx, properties, layout.properties().length(), newKind);
    }
  }

#ifndef JS_CODEGEN_NONE
  if (!cx->helperThread() && !group->unknownProperties(*sweep) &&
      !layout.constructorCode() && cx->runtime()->jitSupportsFloatingPoint &&
      jit::CanLikelyAllocateMoreExecutableMemory()) {
    if (!UnboxedLayout::makeConstructorCode(cx, group)) {
      return nullptr;
    }
  }
#endif

  return obj;
}

/* static */
bool UnboxedPlainObject::obj_lookupProperty(
    JSContext* cx, HandleObject obj, HandleId id, MutableHandleObject objp,
    MutableHandle<PropertyResult> propp) {
  if (obj->as<UnboxedPlainObject>().containsUnboxedOrExpandoProperty(cx, id)) {
    propp.setNonNativeProperty();
    objp.set(obj);
    return true;
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    objp.set(nullptr);
    propp.setNotFound();
    return true;
  }

  return LookupProperty(cx, proto, id, objp, propp);
}

/* static */
bool UnboxedPlainObject::obj_defineProperty(JSContext* cx, HandleObject obj,
                                            HandleId id,
                                            Handle<PropertyDescriptor> desc,
                                            ObjectOpResult& result) {
  const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();

  if (const UnboxedLayout::Property* property = layout.lookup(id)) {
    if (!desc.getter() && !desc.setter() &&
        desc.attributes() == JSPROP_ENUMERATE) {
      // This define is equivalent to setting an existing property.
      if (obj->as<UnboxedPlainObject>().setValue(cx, *property, desc.value())) {
        return result.succeed();
      }
    }

    // Trying to incompatibly redefine an existing property requires the
    // object to be converted to a native object.
    if (!convertToNative(cx, obj)) {
      return false;
    }

    return DefineProperty(cx, obj, id, desc, result);
  }

  // Define the property on the expando object.
  Rooted<UnboxedExpandoObject*> expando(
      cx, ensureExpando(cx, obj.as<UnboxedPlainObject>()));
  if (!expando) {
    return false;
  }

  // Update property types on the unboxed object as well.
  AddTypePropertyId(cx, obj, id, desc.value());

  return DefineProperty(cx, expando, id, desc, result);
}

/* static */
bool UnboxedPlainObject::obj_hasProperty(JSContext* cx, HandleObject obj,
                                         HandleId id, bool* foundp) {
  if (obj->as<UnboxedPlainObject>().containsUnboxedOrExpandoProperty(cx, id)) {
    *foundp = true;
    return true;
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    *foundp = false;
    return true;
  }

  return HasProperty(cx, proto, id, foundp);
}

/* static */
bool UnboxedPlainObject::obj_getProperty(JSContext* cx, HandleObject obj,
                                         HandleValue receiver, HandleId id,
                                         MutableHandleValue vp) {
  const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();

  if (const UnboxedLayout::Property* property = layout.lookup(id)) {
    vp.set(obj->as<UnboxedPlainObject>().getValue(*property));
    return true;
  }

  if (UnboxedExpandoObject* expando =
          obj->as<UnboxedPlainObject>().maybeExpando()) {
    if (expando->containsShapeOrElement(cx, id)) {
      RootedObject nexpando(cx, expando);
      return GetProperty(cx, nexpando, receiver, id, vp);
    }
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    vp.setUndefined();
    return true;
  }

  return GetProperty(cx, proto, receiver, id, vp);
}

/* static */
bool UnboxedPlainObject::obj_setProperty(JSContext* cx, HandleObject obj,
                                         HandleId id, HandleValue v,
                                         HandleValue receiver,
                                         ObjectOpResult& result) {
  const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();

  if (const UnboxedLayout::Property* property = layout.lookup(id)) {
    if (receiver.isObject() && obj == &receiver.toObject()) {
      if (obj->as<UnboxedPlainObject>().setValue(cx, *property, v)) {
        return result.succeed();
      }

      if (!convertToNative(cx, obj)) {
        return false;
      }
      return SetProperty(cx, obj, id, v, receiver, result);
    }

    return SetPropertyByDefining(cx, id, v, receiver, result);
  }

  if (UnboxedExpandoObject* expando =
          obj->as<UnboxedPlainObject>().maybeExpando()) {
    if (expando->containsShapeOrElement(cx, id)) {
      // Update property types on the unboxed object as well.
      AddTypePropertyId(cx, obj, id, v);

      RootedObject nexpando(cx, expando);
      return SetProperty(cx, nexpando, id, v, receiver, result);
    }
  }

  return SetPropertyOnProto(cx, obj, id, v, receiver, result);
}

/* static */
bool UnboxedPlainObject::obj_getOwnPropertyDescriptor(
    JSContext* cx, HandleObject obj, HandleId id,
    MutableHandle<PropertyDescriptor> desc) {
  const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();

  if (const UnboxedLayout::Property* property = layout.lookup(id)) {
    desc.value().set(obj->as<UnboxedPlainObject>().getValue(*property));
    desc.setAttributes(JSPROP_ENUMERATE);
    desc.object().set(obj);
    return true;
  }

  if (UnboxedExpandoObject* expando =
          obj->as<UnboxedPlainObject>().maybeExpando()) {
    if (expando->containsShapeOrElement(cx, id)) {
      RootedObject nexpando(cx, expando);
      if (!GetOwnPropertyDescriptor(cx, nexpando, id, desc)) {
        return false;
      }
      if (desc.object() == nexpando) {
        desc.object().set(obj);
      }
      return true;
    }
  }

  desc.object().set(nullptr);
  return true;
}

/* static */
bool UnboxedPlainObject::obj_deleteProperty(JSContext* cx, HandleObject obj,
                                            HandleId id,
                                            ObjectOpResult& result) {
  if (!convertToNative(cx, obj)) {
    return false;
  }
  return DeleteProperty(cx, obj, id, result);
}

/* static */
bool UnboxedPlainObject::newEnumerate(JSContext* cx, HandleObject obj,
                                      AutoIdVector& properties,
                                      bool enumerableOnly) {
  // Ignore expando properties here, they are special-cased by the property
  // enumeration code.

  const UnboxedLayout::PropertyVector& unboxed =
      obj->as<UnboxedPlainObject>().layout().properties();
  for (size_t i = 0; i < unboxed.length(); i++) {
    if (!properties.append(NameToId(unboxed[i].name))) {
      return false;
    }
  }

  return true;
}

const Class UnboxedExpandoObject::class_ = {"UnboxedExpandoObject", 0};

static const ClassOps UnboxedPlainObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate   */
    UnboxedPlainObject::newEnumerate,
    nullptr, /* resolve     */
    nullptr, /* mayResolve  */
    nullptr, /* finalize    */
    nullptr, /* call        */
    nullptr, /* hasInstance */
    nullptr, /* construct   */
    UnboxedPlainObject::trace,
};

static const ObjectOps UnboxedPlainObjectObjectOps = {
    UnboxedPlainObject::obj_lookupProperty,
    UnboxedPlainObject::obj_defineProperty,
    UnboxedPlainObject::obj_hasProperty,
    UnboxedPlainObject::obj_getProperty,
    UnboxedPlainObject::obj_setProperty,
    UnboxedPlainObject::obj_getOwnPropertyDescriptor,
    UnboxedPlainObject::obj_deleteProperty,
    nullptr, /* getElements */
    nullptr  /* funToString */
};

const Class UnboxedPlainObject::class_ = {
    js_Object_str,
    Class::NON_NATIVE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object) |
        JSCLASS_DELAY_METADATA_BUILDER,
    &UnboxedPlainObjectClassOps,
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    &UnboxedPlainObjectObjectOps};

/////////////////////////////////////////////////////////////////////
// API
/////////////////////////////////////////////////////////////////////

static inline Value NextValue(Handle<GCVector<Value>> values,
                              size_t* valueCursor) {
  return values[(*valueCursor)++];
}

void UnboxedPlainObject::fillAfterConvert(JSContext* cx,
                                          Handle<GCVector<Value>> values,
                                          size_t* valueCursor) {
  initExpando();
  memset(data(), 0, layout().size());
  for (size_t i = 0; i < layout().properties().length(); i++) {
    MOZ_ALWAYS_TRUE(
        setValue(cx, layout().properties()[i], NextValue(values, valueCursor)));
  }
}