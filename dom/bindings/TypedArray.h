/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TypedArray_h
#define mozilla_dom_TypedArray_h

#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/SpiderMonkeyInterface.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

/*
 * Various typed array classes for argument conversion.  We have a base class
 * that has a way of initializing a TypedArray from an existing typed array, and
 * a subclass of the base class that supports creation of a relevant typed array
 * or array buffer object.
 */
template<typename T,
         JSObject* UnwrapArray(JSObject*),
         void GetLengthAndDataAndSharedness(JSObject*, uint32_t*, bool*, T**)>
struct TypedArray_base : public SpiderMonkeyInterfaceObjectStorage,
                         AllTypedArraysBase
{
  typedef T element_type;

  TypedArray_base()
    : mData(nullptr),
      mLength(0),
      mShared(false),
      mComputed(false)
  {
  }

  TypedArray_base(TypedArray_base&& aOther)
    : SpiderMonkeyInterfaceObjectStorage(std::move(aOther)),
      mData(aOther.mData),
      mLength(aOther.mLength),
      mShared(aOther.mShared),
      mComputed(aOther.mComputed)
  {
    aOther.mData = nullptr;
    aOther.mLength = 0;
    aOther.mShared = false;
    aOther.mComputed = false;
  }

private:
  mutable T* mData;
  mutable uint32_t mLength;
  mutable bool mShared;
  mutable bool mComputed;

public:
  inline bool Init(JSObject* obj)
  {
    MOZ_ASSERT(!inited());
    mImplObj = mWrappedObj = UnwrapArray(obj);
    return inited();
  }

  // About shared memory:
  //
  // Any DOM TypedArray as well as any DOM ArrayBufferView can map the
  // memory of either a JS ArrayBuffer or a JS SharedArrayBuffer.  If
  // the TypedArray maps a SharedArrayBuffer the Length() and Data()
  // accessors on the DOM view will return zero and nullptr; to get
  // the actual length and data, call the LengthAllowShared() and
  // DataAllowShared() accessors instead.
  //
  // Two methods are available for determining if a DOM view maps
  // shared memory.  The IsShared() method is cheap and can be called
  // if the view has been computed; the JS_GetTypedArraySharedness()
  // method is slightly more expensive and can be called on the Obj()
  // value if the view may not have been computed and if the value is
  // known to represent a JS TypedArray.
  //
  // (Just use JS_IsSharedArrayBuffer() to test if any object is of
  // that type.)
  //
  // Code that elects to allow views that map shared memory to be used
  // -- ie, code that "opts in to shared memory" -- should generally
  // not access the raw data buffer with standard C++ mechanisms as
  // that creates the possibility of C++ data races, which is
  // undefined behavior.  The JS engine will eventually export (bug
  // 1225033) a suite of methods that avoid undefined behavior.
  //
  // Callers of Obj() that do not opt in to shared memory can produce
  // better diagnostics by checking whether the JSObject in fact maps
  // shared memory and throwing an error if it does.  However, it is
  // safe to use the value of Obj() without such checks.
  //
  // The DOM TypedArray abstraction prevents the underlying buffer object
  // from being accessed directly, but JS_GetArrayBufferViewBuffer(Obj())
  // will obtain the buffer object.  Code that calls that function must
  // not assume the returned buffer is an ArrayBuffer.  That is guarded
  // against by an out parameter on that call that communicates the
  // sharedness of the buffer.
  //
  // Finally, note that the buffer memory of a SharedArrayBuffer is
  // not detachable.

  inline bool IsShared() const {
    MOZ_ASSERT(mComputed);
    return mShared;
  }

  inline T *Data() const {
    MOZ_ASSERT(mComputed);
    if (mShared)
      return nullptr;
    return mData;
  }

  inline T *DataAllowShared() const {
    MOZ_ASSERT(mComputed);
    return mData;
  }

  inline uint32_t Length() const {
    MOZ_ASSERT(mComputed);
    if (mShared)
      return 0;
    return mLength;
  }

  inline uint32_t LengthAllowShared() const {
    MOZ_ASSERT(mComputed);
    return mLength;
  }

  inline void ComputeLengthAndData() const
  {
    MOZ_ASSERT(inited());
    MOZ_ASSERT(!mComputed);
    GetLengthAndDataAndSharedness(mImplObj, &mLength, &mShared, &mData);
    mComputed = true;
  }

private:
  TypedArray_base(const TypedArray_base&) = delete;
};

template<typename T,
         JSObject* UnwrapArray(JSObject*),
         T* GetData(JSObject*, bool* isShared, const JS::AutoRequireNoGC&),
         void GetLengthAndDataAndSharedness(JSObject*, uint32_t*, bool*, T**),
         JSObject* CreateNew(JSContext*, uint32_t)>
struct TypedArray
  : public TypedArray_base<T, UnwrapArray, GetLengthAndDataAndSharedness>
{
private:
  typedef TypedArray_base<T, UnwrapArray, GetLengthAndDataAndSharedness> Base;

public:
  TypedArray()
    : Base()
  {}

  TypedArray(TypedArray&& aOther)
    : Base(std::move(aOther))
  {
  }

  static inline JSObject*
  Create(JSContext* cx, nsWrapperCache* creator, uint32_t length,
         const T* data = nullptr) {
    JS::Rooted<JSObject*> creatorWrapper(cx);
    Maybe<JSAutoRealm> ar;
    if (creator && (creatorWrapper = creator->GetWrapperPreserveColor())) {
      ar.emplace(cx, creatorWrapper);
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
      bool isShared;
      T* buf = static_cast<T*>(GetData(obj, &isShared, nogc));
      // Data will not be shared, until a construction protocol exists
      // for constructing shared data.
      MOZ_ASSERT(!isShared);
      memcpy(buf, data, length*sizeof(T));
    }
    return obj;
  }

  TypedArray(const TypedArray&) = delete;
};

template<JSObject* UnwrapArray(JSObject*),
         void GetLengthAndDataAndSharedness(JSObject*, uint32_t*, bool*,
                                            uint8_t**),
         js::Scalar::Type GetViewType(JSObject*)>
struct ArrayBufferView_base
  : public TypedArray_base<uint8_t, UnwrapArray, GetLengthAndDataAndSharedness>
{
private:
  typedef TypedArray_base<uint8_t, UnwrapArray, GetLengthAndDataAndSharedness>
          Base;

public:
  ArrayBufferView_base()
    : Base()
  {
  }

  ArrayBufferView_base(ArrayBufferView_base&& aOther)
    : Base(std::move(aOther)),
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

typedef TypedArray<uint8_t, js::UnwrapSharedArrayBuffer, JS_GetSharedArrayBufferData,
                   js::GetSharedArrayBufferLengthAndData, JS_NewSharedArrayBuffer>
        SharedArrayBuffer;

// A class for converting an nsTArray to a TypedArray
// Note: A TypedArrayCreator must not outlive the nsTArray it was created from.
//       So this is best used to pass from things that understand nsTArray to
//       things that understand TypedArray, as with ToJSValue.
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

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_TypedArray_h */
