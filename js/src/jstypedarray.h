/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla WebGL impl
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jstypedarray_h
#define jstypedarray_h

#include "jsapi.h"
#include "jsclass.h"
#include "jsobj.h"

#include "gc/Barrier.h"

typedef struct JSProperty JSProperty;

namespace js {

/*
 * ArrayBufferObject
 *
 * This class holds the underlying raw buffer that the various ArrayBufferView
 * subclasses (DataView and the TypedArrays) access. It can be created
 * explicitly and passed to an ArrayBufferView subclass, or can be created
 * implicitly by constructing a TypedArray with a size.
 */
class ArrayBufferObject : public JSObject
{
  public:
    static Class protoClass;
    static JSPropertySpec jsprops[];
    static JSFunctionSpec jsfuncs[];

    static JSBool prop_getByteLength(JSContext *cx, JSObject *obj, jsid id, Value *vp);

    static JSBool fun_slice(JSContext *cx, unsigned argc, Value *vp);

    static JSBool class_constructor(JSContext *cx, unsigned argc, Value *vp);

    static JSObject *create(JSContext *cx, uint32_t nbytes, uint8_t *contents = NULL);

    static JSObject *createSlice(JSContext *cx, ArrayBufferObject &arrayBuffer,
                                 uint32_t begin, uint32_t end);

    static void
    obj_trace(JSTracer *trc, JSObject *obj);

    static JSBool
    obj_lookupGeneric(JSContext *cx, JSObject *obj, jsid id,
                      JSObject **objp, JSProperty **propp);
    static JSBool
    obj_lookupProperty(JSContext *cx, JSObject *obj, PropertyName *name,
                       JSObject **objp, JSProperty **propp);
    static JSBool
    obj_lookupElement(JSContext *cx, JSObject *obj, uint32_t index,
                      JSObject **objp, JSProperty **propp);
    static JSBool
    obj_lookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid, JSObject **objp,
                      JSProperty **propp);

    static JSBool
    obj_defineGeneric(JSContext *cx, JSObject *obj, jsid id, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool
    obj_defineProperty(JSContext *cx, JSObject *obj, PropertyName *name, const Value *v,
                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool
    obj_defineElement(JSContext *cx, JSObject *obj, uint32_t index, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool
    obj_defineSpecial(JSContext *cx, JSObject *obj, SpecialId sid, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static JSBool
    obj_getGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp);

    static JSBool
    obj_getProperty(JSContext *cx, JSObject *obj, JSObject *receiver, PropertyName *name,
                    Value *vp);

    static JSBool
    obj_getElement(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index, Value *vp);
    static JSBool
    obj_getElementIfPresent(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index,
                            Value *vp, bool *present);

    static JSBool
    obj_getSpecial(JSContext *cx, JSObject *obj, JSObject *receiver, SpecialId sid, Value *vp);

    static JSBool
    obj_setGeneric(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict);
    static JSBool
    obj_setProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *vp, JSBool strict);
    static JSBool
    obj_setElement(JSContext *cx, JSObject *obj, uint32_t index, Value *vp, JSBool strict);
    static JSBool
    obj_setSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *vp, JSBool strict);

    static JSBool
    obj_getGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp);
    static JSBool
    obj_getPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp);
    static JSBool
    obj_getElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp);
    static JSBool
    obj_getSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp);

    static JSBool
    obj_setGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp);
    static JSBool
    obj_setPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp);
    static JSBool
    obj_setElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp);
    static JSBool
    obj_setSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp);

    static JSBool
    obj_deleteProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *rval, JSBool strict);
    static JSBool
    obj_deleteElement(JSContext *cx, JSObject *obj, uint32_t index, Value *rval, JSBool strict);
    static JSBool
    obj_deleteSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *rval, JSBool strict);

    static JSBool
    obj_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                  Value *statep, jsid *idp);

    static JSType
    obj_typeOf(JSContext *cx, JSObject *obj);

    bool
    allocateSlots(JSContext *cx, uint32_t size, uint8_t *contents = NULL);

    inline uint32_t byteLength() const;

    inline uint8_t * dataPointer() const;

   /*
     * Check if the arrayBuffer contains any data. This will return false for
     * ArrayBuffer.prototype and neutered ArrayBuffers.
     */
    inline bool hasData() const;
};

/*
 * TypedArray
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

struct TypedArray {
    enum {
        TYPE_INT8 = 0,
        TYPE_UINT8,
        TYPE_INT16,
        TYPE_UINT16,
        TYPE_INT32,
        TYPE_UINT32,
        TYPE_FLOAT32,
        TYPE_FLOAT64,

        /*
         * Special type that's a uint8, but assignments are clamped to 0 .. 255.
         * Treat the raw data type as a uint8.
         */
        TYPE_UINT8_CLAMPED,

        TYPE_MAX
    };

    enum {
        /* Properties of the typed array stored in reserved slots. */
        FIELD_LENGTH = 0,
        FIELD_BYTEOFFSET,
        FIELD_BYTELENGTH,
        FIELD_TYPE,
        FIELD_BUFFER,
        FIELD_MAX,
        NUM_FIXED_SLOTS = 7
    };

    // and MUST NOT be used to construct new objects.
    static Class classes[TYPE_MAX];

    // These are the proto/original classes, used
    // fo constructing new objects
    static Class protoClasses[TYPE_MAX];

    static JSPropertySpec jsprops[];

    static JSBool prop_getBuffer(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static JSBool prop_getByteOffset(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static JSBool prop_getByteLength(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static JSBool prop_getLength(JSContext *cx, JSObject *obj, jsid id, Value *vp);

    static JSBool obj_lookupGeneric(JSContext *cx, JSObject *obj, jsid id,
                                    JSObject **objp, JSProperty **propp);
    static JSBool obj_lookupProperty(JSContext *cx, JSObject *obj, PropertyName *name,
                                     JSObject **objp, JSProperty **propp);
    static JSBool obj_lookupElement(JSContext *cx, JSObject *obj, uint32_t index,
                                    JSObject **objp, JSProperty **propp);
    static JSBool obj_lookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid,
                                    JSObject **objp, JSProperty **propp);

    static JSBool obj_getGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp);
    static JSBool obj_getPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp);
    static JSBool obj_getElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp);
    static JSBool obj_getSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp);

    static JSBool obj_setGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp);
    static JSBool obj_setPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp);
    static JSBool obj_setElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp);
    static JSBool obj_setSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp);

    static uint32_t getLength(JSObject *obj);
    static uint32_t getByteOffset(JSObject *obj);
    static uint32_t getByteLength(JSObject *obj);
    static uint32_t getType(JSObject *obj);
    static ArrayBufferObject * getBuffer(JSObject *obj);
    static void * getDataOffset(JSObject *obj);

  public:
    static bool
    isArrayIndex(JSContext *cx, JSObject *obj, jsid id, uint32_t *ip = NULL);

    static inline uint32_t slotWidth(int atype) {
        switch (atype) {
          case js::TypedArray::TYPE_INT8:
          case js::TypedArray::TYPE_UINT8:
          case js::TypedArray::TYPE_UINT8_CLAMPED:
            return 1;
          case js::TypedArray::TYPE_INT16:
          case js::TypedArray::TYPE_UINT16:
            return 2;
          case js::TypedArray::TYPE_INT32:
          case js::TypedArray::TYPE_UINT32:
          case js::TypedArray::TYPE_FLOAT32:
            return 4;
          case js::TypedArray::TYPE_FLOAT64:
            return 8;
          default:
            JS_NOT_REACHED("invalid typed array type");
            return 0;
        }
    }

    static inline int slotWidth(JSObject *obj) {
        return slotWidth(getType(obj));
    }

    static int lengthOffset();
    static int dataOffset();
};

inline bool
IsTypedArrayClass(const Class *clasp)
{
    return &TypedArray::classes[0] <= clasp &&
           clasp < &TypedArray::classes[TypedArray::TYPE_MAX];
}

inline bool
IsTypedArrayProtoClass(const Class *clasp)
{
    return &TypedArray::protoClasses[0] <= clasp &&
           clasp < &TypedArray::protoClasses[TypedArray::TYPE_MAX];
}

inline bool
IsTypedArray(JSObject *obj)
{
    return IsTypedArrayClass(obj->getClass());
}

inline bool
IsTypedArrayProto(JSObject *obj)
{
    return IsTypedArrayProtoClass(obj->getClass());
}

class DataViewObject : public JSObject
{ 
    static const size_t BYTEOFFSET_SLOT = 0;
    static const size_t BYTELENGTH_SLOT = 1;
    static const size_t BUFFER_SLOT     = 2;

  public:
    static const size_t RESERVED_SLOTS  = 3;

    static JSBool prop_getBuffer(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static JSBool prop_getByteOffset(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static JSBool prop_getByteLength(JSContext *cx, JSObject *obj, jsid id, Value *vp);

    static JSBool class_constructor(JSContext *cx, unsigned argc, Value *vp);

    static inline DataViewObject *
    create(JSContext *cx, uint32_t byteOffset, uint32_t byteLength, Handle<ArrayBufferObject*> arrayBuffer);

    static JSBool fun_getInt8(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_getUint8(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_getInt16(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_getUint16(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_getInt32(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_getUint32(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_getFloat32(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_getFloat64(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_setInt8(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_setUint8(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_setInt16(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_setUint16(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_setInt32(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_setUint32(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_setFloat32(JSContext *cx, unsigned argc, Value *vp);
    static JSBool fun_setFloat64(JSContext *cx, unsigned argc, Value *vp);
    inline uint32_t byteLength();
    inline uint32_t byteOffset();
    inline JSObject & arrayBuffer();
    inline void *dataPointer();
    inline bool hasBuffer() const;
    static JSObject *initClass(JSContext *cx, GlobalObject *global);
    bool getDataPointer(JSContext *cx, CallArgs args, size_t typeSize, uint8_t **data);
    template<typename NativeType>
    bool read(JSContext *cx, CallArgs &args, NativeType *val, const char *method);
    template<typename NativeType>
    bool write(JSContext *cx, CallArgs &args, const char *method);
  private:
    static JSPropertySpec jsprops[];
    static JSFunctionSpec jsfuncs[];
};

bool
IsDataView(JSObject *obj);

} // namespace js

#endif /* jstypedarray_h */
