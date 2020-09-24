/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TypedObjectConstants.h"

///////////////////////////////////////////////////////////////////////////
// Getters and setters for various slots.

// Type object slots

#define DESCR_KIND(obj) \
    UnsafeGetInt32FromReservedSlot(obj, JS_DESCR_SLOT_KIND)
#define DESCR_STRING_REPR(obj) \
    UnsafeGetStringFromReservedSlot(obj, JS_DESCR_SLOT_STRING_REPR)
#define DESCR_ALIGNMENT(obj) \
    UnsafeGetInt32FromReservedSlot(obj, JS_DESCR_SLOT_ALIGNMENT)
#define DESCR_SIZE(obj) \
    UnsafeGetInt32FromReservedSlot(obj, JS_DESCR_SLOT_SIZE)
#define DESCR_OPAQUE(obj) \
    UnsafeGetBooleanFromReservedSlot(obj, JS_DESCR_SLOT_OPAQUE)
#define DESCR_TYPE(obj)   \
    UnsafeGetInt32FromReservedSlot(obj, JS_DESCR_SLOT_TYPE)
#define DESCR_ARRAY_ELEMENT_TYPE(obj) \
    UnsafeGetObjectFromReservedSlot(obj, JS_DESCR_SLOT_ARRAY_ELEM_TYPE)
#define DESCR_ARRAY_LENGTH(obj) \
    TO_INT32(UnsafeGetInt32FromReservedSlot(obj, JS_DESCR_SLOT_ARRAY_LENGTH))
#define DESCR_STRUCT_FIELD_NAMES(obj) \
    UnsafeGetObjectFromReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_NAMES)
#define DESCR_STRUCT_FIELD_TYPES(obj) \
    UnsafeGetObjectFromReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_TYPES)
#define DESCR_STRUCT_FIELD_OFFSETS(obj) \
    UnsafeGetObjectFromReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS)

///////////////////////////////////////////////////////////////////////////
// Getting values
//
// The methods in this section read from the memory pointed at
// by `this` and produce JS values. This process is called *reification*
// in the spec.

// Reifies the value referenced by the pointer, meaning that it
// returns a new object pointing at the value. If the value is
// a scalar, it will return a JS number, but otherwise the reified
// result will be a typedObj of the same class as the ptr's typedObj.
function TypedObjectGet(descr, typedObj, offset) {
  assert(IsObject(descr) && ObjectIsTypeDescr(descr),
         "get() called with bad type descr");

  switch (DESCR_KIND(descr)) {
  case JS_TYPEREPR_SCALAR_KIND:
    return TypedObjectGetScalar(descr, typedObj, offset);

  case JS_TYPEREPR_REFERENCE_KIND:
    return TypedObjectGetReference(descr, typedObj, offset);

  case JS_TYPEREPR_ARRAY_KIND:
  case JS_TYPEREPR_STRUCT_KIND:
    return TypedObjectGetDerived(descr, typedObj, offset);
  }

  assert(false, "Unhandled kind: " + DESCR_KIND(descr));
  return undefined;
}

function TypedObjectGetDerived(descr, typedObj, offset) {
  assert(!TypeDescrIsSimpleType(descr),
         "getDerived() used with simple type");
  return NewDerivedTypedObject(descr, typedObj, offset | 0);
}

function TypedObjectGetScalar(descr, typedObj, offset) {
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_SCALARTYPEREPR_INT8:
    return Load_int8(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_UINT8:
  case JS_SCALARTYPEREPR_UINT8_CLAMPED:
    return Load_uint8(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_INT16:
    return Load_int16(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_UINT16:
    return Load_uint16(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_INT32:
    return Load_int32(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_UINT32:
    return Load_uint32(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_FLOAT32:
    return Load_float32(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_FLOAT64:
    return Load_float64(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_BIGINT64:
    return Load_bigint64(typedObj, offset | 0);

  case JS_SCALARTYPEREPR_BIGUINT64:
    return Load_biguint64(typedObj, offset | 0);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

function TypedObjectGetReference(descr, typedObj, offset) {
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_REFERENCETYPEREPR_ANY:
    return Load_Any(typedObj, offset | 0);

  case JS_REFERENCETYPEREPR_OBJECT:
    return Load_Object(typedObj, offset | 0);

  case JS_REFERENCETYPEREPR_WASM_ANYREF:
    var boxed = Load_WasmAnyRef(typedObj, offset | 0);
    if (!IsBoxedWasmAnyRef(boxed))
      return boxed;
    return UnboxBoxedWasmAnyRef(boxed);

  case JS_REFERENCETYPEREPR_STRING:
    return Load_string(typedObj, offset | 0);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

///////////////////////////////////////////////////////////////////////////
// Setting values
//
// The methods in this section modify the data pointed at by `this`.

// Writes `fromValue` into the `typedObj` at offset `offset`, adapting
// it to `descr` as needed. This is the most general entry point
// and works for any type.
function TypedObjectSet(descr, typedObj, offset, name, fromValue) {
  switch (DESCR_KIND(descr)) {
  case JS_TYPEREPR_SCALAR_KIND:
    TypedObjectSetScalar(descr, typedObj, offset, fromValue);
    return;

  case JS_TYPEREPR_REFERENCE_KIND:
    TypedObjectSetReference(descr, typedObj, offset, name, fromValue);
    return;

  case JS_TYPEREPR_ARRAY_KIND:
    var length = DESCR_ARRAY_LENGTH(descr);
    if (TypedObjectSetArray(descr, length, typedObj, offset, fromValue))
      return;
    break;

  case JS_TYPEREPR_STRUCT_KIND:
    if (!IsObject(fromValue))
      break;

    // Adapt each field.
    var fieldNames = DESCR_STRUCT_FIELD_NAMES(descr);
    var fieldDescrs = DESCR_STRUCT_FIELD_TYPES(descr);
    var fieldOffsets = DESCR_STRUCT_FIELD_OFFSETS(descr);
    for (var i = 0; i < fieldNames.length; i++) {
      var fieldName = fieldNames[i];
      var fieldDescr = fieldDescrs[i];
      var fieldOffset = fieldOffsets[i];
      var fieldValue = fromValue[fieldName];
      TypedObjectSet(fieldDescr, typedObj, offset + fieldOffset, fieldName, fieldValue);
    }
    return;
  }

  ThrowTypeError(JSMSG_CANT_CONVERT_TO,
                 typeof(fromValue),
                 DESCR_STRING_REPR(descr));
}

function TypedObjectSetArray(descr, length, typedObj, offset, fromValue) {
  if (!IsObject(fromValue))
    return false;

  // Check that "array-like" fromValue has an appropriate length.
  if (fromValue.length !== length)
    return false;

  // Adapt each element.
  if (length > 0) {
    var elemDescr = DESCR_ARRAY_ELEMENT_TYPE(descr);
    var elemSize = DESCR_SIZE(elemDescr);
    var elemOffset = offset;
    for (var i = 0; i < length; i++) {
      TypedObjectSet(elemDescr, typedObj, elemOffset, null, fromValue[i]);
      elemOffset += elemSize;
    }
  }
  return true;
}

// Sets `fromValue` to `this` assuming that `this` is a scalar type.
function TypedObjectSetScalar(descr, typedObj, offset, fromValue) {
  assert(DESCR_KIND(descr) === JS_TYPEREPR_SCALAR_KIND,
         "Expected scalar type descriptor");
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_SCALARTYPEREPR_INT8:
    return Store_int8(typedObj, offset | 0,
                      TO_INT32(fromValue) & 0xFF);

  case JS_SCALARTYPEREPR_UINT8:
    return Store_uint8(typedObj, offset | 0,
                       TO_UINT32(fromValue) & 0xFF);

  case JS_SCALARTYPEREPR_UINT8_CLAMPED:
    var v = ClampToUint8(+fromValue);
    return Store_int8(typedObj, offset | 0, v);

  case JS_SCALARTYPEREPR_INT16:
    return Store_int16(typedObj, offset | 0,
                       TO_INT32(fromValue) & 0xFFFF);

  case JS_SCALARTYPEREPR_UINT16:
    return Store_uint16(typedObj, offset | 0,
                        TO_UINT32(fromValue) & 0xFFFF);

  case JS_SCALARTYPEREPR_INT32:
    return Store_int32(typedObj, offset | 0,
                       TO_INT32(fromValue));

  case JS_SCALARTYPEREPR_UINT32:
    return Store_uint32(typedObj, offset | 0,
                        TO_UINT32(fromValue));

  case JS_SCALARTYPEREPR_FLOAT32:
    return Store_float32(typedObj, offset | 0, +fromValue);

  case JS_SCALARTYPEREPR_FLOAT64:
    return Store_float64(typedObj, offset | 0, +fromValue);

  case JS_SCALARTYPEREPR_BIGINT64:
    return Store_bigint64(typedObj, offset | 0, fromValue);

  case JS_SCALARTYPEREPR_BIGUINT64:
    return Store_biguint64(typedObj, offset | 0, fromValue);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

function TypedObjectSetReference(descr, typedObj, offset, name, fromValue) {
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_REFERENCETYPEREPR_ANY:
    return Store_Any(typedObj, offset | 0, name, fromValue);

  case JS_REFERENCETYPEREPR_OBJECT:
    var value = (fromValue === null ? fromValue : ToObject(fromValue));
    return Store_Object(typedObj, offset | 0, name, value);

  case JS_REFERENCETYPEREPR_WASM_ANYREF:
    var value = (IsBoxableWasmAnyRef(fromValue) ? BoxWasmAnyRef(fromValue) : fromValue);
    return Store_WasmAnyRef(typedObj, offset | 0, name, value);

  case JS_REFERENCETYPEREPR_STRING:
    return Store_string(typedObj, offset | 0, name, ToString(fromValue));
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

// Sets `fromValue` to `this` assuming that `this` is a scalar type.
///////////////////////////////////////////////////////////////////////////
// C++ Wrappers
//
// These helpers are invoked by C++ code or used as method bodies.

// Wrapper for use from C++ code.
function ConvertAndCopyTo(destDescr,
                          destTypedObj,
                          destOffset,
                          fieldName,
                          fromValue)
{
  assert(IsObject(destDescr) && ObjectIsTypeDescr(destDescr),
         "ConvertAndCopyTo: not type obj");
  assert(IsObject(destTypedObj) && ObjectIsTypedObject(destTypedObj),
         "ConvertAndCopyTo: not type typedObj");

  TypedObjectSet(destDescr, destTypedObj, destOffset, fieldName, fromValue);
}

// Wrapper for use from C++ code.
function Reify(sourceDescr,
               sourceTypedObj,
               sourceOffset) {
  assert(IsObject(sourceDescr) && ObjectIsTypeDescr(sourceDescr),
         "Reify: not type obj");
  assert(IsObject(sourceTypedObj) && ObjectIsTypedObject(sourceTypedObj),
         "Reify: not type typedObj");

  return TypedObjectGet(sourceDescr, sourceTypedObj, sourceOffset);
}

// Warning: user exposed!
function TypeDescrEquivalent(otherDescr) {
  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  if (!IsObject(otherDescr) || !ObjectIsTypeDescr(otherDescr))
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  return DescrsEquiv(this, otherDescr);
}

// TypedObjectArray.redimension(newArrayType)
//
// Method that "repackages" the data from this array into a new typed
// object whose type is `newArrayType`. Once you strip away all the
// outer array dimensions, the type of `this` array and `newArrayType`
// must share the same innermost element type. Moreover, those
// stripped away dimensions must amount to the same total number of
// elements.
//
// For example, given two equivalent types `T` and `U`, it is legal to
// interconvert between arrays types like:
//     T[32]
//     U[2][16]
//     U[2][2][8]
// Because they all share the same total number (32) of equivalent elements.
// But it would be illegal to convert `T[32]` to `U[31]` or `U[2][17]`, since
// the number of elements differs. And it's just plain incompatible to convert
// if the base element types are not equivalent.
//
// Warning: user exposed!
function TypedObjectArrayRedimension(newArrayType) {
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (!IsObject(newArrayType) || !ObjectIsTypeDescr(newArrayType))
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // Peel away the outermost array layers from the type of `this` to find
  // the core element type. In the process, count the number of elements.
  var oldArrayType = TypedObjectTypeDescr(this);
  var oldElementType = oldArrayType;
  var oldElementCount = 1;

  if (DESCR_KIND(oldArrayType) != JS_TYPEREPR_ARRAY_KIND)
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  while (DESCR_KIND(oldElementType) === JS_TYPEREPR_ARRAY_KIND) {
    oldElementCount *= oldElementType.length;
    oldElementType = oldElementType.elementType;
  }

  // Peel away the outermost array layers from `newArrayType`. In the
  // process, count the number of elements.
  var newElementType = newArrayType;
  var newElementCount = 1;
  while (DESCR_KIND(newElementType) == JS_TYPEREPR_ARRAY_KIND) {
    newElementCount *= newElementType.length;
    newElementType = newElementType.elementType;
  }

  // Check that the total number of elements does not change.
  if (oldElementCount !== newElementCount) {
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }

  // Check that the element types are equivalent.
  if (!DescrsEquiv(oldElementType, newElementType)) {
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }

  // Together, this should imply that the sizes are unchanged.
  assert(DESCR_SIZE(oldArrayType) == DESCR_SIZE(newArrayType),
         "Byte sizes should be equal");

  // Rewrap the data from `this` in a new type.
  return NewDerivedTypedObject(newArrayType, this, 0);
}

///////////////////////////////////////////////////////////////////////////
// Miscellaneous

function DescrsEquiv(descr1, descr2) {
  assert(IsObject(descr1) && ObjectIsTypeDescr(descr1), "descr1 not descr");
  assert(IsObject(descr2) && ObjectIsTypeDescr(descr2), "descr2 not descr");

  // Potential optimization: these two strings are guaranteed to be
  // atoms, and hence this string comparison can just be a pointer
  // comparison.  However, I don't think ion knows that. If this ever
  // becomes a bottleneck, we can add a intrinsic at some point that
  // is treated specially by Ion.  (Bug 976688)

  return DESCR_STRING_REPR(descr1) === DESCR_STRING_REPR(descr2);
}

// toSource() for type descriptors.
//
// Warning: user exposed!
function DescrToSource() {
  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "Type", "toSource", "value");

  return DESCR_STRING_REPR(this);
}

// Warning: user exposed!
function ArrayShorthand(...dims) {
  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var AT = GetTypedObjectModule().ArrayType;

  if (dims.length == 0)
    ThrowTypeError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var accum = this;
  for (var i = dims.length - 1; i >= 0; i--)
    accum = new AT(accum, dims[i]);
  return accum;
}

// This is the `objectType()` function defined in the spec.
// It returns the type of its argument.
//
// Warning: user exposed!
function TypeOfTypedObject(obj) {
  if (IsObject(obj) && ObjectIsTypedObject(obj))
    return TypedObjectTypeDescr(obj);

  // Note: Do not create bindings for `Any`, `String`, etc in
  // Utilities.js, but rather access them through
  // `GetTypedObjectModule()`. The reason is that bindings
  // you create in Utilities.js are part of the self-hosted global,
  // vs the user-accessible global, and hence should not escape to
  // user script.
  var T = GetTypedObjectModule();
  switch (typeof obj) {
    case "object": return T.Object;
    case "function": return T.Object;
    case "string": return T.String;
    case "number": return T.float64;
    case "undefined": return T.Any;
    default: return T.Any;
  }
}
