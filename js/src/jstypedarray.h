/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    static bool byteLengthGetterImpl(JSContext *cx, CallArgs args);
    static bool fun_slice_impl(JSContext *cx, CallArgs args);

  public:
    static Class protoClass;
    static JSFunctionSpec jsfuncs[];

    static JSBool byteLengthGetter(JSContext *cx, unsigned argc, Value *vp);

    static JSBool fun_slice(JSContext *cx, unsigned argc, Value *vp);

    static JSBool class_constructor(JSContext *cx, unsigned argc, Value *vp);

    static JSObject *create(JSContext *cx, uint32_t nbytes, uint8_t *contents = NULL);

    static JSObject *createSlice(JSContext *cx, ArrayBufferObject &arrayBuffer,
                                 uint32_t begin, uint32_t end);

    static bool createDataViewForThisImpl(JSContext *cx, CallArgs args);
    static JSBool createDataViewForThis(JSContext *cx, unsigned argc, Value *vp);

    template<typename T>
    static bool
    createTypedArrayFromBufferImpl(JSContext *cx, CallArgs args);

    template<typename T>
    static JSBool
    createTypedArrayFromBuffer(JSContext *cx, unsigned argc, Value *vp);

    static void
    obj_trace(JSTracer *trc, JSObject *obj);

    static JSBool
    obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                      MutableHandleObject objp, MutableHandleShape propp);
    static JSBool
    obj_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                       MutableHandleObject objp, MutableHandleShape propp);
    static JSBool
    obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                      MutableHandleObject objp, MutableHandleShape propp);
    static JSBool
    obj_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                      MutableHandleObject objp, MutableHandleShape propp);

    static JSBool
    obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool
    obj_defineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, const Value *v,
                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool
    obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool
    obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static JSBool
    obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id, Value *vp);

    static JSBool
    obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                    Value *vp);

    static JSBool
    obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index, Value *vp);
    static JSBool
    obj_getElementIfPresent(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                            Value *vp, bool *present);

    static JSBool
    obj_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid, Value *vp);

    static JSBool
    obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id, Value *vp, JSBool strict);
    static JSBool
    obj_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *vp, JSBool strict);
    static JSBool
    obj_setElement(JSContext *cx, HandleObject obj, uint32_t index, Value *vp, JSBool strict);
    static JSBool
    obj_setSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *vp, JSBool strict);

    static JSBool
    obj_getGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp);
    static JSBool
    obj_getPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp);
    static JSBool
    obj_getElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp);
    static JSBool
    obj_getSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp);

    static JSBool
    obj_setGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp);
    static JSBool
    obj_setPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp);
    static JSBool
    obj_setElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp);
    static JSBool
    obj_setSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp);

    static JSBool
    obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *rval, JSBool strict);
    static JSBool
    obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index, Value *rval, JSBool strict);
    static JSBool
    obj_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *rval, JSBool strict);

    static JSBool
    obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                  Value *statep, jsid *idp);

    static JSType
    obj_typeOf(JSContext *cx, HandleObject obj);

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

    static JSBool obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                    MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                     MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                    MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                    MutableHandleObject objp, MutableHandleShape propp);

    static JSBool obj_getGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp);
    static JSBool obj_getPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp);
    static JSBool obj_getElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp);
    static JSBool obj_getSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp);

    static JSBool obj_setGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp);
    static JSBool obj_setPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp);
    static JSBool obj_setElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp);
    static JSBool obj_setSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp);

    static inline Value bufferValue(JSObject *obj);
    static inline Value byteOffsetValue(JSObject *obj);
    static inline Value byteLengthValue(JSObject *obj);
    static inline Value lengthValue(JSObject *obj);

    static inline ArrayBufferObject * buffer(JSObject *obj);
    static inline uint32_t byteOffset(JSObject *obj);
    static inline uint32_t byteLength(JSObject *obj);
    static inline uint32_t length(JSObject *obj);

    static inline uint32_t type(JSObject *obj);
    static inline void * viewData(JSObject *obj);

  public:
    static bool
    isArrayIndex(JSContext *cx, JSObject *obj, jsid id, uint32_t *ip = NULL);

    static inline uint32_t slotWidth(int atype);
    static inline int slotWidth(JSObject *obj);

    /*
     * Byte length above which created typed arrays and data views will have
     * singleton types regardless of the context in which they are created.
     */
    static const uint32_t SINGLETON_TYPE_BYTE_LENGTH = 1024 * 1024 * 10;

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
    static Class protoClass;

    static const size_t BYTEOFFSET_SLOT = 0;
    static const size_t BYTELENGTH_SLOT = 1;
    static const size_t BUFFER_SLOT     = 2;

    static inline bool is(const Value &v);

    template<Value ValueGetter(DataViewObject &view)>
    static bool
    getterImpl(JSContext *cx, CallArgs args);

    template<Value ValueGetter(DataViewObject &view)>
    static JSBool
    getter(JSContext *cx, unsigned argc, Value *vp);

    template<Value ValueGetter(DataViewObject &view)>
    static bool
    defineGetter(JSContext *cx, PropertyName *name, HandleObject proto);

  public:
    static const size_t RESERVED_SLOTS  = 3;

    static inline Value bufferValue(DataViewObject &view);
    static inline Value byteOffsetValue(DataViewObject &view);
    static inline Value byteLengthValue(DataViewObject &view);

    static JSBool class_constructor(JSContext *cx, unsigned argc, Value *vp);
    static JSBool constructWithProto(JSContext *cx, unsigned argc, Value *vp);
    static JSBool construct(JSContext *cx, JSObject *bufobj, const CallArgs &args, JSObject *proto);

    static inline DataViewObject *
    create(JSContext *cx, uint32_t byteOffset, uint32_t byteLength,
           Handle<ArrayBufferObject*> arrayBuffer, JSObject *proto);

    static bool getInt8Impl(JSContext *cx, CallArgs args);
    static JSBool fun_getInt8(JSContext *cx, unsigned argc, Value *vp);

    static bool getUint8Impl(JSContext *cx, CallArgs args);
    static JSBool fun_getUint8(JSContext *cx, unsigned argc, Value *vp);

    static bool getInt16Impl(JSContext *cx, CallArgs args);
    static JSBool fun_getInt16(JSContext *cx, unsigned argc, Value *vp);

    static bool getUint16Impl(JSContext *cx, CallArgs args);
    static JSBool fun_getUint16(JSContext *cx, unsigned argc, Value *vp);

    static bool getInt32Impl(JSContext *cx, CallArgs args);
    static JSBool fun_getInt32(JSContext *cx, unsigned argc, Value *vp);

    static bool getUint32Impl(JSContext *cx, CallArgs args);
    static JSBool fun_getUint32(JSContext *cx, unsigned argc, Value *vp);

    static bool getFloat32Impl(JSContext *cx, CallArgs args);
    static JSBool fun_getFloat32(JSContext *cx, unsigned argc, Value *vp);

    static bool getFloat64Impl(JSContext *cx, CallArgs args);
    static JSBool fun_getFloat64(JSContext *cx, unsigned argc, Value *vp);

    static bool setInt8Impl(JSContext *cx, CallArgs args);
    static JSBool fun_setInt8(JSContext *cx, unsigned argc, Value *vp);

    static bool setUint8Impl(JSContext *cx, CallArgs args);
    static JSBool fun_setUint8(JSContext *cx, unsigned argc, Value *vp);

    static bool setInt16Impl(JSContext *cx, CallArgs args);
    static JSBool fun_setInt16(JSContext *cx, unsigned argc, Value *vp);

    static bool setUint16Impl(JSContext *cx, CallArgs args);
    static JSBool fun_setUint16(JSContext *cx, unsigned argc, Value *vp);

    static bool setInt32Impl(JSContext *cx, CallArgs args);
    static JSBool fun_setInt32(JSContext *cx, unsigned argc, Value *vp);

    static bool setUint32Impl(JSContext *cx, CallArgs args);
    static JSBool fun_setUint32(JSContext *cx, unsigned argc, Value *vp);

    static bool setFloat32Impl(JSContext *cx, CallArgs args);
    static JSBool fun_setFloat32(JSContext *cx, unsigned argc, Value *vp);

    static bool setFloat64Impl(JSContext *cx, CallArgs args);
    static JSBool fun_setFloat64(JSContext *cx, unsigned argc, Value *vp);

    inline uint32_t byteLength();
    inline uint32_t byteOffset();
    inline JSObject & arrayBuffer();
    inline void *dataPointer();
    inline bool hasBuffer() const;
    static JSObject *initClass(JSContext *cx);
    static bool getDataPointer(JSContext *cx, Handle<DataViewObject*> obj,
                               CallArgs args, size_t typeSize, uint8_t **data);
    template<typename NativeType>
    static bool read(JSContext *cx, Handle<DataViewObject*> obj,
                     CallArgs &args, NativeType *val, const char *method);
    template<typename NativeType>
    static bool write(JSContext *cx, Handle<DataViewObject*> obj,
                      CallArgs &args, const char *method);
  private:
    static JSFunctionSpec jsfuncs[];
};

bool
IsDataView(JSObject *obj);

} // namespace js

#endif /* jstypedarray_h */
