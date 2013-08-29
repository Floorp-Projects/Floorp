/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_BinaryData_h
#define builtin_BinaryData_h

#include "jsapi.h"
#include "jsobj.h"

#include "builtin/TypeRepresentation.h"

namespace js {

// Slots common to all type descriptors:
enum TypeCommonSlots {
    // Canonical type representation of this type (see TypeRepresentation.h).
    SLOT_TYPE_REPR=0,
    TYPE_RESERVED_SLOTS
};

// Slots for ArrayType type descriptors:
enum ArrayTypeCommonSlots {
    // Type descriptor for the element type.
    SLOT_ARRAY_ELEM_TYPE = TYPE_RESERVED_SLOTS,
    ARRAY_TYPE_RESERVED_SLOTS
};

// Slots for StructType type descriptors:
enum StructTypeCommonSlots {
    // JS array containing type descriptors for each field, in order.
    SLOT_STRUCT_FIELD_TYPES = TYPE_RESERVED_SLOTS,
    STRUCT_TYPE_RESERVED_SLOTS
};

// Slots for data blocks:
enum BlockCommonSlots {
    // The type descriptor with which this block is associated.
    SLOT_DATATYPE = 0,

    // If this value is NULL, then the block instance owns the
    // uint8_t* in its priate data. Otherwise, this field contains the
    // owner, and thus keeps the owner alive.
    SLOT_BLOCKREFOWNER,

    BLOCK_RESERVED_SLOTS
};

extern Class DataClass;

extern Class TypeClass;

template <ScalarTypeRepresentation::Type type, typename T>
class NumericType
{
  private:
    static Class * typeToClass();
  public:
    static bool convert(JSContext *cx, HandleValue val, T* converted);
    static bool reify(JSContext *cx, void *mem, MutableHandleValue vp);
    static bool call(JSContext *cx, unsigned argc, Value *vp);
};

extern Class NumericTypeClasses[ScalarTypeRepresentation::TYPE_MAX];

/*
 * Type descriptor created by `new ArrayType(...)`
 */
class ArrayType : public JSObject
{
  private:
  public:
    static Class class_;

    static JSObject *create(JSContext *cx, HandleObject arrayTypeGlobal,
                            HandleObject elementType, size_t length);
    static bool construct(JSContext *cx, unsigned int argc, jsval *vp);
    static bool repeat(JSContext *cx, unsigned int argc, jsval *vp);

    static bool toSource(JSContext *cx, unsigned int argc, jsval *vp);

    static JSObject *elementType(JSContext *cx, HandleObject obj);
};

/*
 * Type descriptor created by `new StructType(...)`
 */
class StructType : public JSObject
{
  private:
    static JSObject *create(JSContext *cx, HandleObject structTypeGlobal,
                            HandleObject fields);
    /**
     * Sets up structType slots based on calculated memory size
     * and alignment and stores fieldmap as well.
     */
    static bool layout(JSContext *cx, HandleObject structType,
                       HandleObject fields);

  public:
    static Class class_;

    static bool toSource(JSContext *cx, unsigned int argc, jsval *vp);

    static bool convertAndCopyTo(JSContext *cx,
                                 StructTypeRepresentation *typeRepr,
                                 HandleValue from, uint8_t *mem);

    static bool construct(JSContext *cx, unsigned int argc, jsval *vp);
};

/* Binary data objects and handles */
class BinaryBlock
{
  private:
    // Creates a binary data object of the given type and class, but with
    // a NULL memory pointer. Caller must use setPrivate() to set the
    // memory pointer properly.
    static JSObject *createNull(JSContext *cx, HandleObject type,
                                HandleValue owner);

    static void obj_finalize(js::FreeOp *op, JSObject *obj);

    static void obj_trace(JSTracer *trace, JSObject *object);

    static bool obj_lookupGeneric(JSContext *cx, HandleObject obj,
                                    HandleId id, MutableHandleObject objp,
                                    MutableHandleShape propp);

    static bool obj_lookupProperty(JSContext *cx, HandleObject obj,
                                     HandlePropertyName name,
                                     MutableHandleObject objp,
                                     MutableHandleShape propp);

    static bool obj_lookupElement(JSContext *cx, HandleObject obj,
                                    uint32_t index, MutableHandleObject objp,
                                    MutableHandleShape propp);

    static bool obj_lookupSpecial(JSContext *cx, HandleObject obj,
                                    HandleSpecialId sid,
                                    MutableHandleObject objp,
                                    MutableHandleShape propp);

    static bool obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineProperty(JSContext *cx, HandleObject obj,
                                     HandlePropertyName name, HandleValue v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue v,
                                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                                 HandleId id, MutableHandleValue vp);

    static bool obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                                  HandlePropertyName name, MutableHandleValue vp);

    static bool obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                                 uint32_t index, MutableHandleValue vp);

    static bool obj_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver,
                                 HandleSpecialId sid, MutableHandleValue vp);

    static bool obj_getElementIfPresent(JSContext *cx, HandleObject obj,
                                          HandleObject receiver, uint32_t index,
                                          MutableHandleValue vp, bool *present);
    static bool obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                 MutableHandleValue vp, bool strict);
    static bool obj_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                  MutableHandleValue vp, bool strict);
    static bool obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                                 MutableHandleValue vp, bool strict);
    static bool obj_setSpecial(JSContext *cx, HandleObject obj,
                                 HandleSpecialId sid, MutableHandleValue vp, bool strict);

    static bool obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                           HandleId id, unsigned *attrsp);
    static bool obj_getPropertyAttributes(JSContext *cx, HandleObject obj,
                                            HandlePropertyName name, unsigned *attrsp);
    static bool obj_getElementAttributes(JSContext *cx, HandleObject obj,
                                           uint32_t index, unsigned *attrsp);
    static bool obj_getSpecialAttributes(JSContext *cx, HandleObject obj,
                                           HandleSpecialId sid, unsigned *attrsp);

    static bool obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                           HandleId id, unsigned *attrsp);
    static bool obj_setPropertyAttributes(JSContext *cx, HandleObject obj,
                                            HandlePropertyName name, unsigned *attrsp);
    static bool obj_setElementAttributes(JSContext *cx, HandleObject obj,
                                           uint32_t index, unsigned *attrsp);
    static bool obj_setSpecialAttributes(JSContext *cx, HandleObject obj,
                                           HandleSpecialId sid, unsigned *attrsp);

    static bool obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                     bool *succeeded);
    static bool obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                                    bool *succeeded);
    static bool obj_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                    bool *succeeded);

    static bool obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                                MutableHandleValue statep, MutableHandleId idp);

  public:
    static Class class_;

    static bool isBlock(HandleObject val);
    static uint8_t *mem(HandleObject val);

    // creates zeroed memory of size of type
    static JSObject *createZeroed(JSContext *cx, HandleObject type);

    // creates a block that aliases the memory owned by `owner` at the
    // given offset
    static JSObject *createDerived(JSContext *cx, HandleObject type,
                                   HandleObject owner, size_t offset);

    // user-accessible constructor (`new TypeDescriptor(...)`)
    static bool construct(JSContext *cx, unsigned int argc, jsval *vp);
};


} // namespace js

#endif /* builtin_BinaryData_h */
