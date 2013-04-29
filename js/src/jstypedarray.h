/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
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

typedef Vector<ArrayBufferObject *, 0, SystemAllocPolicy> ArrayBufferVector;

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
    static const JSFunctionSpec jsfuncs[];

    static JSBool byteLengthGetter(JSContext *cx, unsigned argc, Value *vp);

    static JSBool fun_slice(JSContext *cx, unsigned argc, Value *vp);

    static JSBool class_constructor(JSContext *cx, unsigned argc, Value *vp);

    static JSObject *create(JSContext *cx, uint32_t nbytes, uint8_t *contents = NULL);

    static JSObject *createSlice(JSContext *cx, ArrayBufferObject &arrayBuffer,
                                 uint32_t begin, uint32_t end);

    static bool createDataViewForThisImpl(JSContext *cx, CallArgs args);
    static JSBool createDataViewForThis(JSContext *cx, unsigned argc, Value *vp);

    template<typename T>
    static bool createTypedArrayFromBufferImpl(JSContext *cx, CallArgs args);

    template<typename T>
    static JSBool createTypedArrayFromBuffer(JSContext *cx, unsigned argc, Value *vp);

    static void obj_trace(JSTracer *trc, RawObject obj);

    static JSBool obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                    MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                     MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                    MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                    MutableHandleObject objp, MutableHandleShape propp);

    static JSBool obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool obj_defineProperty(JSContext *cx, HandleObject obj,
                                     HandlePropertyName name, HandleValue v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool obj_defineSpecial(JSContext *cx, HandleObject obj,
                                    HandleSpecialId sid, HandleValue v,
                                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static JSBool obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                                 HandleId id, MutableHandleValue vp);

    static JSBool obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                                  HandlePropertyName name, MutableHandleValue vp);

    static JSBool obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                                 uint32_t index, MutableHandleValue vp);
    static JSBool obj_getElementIfPresent(JSContext *cx, HandleObject obj, HandleObject receiver,
                                          uint32_t index, MutableHandleValue vp, bool *present);

    static JSBool obj_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver,
                                 HandleSpecialId sid, MutableHandleValue vp);

    static JSBool obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                 MutableHandleValue vp, JSBool strict);
    static JSBool obj_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                  MutableHandleValue vp, JSBool strict);
    static JSBool obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                                 MutableHandleValue vp, JSBool strict);
    static JSBool obj_setSpecial(JSContext *cx, HandleObject obj,
                                 HandleSpecialId sid, MutableHandleValue vp, JSBool strict);

    static JSBool obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                           HandleId id, unsigned *attrsp);
    static JSBool obj_getPropertyAttributes(JSContext *cx, HandleObject obj,
                                            HandlePropertyName name, unsigned *attrsp);
    static JSBool obj_getElementAttributes(JSContext *cx, HandleObject obj,
                                           uint32_t index, unsigned *attrsp);
    static JSBool obj_getSpecialAttributes(JSContext *cx, HandleObject obj,
                                           HandleSpecialId sid, unsigned *attrsp);

    static JSBool obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                           HandleId id, unsigned *attrsp);
    static JSBool obj_setPropertyAttributes(JSContext *cx, HandleObject obj,
                                            HandlePropertyName name, unsigned *attrsp);
    static JSBool obj_setElementAttributes(JSContext *cx, HandleObject obj,
                                           uint32_t index, unsigned *attrsp);
    static JSBool obj_setSpecialAttributes(JSContext *cx, HandleObject obj,
                                           HandleSpecialId sid, unsigned *attrsp);

    static JSBool obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                     JSBool *succeeded);
    static JSBool obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                                    JSBool *succeeded);
    static JSBool obj_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                    JSBool *succeeded);

    static JSBool obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                                MutableHandleValue statep, MutableHandleId idp);

    static void sweep(JSCompartment *rt);

    static void resetArrayBufferList(JSCompartment *rt);
    static bool saveArrayBufferList(JSCompartment *c, ArrayBufferVector &vector);
    static void restoreArrayBufferLists(ArrayBufferVector &vector);

    static bool stealContents(JSContext *cx, JSObject *obj, void **contents,
                              uint8_t **data);

    static inline void setElementsHeader(js::ObjectElements *header, uint32_t bytes);

    void addView(RawObject view);

    bool allocateSlots(JSContext *cx, uint32_t size, uint8_t *contents = NULL);
    void changeContents(JSContext *cx, ObjectElements *newHeader);

    /*
     * Ensure that the data is not stored inline. Used when handing back a
     * GC-safe pointer.
     */
    bool uninlineData(JSContext *cx);

    inline uint32_t byteLength() const;

    inline uint8_t * dataPointer() const;

   /*
     * Check if the arrayBuffer contains any data. This will return false for
     * ArrayBuffer.prototype and neutered ArrayBuffers.
     */
    inline bool hasData() const;

    inline bool isAsmJSArrayBuffer() const;
    static bool prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer);
    static void neuterAsmJSArrayBuffer(ArrayBufferObject &buffer);
    static void releaseAsmJSArrayBuffer(FreeOp *fop, RawObject obj);
};

/*
 * BufferView
 *
 * Common definitions shared by all ArrayBufferViews. (The name ArrayBufferView
 * is currently being used for a namespace in jsfriendapi.h.)
 */

struct BufferView {
    /* Offset of view in underlying ArrayBuffer */
    static const size_t BYTEOFFSET_SLOT  = 0;

    /* Byte length of view */
    static const size_t BYTELENGTH_SLOT  = 1;

    /* Underlying ArrayBuffer */
    static const size_t BUFFER_SLOT      = 2;

    /* ArrayBuffers point to a linked list of views, chained through this slot */
    static const size_t NEXT_VIEW_SLOT   = 3;

    /*
     * When ArrayBuffers are traced during GC, they construct a linked list of
     * ArrayBuffers with more than one view, chained through this slot of the
     * first view of each ArrayBuffer
     */
    static const size_t NEXT_BUFFER_SLOT = 4;

    static const size_t NUM_SLOTS        = 5;
};

/*
 * TypedArray
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

struct TypedArray : public BufferView {
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
         * Special type that's a uint8_t, but assignments are clamped to 0 .. 255.
         * Treat the raw data type as a uint8_t.
         */
        TYPE_UINT8_CLAMPED,

        TYPE_MAX
    };

    /*
     * Typed array properties stored in slots, beyond those shared by all
     * ArrayBufferViews.
     */
    static const size_t LENGTH_SLOT     = BufferView::NUM_SLOTS;
    static const size_t TYPE_SLOT       = BufferView::NUM_SLOTS + 1;
    static const size_t RESERVED_SLOTS  = BufferView::NUM_SLOTS + 2;
    static const size_t DATA_SLOT       = 7; // private slot, based on alloc kind

    static Class classes[TYPE_MAX];
    static Class protoClasses[TYPE_MAX];

    static JSBool obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                    MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                     MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                    MutableHandleObject objp, MutableHandleShape propp);
    static JSBool obj_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                    MutableHandleObject objp, MutableHandleShape propp);

    static JSBool obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                           HandleId id, unsigned *attrsp);
    static JSBool obj_getPropertyAttributes(JSContext *cx, HandleObject obj,
                                            HandlePropertyName name, unsigned *attrsp);
    static JSBool obj_getElementAttributes(JSContext *cx, HandleObject obj,
                                           uint32_t index, unsigned *attrsp);
    static JSBool obj_getSpecialAttributes(JSContext *cx, HandleObject obj,
                                           HandleSpecialId sid, unsigned *attrsp);

    static JSBool obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                           HandleId id, unsigned *attrsp);
    static JSBool obj_setPropertyAttributes(JSContext *cx, HandleObject obj,
                                            HandlePropertyName name, unsigned *attrsp);
    static JSBool obj_setElementAttributes(JSContext *cx, HandleObject obj,
                                           uint32_t index, unsigned *attrsp);
    static JSBool obj_setSpecialAttributes(JSContext *cx, HandleObject obj,
                                           HandleSpecialId sid, unsigned *attrsp);

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
    static bool isArrayIndex(JSObject *obj, jsid id, uint32_t *ip = NULL);

    static void neuter(RawObject tarray);

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

bool
IsTypedArrayConstructor(const Value &v, uint32_t type);

bool
IsTypedArrayBuffer(const Value &v);

static inline unsigned
TypedArrayShift(ArrayBufferView::ViewType viewType)
{
    switch (viewType) {
      case ArrayBufferView::TYPE_INT8:
      case ArrayBufferView::TYPE_UINT8:
      case ArrayBufferView::TYPE_UINT8_CLAMPED:
        return 0;
      case ArrayBufferView::TYPE_INT16:
      case ArrayBufferView::TYPE_UINT16:
        return 1;
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT32:
      case ArrayBufferView::TYPE_FLOAT32:
        return 2;
      case ArrayBufferView::TYPE_FLOAT64:
        return 3;
      default:;
    }
    JS_NOT_REACHED("Unexpected array type");
    return 0;
}

class DataViewObject : public JSObject, public BufferView
{
public:

private:
    static Class protoClass;

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
    static const size_t RESERVED_SLOTS = BufferView::NUM_SLOTS;
    static const size_t DATA_SLOT       = 7; // private slot, based on alloc kind

    static inline Value bufferValue(DataViewObject &view);
    static inline Value byteOffsetValue(DataViewObject &view);
    static inline Value byteLengthValue(DataViewObject &view);

    static JSBool class_constructor(JSContext *cx, unsigned argc, Value *vp);
    static JSBool constructWithProto(JSContext *cx, unsigned argc, Value *vp);
    static JSBool construct(JSContext *cx, JSObject *bufobj, const CallArgs &args,
                            HandleObject proto);

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
    inline ArrayBufferObject & arrayBuffer();
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
    static const JSFunctionSpec jsfuncs[];
};

bool
IsDataView(JSObject *obj);

} // namespace js

#endif /* jstypedarray_h */
