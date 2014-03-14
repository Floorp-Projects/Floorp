/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_TypedArrayObject_h
#define vm_TypedArrayObject_h

#include "jsobj.h"

#include "builtin/TypedObject.h"
#include "gc/Barrier.h"
#include "js/Class.h"
#include "vm/ArrayBufferObject.h"

typedef struct JSProperty JSProperty;

namespace js {

/*
 * TypedArrayObject
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

class TypedArrayObject : public ArrayBufferViewObject
{
  protected:
    // Typed array properties stored in slots, beyond those shared by all
    // ArrayBufferViews.
    static const size_t LENGTH_SLOT    = JS_TYPEDOBJ_SLOT_LENGTH;
    static const size_t TYPE_SLOT      = JS_TYPEDOBJ_SLOT_TYPE_DESCR;
    static const size_t RESERVED_SLOTS = JS_TYPEDOBJ_SLOTS;
    static const size_t DATA_SLOT      = JS_TYPEDOBJ_SLOT_DATA;

  public:
    static const Class classes[ScalarTypeDescr::TYPE_MAX];
    static const Class protoClasses[ScalarTypeDescr::TYPE_MAX];

    static Value bufferValue(TypedArrayObject *tarr) {
        return tarr->getFixedSlot(BUFFER_SLOT);
    }
    static Value byteOffsetValue(TypedArrayObject *tarr) {
        return tarr->getFixedSlot(BYTEOFFSET_SLOT);
    }
    static Value byteLengthValue(TypedArrayObject *tarr) {
        return tarr->getFixedSlot(BYTELENGTH_SLOT);
    }
    static Value lengthValue(TypedArrayObject *tarr) {
        return tarr->getFixedSlot(LENGTH_SLOT);
    }

    ArrayBufferObject *sharedBuffer() const;
    ArrayBufferObject *buffer() const {
        JSObject &obj = bufferValue(const_cast<TypedArrayObject*>(this)).toObject();
        if (obj.is<ArrayBufferObject>())
            return &obj.as<ArrayBufferObject>();
        return sharedBuffer();
    }
    uint32_t byteOffset() const {
        return byteOffsetValue(const_cast<TypedArrayObject*>(this)).toInt32();
    }
    uint32_t byteLength() const {
        return byteLengthValue(const_cast<TypedArrayObject*>(this)).toInt32();
    }
    uint32_t length() const {
        return lengthValue(const_cast<TypedArrayObject*>(this)).toInt32();
    }

    uint32_t type() const {
        return getFixedSlot(TYPE_SLOT).toInt32();
    }
    void *viewData() const {
        return static_cast<void*>(getPrivate(DATA_SLOT));
    }

    inline bool isArrayIndex(jsid id, uint32_t *ip = nullptr);
    Value getElement(uint32_t index);
    bool setElement(ThreadSafeContext *cx, uint32_t index, const Value &value);

    void neuter(JSContext *cx);

    static uint32_t slotWidth(int atype) {
        switch (atype) {
          case ScalarTypeDescr::TYPE_INT8:
          case ScalarTypeDescr::TYPE_UINT8:
          case ScalarTypeDescr::TYPE_UINT8_CLAMPED:
            return 1;
          case ScalarTypeDescr::TYPE_INT16:
          case ScalarTypeDescr::TYPE_UINT16:
            return 2;
          case ScalarTypeDescr::TYPE_INT32:
          case ScalarTypeDescr::TYPE_UINT32:
          case ScalarTypeDescr::TYPE_FLOAT32:
            return 4;
          case ScalarTypeDescr::TYPE_FLOAT64:
            return 8;
          default:
            MOZ_ASSUME_UNREACHABLE("invalid typed array type");
        }
    }

    int slotWidth() {
        return slotWidth(type());
    }

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
    return &TypedArrayObject::classes[0] <= clasp &&
           clasp < &TypedArrayObject::classes[ScalarTypeDescr::TYPE_MAX];
}

inline bool
IsTypedArrayProtoClass(const Class *clasp)
{
    return &TypedArrayObject::protoClasses[0] <= clasp &&
           clasp < &TypedArrayObject::protoClasses[ScalarTypeDescr::TYPE_MAX];
}

bool
IsTypedArrayConstructor(HandleValue v, uint32_t type);

bool
IsTypedArrayBuffer(HandleValue v);

ArrayBufferObject &
AsTypedArrayBuffer(HandleValue v);

// Return value is whether the string is some integer. If the string is an
// integer which is not representable as a uint64_t, the return value is true
// and the resulting index is UINT64_MAX.
bool
StringIsTypedArrayIndex(JSLinearString *str, uint64_t *indexp);

inline bool
IsTypedArrayIndex(jsid id, uint64_t *indexp)
{
    if (JSID_IS_INT(id)) {
        int32_t i = JSID_TO_INT(id);
        JS_ASSERT(i >= 0);
        *indexp = (double)i;
        return true;
    }

    if (MOZ_UNLIKELY(!JSID_IS_STRING(id)))
        return false;

    JSAtom *atom = JSID_TO_ATOM(id);

    jschar c = atom->chars()[0];
    if (!JS7_ISDEC(c) && c != '-')
        return false;

    return StringIsTypedArrayIndex(atom, indexp);
}

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
    MOZ_ASSUME_UNREACHABLE("Unexpected array type");
}

class DataViewObject : public ArrayBufferViewObject
{
    static const size_t RESERVED_SLOTS = JS_DATAVIEW_SLOTS;
    static const size_t DATA_SLOT      = JS_TYPEDOBJ_SLOT_DATA;

  private:
    static const Class protoClass;

    static bool is(HandleValue v) {
        return v.isObject() && v.toObject().hasClass(&class_);
    }

    template<Value ValueGetter(DataViewObject *view)>
    static bool
    getterImpl(JSContext *cx, CallArgs args);

    template<Value ValueGetter(DataViewObject *view)>
    static bool
    getter(JSContext *cx, unsigned argc, Value *vp);

    template<Value ValueGetter(DataViewObject *view)>
    static bool
    defineGetter(JSContext *cx, PropertyName *name, HandleObject proto);

  public:
    static const Class class_;

    static Value byteOffsetValue(DataViewObject *view) {
        Value v = view->getReservedSlot(BYTEOFFSET_SLOT);
        JS_ASSERT(v.toInt32() >= 0);
        return v;
    }

    static Value byteLengthValue(DataViewObject *view) {
        Value v = view->getReservedSlot(BYTELENGTH_SLOT);
        JS_ASSERT(v.toInt32() >= 0);
        return v;
    }

    uint32_t byteOffset() const {
        return byteOffsetValue(const_cast<DataViewObject*>(this)).toInt32();
    }

    uint32_t byteLength() const {
        return byteLengthValue(const_cast<DataViewObject*>(this)).toInt32();
    }

    bool hasBuffer() const {
        return getReservedSlot(BUFFER_SLOT).isObject();
    }

    ArrayBufferObject &arrayBuffer() const {
        return getReservedSlot(BUFFER_SLOT).toObject().as<ArrayBufferObject>();
    }

    static Value bufferValue(DataViewObject *view) {
        return view->hasBuffer() ? ObjectValue(view->arrayBuffer()) : UndefinedValue();
    }

    void *dataPointer() const {
        return getPrivate();
    }

    static bool class_constructor(JSContext *cx, unsigned argc, Value *vp);
    static bool constructWithProto(JSContext *cx, unsigned argc, Value *vp);
    static bool construct(JSContext *cx, JSObject *bufobj, const CallArgs &args,
                          HandleObject proto);

    static inline DataViewObject *
    create(JSContext *cx, uint32_t byteOffset, uint32_t byteLength,
           Handle<ArrayBufferObject*> arrayBuffer, JSObject *proto);

    static bool getInt8Impl(JSContext *cx, CallArgs args);
    static bool fun_getInt8(JSContext *cx, unsigned argc, Value *vp);

    static bool getUint8Impl(JSContext *cx, CallArgs args);
    static bool fun_getUint8(JSContext *cx, unsigned argc, Value *vp);

    static bool getInt16Impl(JSContext *cx, CallArgs args);
    static bool fun_getInt16(JSContext *cx, unsigned argc, Value *vp);

    static bool getUint16Impl(JSContext *cx, CallArgs args);
    static bool fun_getUint16(JSContext *cx, unsigned argc, Value *vp);

    static bool getInt32Impl(JSContext *cx, CallArgs args);
    static bool fun_getInt32(JSContext *cx, unsigned argc, Value *vp);

    static bool getUint32Impl(JSContext *cx, CallArgs args);
    static bool fun_getUint32(JSContext *cx, unsigned argc, Value *vp);

    static bool getFloat32Impl(JSContext *cx, CallArgs args);
    static bool fun_getFloat32(JSContext *cx, unsigned argc, Value *vp);

    static bool getFloat64Impl(JSContext *cx, CallArgs args);
    static bool fun_getFloat64(JSContext *cx, unsigned argc, Value *vp);

    static bool setInt8Impl(JSContext *cx, CallArgs args);
    static bool fun_setInt8(JSContext *cx, unsigned argc, Value *vp);

    static bool setUint8Impl(JSContext *cx, CallArgs args);
    static bool fun_setUint8(JSContext *cx, unsigned argc, Value *vp);

    static bool setInt16Impl(JSContext *cx, CallArgs args);
    static bool fun_setInt16(JSContext *cx, unsigned argc, Value *vp);

    static bool setUint16Impl(JSContext *cx, CallArgs args);
    static bool fun_setUint16(JSContext *cx, unsigned argc, Value *vp);

    static bool setInt32Impl(JSContext *cx, CallArgs args);
    static bool fun_setInt32(JSContext *cx, unsigned argc, Value *vp);

    static bool setUint32Impl(JSContext *cx, CallArgs args);
    static bool fun_setUint32(JSContext *cx, unsigned argc, Value *vp);

    static bool setFloat32Impl(JSContext *cx, CallArgs args);
    static bool fun_setFloat32(JSContext *cx, unsigned argc, Value *vp);

    static bool setFloat64Impl(JSContext *cx, CallArgs args);
    static bool fun_setFloat64(JSContext *cx, unsigned argc, Value *vp);

    static bool initClass(JSContext *cx);
    static void neuter(JSObject *view);
    static bool getDataPointer(JSContext *cx, Handle<DataViewObject*> obj,
                               CallArgs args, size_t typeSize, uint8_t **data);
    template<typename NativeType>
    static bool read(JSContext *cx, Handle<DataViewObject*> obj,
                     CallArgs &args, NativeType *val, const char *method);
    template<typename NativeType>
    static bool write(JSContext *cx, Handle<DataViewObject*> obj,
                      CallArgs &args, const char *method);

    void neuter();

  private:
    static const JSFunctionSpec jsfuncs[];
};

static inline int32_t
ClampIntForUint8Array(int32_t x)
{
    if (x < 0)
        return 0;
    if (x > 255)
        return 255;
    return x;
}

extern js::ArrayBufferObject * const UNSET_BUFFER_LINK;

} // namespace js

template <>
inline bool
JSObject::is<js::TypedArrayObject>() const
{
    return js::IsTypedArrayClass(getClass());
}

template <>
inline bool
JSObject::is<js::ArrayBufferViewObject>() const
{
    return is<js::DataViewObject>() || is<js::TypedArrayObject>() ||
           IsTypedObjectClass(getClass());
}

#endif /* vm_TypedArrayObject_h */
