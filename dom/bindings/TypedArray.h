/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TypedArray_h
#define mozilla_dom_TypedArray_h

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/RootingAPI.h"
#include "js/Tracer.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/Util.h" // for Maybe
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

/*
 * Class that just handles the JSObject storage and tracing for typed arrays
 */
struct TypedArrayObjectStorage : AllTypedArraysBase {
protected:
  JSObject* mObj;

  TypedArrayObjectStorage()
  {
  }

public:
  inline void TraceSelf(JSTracer* trc)
  {
    if (mObj) {
      JS_CallObjectTracer(trc, &mObj, "TypedArray.mObj");
    }
  }

private:
  TypedArrayObjectStorage(const TypedArrayObjectStorage&) MOZ_DELETE;
};

/*
 * Various typed array classes for argument conversion.  We have a base class
 * that has a way of initializing a TypedArray from an existing typed array, and
 * a subclass of the base class that supports creation of a relevant typed array
 * or array buffer object.
 */
template<typename T,
         JSObject* UnboxArray(JSObject*, uint32_t*, T**)>
struct TypedArray_base : public TypedArrayObjectStorage {
  TypedArray_base(JSObject* obj)
  {
    DoInit(obj);
  }

  TypedArray_base()
  {
    mObj = nullptr;
  }

private:
  T* mData;
  uint32_t mLength;

public:
  inline bool Init(JSObject* obj)
  {
    MOZ_ASSERT(!inited());
    DoInit(obj);
    return inited();
  }

  inline bool inited() const {
    return !!mObj;
  }

  inline T *Data() const {
    MOZ_ASSERT(inited());
    return mData;
  }

  inline uint32_t Length() const {
    MOZ_ASSERT(inited());
    return mLength;
  }

  inline JSObject *Obj() const {
    MOZ_ASSERT(inited());
    return mObj;
  }

  inline bool WrapIntoNewCompartment(JSContext* cx)
  {
    return JS_WrapObject(cx,
      JS::MutableHandle<JSObject*>::fromMarkedLocation(&mObj));
  }

protected:
  inline void DoInit(JSObject* obj)
  {
    mObj = UnboxArray(obj, &mLength, &mData);
  }

private:
  TypedArray_base(const TypedArray_base&) MOZ_DELETE;
};


template<typename T,
         T* GetData(JSObject*),
         JSObject* UnboxArray(JSObject*, uint32_t*, T**),
         JSObject* CreateNew(JSContext*, uint32_t)>
struct TypedArray : public TypedArray_base<T,UnboxArray> {
  TypedArray(JSObject* obj) :
    TypedArray_base<T,UnboxArray>(obj)
  {}

  TypedArray() :
    TypedArray_base<T,UnboxArray>()
  {}

  static inline JSObject*
  Create(JSContext* cx, nsWrapperCache* creator, uint32_t length,
         const T* data = nullptr) {
    JS::Rooted<JSObject*> creatorWrapper(cx);
    Maybe<JSAutoCompartment> ac;
    if (creator && (creatorWrapper = creator->GetWrapperPreserveColor())) {
      ac.construct(cx, creatorWrapper);
    }

    return CreateCommon(cx, creatorWrapper, length, data);
  }

  static inline JSObject*
  Create(JSContext* cx, JS::Handle<JSObject*> creator, uint32_t length,
         const T* data = nullptr) {
    Maybe<JSAutoCompartment> ac;
    if (creator) {
      ac.construct(cx, creator);
    }

    return CreateCommon(cx, creator, length, data);
  }

private:
  static inline JSObject*
  CreateCommon(JSContext* cx, JS::Handle<JSObject*> creator, uint32_t length,
               const T* data) {
    JSObject* obj = CreateNew(cx, length);
    if (!obj) {
      return nullptr;
    }
    if (data) {
      T* buf = static_cast<T*>(GetData(obj));
      memcpy(buf, data, length*sizeof(T));
    }
    return obj;
  }

  TypedArray(const TypedArray&) MOZ_DELETE;
};

typedef TypedArray<int8_t, JS_GetInt8ArrayData, JS_GetObjectAsInt8Array,
                   JS_NewInt8Array>
        Int8Array;
typedef TypedArray<uint8_t, JS_GetUint8ArrayData,
                   JS_GetObjectAsUint8Array, JS_NewUint8Array>
        Uint8Array;
typedef TypedArray<uint8_t, JS_GetUint8ClampedArrayData,
                   JS_GetObjectAsUint8ClampedArray, JS_NewUint8ClampedArray>
        Uint8ClampedArray;
typedef TypedArray<int16_t, JS_GetInt16ArrayData,
                   JS_GetObjectAsInt16Array, JS_NewInt16Array>
        Int16Array;
typedef TypedArray<uint16_t, JS_GetUint16ArrayData,
                   JS_GetObjectAsUint16Array, JS_NewUint16Array>
        Uint16Array;
typedef TypedArray<int32_t, JS_GetInt32ArrayData,
                   JS_GetObjectAsInt32Array, JS_NewInt32Array>
        Int32Array;
typedef TypedArray<uint32_t, JS_GetUint32ArrayData,
                   JS_GetObjectAsUint32Array, JS_NewUint32Array>
        Uint32Array;
typedef TypedArray<float, JS_GetFloat32ArrayData,
                   JS_GetObjectAsFloat32Array, JS_NewFloat32Array>
        Float32Array;
typedef TypedArray<double, JS_GetFloat64ArrayData,
                   JS_GetObjectAsFloat64Array, JS_NewFloat64Array>
        Float64Array;
typedef TypedArray_base<uint8_t, JS_GetObjectAsArrayBufferView>
        ArrayBufferView;
typedef TypedArray<uint8_t, JS_GetArrayBufferData,
                   JS_GetObjectAsArrayBuffer, JS_NewArrayBuffer>
        ArrayBuffer;

// A class for rooting an existing TypedArray struct
template<typename ArrayType>
class MOZ_STACK_CLASS TypedArrayRooter : private JS::CustomAutoRooter
{
public:
  TypedArrayRooter(JSContext* cx,
                   ArrayType* aArray MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    JS::CustomAutoRooter(cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT),
    mArray(aArray)
  {
  }

  virtual void trace(JSTracer* trc) MOZ_OVERRIDE
  {
    mArray->TraceSelf(trc);
  }

private:
  TypedArrayObjectStorage* const mArray;
};

// And a specialization for dealing with nullable typed arrays
template<typename Inner> struct Nullable;
template<typename ArrayType>
class MOZ_STACK_CLASS TypedArrayRooter<Nullable<ArrayType> > :
    private JS::CustomAutoRooter
{
public:
  TypedArrayRooter(JSContext* cx,
                   Nullable<ArrayType>* aArray MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    JS::CustomAutoRooter(cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT),
    mArray(aArray)
  {
  }

  virtual void trace(JSTracer* trc) MOZ_OVERRIDE
  {
    if (!mArray->IsNull()) {
      mArray->Value().TraceSelf(trc);
    }
  }

private:
  Nullable<ArrayType>* const mArray;
};

// Class for easily setting up a rooted typed array object on the stack
template<typename ArrayType>
class MOZ_STACK_CLASS RootedTypedArray : public ArrayType,
                                         private TypedArrayRooter<ArrayType>
{
public:
  RootedTypedArray(JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    ArrayType(),
    TypedArrayRooter<ArrayType>(cx,
                                MOZ_THIS_IN_INITIALIZER_LIST()
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT)
  {
  }

  RootedTypedArray(JSContext* cx, JSObject* obj MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    ArrayType(obj),
    TypedArrayRooter<ArrayType>(cx,
                                MOZ_THIS_IN_INITIALIZER_LIST()
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT)
  {
  }
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_TypedArray_h */
