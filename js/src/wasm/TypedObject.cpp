/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "wasm/TypedObject-inl.h"

#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"

#include <algorithm>

#include "gc/Marking.h"
#include "js/CharacterEncoding.h"
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

#include "wasm/WasmJS.h"     // WasmNamespaceObject
#include "wasm/WasmTypes.h"  // WasmValueBox
#include "gc/Marking-inl.h"
#include "gc/Nursery-inl.h"
#include "gc/StoreBuffer-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Shape-inl.h"

using mozilla::AssertedCast;
using mozilla::CheckedInt32;
using mozilla::IsPowerOfTwo;
using mozilla::PodCopy;
using mozilla::PointerRangeSize;

using namespace js;

template <class T>
static inline T* ToObjectIf(HandleValue value) {
  if (!value.isObject()) {
    return nullptr;
  }

  if (!value.toObject().is<T>()) {
    return nullptr;
  }

  return &value.toObject().as<T>();
}

static inline CheckedInt32 RoundUpToAlignment(CheckedInt32 address,
                                              uint32_t align) {
  MOZ_ASSERT(IsPowerOfTwo(align));

  // Note: Be careful to order operators such that we first make the
  // value smaller and then larger, so that we don't get false
  // overflow errors due to (e.g.) adding `align` and then
  // subtracting `1` afterwards when merely adding `align-1` would
  // not have overflowed. Note that due to the nature of two's
  // complement representation, if `address` is already aligned,
  // then adding `align-1` cannot itself cause an overflow.

  return ((address + (align - 1)) / align) * align;
}

/***************************************************************************
 * Typed Prototypes
 *
 * Every type descriptor has an associated prototype. Instances of
 * that type descriptor use this as their prototype. Per the spec,
 * typed object prototypes cannot be mutated.
 */

const JSClass js::TypedProto::class_ = {"TypedProto"};

/***************************************************************************
 * Scalar type objects
 *
 * Scalar type objects like `uint8`, `uint16`, are all instances of
 * the ScalarTypeDescr class. Like all type objects, they have a reserved
 * slot pointing to a TypeRepresentation object, which is used to
 * distinguish which scalar type object this actually is.
 */

static const JSClassOps ScalarTypeDescrClassOps = {
    nullptr,                // addProperty
    nullptr,                // delProperty
    nullptr,                // enumerate
    nullptr,                // newEnumerate
    nullptr,                // resolve
    nullptr,                // mayResolve
    TypeDescr::finalize,    // finalize
    ScalarTypeDescr::call,  // call
    nullptr,                // hasInstance
    nullptr,                // construct
    nullptr,                // trace
};

const JSClass js::ScalarTypeDescr::class_ = {
    "Scalar",
    JSCLASS_HAS_RESERVED_SLOTS(TypeDescr::SlotCount) |
        JSCLASS_BACKGROUND_FINALIZE,
    &ScalarTypeDescrClassOps};

uint32_t ScalarTypeDescr::size(Type t) {
  return AssertedCast<uint32_t>(Scalar::byteSize(t));
}

uint32_t ScalarTypeDescr::alignment(Type t) {
  return AssertedCast<uint32_t>(Scalar::byteSize(t));
}

/*static*/ const char* ScalarTypeDescr::typeName(Type type) {
  switch (type) {
#define NUMERIC_TYPE_TO_STRING(constant_, type_, name_, slot_) \
  case constant_:                                              \
    return #name_;
    JS_FOR_EACH_SCALAR_TYPE_REPR(NUMERIC_TYPE_TO_STRING)
#undef NUMERIC_TYPE_TO_STRING
    default:
      break;
  }
  MOZ_CRASH("Invalid type");
}

template <typename T>
BigInt* CreateBigInt(JSContext* cx, T i);
template <>
BigInt* CreateBigInt<int64_t>(JSContext* cx, int64_t i) {
  return BigInt::createFromInt64(cx, i);
}
template <>
BigInt* CreateBigInt<uint64_t>(JSContext* cx, uint64_t u) {
  return BigInt::createFromUint64(cx, u);
}

template <typename T>
T ConvertBigInt(BigInt* bi);
template <>
int64_t ConvertBigInt<int64_t>(BigInt* bi) {
  return BigInt::toInt64(bi);
}
template <>
uint64_t ConvertBigInt<uint64_t>(BigInt* bi) {
  return BigInt::toUint64(bi);
}

/*
 * Helper method for converting a double into other scalar
 * types in the same way that JavaScript would. In particular,
 * simple C casting from double to int32_t gets things wrong
 * for values like 0xF0000000.
 */
template <typename T>
static T ConvertScalar(double d) {
  if (TypeIsFloatingPoint<T>()) {
    return T(d);
  }
  if (TypeIsUnsigned<T>()) {
    uint32_t n = JS::ToUint32(d);
    return T(n);
  }
  int32_t n = JS::ToInt32(d);
  return T(n);
}

bool ScalarTypeDescr::call(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, args.callee().getClass()->name, 1)) {
    return false;
  }

  Rooted<ScalarTypeDescr*> descr(cx, &args.callee().as<ScalarTypeDescr>());
  ScalarTypeDescr::Type type = descr->type();

  switch (type) {
#define NUMBER_CALL(constant_, type_, name_, slot_) \
  case constant_: {                                 \
    double number;                                  \
    if (!ToNumber(cx, args[0], &number)) {          \
      return false;                                 \
    }                                               \
    if (type == Scalar::Uint8Clamped) {             \
      number = ClampDoubleToUint8(number);          \
    }                                               \
    type_ converted = ConvertScalar<type_>(number); \
    args.rval().setNumber((double)converted);       \
    return true;                                    \
  }
    JS_FOR_EACH_SCALAR_NUMBER_TYPE_REPR(NUMBER_CALL)
#undef NUMBER_CALL
#define BIGINT_CALL(constant_, type_, name_, slot_)   \
  case constant_: {                                   \
    BigInt* bi = ToBigInt(cx, args[0]);               \
    if (!bi) {                                        \
      return false;                                   \
    }                                                 \
    type_ converted = ConvertBigInt<type_>(bi);       \
    BigInt* ret = CreateBigInt<type_>(cx, converted); \
    if (!ret) {                                       \
      return false;                                   \
    }                                                 \
    args.rval().setBigInt(ret);                       \
    return true;                                      \
  }
    JS_FOR_EACH_SCALAR_BIGINT_TYPE_REPR(BIGINT_CALL)
#undef BIGINT_CALL
    default:
      MOZ_CRASH();
  }
  return true;
}

/* static */
TypeDescr* GlobalObject::getOrCreateScalarTypeDescr(
    JSContext* cx, Handle<GlobalObject*> global, Scalar::Type scalarType) {
  int32_t slot = 0;
  switch (scalarType) {
    case Scalar::Int32:
      slot = WasmNamespaceObject::Int32Desc;
      break;
    case Scalar::Int64:
      slot = WasmNamespaceObject::Int64Desc;
      break;
    case Scalar::Float32:
      slot = WasmNamespaceObject::Float32Desc;
      break;
    case Scalar::Float64:
      slot = WasmNamespaceObject::Float64Desc;
      break;
    default:
      MOZ_CRASH("NYI");
  }

  Rooted<WasmNamespaceObject*> namespaceObject(
      cx, &GlobalObject::getOrCreateWebAssemblyNamespace(cx, global)
               ->as<WasmNamespaceObject>());
  if (!namespaceObject) {
    return nullptr;
  }
  return &namespaceObject->getReservedSlot(slot).toObject().as<TypeDescr>();
}

/* static */
TypeDescr* GlobalObject::getOrCreateReferenceTypeDescr(
    JSContext* cx, Handle<GlobalObject*> global, ReferenceType type) {
  int32_t slot = 0;
  switch (type) {
    case ReferenceType::TYPE_OBJECT:
      slot = WasmNamespaceObject::ObjectDesc;
      break;
    case ReferenceType::TYPE_WASM_ANYREF:
      slot = WasmNamespaceObject::WasmAnyRefDesc;
      break;
    default:
      MOZ_CRASH("NYI");
  }

  Rooted<WasmNamespaceObject*> namespaceObject(
      cx, &GlobalObject::getOrCreateWebAssemblyNamespace(cx, global)
               ->as<WasmNamespaceObject>());
  if (!namespaceObject) {
    return nullptr;
  }
  return &namespaceObject->getReservedSlot(slot).toObject().as<TypeDescr>();
}

/***************************************************************************
 * Reference type objects
 *
 * Reference type objects like `Any` or `Object` basically work the
 * same way that the scalar type objects do. There is one class with
 * many instances, and each instance has a reserved slot with a
 * TypeRepresentation object, which is used to distinguish which
 * reference type object this actually is.
 */

static const JSClassOps ReferenceTypeDescrClassOps = {
    nullptr,                   // addProperty
    nullptr,                   // delProperty
    nullptr,                   // enumerate
    nullptr,                   // newEnumerate
    nullptr,                   // resolve
    nullptr,                   // mayResolve
    TypeDescr::finalize,       // finalize
    ReferenceTypeDescr::call,  // call
    nullptr,                   // hasInstance
    nullptr,                   // construct
    nullptr,                   // trace
};

const JSClass js::ReferenceTypeDescr::class_ = {
    "Reference",
    JSCLASS_HAS_RESERVED_SLOTS(TypeDescr::SlotCount) |
        JSCLASS_BACKGROUND_FINALIZE,
    &ReferenceTypeDescrClassOps};

static const uint32_t ReferenceSizes[] = {
#define REFERENCE_SIZE(_kind, _type, _name, slot_) sizeof(_type),
    JS_FOR_EACH_REFERENCE_TYPE_REPR(REFERENCE_SIZE) 0
#undef REFERENCE_SIZE
};

uint32_t ReferenceTypeDescr::size(Type t) {
  return ReferenceSizes[uint32_t(t)];
}

uint32_t ReferenceTypeDescr::alignment(Type t) {
  return ReferenceSizes[uint32_t(t)];
}

/*static*/ const char* ReferenceTypeDescr::typeName(Type type) {
  switch (type) {
#define NUMERIC_TYPE_TO_STRING(constant_, type_, name_, slot_) \
  case constant_:                                              \
    return #name_;
    JS_FOR_EACH_REFERENCE_TYPE_REPR(NUMERIC_TYPE_TO_STRING)
#undef NUMERIC_TYPE_TO_STRING
  }
  MOZ_CRASH("Invalid type");
}

bool js::ReferenceTypeDescr::call(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  MOZ_ASSERT(args.callee().is<ReferenceTypeDescr>());
  Rooted<ReferenceTypeDescr*> descr(cx,
                                    &args.callee().as<ReferenceTypeDescr>());

  if (!args.requireAtLeast(cx, descr->typeName(), 1)) {
    return false;
  }

  switch (descr->type()) {
    case ReferenceType::TYPE_WASM_ANYREF:
      // As a cast in JS, anyref is an identity operation.
      args.rval().set(args[0]);
      return true;

    case ReferenceType::TYPE_OBJECT: {
      RootedObject obj(cx, ToObject(cx, args[0]));
      if (!obj) {
        return false;
      }
      args.rval().setObject(*obj);
      return true;
    }
  }

  MOZ_CRASH("Unhandled Reference type");
}

/***************************************************************************
 * ArrayMetaTypeDescr class
 */

static const JSClassOps ArrayTypeDescrClassOps = {
    nullptr,                 // addProperty
    nullptr,                 // delProperty
    nullptr,                 // enumerate
    nullptr,                 // newEnumerate
    nullptr,                 // resolve
    nullptr,                 // mayResolve
    TypeDescr::finalize,     // finalize
    nullptr,                 // call
    nullptr,                 // hasInstance
    TypedObject::construct,  // construct
    nullptr,                 // trace
};

const JSClass ArrayTypeDescr::class_ = {
    "ArrayType",
    JSCLASS_HAS_RESERVED_SLOTS(TypeDescr::SlotCount) |
        JSCLASS_BACKGROUND_FINALIZE,
    &ArrayTypeDescrClassOps};

static bool CreateTraceList(JSContext* cx, HandleTypeDescr descr);

ArrayTypeDescr* ArrayMetaTypeDescr::create(JSContext* cx,
                                           HandleObject arrayTypePrototype,
                                           HandleTypeDescr elementType,
                                           HandleAtom stringRepr, int32_t size,
                                           int32_t length) {
  MOZ_ASSERT(arrayTypePrototype);
  Rooted<ArrayTypeDescr*> obj(cx);
  obj =
      NewSingletonObjectWithGivenProto<ArrayTypeDescr>(cx, arrayTypePrototype);
  if (!obj) {
    return nullptr;
  }

  obj->initReservedSlot(TypeDescr::Kind,
                        Int32Value(int32_t(ArrayTypeDescr::Kind)));
  obj->initReservedSlot(TypeDescr::StringRepr, StringValue(stringRepr));
  obj->initReservedSlot(TypeDescr::Alignment,
                        Int32Value(elementType->alignment()));
  obj->initReservedSlot(TypeDescr::Size, Int32Value(size));
  obj->initReservedSlot(TypeDescr::ArrayElemType, ObjectValue(*elementType));
  obj->initReservedSlot(TypeDescr::ArrayLength, Int32Value(length));

  RootedValue elementTypeVal(cx, ObjectValue(*elementType));
  if (!DefineDataProperty(cx, obj, cx->names().elementType, elementTypeVal,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
    return nullptr;
  }

  RootedValue lengthValue(cx, NumberValue(length));
  if (!DefineDataProperty(cx, obj, cx->names().length, lengthValue,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
    return nullptr;
  }

  Rooted<TypedProto*> prototypeObj(cx, TypedProto::create(cx));
  if (!prototypeObj) {
    return nullptr;
  }

  obj->initReservedSlot(TypeDescr::Proto, ObjectValue(*prototypeObj));

  if (!LinkConstructorAndPrototype(cx, obj, prototypeObj)) {
    return nullptr;
  }

  if (!CreateTraceList(cx, obj)) {
    return nullptr;
  }

  if (!cx->zone()->addTypeDescrObject(cx, obj)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return obj;
}

bool ArrayMetaTypeDescr::construct(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TYPEDOBJECT_NOT_CONSTRUCTIBLE);
  return false;
}

/*********************************
 * StructType class
 */

static const JSClassOps StructTypeDescrClassOps = {
    nullptr,                 // addProperty
    nullptr,                 // delProperty
    nullptr,                 // enumerate
    nullptr,                 // newEnumerate
    nullptr,                 // resolve
    nullptr,                 // mayResolve
    TypeDescr::finalize,     // finalize
    nullptr,                 // call
    nullptr,                 // hasInstance
    TypedObject::construct,  // construct
    nullptr,                 // trace
};

const JSClass StructTypeDescr::class_ = {
    "StructType",
    JSCLASS_HAS_RESERVED_SLOTS(TypeDescr::SlotCount) |
        JSCLASS_BACKGROUND_FINALIZE,
    &StructTypeDescrClassOps};

CheckedInt32 StructMetaTypeDescr::Layout::addField(int32_t fieldAlignment,
                                                   int32_t fieldSize) {
  // Alignment of the struct is the max of the alignment of its fields.
  structAlignment = std::max(structAlignment, fieldAlignment);

  // Align the pointer.
  CheckedInt32 offset = RoundUpToAlignment(sizeSoFar, fieldAlignment);
  if (!offset.isValid()) {
    return offset;
  }

  // Allocate space.
  sizeSoFar = offset + fieldSize;
  if (!sizeSoFar.isValid()) {
    return sizeSoFar;
  }

  return offset;
}

CheckedInt32 StructMetaTypeDescr::Layout::addScalar(Scalar::Type type) {
  return addField(ScalarTypeDescr::alignment(type),
                  ScalarTypeDescr::size(type));
}

CheckedInt32 StructMetaTypeDescr::Layout::addReference(ReferenceType type) {
  return addField(ReferenceTypeDescr::alignment(type),
                  ReferenceTypeDescr::size(type));
}

CheckedInt32 StructMetaTypeDescr::Layout::close(int32_t* alignment) {
  if (alignment) {
    *alignment = structAlignment;
  }
  return RoundUpToAlignment(sizeSoFar, structAlignment);
}

/* static */
StructTypeDescr* StructMetaTypeDescr::createFromArrays(
    JSContext* cx, HandleObject structTypePrototype, HandleIdVector ids,
    JS::HandleValueVector fieldTypeObjs, Vector<StructFieldProps>& fieldProps) {
  StringBuffer stringBuffer(cx);       // Canonical string repr
  RootedValueVector fieldNames(cx);    // Name of each field.
  RootedValueVector fieldOffsets(cx);  // Offset of each field field.
  RootedValueVector fieldMuts(cx);     // Mutability of each field.
  RootedObject userFieldOffsets(cx);   // User-exposed {f:offset} object
  RootedObject userFieldTypes(cx);     // User-exposed {f:descr} object.
  Layout layout;                       // Field offsetter

  userFieldOffsets = NewTenuredBuiltinClassInstance<PlainObject>(cx);
  if (!userFieldOffsets) {
    return nullptr;
  }

  userFieldTypes = NewTenuredBuiltinClassInstance<PlainObject>(cx);
  if (!userFieldTypes) {
    return nullptr;
  }

  if (!stringBuffer.append("new StructType({")) {
    return nullptr;
  }

  RootedId id(cx);
  Rooted<TypeDescr*> fieldType(cx);

  for (unsigned int i = 0; i < ids.length(); i++) {
    id = ids[i];

    // Collect field name
    RootedValue fieldName(cx, IdToValue(id));
    if (!fieldNames.append(fieldName)) {
      return nullptr;
    }

    fieldType = ToObjectIf<TypeDescr>(fieldTypeObjs[i]);

    // userFieldTypes[id] = typeObj
    if (!DefineDataProperty(cx, userFieldTypes, id, fieldTypeObjs[i],
                            JSPROP_READONLY | JSPROP_PERMANENT)) {
      return nullptr;
    }

    // Append "f:Type" to the string repr
    if (i > 0 && !stringBuffer.append(", ")) {
      return nullptr;
    }
    if (!stringBuffer.append(JSID_TO_ATOM(id))) {
      return nullptr;
    }
    if (!stringBuffer.append(": ")) {
      return nullptr;
    }
    if (!stringBuffer.append(&fieldType->stringRepr())) {
      return nullptr;
    }

    CheckedInt32 offset;
    if (fieldProps[i].alignAsInt64) {
      offset = layout.addField(ScalarTypeDescr::alignment(Scalar::Int64),
                               fieldType->size());
    } else if (fieldProps[i].alignAsV128) {
      offset = layout.addField(ScalarTypeDescr::alignment(Scalar::Simd128),
                               fieldType->size());
    } else {
      offset = layout.addField(fieldType->alignment(), fieldType->size());
    }
    if (!offset.isValid()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TYPEDOBJECT_TOO_BIG);
      return nullptr;
    }
    MOZ_ASSERT(offset.value() >= 0);
    if (!fieldOffsets.append(Int32Value(offset.value()))) {
      return nullptr;
    }
    if (!fieldMuts.append(BooleanValue(fieldProps[i].isMutable))) {
      return nullptr;
    }

    // userFieldOffsets[id] = offset
    RootedValue offsetValue(cx, Int32Value(offset.value()));
    if (!DefineDataProperty(cx, userFieldOffsets, id, offsetValue,
                            JSPROP_READONLY | JSPROP_PERMANENT)) {
      return nullptr;
    }
  }

  // Complete string representation.
  if (!stringBuffer.append("})")) {
    return nullptr;
  }

  RootedAtom stringRepr(cx, stringBuffer.finishAtom());
  if (!stringRepr) {
    return nullptr;
  }

  int32_t alignment;
  CheckedInt32 totalSize = layout.close(&alignment);
  if (!totalSize.isValid()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPEDOBJECT_TOO_BIG);
    return nullptr;
  }

  // Now create the resulting type descriptor.

  Rooted<StructTypeDescr*> descr(cx);
  descr = NewSingletonObjectWithGivenProto<StructTypeDescr>(
      cx, structTypePrototype);
  if (!descr) {
    return nullptr;
  }

  descr->initReservedSlot(TypeDescr::Kind,
                          Int32Value(int32_t(TypeKind::Struct)));
  descr->initReservedSlot(TypeDescr::StringRepr, StringValue(stringRepr));
  descr->initReservedSlot(TypeDescr::Alignment,
                          Int32Value(AssertedCast<int32_t>(alignment)));
  descr->initReservedSlot(TypeDescr::Size, Int32Value(totalSize.value()));

  // Construct for internal use an array with the name for each field.
  {
    RootedObject fieldNamesVec(cx);
    fieldNamesVec = NewDenseCopiedArray(
        cx, fieldNames.length(), fieldNames.begin(), nullptr, TenuredObject);
    if (!fieldNamesVec) {
      return nullptr;
    }
    descr->initReservedSlot(TypeDescr::StructFieldNames,
                            ObjectValue(*fieldNamesVec));
  }

  // Construct for internal use an array with the type object for each field.
  RootedObject fieldTypeVec(cx);
  fieldTypeVec =
      NewDenseCopiedArray(cx, fieldTypeObjs.length(), fieldTypeObjs.begin(),
                          nullptr, TenuredObject);
  if (!fieldTypeVec) {
    return nullptr;
  }
  descr->initReservedSlot(TypeDescr::StructFieldTypes,
                          ObjectValue(*fieldTypeVec));

  // Construct for internal use an array with the offset for each field.
  {
    RootedObject fieldOffsetsVec(cx);
    fieldOffsetsVec =
        NewDenseCopiedArray(cx, fieldOffsets.length(), fieldOffsets.begin(),
                            nullptr, TenuredObject);
    if (!fieldOffsetsVec) {
      return nullptr;
    }
    descr->initReservedSlot(TypeDescr::StructFieldOffsets,
                            ObjectValue(*fieldOffsetsVec));
  }

  // Construct for internal use an array with the mutability for each field.
  {
    RootedObject fieldMutsVec(cx);
    fieldMutsVec = NewDenseCopiedArray(
        cx, fieldMuts.length(), fieldMuts.begin(), nullptr, TenuredObject);
    if (!fieldMutsVec) {
      return nullptr;
    }
    descr->initReservedSlot(TypeDescr::StructFieldMuts,
                            ObjectValue(*fieldMutsVec));
  }

  // Create data properties fieldOffsets and fieldTypes
  // TODO: Probably also want to track mutability here, but not important yet.
  if (!FreezeObject(cx, userFieldOffsets)) {
    return nullptr;
  }
  if (!FreezeObject(cx, userFieldTypes)) {
    return nullptr;
  }
  RootedValue userFieldOffsetsValue(cx, ObjectValue(*userFieldOffsets));
  if (!DefineDataProperty(cx, descr, cx->names().fieldOffsets,
                          userFieldOffsetsValue,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
    return nullptr;
  }
  RootedValue userFieldTypesValue(cx, ObjectValue(*userFieldTypes));
  if (!DefineDataProperty(cx, descr, cx->names().fieldTypes,
                          userFieldTypesValue,
                          JSPROP_READONLY | JSPROP_PERMANENT)) {
    return nullptr;
  }

  Rooted<TypedProto*> prototypeObj(cx, TypedProto::create(cx));
  if (!prototypeObj) {
    return nullptr;
  }

  descr->initReservedSlot(TypeDescr::Proto, ObjectValue(*prototypeObj));

  if (!LinkConstructorAndPrototype(cx, descr, prototypeObj)) {
    return nullptr;
  }

  if (!CreateTraceList(cx, descr)) {
    return nullptr;
  }

  if (!cx->zone()->addTypeDescrObject(cx, descr)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return descr;
}

bool StructMetaTypeDescr::construct(JSContext* cx, unsigned int argc,
                                    Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TYPEDOBJECT_NOT_CONSTRUCTIBLE);
  return false;
}

size_t StructTypeDescr::fieldCount() const {
  return fieldInfoObject(TypeDescr::StructFieldNames)
      .getDenseInitializedLength();
}

bool StructTypeDescr::fieldIndex(jsid id, size_t* out) const {
  ArrayObject& fieldNames = fieldInfoObject(TypeDescr::StructFieldNames);
  size_t l = fieldNames.getDenseInitializedLength();
  for (size_t i = 0; i < l; i++) {
    JSAtom& a = fieldNames.getDenseElement(i).toString()->asAtom();
    if (JSID_IS_ATOM(id, &a)) {
      *out = i;
      return true;
    }
  }
  return false;
}

JSAtom& StructTypeDescr::fieldName(size_t index) const {
  return fieldInfoObject(TypeDescr::StructFieldNames)
      .getDenseElement(index)
      .toString()
      ->asAtom();
}

size_t StructTypeDescr::fieldOffset(size_t index) const {
  ArrayObject& fieldOffsets = fieldInfoObject(TypeDescr::StructFieldOffsets);
  MOZ_ASSERT(index < fieldOffsets.getDenseInitializedLength());
  return AssertedCast<size_t>(fieldOffsets.getDenseElement(index).toInt32());
}

bool StructTypeDescr::fieldIsMutable(size_t index) const {
  ArrayObject& fieldMuts = fieldInfoObject(TypeDescr::StructFieldMuts);
  MOZ_ASSERT(index < fieldMuts.getDenseInitializedLength());
  return fieldMuts.getDenseElement(index).toBoolean();
}

TypeDescr& StructTypeDescr::fieldDescr(size_t index) const {
  ArrayObject& fieldDescrs = fieldInfoObject(TypeDescr::StructFieldTypes);
  MOZ_ASSERT(index < fieldDescrs.getDenseInitializedLength());
  return fieldDescrs.getDenseElement(index).toObject().as<TypeDescr>();
}

/******************************************************************************
 * Creating the TypedObject "module"
 *
 * We create one global, `TypedObject`, which contains the following
 * members:
 *
 * 1. uint8, uint16, etc
 * 2. ArrayType
 * 3. StructType
 *
 * Each of these is a function and hence their prototype is
 * `Function.__proto__` (in terms of the JS Engine, they are not
 * JSFunctions but rather instances of their own respective JSClasses
 * which override the call and construct operations).
 *
 * Each type object also has its own `prototype` field. Therefore,
 * using `StructType` as an example, the basic setup is:
 *
 *   StructType --__proto__--> Function.__proto__
 *        |
 *    prototype -- prototype --> { }
 *        |
 *        v
 *       { } -----__proto__--> Function.__proto__
 *
 * When a new type object (e.g., an instance of StructType) is created,
 * it will look as follows:
 *
 *   MyStruct -__proto__-> StructType.prototype -__proto__-> Function.__proto__
 *        |                          |
 *        |                     prototype
 *        |                          |
 *        |                          v
 *    prototype -----__proto__----> { }
 *        |
 *        v
 *       { } --__proto__-> Object.prototype
 *
 * Finally, when an instance of `MyStruct` is created, its
 * structure is as follows:
 *
 *    object -__proto__->
 *      MyStruct.prototype -__proto__->
 *        StructType.prototype.prototype -__proto__->
 *          Object.prototype
 */

// Here `T` is either `ScalarTypeDescr` or `ReferenceTypeDescr`
template <typename T>
static bool DefineSimpleTypeDescr(JSContext* cx, Handle<GlobalObject*> global,
                                  Handle<WasmNamespaceObject*> namespaceObject,
                                  typename T::Type type,
                                  HandlePropertyName className,
                                  WasmNamespaceObject::Slot descSlot) {
  RootedObject objProto(cx,
                        GlobalObject::getOrCreateObjectPrototype(cx, global));
  if (!objProto) {
    return false;
  }

  RootedObject funcProto(
      cx, GlobalObject::getOrCreateFunctionPrototype(cx, global));
  if (!funcProto) {
    return false;
  }

  Rooted<T*> descr(cx, NewSingletonObjectWithGivenProto<T>(cx, funcProto));
  if (!descr) {
    return false;
  }

  descr->initReservedSlot(TypeDescr::Kind, Int32Value(int32_t(T::Kind)));
  descr->initReservedSlot(TypeDescr::StringRepr, StringValue(className));
  descr->initReservedSlot(TypeDescr::Alignment, Int32Value(T::alignment(type)));
  descr->initReservedSlot(TypeDescr::Size,
                          Int32Value(AssertedCast<int32_t>(T::size(type))));
  descr->initReservedSlot(TypeDescr::Type, Int32Value(int32_t(type)));

  // Create the typed prototype for the scalar type. This winds up
  // not being user accessible, but we still create one for consistency.
  Rooted<TypedProto*> proto(cx);
  proto = NewTenuredObjectWithGivenProto<TypedProto>(cx, objProto);
  if (!proto) {
    return false;
  }
  descr->initReservedSlot(TypeDescr::Proto, ObjectValue(*proto));

  if (!CreateTraceList(cx, descr)) {
    return false;
  }

  if (!cx->zone()->addTypeDescrObject(cx, descr)) {
    return false;
  }

  namespaceObject->initReservedSlot(descSlot, ObjectValue(*descr));

  return true;
}

///////////////////////////////////////////////////////////////////////////

template <typename T>
static bool DefineMetaTypeDescr(JSContext* cx, const char* name,
                                Handle<GlobalObject*> global,
                                Handle<WasmNamespaceObject*> namespaceObject,
                                WasmNamespaceObject::Slot protoSlot) {
  RootedAtom className(cx, Atomize(cx, name, strlen(name)));
  if (!className) {
    return false;
  }

  RootedObject proto(cx,
                     GlobalObject::getOrCreateFunctionPrototype(cx, global));
  if (!proto) {
    return false;
  }

  const int constructorLength = 2;
  RootedFunction ctor(cx);
  ctor = GlobalObject::createConstructor(cx, T::construct, className,
                                         constructorLength);
  if (!ctor || !LinkConstructorAndPrototype(cx, ctor, proto)) {
    return false;
  }

  namespaceObject->initReservedSlot(protoSlot, ObjectValue(*proto));

  return true;
}

bool js::InitTypedObjectSlots(JSContext* cx,
                              Handle<WasmNamespaceObject*> namespaceObject) {
  Handle<GlobalObject*> global = cx->global();

  // int32, int64, etc

#define BINARYDATA_SCALAR_DEFINE(constant_, type_, name_, slot_)             \
  if (!DefineSimpleTypeDescr<ScalarTypeDescr>(                               \
          cx, global, namespaceObject, constant_, cx->names().name_, slot_)) \
    return false;
  JS_FOR_EACH_SCALAR_TYPE_REPR(BINARYDATA_SCALAR_DEFINE)
#undef BINARYDATA_SCALAR_DEFINE

#define BINARYDATA_REFERENCE_DEFINE(constant_, type_, name_, slot_)          \
  if (!DefineSimpleTypeDescr<ReferenceTypeDescr>(                            \
          cx, global, namespaceObject, constant_, cx->names().name_, slot_)) \
    return false;
  JS_FOR_EACH_REFERENCE_TYPE_REPR(BINARYDATA_REFERENCE_DEFINE)
#undef BINARYDATA_REFERENCE_DEFINE

  // ArrayType.

  if (!DefineMetaTypeDescr<ArrayMetaTypeDescr>(
          cx, "ArrayType", global, namespaceObject,
          WasmNamespaceObject::ArrayTypePrototype)) {
    return false;
  }

  // StructType.

  if (!DefineMetaTypeDescr<StructMetaTypeDescr>(
          cx, "StructType", global, namespaceObject,
          WasmNamespaceObject::StructTypePrototype)) {
    return false;
  }

  return true;
}

TypedProto* TypedProto::create(JSContext* cx) {
  Handle<GlobalObject*> global = cx->global();
  RootedObject objProto(cx,
                        GlobalObject::getOrCreateObjectPrototype(cx, global));
  if (!objProto) {
    return nullptr;
  }

  return NewSingletonObjectWithGivenProto<TypedProto>(cx, objProto);
}

/******************************************************************************
 * Typed objects
 */

uint32_t TypedObject::offset() const {
  if (is<InlineTypedObject>()) {
    return 0;
  }
  return PointerRangeSize(typedMemBase(), typedMem());
}

uint32_t TypedObject::length() const {
  return typeDescr().as<ArrayTypeDescr>().length();
}

uint8_t* TypedObject::typedMem() const {
  if (is<InlineTypedObject>()) {
    return as<InlineTypedObject>().inlineTypedMem();
  }
  return as<OutlineTypedObject>().outOfLineTypedMem();
}

uint8_t* TypedObject::typedMemBase() const {
  MOZ_ASSERT(is<OutlineTypedObject>());

  JSObject& owner = as<OutlineTypedObject>().owner();
  if (owner.is<ArrayBufferObject>()) {
    return owner.as<ArrayBufferObject>().dataPointer();
  }
  return owner.as<InlineTypedObject>().inlineTypedMem();
}

/******************************************************************************
 * Outline typed objects
 */

void OutlineTypedObject::setOwnerAndData(JSObject* owner, uint8_t* data) {
  // Typed objects cannot move from one owner to another, so don't worry
  // about pre barriers during this initialization.
  owner_ = owner;
  data_ = data;

  if (owner) {
    if (!IsInsideNursery(this) && IsInsideNursery(owner)) {
      // Trigger a post barrier when attaching an object outside the nursery to
      // one that is inside it.
      owner->storeBuffer()->putWholeCell(this);
    } else if (IsInsideNursery(this) && !IsInsideNursery(owner)) {
      // ...and also when attaching an object inside the nursery to one that is
      // outside it, for a subtle reason -- the outline object now points to
      // the memory owned by 'owner', and can modify object/string references
      // stored in that memory, potentially storing nursery pointers in it. If
      // the outline object is in the nursery, then the post barrier will do
      // nothing; you will be writing a nursery pointer "into" a nursery
      // object. But that will result in the tenured owner's data containing a
      // nursery pointer, and thus we need a store buffer edge. Since we can't
      // catch the actual write, register the owner preemptively now.
      storeBuffer()->putWholeCell(owner);
    }
  }
}

/*static*/
OutlineTypedObject* OutlineTypedObject::createUnattached(JSContext* cx,
                                                         HandleTypeDescr descr,
                                                         gc::InitialHeap heap) {
  AutoSetNewObjectMetadata metadata(cx);

  RootedObjectGroup group(cx, ObjectGroup::defaultNewGroup(
                                  cx, &OutlineTypedObject::class_,
                                  TaggedProto(&descr->typedProto()), descr));
  if (!group) {
    return nullptr;
  }

  NewObjectKind newKind =
      (heap == gc::TenuredHeap) ? TenuredObject : GenericObject;
  OutlineTypedObject* obj = NewObjectWithGroup<OutlineTypedObject>(
      cx, group, gc::AllocKind::OBJECT0, newKind);
  if (!obj) {
    return nullptr;
  }

  obj->setOwnerAndData(nullptr, nullptr);
  return obj;
}

void OutlineTypedObject::attach(ArrayBufferObject& buffer, uint32_t offset) {
  MOZ_ASSERT(offset <= wasm::ByteLength32(buffer));
  MOZ_ASSERT(size() <= wasm::ByteLength32(buffer) - offset);
  MOZ_ASSERT(buffer.hasTypedObjectViews());
  MOZ_ASSERT(!buffer.isDetached());

  setOwnerAndData(&buffer, buffer.dataPointer() + offset);
}

void OutlineTypedObject::attach(JSContext* cx, TypedObject& typedObj,
                                uint32_t offset) {
  JSObject* owner = &typedObj;
  if (typedObj.is<OutlineTypedObject>()) {
    owner = &typedObj.as<OutlineTypedObject>().owner();
    MOZ_ASSERT(typedObj.offset() <= UINT32_MAX - offset);
    offset += typedObj.offset();
  }

  if (owner->is<ArrayBufferObject>()) {
    attach(owner->as<ArrayBufferObject>(), offset);
  } else {
    MOZ_ASSERT(owner->is<InlineTypedObject>());
    JS::AutoCheckCannotGC nogc(cx);
    setOwnerAndData(
        owner, owner->as<InlineTypedObject>().inlineTypedMem(nogc) + offset);
  }
}

/*static*/
OutlineTypedObject* OutlineTypedObject::createZeroed(JSContext* cx,
                                                     HandleTypeDescr descr,
                                                     gc::InitialHeap heap) {
  // Create unattached wrapper object.
  Rooted<OutlineTypedObject*> obj(
      cx, OutlineTypedObject::createUnattached(cx, descr, heap));
  if (!obj) {
    return nullptr;
  }

  // Allocate and initialize the memory for this instance.
  size_t totalSize = descr->size();
  Rooted<ArrayBufferObject*> buffer(cx);
  buffer = ArrayBufferObject::createForTypedObject(cx, totalSize);
  if (!buffer) {
    return nullptr;
  }
  descr->initInstance(buffer->dataPointer());
  obj->attach(*buffer, 0);
  return obj;
}

/*static*/
TypedObject* TypedObject::createZeroed(JSContext* cx, HandleTypeDescr descr,
                                       gc::InitialHeap heap) {
  // If possible, create an object with inline data.
  if (InlineTypedObject::canAccommodateType(descr)) {
    AutoSetNewObjectMetadata metadata(cx);

    InlineTypedObject* obj = InlineTypedObject::create(cx, descr, heap);
    if (!obj) {
      return nullptr;
    }
    JS::AutoCheckCannotGC nogc(cx);
    descr->initInstance(obj->inlineTypedMem(nogc));
    return obj;
  }

  return OutlineTypedObject::createZeroed(cx, descr, heap);
}

/* static */
void OutlineTypedObject::obj_trace(JSTracer* trc, JSObject* object) {
  OutlineTypedObject& typedObj = object->as<OutlineTypedObject>();

  TraceEdge(trc, typedObj.shapePtr(), "OutlineTypedObject_shape");

  if (!typedObj.owner_) {
    MOZ_ASSERT(!typedObj.data_);
    return;
  }
  MOZ_ASSERT(typedObj.data_);

  TypeDescr& descr = typedObj.typeDescr();

  // Mark the owner, watching in case it is moved by the tracer.
  JSObject* oldOwner = typedObj.owner_;
  TraceManuallyBarrieredEdge(trc, &typedObj.owner_, "typed object owner");
  JSObject* owner = typedObj.owner_;

  uint8_t* oldData = typedObj.outOfLineTypedMem();
  uint8_t* newData = oldData;

  // Update the data pointer if the owner moved and the owner's data is
  // inline with it.
  if (owner != oldOwner &&
      (IsInlineTypedObjectClass(gc::MaybeForwardedObjectClass(owner)) ||
       gc::MaybeForwardedObjectAs<ArrayBufferObject>(owner).hasInlineData())) {
    newData += reinterpret_cast<uint8_t*>(owner) -
               reinterpret_cast<uint8_t*>(oldOwner);
    typedObj.setData(newData);

    if (trc->isTenuringTracer()) {
      Nursery& nursery = trc->runtime()->gc.nursery();
      nursery.maybeSetForwardingPointer(trc, oldData, newData,
                                        /* direct = */ false);
    }
  }

  if (descr.hasTraceList()) {
    gc::VisitTraceList(trc, object, descr.traceList(), newData);
    return;
  }

  descr.traceInstance(trc, newData);
}

bool TypeDescr::hasProperty(const JSAtomState& names, jsid id) {
  switch (kind()) {
    case TypeKind::Scalar:
    case TypeKind::Reference:
      return false;

    case TypeKind::Array: {
      uint32_t index;
      return IdIsIndex(id, &index) || JSID_IS_ATOM(id, names.length);
    }

    case TypeKind::Struct: {
      size_t index;
      return as<StructTypeDescr>().fieldIndex(id, &index);
    }
  }

  MOZ_CRASH("Unexpected kind");
}

/* static */
bool TypedObject::obj_lookupProperty(JSContext* cx, HandleObject obj,
                                     HandleId id, MutableHandleObject objp,
                                     MutableHandle<PropertyResult> propp) {
  if (obj->as<TypedObject>().typeDescr().hasProperty(cx->names(), id)) {
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

bool TypedObject::obj_defineProperty(JSContext* cx, HandleObject obj,
                                     HandleId id,
                                     Handle<PropertyDescriptor> desc,
                                     ObjectOpResult& result) {
  // Serialize the type string of |obj|.
  RootedAtom typeReprAtom(cx, &obj->as<TypedObject>().typeDescr().stringRepr());
  UniqueChars typeReprStr = StringToNewUTF8CharsZ(cx, *typeReprAtom);
  if (!typeReprStr) {
    return false;
  }

  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_OBJECT_NOT_EXTENSIBLE, typeReprStr.get());
  return false;
}

bool TypedObject::obj_hasProperty(JSContext* cx, HandleObject obj, HandleId id,
                                  bool* foundp) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());
  switch (typedObj->typeDescr().kind()) {
    case TypeKind::Scalar:
    case TypeKind::Reference:
      break;

    case TypeKind::Array: {
      if (JSID_IS_ATOM(id, cx->names().length)) {
        *foundp = true;
        return true;
      }
      uint32_t index;
      // Elements are not inherited from the prototype.
      if (IdIsIndex(id, &index)) {
        *foundp = (index < uint32_t(typedObj->length()));
        return true;
      }
      break;
    }

    case TypeKind::Struct:
      size_t index;
      if (typedObj->typeDescr().as<StructTypeDescr>().fieldIndex(id, &index)) {
        *foundp = true;
        return true;
      }
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

  // Dispatch elements to obj_getElement:
  uint32_t index;
  if (IdIsIndex(id, &index)) {
    return obj_getElement(cx, obj, receiver, index, vp);
  }

  // Handle everything else here:

  switch (typedObj->typeDescr().kind()) {
    case TypeKind::Scalar:
    case TypeKind::Reference:
      break;

    case TypeKind::Array:
      if (JSID_IS_ATOM(id, cx->names().length)) {
        vp.setInt32(typedObj->length());
        return true;
      }
      break;

    case TypeKind::Struct: {
      Rooted<StructTypeDescr*> descr(
          cx, &typedObj->typeDescr().as<StructTypeDescr>());

      size_t fieldIndex;
      if (!descr->fieldIndex(id, &fieldIndex)) {
        break;
      }

      size_t offset = descr->fieldOffset(fieldIndex);
      Rooted<TypeDescr*> fieldType(cx, &descr->fieldDescr(fieldIndex));

      return typedObj->loadValue(cx, offset, fieldType, vp);
    }
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    vp.setUndefined();
    return true;
  }

  return GetProperty(cx, proto, receiver, id, vp);
}

bool TypedObject::obj_getElement(JSContext* cx, HandleObject obj,
                                 HandleValue receiver, uint32_t index,
                                 MutableHandleValue vp) {
  MOZ_ASSERT(obj->is<TypedObject>());
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());
  Rooted<TypeDescr*> descr(cx, &typedObj->typeDescr());

  switch (descr->kind()) {
    case TypeKind::Scalar:
    case TypeKind::Reference:
    case TypeKind::Struct:
      break;

    case TypeKind::Array:
      return obj_getArrayElement(cx, typedObj, descr, index, vp);
  }

  RootedObject proto(cx, obj->staticPrototype());
  if (!proto) {
    vp.setUndefined();
    return true;
  }

  return GetElement(cx, proto, receiver, index, vp);
}

/*static*/
bool TypedObject::obj_getArrayElement(JSContext* cx,
                                      Handle<TypedObject*> typedObj,
                                      Handle<TypeDescr*> typeDescr,
                                      uint32_t index, MutableHandleValue vp) {
  // Elements are not inherited from the prototype.
  if (index >= (size_t)typedObj->length()) {
    vp.setUndefined();
    return true;
  }

  Rooted<TypeDescr*> elementType(
      cx, &typeDescr->as<ArrayTypeDescr>().elementType());
  size_t offset = elementType->size() * index;
  return typedObj->loadValue(cx, offset, elementType, vp);
}

bool TypedObject::obj_setProperty(JSContext* cx, HandleObject obj, HandleId id,
                                  HandleValue v, HandleValue receiver,
                                  ObjectOpResult& result) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());

  switch (typedObj->typeDescr().kind()) {
    case TypeKind::Scalar:
    case TypeKind::Reference:
      break;

    case TypeKind::Array: {
      if (JSID_IS_ATOM(id, cx->names().length)) {
        if (receiver.isObject() && obj == &receiver.toObject()) {
          JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                    JSMSG_CANT_REDEFINE_ARRAY_LENGTH);
          return false;
        }
        return result.failReadOnly();
      }

      uint32_t index;
      if (IdIsIndex(id, &index)) {
        if (!receiver.isObject() || obj != &receiver.toObject()) {
          return SetPropertyByDefining(cx, id, v, receiver, result);
        }

        if (index >= uint32_t(typedObj->length())) {
          JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                    JSMSG_TYPEDOBJECT_BINARYARRAY_BAD_INDEX);
          return false;
        }

        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_TYPEDOBJECT_SETTING_IMMUTABLE);
        return false;
      }
      break;
    }

    case TypeKind::Struct: {
      Rooted<StructTypeDescr*> descr(
          cx, &typedObj->typeDescr().as<StructTypeDescr>());

      size_t fieldIndex;
      if (!descr->fieldIndex(id, &fieldIndex)) {
        break;
      }

      if (!descr->fieldIsMutable(fieldIndex)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_TYPEDOBJECT_SETTING_IMMUTABLE);
        return false;
      }

      if (!receiver.isObject() || obj != &receiver.toObject()) {
        return SetPropertyByDefining(cx, id, v, receiver, result);
      }

      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TYPEDOBJECT_SETTING_IMMUTABLE);
      return false;
    }
  }

  return SetPropertyOnProto(cx, obj, id, v, receiver, result);
}

bool TypedObject::obj_getOwnPropertyDescriptor(
    JSContext* cx, HandleObject obj, HandleId id,
    MutableHandle<PropertyDescriptor> desc) {
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());
  Rooted<TypeDescr*> descr(cx, &typedObj->typeDescr());
  switch (descr->kind()) {
    case TypeKind::Scalar:
    case TypeKind::Reference:
      break;

    case TypeKind::Array: {
      uint32_t index;
      if (IdIsIndex(id, &index)) {
        if (!obj_getArrayElement(cx, typedObj, descr, index, desc.value())) {
          return false;
        }
        desc.setAttributes(JSPROP_ENUMERATE | JSPROP_PERMANENT);
        desc.object().set(obj);
        return true;
      }

      if (JSID_IS_ATOM(id, cx->names().length)) {
        desc.value().setInt32(typedObj->length());
        desc.setAttributes(JSPROP_READONLY | JSPROP_PERMANENT);
        desc.object().set(obj);
        return true;
      }
      break;
    }

    case TypeKind::Struct: {
      Rooted<StructTypeDescr*> descr(
          cx, &typedObj->typeDescr().as<StructTypeDescr>());

      size_t fieldIndex;
      if (!descr->fieldIndex(id, &fieldIndex)) {
        break;
      }

      size_t offset = descr->fieldOffset(fieldIndex);
      Rooted<TypeDescr*> fieldType(cx, &descr->fieldDescr(fieldIndex));
      if (!typedObj->loadValue(cx, offset, fieldType, desc.value())) {
        return false;
      }

      desc.setAttributes(JSPROP_ENUMERATE | JSPROP_PERMANENT);
      desc.object().set(obj);
      return true;
    }
  }

  desc.object().set(nullptr);
  return true;
}

static bool IsOwnId(JSContext* cx, HandleObject obj, HandleId id) {
  uint32_t index;
  Rooted<TypedObject*> typedObj(cx, &obj->as<TypedObject>());
  switch (typedObj->typeDescr().kind()) {
    case TypeKind::Scalar:
    case TypeKind::Reference:
      return false;

    case TypeKind::Array:
      return IdIsIndex(id, &index) || JSID_IS_ATOM(id, cx->names().length);

    case TypeKind::Struct:
      size_t index;
      if (typedObj->typeDescr().as<StructTypeDescr>().fieldIndex(id, &index)) {
        return true;
      }
  }

  return false;
}

bool TypedObject::obj_deleteProperty(JSContext* cx, HandleObject obj,
                                     HandleId id, ObjectOpResult& result) {
  if (IsOwnId(cx, obj, id)) {
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
  Rooted<TypeDescr*> descr(cx, &typedObj->typeDescr());

  RootedId id(cx);
  switch (descr->kind()) {
    case TypeKind::Scalar:
    case TypeKind::Reference: {
      // Nothing to enumerate.
      break;
    }

    case TypeKind::Array: {
      if (!properties.reserve(typedObj->length())) {
        return false;
      }

      for (uint32_t index = 0; index < typedObj->length(); index++) {
        id = INT_TO_JSID(index);
        properties.infallibleAppend(id);
      }
      break;
    }

    case TypeKind::Struct: {
      size_t fieldCount = descr->as<StructTypeDescr>().fieldCount();
      if (!properties.reserve(fieldCount)) {
        return false;
      }

      for (size_t index = 0; index < fieldCount; index++) {
        id = AtomToId(&descr->as<StructTypeDescr>().fieldName(index));
        properties.infallibleAppend(id);
      }
      break;
    }
  }

  return true;
}

static void LoadReference_Object(GCPtrObject* heap, MutableHandleValue v) {
  if (*heap) {
    v.setObject(**heap);
  } else {
    v.setNull();
  }
}

static void LoadReference_WasmAnyRef(GCPtrObject* heap, MutableHandleValue v) {
  // TODO/AnyRef-boxing: With boxed immediates and strings this will change.
  if (*heap) {
    JSObject* object = heap->get();
    if (object->is<wasm::WasmValueBox>()) {
      v.set(object->as<wasm::WasmValueBox>().value());
    } else {
      v.setObject(*object);
    }
  } else {
    v.setNull();
  }
}

#define JS_LOAD_NUMBER_IMPL(constant, T, _name, _slot)        \
  case constant: {                                            \
    /* Should be guaranteed by the typed objects API: */      \
    MOZ_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                 \
                                                              \
    JS::AutoCheckCannotGC nogc(cx);                           \
    T* target = reinterpret_cast<T*>(typedMem(offset, nogc)); \
    vp.setNumber(JS::CanonicalizeNaN((double)*target));       \
    return true;                                              \
  }

#define JS_LOAD_BIGINT_IMPL(constant, T, _name, _slot)       \
  case constant: {                                           \
    /* Should be guaranteed by the typed objects API: */     \
    MOZ_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                \
                                                             \
    T value;                                                 \
    {                                                        \
      JS::AutoCheckCannotGC nogc(cx);                        \
      value = *reinterpret_cast<T*>(typedMem(offset, nogc)); \
    }                                                        \
    BigInt* bi = CreateBigInt<T>(cx, value);                 \
    if (!bi) {                                               \
      return false;                                          \
    }                                                        \
    vp.setBigInt(bi);                                        \
    return true;                                             \
  }

#define JS_LOAD_REFERENCE_IMPL(constant, T, name, _slot)      \
  case constant: {                                            \
    /* Should be guaranteed by the typed objects API: */      \
    MOZ_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                 \
                                                              \
    JS::AutoCheckCannotGC nogc(cx);                           \
    T* target = reinterpret_cast<T*>(typedMem(offset, nogc)); \
    LoadReference_##name(target, vp);                         \
    return true;                                              \
  }

bool TypedObject::loadValue(JSContext* cx, size_t offset, HandleTypeDescr type,
                            MutableHandleValue vp) {
  MOZ_ASSERT(type->is<SimpleTypeDescr>());

  switch (type->kind()) {
    case TypeKind::Scalar: {
      ScalarTypeDescr::Type scalarType = type->as<ScalarTypeDescr>().type();
      switch (scalarType) {
        JS_FOR_EACH_SCALAR_NUMBER_TYPE_REPR(JS_LOAD_NUMBER_IMPL)
        JS_FOR_EACH_SCALAR_BIGINT_TYPE_REPR(JS_LOAD_BIGINT_IMPL)
        default:
          MOZ_CRASH("unsupported");
      }
      break;
    }
    case TypeKind::Reference: {
      ReferenceTypeDescr::Type referenceType =
          type->as<ReferenceTypeDescr>().type();
      switch (referenceType) {
        JS_FOR_EACH_REFERENCE_TYPE_REPR(JS_LOAD_REFERENCE_IMPL)
        default:
          MOZ_CRASH("unsupported");
      }
    }
    default:
      MOZ_CRASH("unsupported");
  }

  return false;
}

/******************************************************************************
 * Inline typed objects
 */

/* static */
InlineTypedObject* InlineTypedObject::create(JSContext* cx,
                                             HandleTypeDescr descr,
                                             gc::InitialHeap heap) {
  gc::AllocKind allocKind = allocKindForTypeDescriptor(descr);

  RootedObjectGroup group(cx, ObjectGroup::defaultNewGroup(
                                  cx, &InlineTypedObject::class_,
                                  TaggedProto(&descr->typedProto()), descr));
  if (!group) {
    return nullptr;
  }

  NewObjectKind newKind =
      (heap == gc::TenuredHeap) ? TenuredObject : GenericObject;
  return NewObjectWithGroup<InlineTypedObject>(cx, group, allocKind, newKind);
}

/* static */
void InlineTypedObject::obj_trace(JSTracer* trc, JSObject* object) {
  InlineTypedObject& typedObj = object->as<InlineTypedObject>();

  TraceEdge(trc, typedObj.shapePtr(), "InlineTypedObject_shape");

  TypeDescr& descr = typedObj.typeDescr();
  if (descr.hasTraceList()) {
    gc::VisitTraceList(trc, object, typedObj.typeDescr().traceList(),
                       typedObj.inlineTypedMem());
    return;
  }

  descr.traceInstance(trc, typedObj.inlineTypedMem());
}

/* static */
size_t InlineTypedObject::obj_moved(JSObject* dst, JSObject* src) {
  if (!IsInsideNursery(src)) {
    return 0;
  }

  // Inline typed object element arrays can be preserved on the stack by Ion
  // and need forwarding pointers created during a minor GC. We can't do this
  // in the trace hook because we don't have any stale data to determine
  // whether this object moved and where it was moved from.
  TypeDescr& descr = dst->as<InlineTypedObject>().typeDescr();
  if (descr.kind() == TypeKind::Array) {
    // The forwarding pointer can be direct as long as there is enough
    // space for it. Other objects might point into the object's buffer,
    // but they will not set any direct forwarding pointers.
    uint8_t* oldData = reinterpret_cast<uint8_t*>(src) + offsetOfDataStart();
    uint8_t* newData = dst->as<InlineTypedObject>().inlineTypedMem();
    auto& nursery = dst->runtimeFromMainThread()->gc.nursery();
    bool direct = descr.size() >= sizeof(uintptr_t);
    nursery.setForwardingPointerWhileTenuring(oldData, newData, direct);
  }

  return 0;
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

#define DEFINE_TYPEDOBJ_CLASS(Name, Trace, Moved)                            \
  static const JSClassOps Name##ClassOps = {                                 \
      nullptr, /* addProperty */                                             \
      nullptr, /* delProperty */                                             \
      nullptr, /* enumerate   */                                             \
      TypedObject::obj_newEnumerate,                                         \
      nullptr, /* resolve     */                                             \
      nullptr, /* mayResolve  */                                             \
      nullptr, /* finalize    */                                             \
      nullptr, /* call        */                                             \
      nullptr, /* hasInstance */                                             \
      nullptr, /* construct   */                                             \
      Trace,                                                                 \
  };                                                                         \
  static const ClassExtension Name##ClassExt = {                             \
      Moved /* objectMovedOp */                                              \
  };                                                                         \
  const JSClass Name::class_ = {                                             \
      #Name,           JSClass::NON_NATIVE | JSCLASS_DELAY_METADATA_BUILDER, \
      &Name##ClassOps, JS_NULL_CLASS_SPEC,                                   \
      &Name##ClassExt, &TypedObject::objectOps_}

DEFINE_TYPEDOBJ_CLASS(OutlineTypedObject, OutlineTypedObject::obj_trace,
                      nullptr);
DEFINE_TYPEDOBJ_CLASS(InlineTypedObject, InlineTypedObject::obj_trace,
                      InlineTypedObject::obj_moved);

/*static*/
bool TypedObject::construct(JSContext* cx, unsigned int argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  MOZ_ASSERT(args.callee().is<TypeDescr>());
  Rooted<TypeDescr*> callee(cx, &args.callee().as<TypeDescr>());

  MOZ_ASSERT(cx->realm() == callee->realm());
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TYPEDOBJECT_NOT_CONSTRUCTIBLE);
  return false;
}

/* static */ JS::Result<TypedObject*, JS::OOM> TypedObject::create(
    JSContext* cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
    js::HandleShape shape, js::HandleObjectGroup group) {
  debugCheckNewObject(group, shape, kind, heap);

  const JSClass* clasp = group->clasp();
  MOZ_ASSERT(::IsTypedObjectClass(clasp));

  JSObject* obj =
      js::AllocateObject(cx, kind, /* nDynamicSlots = */ 0, heap, clasp);
  if (!obj) {
    return cx->alreadyReportedOOM();
  }

  TypedObject* tobj = static_cast<TypedObject*>(obj);
  tobj->initGroup(group);
  tobj->initShape(shape);

  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  cx->realm()->setObjectPendingMetadata(cx, tobj);

  js::gc::gcprobes::CreateObject(tobj);

  return tobj;
}

///////////////////////////////////////////////////////////////////////////
// Walking memory

template <typename V>
static void VisitReferences(TypeDescr& descr, uint8_t* base, V& visitor,
                            size_t offset) {
  switch (descr.kind()) {
    case TypeKind::Scalar:
      return;

    case TypeKind::Reference:
      visitor.visitReference(descr.as<ReferenceTypeDescr>(), base, offset);
      return;

    case TypeKind::Array: {
      ArrayTypeDescr& arrayDescr = descr.as<ArrayTypeDescr>();
      TypeDescr& elementDescr = arrayDescr.elementType();
      for (uint32_t i = 0; i < arrayDescr.length(); i++) {
        VisitReferences(elementDescr, base, visitor, offset);
        offset += elementDescr.size();
      }
      return;
    }

    case TypeKind::Struct: {
      StructTypeDescr& structDescr = descr.as<StructTypeDescr>();
      for (size_t i = 0; i < structDescr.fieldCount(); i++) {
        TypeDescr& descr = structDescr.fieldDescr(i);
        VisitReferences(descr, base, visitor,
                        offset + structDescr.fieldOffset(i));
      }
      return;
    }
  }

  MOZ_CRASH("Invalid type repr kind");
}

///////////////////////////////////////////////////////////////////////////
// Initializing instances

namespace {

class MemoryInitVisitor {
 public:
  void visitReference(ReferenceTypeDescr& descr, uint8_t* base, size_t offset);
};

}  // namespace

void MemoryInitVisitor::visitReference(ReferenceTypeDescr& descr, uint8_t* base,
                                       size_t offset) {
  switch (descr.type()) {
    case ReferenceType::TYPE_WASM_ANYREF:
    case ReferenceType::TYPE_OBJECT: {
      js::GCPtrObject* objectPtr =
          reinterpret_cast<js::GCPtrObject*>(base + offset);
      objectPtr->init(nullptr);
      return;
    }
  }

  MOZ_CRASH("Invalid kind");
}

void TypeDescr::initInstance(uint8_t* mem) {
  MemoryInitVisitor visitor;

  // Initialize the instance
  memset(mem, 0, size());
  VisitReferences(*this, mem, visitor, 0);
}

///////////////////////////////////////////////////////////////////////////
// Tracing instances

namespace {

class MemoryTracingVisitor {
  JSTracer* trace_;

 public:
  explicit MemoryTracingVisitor(JSTracer* trace) : trace_(trace) {}

  void visitReference(ReferenceTypeDescr& descr, uint8_t* base, size_t offset);
};

}  // namespace

void MemoryTracingVisitor::visitReference(ReferenceTypeDescr& descr,
                                          uint8_t* base, size_t offset) {
  switch (descr.type()) {
    case ReferenceType::TYPE_WASM_ANYREF:
      // TODO/AnyRef-boxing: With boxed immediates and strings the tracing code
      // will be more complicated.  For now, tracing as an object is fine.
    case ReferenceType::TYPE_OBJECT: {
      GCPtrObject* objectPtr =
          reinterpret_cast<js::GCPtrObject*>(base + offset);
      TraceNullableEdge(trace_, objectPtr, "reference-obj");
      return;
    }
  }

  MOZ_CRASH("Invalid kind");
}

void TypeDescr::traceInstance(JSTracer* trace, uint8_t* mem) {
  MemoryTracingVisitor visitor(trace);

  VisitReferences(*this, mem, visitor, 0);
}

namespace {

struct TraceListVisitor {
  using OffsetVector = Vector<uint32_t, 0, SystemAllocPolicy>;
  OffsetVector stringOffsets;
  OffsetVector objectOffsets;
  OffsetVector valueOffsets;

  void visitReference(ReferenceTypeDescr& descr, uint8_t* base, size_t offset);

  bool fillList(Vector<uint32_t>& entries);
};

}  // namespace

void TraceListVisitor::visitReference(ReferenceTypeDescr& descr, uint8_t* base,
                                      size_t offset) {
  MOZ_ASSERT(!base);

  OffsetVector* offsets;
  // TODO/AnyRef-boxing: Once a WasmAnyRef is no longer just a JSObject*
  // we must revisit this structure.
  switch (descr.type()) {
    case ReferenceType::TYPE_OBJECT:
      offsets = &objectOffsets;
      break;
    case ReferenceType::TYPE_WASM_ANYREF:
      offsets = &objectOffsets;
      break;
    default:
      MOZ_CRASH("Invalid kind");
  }

  AutoEnterOOMUnsafeRegion oomUnsafe;

  MOZ_ASSERT(offset <= UINT32_MAX);
  if (!offsets->append(offset)) {
    oomUnsafe.crash("TraceListVisitor::visitReference");
  }
}

bool TraceListVisitor::fillList(Vector<uint32_t>& entries) {
  return entries.append(stringOffsets.length()) &&
         entries.append(objectOffsets.length()) &&
         entries.append(valueOffsets.length()) &&
         entries.appendAll(stringOffsets) && entries.appendAll(objectOffsets) &&
         entries.appendAll(valueOffsets);
}

static bool CreateTraceList(JSContext* cx, HandleTypeDescr descr) {
  // Trace lists are only used for inline typed objects. We don't use them
  // for larger objects, both to limit the size of the trace lists and
  // because tracing outline typed objects is considerably more complicated
  // than inline ones.
  if (!InlineTypedObject::canAccommodateType(descr)) {
    return true;
  }

  TraceListVisitor visitor;
  VisitReferences(*descr, nullptr, visitor, 0);

  Vector<uint32_t> entries(cx);
  if (!visitor.fillList(entries)) {
    return false;
  }

  // Trace lists aren't necessary for descriptors with no references.
  MOZ_ASSERT(entries.length() >= 3);
  if (entries.length() == 3) {
    MOZ_ASSERT(entries[0] == 0 && entries[1] == 0 && entries[2] == 0);
    return true;
  }

  uint32_t* list = cx->pod_malloc<uint32_t>(entries.length());
  if (!list) {
    return false;
  }

  PodCopy(list, entries.begin(), entries.length());

  size_t size = entries.length() * sizeof(uint32_t);
  InitReservedSlot(descr, TypeDescr::TraceList, list, size,
                   MemoryUse::TypeDescrTraceList);
  return true;
}

/* static */
void TypeDescr::finalize(JSFreeOp* fop, JSObject* obj) {
  TypeDescr& descr = obj->as<TypeDescr>();
  if (descr.hasTraceList()) {
    auto list = const_cast<uint32_t*>(descr.traceList());
    size_t size = (3 + list[0] + list[1] + list[2]) * sizeof(uint32_t);
    fop->free_(obj, list, size, MemoryUse::TypeDescrTraceList);
  }
}
