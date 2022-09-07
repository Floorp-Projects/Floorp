/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "wasm/TypedObject-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"

#include <algorithm>

#include "gc/Marking.h"
#include "js/CharacterEncoding.h"
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/PropertySpec.h"
#include "js/ScalarType.h"  // js::Scalar::Type
#include "js/Vector.h"
#include "util/StringBuffer.h"
#include "vm/GlobalObject.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/Realm.h"
#include "vm/SelfHosting.h"
#include "vm/StringType.h"
#include "vm/TypedArrayObject.h"
#include "vm/Uint8Clamped.h"

#include "gc/GCContext-inl.h"
#include "gc/Marking-inl.h"
#include "gc/Nursery-inl.h"
#include "gc/StoreBuffer-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Shape-inl.h"

using mozilla::AssertedCast;
using mozilla::CheckedUint32;
using mozilla::IsPowerOfTwo;
using mozilla::PodCopy;
using mozilla::PointerRangeSize;

using namespace js;
using namespace wasm;

static const JSClassOps RttValueClassOps = {
    nullptr,             // addProperty
    nullptr,             // delProperty
    nullptr,             // enumerate
    nullptr,             // newEnumerate
    nullptr,             // resolve
    nullptr,             // mayResolve
    RttValue::finalize,  // finalize
    nullptr,             // call
    nullptr,             // construct
    RttValue::trace,     // trace
};

const JSClass js::RttValue::class_ = {
    "RttValue",
    JSCLASS_FOREGROUND_FINALIZE |
        JSCLASS_HAS_RESERVED_SLOTS(RttValue::SlotCount),
    &RttValueClassOps};

RttValue* RttValue::create(JSContext* cx, TypeHandle handle) {
  Rooted<RttValue*> rtt(cx,
                        NewTenuredObjectWithGivenProto<RttValue>(cx, nullptr));
  if (!rtt) {
    return nullptr;
  }

  // Store the TypeContext in a slot and keep it alive until finalization by
  // manually addref'ing the RefPtr
  const SharedTypeContext& typeContext = handle.context();
  typeContext.get()->AddRef();
  rtt->initReservedSlot(RttValue::TypeContext,
                        PrivateValue((void*)typeContext.get()));
  rtt->initReservedSlot(RttValue::TypeDef, PrivateValue((void*)&handle.def()));
  rtt->initReservedSlot(RttValue::Parent, NullValue());
  rtt->initReservedSlot(RttValue::Children, PrivateValue(nullptr));

  MOZ_ASSERT(!rtt->isNewborn());

  if (!cx->zone()->addRttValueObject(cx, rtt)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return rtt;
}

RttValue* RttValue::rttCanon(JSContext* cx, TypeHandle handle) {
  return RttValue::create(cx, handle);
}

RttValue* RttValue::rttSub(JSContext* cx, Handle<RttValue*> parent,
                           Handle<RttValue*> subCanon) {
  if (!parent->ensureChildren(cx)) {
    return nullptr;
  }

  ObjectWeakMap& parentChildren = parent->children();
  if (JSObject* child = parentChildren.lookup(subCanon)) {
    return &child->as<RttValue>();
  }

  Rooted<RttValue*> rtt(cx, create(cx, parent->typeHandle()));
  if (!rtt) {
    return nullptr;
  }
  rtt->setReservedSlot(RttValue::Parent, ObjectValue(*parent.get()));
  if (!parentChildren.add(cx, subCanon, rtt)) {
    return nullptr;
  }
  return rtt;
}

bool RttValue::ensureChildren(JSContext* cx) {
  if (maybeChildren()) {
    return true;
  }
  Rooted<UniquePtr<ObjectWeakMap>> children(cx,
                                            cx->make_unique<ObjectWeakMap>(cx));
  if (!children) {
    return false;
  }
  setReservedSlot(Slot::Children, PrivateValue(children.release()));
  AddCellMemory(this, sizeof(ObjectWeakMap), MemoryUse::WasmRttValueChildren);
  return true;
}

/* static */
void RttValue::trace(JSTracer* trc, JSObject* obj) {
  auto* rttValue = &obj->as<RttValue>();
  if (rttValue->isNewborn()) {
    return;
  }

  if (ObjectWeakMap* children = rttValue->maybeChildren()) {
    children->trace(trc);
  }
}

/* static */
void RttValue::finalize(JS::GCContext* gcx, JSObject* obj) {
  auto* rttValue = &obj->as<RttValue>();

  // Nothing to free if we're not initialized yet
  if (rttValue->isNewborn()) {
    return;
  }

  // Free the ref-counted TypeContext we took a strong reference to upon
  // creation
  rttValue->typeContext()->Release();
  rttValue->setReservedSlot(Slot::TypeContext, PrivateValue(nullptr));

  // Free the lazy-allocated children map, if any
  if (ObjectWeakMap* children = rttValue->maybeChildren()) {
    gcx->delete_(obj, children, MemoryUse::WasmRttValueChildren);
  }
}

/******************************************************************************
 * Typed objects
 */

/*static*/
TypedObject* TypedObject::createStruct(JSContext* cx, Handle<RttValue*> rtt,
                                       gc::InitialHeap heap) {
  MOZ_ASSERT(rtt->kind() == wasm::TypeDefKind::Struct);

  uint32_t totalBytes = rtt->typeDef().structType().size_;
  uint32_t inlineBytes, outlineBytes;
  WasmStructObject::getDataByteSizes(totalBytes, &inlineBytes, &outlineBytes);

  uint8_t* outlineData = nullptr;
  if (outlineBytes > 0) {
    outlineData = (uint8_t*)js_malloc(outlineBytes);
    if (!outlineData) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
  }

  Rooted<WasmStructObject*> structObj(cx);
  AutoSetNewObjectMetadata metadata(cx);
  structObj = TypedObject::create<WasmStructObject>(
      cx, WasmStructObject::allocKindForRttValue(rtt), heap);
  if (!structObj) {
    ReportOutOfMemory(cx);
    if (outlineData) {
      js_free(outlineData);
    }
    return nullptr;
  }
  structObj->outlineData_ = outlineData;
  structObj->rttValue_.init(rtt);

  // Initialize the values to their defaults, including *outlineData_
  // inlineData_.
  structObj->initDefault();

  return structObj;
}

/*static*/
TypedObject* TypedObject::createArray(JSContext* cx, Handle<RttValue*> rtt,
                                      uint32_t numElements,
                                      gc::InitialHeap heap) {
  // Calculate the byte length of the outline storage, being careful to check
  // for overflow. We stick to uint32_t as an implicit implementation limit.
  CheckedUint32 numBytes = rtt->typeDef().arrayType().elementType_.size();
  numBytes *= numElements;
  numBytes += WasmArrayObject::offsetOfNumElements() +
              sizeof(WasmArrayObject::NumElements);
  if (!numBytes.isValid()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_ARRAY_IMP_LIMIT);
    return nullptr;
  }

  // Always create arrays outlined
  Rooted<WasmArrayObject*> typedObj(
      cx, WasmArrayObject::create(cx, rtt, numBytes.value(), heap));
  if (!typedObj) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  // Initialize the values to their defaults
  typedObj->setNumElements(numElements);
  MOZ_ASSERT(typedObj->numElements() == numElements);
  typedObj->initDefault();

  return typedObj;
}

template <typename V>
void TypedObject::visitReferences(V& visitor) {
  RttValue& rtt = rttValue();
  const auto& typeDef = rtt.typeDef();

  switch (typeDef.kind()) {
    case TypeDefKind::Struct: {
      MOZ_ASSERT(is<WasmStructObject>());
      WasmStructObject& structObj = as<WasmStructObject>();
      const auto& structType = typeDef.structType();
      for (const StructField& field : structType.fields_) {
        if (field.type.isRefRepr()) {
          // Ensure no out-of-range access possible
          MOZ_RELEASE_ASSERT(field.offset + field.type.size() <=
                             structType.size_);
          uint8_t* fieldAddr =
              structObj.fieldOffsetToAddress(field.type, field.offset);
          visitor.visitReference(fieldAddr, 0);
        }
      }
      break;
    }
    case TypeDefKind::Array: {
      MOZ_ASSERT(is<WasmArrayObject>());
      const auto& arrayType = typeDef.arrayType();
      if (arrayType.elementType_.isRefRepr()) {
        WasmArrayObject& arrayObj = as<WasmArrayObject>();
        uint8_t* base = arrayObj.outOfLineTypedMem();
        uint8_t* elemBase = base + WasmArrayObject::offsetOfNumElements() +
                            sizeof(WasmArrayObject::NumElements);
        uint32_t numElements = arrayObj.numElements();
        for (uint32_t i = 0; i < numElements; i++) {
          visitor.visitReference(elemBase, i * arrayType.elementType_.size());
        }
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
  }
}

///////////////////////////////////////////////////////////////////////////
// Tracing instances

namespace {

class MemoryTracingVisitor {
  JSTracer* trace_;

 public:
  explicit MemoryTracingVisitor(JSTracer* trace) : trace_(trace) {}

  void visitReference(uint8_t* base, size_t offset);
};

}  // namespace

void MemoryTracingVisitor::visitReference(uint8_t* base, size_t offset) {
  GCPtr<JSObject*>* objectPtr =
      reinterpret_cast<GCPtr<JSObject*>*>(base + offset);
  TraceNullableEdge(trace_, objectPtr, "reference-obj");
}

/******************************************************************************
 * WasmArrayObject
 */

/*static*/
WasmArrayObject* WasmArrayObject::create(JSContext* cx, Handle<RttValue*> rtt,
                                         size_t numBytes,
                                         gc::InitialHeap heap) {
  STATIC_ASSERT_NUMELEMENTS_IS_U32;
  MOZ_ASSERT(numBytes >= sizeof(NumElements));
  AutoSetNewObjectMetadata metadata(cx);

  auto* obj = TypedObject::create<WasmArrayObject>(cx, allocKind(), heap);
  if (!obj) {
    return nullptr;
  }

  obj->rttValue_.init(rtt);
  obj->data_ = (uint8_t*)js_malloc(numBytes);
  if (!obj->data_) {
    return nullptr;
  }

  return obj;
}

/* static */
gc::AllocKind WasmArrayObject::allocKind() {
  return gc::GetGCObjectKindForBytes(sizeof(WasmArrayObject));
}

/* static */
void WasmArrayObject::obj_trace(JSTracer* trc, JSObject* object) {
  WasmArrayObject& typedObj = object->as<WasmArrayObject>();

  TraceEdge(trc, &typedObj.rttValue_, "WasmArrayObject_rttvalue");

  if (!typedObj.data_) {
    return;
  }

  MemoryTracingVisitor visitor(trc);
  typedObj.visitReferences(visitor);
}

/* static */
void WasmArrayObject::obj_finalize(JS::GCContext* gcx, JSObject* object) {
  WasmArrayObject& typedObj = object->as<WasmArrayObject>();

  if (typedObj.data_) {
    js_free(typedObj.data_);
    typedObj.data_ = nullptr;
  }
}

bool RttValue::lookupProperty(JSContext* cx, Handle<TypedObject*> object,
                              jsid id, PropOffset* offset, FieldType* type) {
  const auto& typeDef = this->typeDef();

  switch (typeDef.kind()) {
    case wasm::TypeDefKind::Struct: {
      const auto& structType = typeDef.structType();
      uint32_t index;
      if (!IdIsIndex(id, &index)) {
        return false;
      }
      if (index >= structType.fields_.length()) {
        return false;
      }
      const StructField& field = structType.fields_[index];
      offset->set(field.offset);
      *type = field.type;
      return true;
    }
    case wasm::TypeDefKind::Array: {
      const auto& arrayType = typeDef.arrayType();

      // Special case for property 'length' that loads the length field at the
      // beginning of the data buffer
      if (id.isString() &&
          id.toString() == cx->runtime()->commonNames->length) {
        STATIC_ASSERT_NUMELEMENTS_IS_U32;
        *type = FieldType::I32;
        offset->set(WasmArrayObject::offsetOfNumElements());
        return true;
      }

      // Normal case of indexed properties for loading array elements
      uint32_t index;
      if (!IdIsIndex(id, &index)) {
        return false;
      }
      WasmArrayObject::NumElements numElements =
          object->as<WasmArrayObject>().numElements();
      if (index >= numElements) {
        return false;
      }
      offset->set(WasmArrayObject::offsetOfNumElements() +
                  sizeof(WasmArrayObject::NumElements) +
                  index * arrayType.elementType_.size());
      *type = arrayType.elementType_;
      return true;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
      return false;
  }
}

/* static */
bool TypedObject::obj_lookupProperty(JSContext* cx, HandleObject obj,
                                     HandleId id, MutableHandleObject objp,
                                     PropertyResult* propp) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());
  if (typedObj->rttValue().hasProperty(cx, typedObj, id)) {
    propp->setTypedObjectProperty();
    objp.set(obj);
    return true;
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    objp.set(nullptr);
    propp->setNotFound();
    return true;
  }

  return LookupProperty(cx, proto, id, objp, propp);
}

bool TypedObject::obj_defineProperty(JSContext* cx, HandleObject obj,
                                     HandleId id,
                                     Handle<PropertyDescriptor> desc,
                                     ObjectOpResult& result) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_OBJECT_NOT_EXTENSIBLE, "TypedObject");
  return false;
}

bool TypedObject::obj_hasProperty(JSContext* cx, HandleObject obj, HandleId id,
                                  bool* foundp) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());
  if (typedObj->rttValue().hasProperty(cx, typedObj, id)) {
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

bool TypedObject::obj_getProperty(JSContext* cx, HandleObject obj,
                                  HandleValue receiver, HandleId id,
                                  MutableHandleValue vp) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());

  RttValue::PropOffset offset;
  FieldType type;
  if (typedObj->rttValue().lookupProperty(cx, typedObj, id, &offset, &type)) {
    return typedObj->loadValue(cx, offset, type, vp);
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    vp.setUndefined();
    return true;
  }

  return GetProperty(cx, proto, receiver, id, vp);
}

bool TypedObject::obj_setProperty(JSContext* cx, HandleObject obj, HandleId id,
                                  HandleValue v, HandleValue receiver,
                                  ObjectOpResult& result) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());

  if (typedObj->rttValue().hasProperty(cx, typedObj, id)) {
    if (!receiver.isObject() || obj != &receiver.toObject()) {
      return SetPropertyByDefining(cx, id, v, receiver, result);
    }

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPEDOBJECT_SETTING_IMMUTABLE);
    return false;
  }

  return SetPropertyOnProto(cx, obj, id, v, receiver, result);
}

bool TypedObject::obj_getOwnPropertyDescriptor(
    JSContext* cx, HandleObject obj, HandleId id,
    MutableHandle<mozilla::Maybe<PropertyDescriptor>> desc) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());

  RttValue::PropOffset offset;
  FieldType type;
  if (typedObj->rttValue().lookupProperty(cx, typedObj, id, &offset, &type)) {
    RootedValue value(cx);
    if (!typedObj->loadValue(cx, offset, type, &value)) {
      return false;
    }
    desc.set(mozilla::Some(PropertyDescriptor::Data(
        value,
        {JS::PropertyAttribute::Enumerable, JS::PropertyAttribute::Writable})));
    return true;
  }

  desc.reset();
  return true;
}

bool TypedObject::obj_deleteProperty(JSContext* cx, HandleObject obj,
                                     HandleId id, ObjectOpResult& result) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());
  if (typedObj->rttValue().hasProperty(cx, typedObj, id)) {
    return Throw(cx, id, JSMSG_CANT_DELETE);
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    return result.succeed();
  }

  return DeleteProperty(cx, proto, id, result);
}

bool TypedObject::obj_newEnumerate(JSContext* cx, HandleObject obj,
                                   MutableHandleIdVector properties,
                                   bool enumerableOnly) {
  MOZ_ASSERT(obj->is<TypedObject>());
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());

  const auto& rtt = typedObj->rttValue();
  const auto& typeDef = rtt.typeDef();

  size_t indexCount = 0;
  size_t otherCount = 0;
  switch (typeDef.kind()) {
    case wasm::TypeDefKind::Struct: {
      indexCount = typeDef.structType().fields_.length();
      break;
    }
    case wasm::TypeDefKind::Array: {
      indexCount = typedObj->as<WasmArrayObject>().numElements();
      otherCount = 1;
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
  }

  if (!properties.reserve(indexCount + otherCount)) {
    return false;
  }
  RootedId id(cx);
  for (size_t index = 0; index < indexCount; index++) {
    id = PropertyKey::Int(index);
    properties.infallibleAppend(id);
  }

  if (typeDef.kind() == wasm::TypeDefKind::Array) {
    properties.infallibleAppend(NameToId(cx->runtime()->commonNames->length));
  }

  return true;
}

bool TypedObject::loadValue(JSContext* cx, const RttValue::PropOffset& offset,
                            FieldType type, MutableHandleValue vp) {
  // Temporary hack, (ref T) is not exposable to JS yet but some tests would
  // like to access it so we erase (ref T) with eqref when loading. This is
  // safe as (ref T) <: eqref and we're not in the writing case where we
  // would need to perform a type check.
  if (type.isTypeIndex()) {
    type = RefType::fromTypeCode(TypeCode::EqRef, true);
  }
  if (!type.isExposable()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_VAL_TYPE);
    return false;
  }
  if (is<WasmStructObject>()) {
    // `offset` is the field offset, without regard to the in/out-line split.
    // That is handled by the call to `fieldOffsetToAddress`.
    WasmStructObject& structObj = as<WasmStructObject>();
    // Ensure no out-of-range access possible
    const RttValue& rtt = structObj.rttValue();
    MOZ_RELEASE_ASSERT(rtt.kind() == TypeDefKind::Struct);
    MOZ_RELEASE_ASSERT(offset.get() + type.size() <=
                       rtt.typeDef().structType().size_);
    return ToJSValue(cx, structObj.fieldOffsetToAddress(type, offset.get()),
                     type, vp);
  } else {
    MOZ_ASSERT(is<WasmArrayObject>());
    WasmArrayObject& arrayObj = as<WasmArrayObject>();
    return ToJSValue(cx, arrayObj.outOfLineTypedMem() + offset.get(), type, vp);
  }
}

void TypedObject::initDefault() {
  RttValue& rtt = rttValue();
  switch (rtt.kind()) {
    case TypeDefKind::Struct: {
      MOZ_ASSERT(is<WasmStructObject>());
      WasmStructObject& structObj = as<WasmStructObject>();
      uint32_t totalBytes = rtt.typeDef().structType().size_;
      uint32_t inlineBytes, outlineBytes;
      WasmStructObject::getDataByteSizes(totalBytes, &inlineBytes,
                                         &outlineBytes);
      MOZ_ASSERT((outlineBytes == 0) == (structObj.outlineData_ == nullptr));
      if (outlineBytes > 0) {
        memset(structObj.outlineData_, 0, outlineBytes);
      }
      memset(&structObj.inlineData_[0], 0, inlineBytes);
      break;
    }
    case TypeDefKind::Array: {
      MOZ_ASSERT(is<WasmArrayObject>());
      WasmArrayObject& arrayObj = as<WasmArrayObject>();
      uint32_t numElements = arrayObj.numElements();
      memset(arrayObj.outOfLineTypedMem() + sizeof(uint32_t), 0,
             rtt.typeDef().arrayType().elementType_.size() * numElements);
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
  }
}

/******************************************************************************
 * WasmStructObject
 */

/* static */
void WasmStructObject::obj_trace(JSTracer* trc, JSObject* object) {
  WasmStructObject& typedObj = object->as<WasmStructObject>();

  TraceEdge(trc, &typedObj.rttValue_, "WasmStructObject_rttvalue");

  MemoryTracingVisitor visitor(trc);
  typedObj.visitReferences(visitor);
}

/* static */
size_t WasmStructObject::obj_moved(JSObject* dst, JSObject* src) { return 0; }

/* static */
void WasmStructObject::obj_finalize(JS::GCContext* gcx, JSObject* object) {
  WasmStructObject& structObj = object->as<WasmStructObject>();

  if (structObj.outlineData_) {
    js_free(structObj.outlineData_);
    structObj.outlineData_ = nullptr;
  }
}

/******************************************************************************
 * Typed object classes
 */

const ObjectOps TypedObject::objectOps_ = {
    TypedObject::obj_lookupProperty,            // lookupProperty
    TypedObject::obj_defineProperty,            // defineProperty
    TypedObject::obj_hasProperty,               // hasProperty
    TypedObject::obj_getProperty,               // getProperty
    TypedObject::obj_setProperty,               // setProperty
    TypedObject::obj_getOwnPropertyDescriptor,  // getOwnPropertyDescriptor
    TypedObject::obj_deleteProperty,            // deleteProperty
    nullptr,                                    // getElements
    nullptr,                                    // funToString
};

#define DEFINE_TYPEDOBJ_CLASS(Name, Trace, Finalize, Moved, Flags)    \
  static const JSClassOps Name##ClassOps = {                          \
      nullptr, /* addProperty */                                      \
      nullptr, /* delProperty */                                      \
      nullptr, /* enumerate   */                                      \
      TypedObject::obj_newEnumerate,                                  \
      nullptr,  /* resolve     */                                     \
      nullptr,  /* mayResolve  */                                     \
      Finalize, /* finalize    */                                     \
      nullptr,  /* call        */                                     \
      nullptr,  /* construct   */                                     \
      Trace,                                                          \
  };                                                                  \
  static const ClassExtension Name##ClassExt = {                      \
      Moved /* objectMovedOp */                                       \
  };                                                                  \
  const JSClass Name::class_ = {                                      \
      #Name,                                                          \
      JSClass::NON_NATIVE | JSCLASS_DELAY_METADATA_BUILDER | (Flags), \
      &Name##ClassOps,                                                \
      JS_NULL_CLASS_SPEC,                                             \
      &Name##ClassExt,                                                \
      &TypedObject::objectOps_}

DEFINE_TYPEDOBJ_CLASS(WasmArrayObject, WasmArrayObject::obj_trace,
                      WasmArrayObject::obj_finalize, nullptr,
                      JSCLASS_FOREGROUND_FINALIZE);
DEFINE_TYPEDOBJ_CLASS(WasmStructObject, WasmStructObject::obj_trace, nullptr,
                      WasmStructObject::obj_moved, 0);

/* static */
template <typename T>
T* TypedObject::create(JSContext* cx, js::gc::AllocKind allocKind,
                       js::gc::InitialHeap heap) {
  const JSClass* clasp = &T::class_;
  MOZ_ASSERT(IsTypedObjectClass(clasp));
  MOZ_ASSERT(!clasp->isNativeObject());

  if (CanChangeToBackgroundAllocKind(allocKind, clasp)) {
    allocKind = ForegroundToBackgroundAllocKind(allocKind);
  }

  Rooted<Shape*> shape(
      cx, SharedShape::getInitialShape(cx, clasp, cx->realm(), TaggedProto(),
                                       /* nfixed = */ 0, ObjectFlags()));
  if (!shape) {
    return nullptr;
  }

  NewObjectKind newKind =
      (heap == gc::TenuredHeap) ? TenuredObject : GenericObject;
  heap = GetInitialHeap(newKind, clasp);

  debugCheckNewObject(shape, allocKind, heap);

  T* tobj = cx->newCell<T>(allocKind, /* nDynamicSlots = */ 0, heap, clasp);
  if (!tobj) {
    return nullptr;
  }

  tobj->initShape(shape);

  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  cx->realm()->setObjectPendingMetadata(cx, tobj);

  js::gc::gcprobes::CreateObject(tobj);
  probes::CreateObject(cx, tobj);

  return tobj;
}

bool TypedObject::isRuntimeSubtype(Handle<RttValue*> rtt) const {
  RttValue* current = &rttValue();
  while (current != nullptr) {
    if (current == rtt.get()) {
      return true;
    }
    current = current->parent();
  }
  return false;
}
