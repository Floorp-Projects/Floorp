/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TypedArray_h
#define mozilla_dom_TypedArray_h

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

/*
 * Class that just handles the JSObject storage and tracing for typed arrays
 */
struct TypedArrayObjectStorage : AllTypedArraysBase {
protected:
  JSObject* mTypedObj;
  JSObject* mWrappedObj;

  TypedArrayObjectStorage()
    : mTypedObj(nullptr),
      mWrappedObj(nullptr)
  {
  }

  TypedArrayObjectStorage(TypedArrayObjectStorage&& aOther)
    : mTypedObj(aOther.mTypedObj),
      mWrappedObj(aOther.mWrappedObj)
  {
    aOther.mTypedObj = nullptr;
    aOther.mWrappedObj = nullptr;
  }

public:
  inline void TraceSelf(JSTracer* trc)
  {
    if (mTypedObj) {
      JS_CallUnbarrieredObjectTracer(trc, &mTypedObj, "TypedArray.mTypedObj");
    }
    if (mWrappedObj) {
      JS_CallUnbarrieredObjectTracer(trc, &mTypedObj, "TypedArray.mWrappedObj");
    }
  }

private:
  TypedArrayObjectStorage(const TypedArrayObjectStorage&) = delete;
};

/*
 * Various typed array classes for argument conversion.  We have a base class
 * that has a way of initializing a TypedArray from an existing typed array, and
 * a subclass of the base class that supports creation of a relevant typed array
 * or array buffer object.
 */
template<typename T,
         JSObject* UnwrapArray(JSObject*),
         void GetLengthAndData(JSObject*, uint32_t*, T**)>
struct TypedArray_base : public TypedArrayObjectStorage {
  typedef T element_type;

  TypedArray_base()
    : mData(nullptr),
      mLength(0),
      mComputed(false)
  {
  }

  TypedArray_base(TypedArray_base&& aOther)
    : TypedArrayObjectStorage(Move(aOther)),
      mData(aOther.mData),
      mLength(aOther.mLength),
      mComputed(aOther.mComputed)
  {
    aOther.mData = nullptr;
    aOther.mLength = 0;
    aOther.mComputed = false;
  }

private:
  mutable T* mData;
  mutable uint32_t mLength;
  mutable bool mComputed;

public:
  inline bool Init(JSObject* obj)
  {
    MOZ_ASSERT(!inited());
    mTypedObj = mWrappedObj = UnwrapArray(obj);
    return inited();
  }

  inline bool inited() const {
    return !!mTypedObj;
  }

  inline T *Data() const {
    MOZ_ASSERT(mComputed);
    return mData;
  }

  inline uint32_t Length() const {
    MOZ_ASSERT(mComputed);
    return mLength;
  }

  inline JSObject *Obj() const {
    MOZ_ASSERT(inited());
    return mWrappedObj;
  }

  inline bool WrapIntoNewCompartment(JSContext* cx)
  {
    return JS_WrapObject(cx,
      JS::MutableHandle<JSObject*>::fromMarkedLocation(&mWrappedObj));
  }

  inline void ComputeLengthAndData() const
  {
    MOZ_ASSERT(inited());
    MOZ_ASSERT(!mComputed);
    GetLengthAndData(mTypedObj, &mLength, &mData);
    mComputed = true;
  }

private:
  TypedArray_base(const TypedArray_base&) = delete;
};

template<typename T,
         JSObject* UnwrapArray(JSObject*),
         T* GetData(JSObject*, const JS::AutoCheckCannotGC&),
         void GetLengthAndData(JSObject*, uint32_t*, T**),
         JSObject* CreateNew(JSContext*, uint32_t)>
struct TypedArray : public TypedArray_base<T, UnwrapArray, GetLengthAndData> {
private:
  typedef TypedArray_base<T, UnwrapArray, GetLengthAndData> Base;

public:
  TypedArray()
    : Base()
  {}

  TypedArray(TypedArray&& aOther)
    : Base(Move(aOther))
  {
  }

  static inline JSObject*
  Create(JSContext* cx, nsWrapperCache* creator, uint32_t length,
         const T* data = nullptr) {
    JS::Rooted<JSObject*> creatorWrapper(cx);
    Maybe<JSAutoCompartment> ac;
    if (creator && (creatorWrapper = creator->GetWrapperPreserveColor())) {
      ac.emplace(cx, creatorWrapper);
    }

    return CreateCommon(cx, length, data);
  }

  static inline JSObject*
  Create(JSContext* cx, uint32_t length, const T* data = nullptr) {
    return CreateCommon(cx, length, data);
  }

private:
  static inline JSObject*
  CreateCommon(JSContext* cx, uint32_t length, const T* data) {
    JSObject* obj = CreateNew(cx, length);
    if (!obj) {
      return nullptr;
    }
    if (data) {
      JS::AutoCheckCannotGC nogc;
      T* buf = static_cast<T*>(GetData(obj, nogc));
      memcpy(buf, data, length*sizeof(T));
    }
    return obj;
  }

  TypedArray(const TypedArray&) = delete;
};

template<JSObject* UnwrapArray(JSObject*),
         void GetLengthAndData(JSObject*, uint32_t*, uint8_t**),
         js::Scalar::Type GetViewType(JSObject*)>
struct ArrayBufferView_base : public TypedArray_base<uint8_t, UnwrapArray,
                                                     GetLengthAndData> {
private:
  typedef TypedArray_base<uint8_t, UnwrapArray, GetLengthAndData> Base;

public:
  ArrayBufferView_base()
    : Base()
  {
  }

  ArrayBufferView_base(ArrayBufferView_base&& aOther)
    : Base(Move(aOther)),
      mType(aOther.mType)
  {
    aOther.mType = js::Scalar::MaxTypedArrayViewType;
  }

private:
  js::Scalar::Type mType;

public:
  inline bool Init(JSObject* obj)
  {
    if (!Base::Init(obj)) {
      return false;
    }

    mType = GetViewType(this->Obj());
    return true;
  }

  inline js::Scalar::Type Type() const
  {
    MOZ_ASSERT(this->inited());
    return mType;
  }
};

typedef TypedArray<int8_t, js::UnwrapInt8Array, JS_GetInt8ArrayData,
                   js::GetInt8ArrayLengthAndData, JS_NewInt8Array>
        Int8Array;
typedef TypedArray<uint8_t, js::UnwrapUint8Array, JS_GetUint8ArrayData,
                   js::GetUint8ArrayLengthAndData, JS_NewUint8Array>
        Uint8Array;
typedef TypedArray<uint8_t, js::UnwrapUint8ClampedArray, JS_GetUint8ClampedArrayData,
                   js::GetUint8ClampedArrayLengthAndData, JS_NewUint8ClampedArray>
        Uint8ClampedArray;
typedef TypedArray<int16_t, js::UnwrapInt16Array, JS_GetInt16ArrayData,
                   js::GetInt16ArrayLengthAndData, JS_NewInt16Array>
        Int16Array;
typedef TypedArray<uint16_t, js::UnwrapUint16Array, JS_GetUint16ArrayData,
                   js::GetUint16ArrayLengthAndData, JS_NewUint16Array>
        Uint16Array;
typedef TypedArray<int32_t, js::UnwrapInt32Array, JS_GetInt32ArrayData,
                   js::GetInt32ArrayLengthAndData, JS_NewInt32Array>
        Int32Array;
typedef TypedArray<uint32_t, js::UnwrapUint32Array, JS_GetUint32ArrayData,
                   js::GetUint32ArrayLengthAndData, JS_NewUint32Array>
        Uint32Array;
typedef TypedArray<float, js::UnwrapFloat32Array, JS_GetFloat32ArrayData,
                   js::GetFloat32ArrayLengthAndData, JS_NewFloat32Array>
        Float32Array;
typedef TypedArray<double, js::UnwrapFloat64Array, JS_GetFloat64ArrayData,
                   js::GetFloat64ArrayLengthAndData, JS_NewFloat64Array>
        Float64Array;
typedef ArrayBufferView_base<js::UnwrapArrayBufferView,
                             js::GetArrayBufferViewLengthAndData,
                             JS_GetArrayBufferViewType>
        ArrayBufferView;
typedef TypedArray<uint8_t, js::UnwrapArrayBuffer, JS_GetArrayBufferData,
                   js::GetArrayBufferLengthAndData, JS_NewArrayBuffer>
        ArrayBuffer;

typedef TypedArray<int8_t, js::UnwrapSharedInt8Array, JS_GetSharedInt8ArrayData,
                   js::GetSharedInt8ArrayLengthAndData, JS_NewSharedInt8Array>
        SharedInt8Array;
typedef TypedArray<uint8_t, js::UnwrapSharedUint8Array, JS_GetSharedUint8ArrayData,
                   js::GetSharedUint8ArrayLengthAndData, JS_NewSharedUint8Array>
        SharedUint8Array;
typedef TypedArray<uint8_t, js::UnwrapSharedUint8ClampedArray, JS_GetSharedUint8ClampedArrayData,
                   js::GetSharedUint8ClampedArrayLengthAndData, JS_NewSharedUint8ClampedArray>
        SharedUint8ClampedArray;
typedef TypedArray<int16_t, js::UnwrapSharedInt16Array, JS_GetSharedInt16ArrayData,
                   js::GetSharedInt16ArrayLengthAndData, JS_NewSharedInt16Array>
        SharedInt16Array;
typedef TypedArray<uint16_t, js::UnwrapSharedUint16Array, JS_GetSharedUint16ArrayData,
                   js::GetSharedUint16ArrayLengthAndData, JS_NewSharedUint16Array>
        SharedUint16Array;
typedef TypedArray<int32_t, js::UnwrapSharedInt32Array, JS_GetSharedInt32ArrayData,
                   js::GetSharedInt32ArrayLengthAndData, JS_NewSharedInt32Array>
        SharedInt32Array;
typedef TypedArray<uint32_t, js::UnwrapSharedUint32Array, JS_GetSharedUint32ArrayData,
                   js::GetSharedUint32ArrayLengthAndData, JS_NewSharedUint32Array>
        SharedUint32Array;
typedef TypedArray<float, js::UnwrapSharedFloat32Array, JS_GetSharedFloat32ArrayData,
                   js::GetSharedFloat32ArrayLengthAndData, JS_NewSharedFloat32Array>
        SharedFloat32Array;
typedef TypedArray<double, js::UnwrapSharedFloat64Array, JS_GetSharedFloat64ArrayData,
                   js::GetSharedFloat64ArrayLengthAndData, JS_NewSharedFloat64Array>
        SharedFloat64Array;
typedef ArrayBufferView_base<js::UnwrapSharedArrayBufferView,
                             js::GetSharedArrayBufferViewLengthAndData,
                             JS_GetSharedArrayBufferViewType>
        SharedArrayBufferView;
typedef TypedArray<uint8_t, js::UnwrapSharedArrayBuffer, JS_GetSharedArrayBufferData,
                   js::GetSharedArrayBufferLengthAndData, JS_NewSharedArrayBuffer>
        SharedArrayBuffer;

// A class for converting an nsTArray to a TypedArray
// Note: A TypedArrayCreator must not outlive the nsTArray it was created from.
//       So this is best used to pass from things that understand nsTArray to
//       things that understand TypedArray, as with Promise::ArgumentToJSValue.
template<typename TypedArrayType>
class TypedArrayCreator
{
  typedef nsTArray<typename TypedArrayType::element_type> ArrayType;

  public:
    explicit TypedArrayCreator(const ArrayType& aArray)
      : mArray(aArray)
    {}

    JSObject* Create(JSContext* aCx) const
    {
      return TypedArrayType::Create(aCx, mArray.Length(), mArray.Elements());
    }

  private:
    const ArrayType& mArray;
};

// A class for rooting an existing TypedArray struct
template<typename ArrayType>
class MOZ_RAII TypedArrayRooter : private JS::CustomAutoRooter
{
public:
  TypedArrayRooter(JSContext* cx,
                   ArrayType* aArray MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    JS::CustomAutoRooter(cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT),
    mArray(aArray)
  {
  }

  virtual void trace(JSTracer* trc) override
  {
    mArray->TraceSelf(trc);
  }

private:
  TypedArrayObjectStorage* const mArray;
};

// And a specialization for dealing with nullable typed arrays
template<typename Inner> struct Nullable;
template<typename ArrayType>
class MOZ_RAII TypedArrayRooter<Nullable<ArrayType> > :
    private JS::CustomAutoRooter
{
public:
  TypedArrayRooter(JSContext* cx,
                   Nullable<ArrayType>* aArray MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    JS::CustomAutoRooter(cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT),
    mArray(aArray)
  {
  }

  virtual void trace(JSTracer* trc) override
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
class MOZ_RAII RootedTypedArray : public ArrayType,
                                  private TypedArrayRooter<ArrayType>
{
public:
  explicit RootedTypedArray(JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    ArrayType(),
    TypedArrayRooter<ArrayType>(cx, this
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT)
  {
  }

  RootedTypedArray(JSContext* cx, JSObject* obj MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    ArrayType(obj),
    TypedArrayRooter<ArrayType>(cx, this
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT)
  {
  }
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_TypedArray_h */
