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
  }

  assert(false, "Unhandled kind: " + DESCR_KIND(descr));
  return undefined;
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

// Sets `fromValue` to `this` assuming that `this` is a scalar type.
///////////////////////////////////////////////////////////////////////////
// C++ Wrappers
//
// These helpers are invoked by C++ code or used as method bodies.

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
