/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TypedArray_h
#define mozilla_dom_TypedArray_h

#include <utility>

#include "js/ArrayBuffer.h"
#include "js/ArrayBufferMaybeShared.h"
#include "js/experimental/TypedData.h"  // js::Unwrap(Ui|I)nt(8|16|32)Array, js::Get(Ui|I)nt(8|16|32)ArrayLengthAndData, js::UnwrapUint8ClampedArray, js::GetUint8ClampedArrayLengthAndData, js::UnwrapFloat(32|64)Array, js::GetFloat(32|64)ArrayLengthAndData, JS_GetArrayBufferViewType
#include "js/GCAPI.h"                   // JS::AutoCheckCannotGC
#include "js/RootingAPI.h"              // JS::Rooted
#include "js/ScalarType.h"              // JS::Scalar::Type
#include "js/SharedArrayBuffer.h"
#include "mozilla/Attributes.h"
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
template <class ArrayT>
struct TypedArray_base : public SpiderMonkeyInterfaceObjectStorage,
                         AllTypedArraysBase {
  using element_type = typename ArrayT::DataType;

  TypedArray_base()
      : mData(nullptr), mLength(0), mShared(false), mComputed(false) {}

  TypedArray_base(TypedArray_base&& aOther)
      : SpiderMonkeyInterfaceObjectStorage(std::move(aOther)),
        mData(aOther.mData),
        mLength(aOther.mLength),
        mShared(aOther.mShared),
        mComputed(aOther.mComputed) {
    aOther.Reset();
  }

 private:
  mutable element_type* mData;
  mutable uint32_t mLength;
  mutable bool mShared;
  mutable bool mComputed;

 public:
  inline bool Init(JSObject* obj) {
    MOZ_ASSERT(!inited());
    mImplObj = mWrappedObj = ArrayT::unwrap(obj).asObject();
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
  // (Just use JS::IsSharedArrayBuffer() to test if any object is of
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

  inline element_type* Data() const {
    MOZ_ASSERT(mComputed);
    return mData;
  }

  // Return a pointer to data that will not move during a GC.
  //
  // For some smaller views, this will copy the data into the provided buffer
  // and return that buffer as the pointer. Otherwise, this will return a
  // direct pointer to the actual data with no copying. If the provided buffer
  // is not large enough, nullptr will be returned. If bufSize is at least
  // JS_MaxMovableTypedArraySize(), the data is guaranteed to fit.
  inline element_type* FixedData(uint8_t* buffer, size_t bufSize) const {
    MOZ_ASSERT(mComputed);
    return JS_GetArrayBufferViewFixedData(mImplObj, buffer, bufSize);
  }

  inline uint32_t Length() const {
    MOZ_ASSERT(mComputed);
    return mLength;
  }

  inline void ComputeState() const {
    MOZ_ASSERT(inited());
    MOZ_ASSERT(!mComputed);
    size_t length;
    JS::AutoCheckCannotGC nogc;
    mData =
        ArrayT::fromObject(mImplObj).getLengthAndData(&length, &mShared, nogc);
    MOZ_RELEASE_ASSERT(length <= INT32_MAX,
                       "Bindings must have checked ArrayBuffer{View} length");
    mLength = length;
    mComputed = true;
  }

  inline void Reset() {
    // This method mostly exists to inform the GC rooting hazard analysis that
    // the variable can be considered dead, at least until you do anything else
    // with it.
    mData = nullptr;
    mLength = 0;
    mShared = false;
    mComputed = false;
  }

 private:
  TypedArray_base(const TypedArray_base&) = delete;
};

template <class ArrayT>
struct TypedArray : public TypedArray_base<ArrayT> {
  using Base = TypedArray_base<ArrayT>;
  using element_type = typename Base::element_type;

  TypedArray() = default;

  TypedArray(TypedArray&& aOther) = default;

  static inline JSObject* Create(JSContext* cx, nsWrapperCache* creator,
                                 uint32_t length,
                                 const element_type* data = nullptr) {
    JS::Rooted<JSObject*> creatorWrapper(cx);
    Maybe<JSAutoRealm> ar;
    if (creator && (creatorWrapper = creator->GetWrapperPreserveColor())) {
      ar.emplace(cx, creatorWrapper);
    }

    return CreateCommon(cx, length, data);
  }

  static inline JSObject* Create(JSContext* cx, uint32_t length,
                                 const element_type* data = nullptr) {
    return CreateCommon(cx, length, data);
  }

  static inline JSObject* Create(JSContext* cx, nsWrapperCache* creator,
                                 Span<const element_type> data) {
    // Span<> uses size_t as a length, and we use uint32_t instead.
    if (MOZ_UNLIKELY(data.Length() > UINT32_MAX)) {
      JS_ReportOutOfMemory(cx);
      return nullptr;
    }
    return Create(cx, creator, data.Length(), data.Elements());
  }

  static inline JSObject* Create(JSContext* cx, Span<const element_type> data) {
    // Span<> uses size_t as a length, and we use uint32_t instead.
    if (MOZ_UNLIKELY(data.Length() > UINT32_MAX)) {
      JS_ReportOutOfMemory(cx);
      return nullptr;
    }
    return CreateCommon(cx, data.Length(), data.Elements());
  }

 private:
  static inline JSObject* CreateCommon(JSContext* cx, uint32_t length,
                                       const element_type* data) {
    auto array = ArrayT::create(cx, length);
    if (!array) {
      return nullptr;
    }
    if (data) {
      JS::AutoCheckCannotGC nogc;
      bool isShared;
      element_type* buf = array.getData(&isShared, nogc);
      // Data will not be shared, until a construction protocol exists
      // for constructing shared data.
      MOZ_ASSERT(!isShared);
      memcpy(buf, data, length * sizeof(element_type));
    }
    return array.asObject();
  }

  TypedArray(const TypedArray&) = delete;
};

template <JS::Scalar::Type GetViewType(JSObject*)>
struct ArrayBufferView_base : public TypedArray_base<JS::ArrayBufferView> {
 private:
  using Base = TypedArray_base<JS::ArrayBufferView>;

 public:
  ArrayBufferView_base() : Base(), mType(JS::Scalar::MaxTypedArrayViewType) {}

  ArrayBufferView_base(ArrayBufferView_base&& aOther)
      : Base(std::move(aOther)), mType(aOther.mType) {
    aOther.mType = JS::Scalar::MaxTypedArrayViewType;
  }

 private:
  JS::Scalar::Type mType;

 public:
  inline bool Init(JSObject* obj) {
    if (!Base::Init(obj)) {
      return false;
    }

    mType = GetViewType(this->Obj());
    return true;
  }

  inline JS::Scalar::Type Type() const {
    MOZ_ASSERT(this->inited());
    return mType;
  }
};

using Int8Array = TypedArray<JS::Int8Array>;
using Uint8Array = TypedArray<JS::Uint8Array>;
using Uint8ClampedArray = TypedArray<JS::Uint8ClampedArray>;
using Int16Array = TypedArray<JS::Int16Array>;
using Uint16Array = TypedArray<JS::Uint16Array>;
using Int32Array = TypedArray<JS::Int32Array>;
using Uint32Array = TypedArray<JS::Uint32Array>;
using Float32Array = TypedArray<JS::Float32Array>;
using Float64Array = TypedArray<JS::Float64Array>;
using ArrayBufferView = ArrayBufferView_base<JS_GetArrayBufferViewType>;
using ArrayBuffer = TypedArray<JS::ArrayBuffer>;

// A class for converting an nsTArray to a TypedArray
// Note: A TypedArrayCreator must not outlive the nsTArray it was created from.
//       So this is best used to pass from things that understand nsTArray to
//       things that understand TypedArray, as with ToJSValue.
template <typename TypedArrayType>
class TypedArrayCreator {
  typedef nsTArray<typename TypedArrayType::element_type> ArrayType;

 public:
  explicit TypedArrayCreator(const ArrayType& aArray) : mArray(aArray) {}

  JSObject* Create(JSContext* aCx) const {
    return TypedArrayType::Create(aCx, mArray.Length(), mArray.Elements());
  }

 private:
  const ArrayType& mArray;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_TypedArray_h */
