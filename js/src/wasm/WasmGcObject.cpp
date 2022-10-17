/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "wasm/WasmGcObject.h"

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

//=========================================================================
// RttValue

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

const JSClass js::RttValue::class_ = {
    "RttValue",
    JSCLASS_FOREGROUND_FINALIZE |
        JSCLASS_HAS_RESERVED_SLOTS(RttValue::SlotCount),
    &RttValueClassOps};

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

bool RttValue::lookupProperty(JSContext* cx, Handle<WasmGcObject*> object,
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
        STATIC_ASSERT_WASMARRAYELEMENTS_NUMELEMENTS_IS_U32;
        *type = FieldType::I32;
        offset->set(UINT32_MAX);
        return true;
      }

      // Normal case of indexed properties for loading array elements
      uint32_t index;
      if (!IdIsIndex(id, &index)) {
        return false;
      }
      uint32_t numElements = object->as<WasmArrayObject>().numElements_;
      if (index >= numElements) {
        return false;
      }
      uint64_t scaledIndex =
          uint64_t(index) * uint64_t(arrayType.elementType_.size());
      if (scaledIndex >= uint64_t(UINT32_MAX)) {
        // It's unrepresentable as an RttValue::PropOffset.  Give up.
        return false;
      }
      offset->set(uint32_t(scaledIndex));
      *type = arrayType.elementType_;
      return true;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
      return false;
  }
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

//=========================================================================
// WasmGcObject

const ObjectOps WasmGcObject::objectOps_ = {
    WasmGcObject::obj_lookupProperty,            // lookupProperty
    WasmGcObject::obj_defineProperty,            // defineProperty
    WasmGcObject::obj_hasProperty,               // hasProperty
    WasmGcObject::obj_getProperty,               // getProperty
    WasmGcObject::obj_setProperty,               // setProperty
    WasmGcObject::obj_getOwnPropertyDescriptor,  // getOwnPropertyDescriptor
    WasmGcObject::obj_deleteProperty,            // deleteProperty
    nullptr,                                     // getElements
    nullptr,                                     // funToString
};

/* static */
bool WasmGcObject::obj_lookupProperty(JSContext* cx, HandleObject obj,
                                      HandleId id, MutableHandleObject objp,
                                      PropertyResult* propp) {
  Rooted<WasmGcObject*> typedObj(cx, &obj->as<WasmGcObject>());
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

bool WasmGcObject::obj_defineProperty(JSContext* cx, HandleObject obj,
                                      HandleId id,
                                      Handle<PropertyDescriptor> desc,
                                      ObjectOpResult& result) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_OBJECT_NOT_EXTENSIBLE, "WasmGcObject");
  return false;
}

bool WasmGcObject::obj_hasProperty(JSContext* cx, HandleObject obj, HandleId id,
                                   bool* foundp) {
  Rooted<WasmGcObject*> typedObj(cx, &obj->as<WasmGcObject>());
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

bool WasmGcObject::obj_getProperty(JSContext* cx, HandleObject obj,
                                   HandleValue receiver, HandleId id,
                                   MutableHandleValue vp) {
  Rooted<WasmGcObject*> typedObj(cx, &obj->as<WasmGcObject>());

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

bool WasmGcObject::obj_setProperty(JSContext* cx, HandleObject obj, HandleId id,
                                   HandleValue v, HandleValue receiver,
                                   ObjectOpResult& result) {
  Rooted<WasmGcObject*> typedObj(cx, &obj->as<WasmGcObject>());

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

bool WasmGcObject::obj_getOwnPropertyDescriptor(
    JSContext* cx, HandleObject obj, HandleId id,
    MutableHandle<mozilla::Maybe<PropertyDescriptor>> desc) {
  Rooted<WasmGcObject*> typedObj(cx, &obj->as<WasmGcObject>());

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

bool WasmGcObject::obj_deleteProperty(JSContext* cx, HandleObject obj,
                                      HandleId id, ObjectOpResult& result) {
  Rooted<WasmGcObject*> typedObj(cx, &obj->as<WasmGcObject>());
  if (typedObj->rttValue().hasProperty(cx, typedObj, id)) {
    return Throw(cx, id, JSMSG_CANT_DELETE);
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    return result.succeed();
  }

  return DeleteProperty(cx, proto, id, result);
}

/* static */
template <typename T>
T* WasmGcObject::create(JSContext* cx, js::gc::AllocKind allocKind,
                        js::gc::InitialHeap heap) {
  const JSClass* clasp = &T::class_;
  MOZ_ASSERT(IsWasmGcObjectClass(clasp));
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

bool WasmGcObject::loadValue(JSContext* cx, const RttValue::PropOffset& offset,
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
    if (offset.get() == UINT32_MAX) {
      // This denotes "length"
      uint32_t numElements = arrayObj.numElements_;
      // We can't use `ToJSValue(.., ValType::I32, ..)` here since it will
      // treat the integer as signed, which it isn't.  `vp.set(..)` will
      // coerce correctly to a JS::Value, though.
      vp.set(NumberValue(numElements));
      return true;
    }
    return ToJSValue(cx, arrayObj.data_ + offset.get(), type, vp);
  }
}

bool WasmGcObject::isRuntimeSubtype(Handle<RttValue*> rtt) const {
  RttValue* current = &rttValue();
  while (current != nullptr) {
    if (current == rtt.get()) {
      return true;
    }
    current = current->parent();
  }
  return false;
}

bool WasmGcObject::obj_newEnumerate(JSContext* cx, HandleObject obj,
                                    MutableHandleIdVector properties,
                                    bool enumerableOnly) {
  MOZ_ASSERT(obj->is<WasmGcObject>());
  Rooted<WasmGcObject*> typedObj(cx, &obj->as<WasmGcObject>());

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
      indexCount = typedObj->as<WasmArrayObject>().numElements_;
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

static void WriteValTo(const Val& val, FieldType ty, void* dest) {
  switch (ty.kind()) {
    case FieldType::I8:
      *((uint8_t*)dest) = val.i32();
      break;
    case FieldType::I16:
      *((uint16_t*)dest) = val.i32();
      break;
    case FieldType::I32:
      *((uint32_t*)dest) = val.i32();
      break;
    case FieldType::I64:
      *((uint64_t*)dest) = val.i64();
      break;
    case FieldType::F32:
      *((float*)dest) = val.f32();
      break;
    case FieldType::F64:
      *((double*)dest) = val.f64();
      break;
    case FieldType::V128:
      *((V128*)dest) = val.v128();
      break;
    case FieldType::Ref:
      *((GCPtr<AnyRef>*)dest) = val.ref();
      break;
  }
}

#define DEFINE_TYPEDOBJ_CLASS(Name, Trace, Finalize, Moved, Flags)    \
  static const JSClassOps Name##ClassOps = {                          \
      nullptr, /* addProperty */                                      \
      nullptr, /* delProperty */                                      \
      nullptr, /* enumerate   */                                      \
      WasmGcObject::obj_newEnumerate,                                 \
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
      &WasmGcObject::objectOps_}

//=========================================================================
// MemoryTracingVisitor (private to this file)

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

//=========================================================================
// WasmArrayObject

/* static */
gc::AllocKind WasmArrayObject::allocKind() {
  return gc::GetGCObjectKindForBytes(sizeof(WasmArrayObject));
}

/*static*/
WasmArrayObject* WasmArrayObject::createArray(JSContext* cx,
                                              Handle<RttValue*> rtt,
                                              uint32_t numElements,
                                              gc::InitialHeap heap) {
  STATIC_ASSERT_WASMARRAYELEMENTS_NUMELEMENTS_IS_U32;
  MOZ_ASSERT(rtt->kind() == wasm::TypeDefKind::Array);

  // Calculate the byte length of the outline storage, being careful to check
  // for overflow. We stick to uint32_t as an implicit implementation limit.
  CheckedUint32 outlineBytes = rtt->typeDef().arrayType().elementType_.size();
  outlineBytes *= numElements;
  if (!outlineBytes.isValid()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_ARRAY_IMP_LIMIT);
    return nullptr;
  }

  uint8_t* outlineData = (uint8_t*)js_malloc(outlineBytes.value());
  if (!outlineData) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  Rooted<WasmArrayObject*> arrayObj(cx);
  AutoSetNewObjectMetadata metadata(cx);
  arrayObj = WasmGcObject::create<WasmArrayObject>(
      cx, WasmArrayObject::allocKind(), heap);
  if (!arrayObj) {
    ReportOutOfMemory(cx);
    js_free(outlineData);
    return nullptr;
  }

  arrayObj->rttValue_.init(rtt);
  arrayObj->numElements_ = numElements;
  arrayObj->data_ = outlineData;
  memset(arrayObj->data_, 0, outlineBytes.value());

  return arrayObj;
}

/* static */
void WasmArrayObject::obj_trace(JSTracer* trc, JSObject* object) {
  WasmArrayObject& arrayObj = object->as<WasmArrayObject>();
  MOZ_ASSERT(arrayObj.data_);

  TraceEdge(trc, &arrayObj.rttValue_, "WasmArrayObject_rttvalue");

  MemoryTracingVisitor visitor(trc);
  RttValue& rtt = arrayObj.rttValue();
  const auto& typeDef = rtt.typeDef();
  const auto& arrayType = typeDef.arrayType();
  if (!arrayType.elementType_.isRefRepr()) {
    return;
  }

  uint8_t* base = arrayObj.data_;
  uint32_t numElements = arrayObj.numElements_;
  for (uint32_t i = 0; i < numElements; i++) {
    visitor.visitReference(base, i * arrayType.elementType_.size());
  }
}

/* static */
void WasmArrayObject::obj_finalize(JS::GCContext* gcx, JSObject* object) {
  WasmArrayObject& arrayObj = object->as<WasmArrayObject>();
  MOZ_ASSERT(arrayObj.data_);

  js_free(arrayObj.data_);
  arrayObj.data_ = nullptr;
}

void WasmArrayObject::storeVal(const Val& val, uint32_t itemIndex) {
  const ArrayType& arrayType = rttValue_->typeDef().arrayType();
  size_t elementSize = arrayType.elementType_.size();
  MOZ_ASSERT(itemIndex < numElements_);
  uint8_t* data = data_ + elementSize * itemIndex;
  WriteValTo(val, arrayType.elementType_, data);
}

void WasmArrayObject::fillVal(const Val& val, uint32_t itemIndex,
                              uint32_t len) {
  const ArrayType& arrayType = rttValue_->typeDef().arrayType();
  size_t elementSize = arrayType.elementType_.size();
  uint8_t* data = data_;
  MOZ_ASSERT(itemIndex <= numElements_ && len <= numElements_ - itemIndex);
  for (uint32_t i = 0; i < len; i++) {
    WriteValTo(val, arrayType.elementType_, data);
    data += elementSize;
  }
}

DEFINE_TYPEDOBJ_CLASS(WasmArrayObject, WasmArrayObject::obj_trace,
                      WasmArrayObject::obj_finalize, nullptr,
                      JSCLASS_FOREGROUND_FINALIZE);

//=========================================================================
// WasmStructObject

/* static */
js::gc::AllocKind js::WasmStructObject::allocKindForRttValue(RttValue* rtt) {
  MOZ_ASSERT(rtt->kind() == wasm::TypeDefKind::Struct);
  size_t nbytes = rtt->typeDef().structType().size_;

  // `nbytes` is the total required size for all struct fields, including
  // padding.  What we need is the size of resulting WasmStructObject,
  // ignoring any space used for out-of-line data.  First, restrict `nbytes`
  // to cover just the inline data.
  if (nbytes > WasmStructObject_MaxInlineBytes) {
    nbytes = WasmStructObject_MaxInlineBytes;
  }

  // Now convert it to size of the WasmStructObject as a whole.
  nbytes = sizeOfIncludingInlineData(nbytes);

  return gc::GetGCObjectKindForBytes(nbytes);
}

/*static*/
WasmStructObject* WasmStructObject::createStruct(JSContext* cx,
                                                 Handle<RttValue*> rtt,
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
  structObj = WasmGcObject::create<WasmStructObject>(
      cx, WasmStructObject::allocKindForRttValue(rtt), heap);
  if (!structObj) {
    ReportOutOfMemory(cx);
    if (outlineData) {
      js_free(outlineData);
    }
    return nullptr;
  }

  structObj->rttValue_.init(rtt);
  structObj->outlineData_ = outlineData;
  if (outlineBytes > 0) {
    memset(structObj->outlineData_, 0, outlineBytes);
  }
  memset(&(structObj->inlineData_[0]), 0, inlineBytes);

  return structObj;
}

/* static */
void WasmStructObject::obj_trace(JSTracer* trc, JSObject* object) {
  WasmStructObject& structObj = object->as<WasmStructObject>();

  TraceEdge(trc, &structObj.rttValue_, "WasmStructObject_rttvalue");

  MemoryTracingVisitor visitor(trc);
  RttValue& rtt = structObj.rttValue();
  const auto& typeDef = rtt.typeDef();
  const auto& structType = typeDef.structType();
  for (const StructField& field : structType.fields_) {
    if (!field.type.isRefRepr()) {
      continue;
    }
    // Ensure no out-of-range access possible
    MOZ_RELEASE_ASSERT(field.offset + field.type.size() <= structType.size_);
    uint8_t* fieldAddr =
        structObj.fieldOffsetToAddress(field.type, field.offset);
    visitor.visitReference(fieldAddr, 0);
  }
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

void WasmStructObject::storeVal(const Val& val, uint32_t fieldIndex) {
  const StructType& structType = rttValue_->typeDef().structType();
  FieldType fieldType = structType.fields_[fieldIndex].type;
  uint32_t fieldOffset = structType.fields_[fieldIndex].offset;

  MOZ_ASSERT(fieldIndex < structType.fields_.length());
  bool areaIsOutline;
  uint32_t areaOffset;
  fieldOffsetToAreaAndOffset(fieldType, fieldOffset, &areaIsOutline,
                             &areaOffset);

  uint8_t* data;
  if (areaIsOutline) {
    data = outlineData_ + areaOffset;
  } else {
    data = inlineData_ + areaOffset;
  }

  WriteValTo(val, fieldType, data);
}

DEFINE_TYPEDOBJ_CLASS(WasmStructObject, WasmStructObject::obj_trace, nullptr,
                      WasmStructObject::obj_moved, 0);
