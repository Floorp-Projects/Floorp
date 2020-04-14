/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/DataViewObject.h"

#include "mozilla/Alignment.h"
#include "mozilla/Casting.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/WrappingOperations.h"

#include <algorithm>
#include <string.h>
#include <type_traits>

#include "jsapi.h"
#include "jsnum.h"

#include "builtin/Array.h"
#include "jit/AtomicOperations.h"
#include "js/Conversions.h"
#include "js/PropertySpec.h"
#include "js/Wrapper.h"
#include "util/Windows.h"
#include "vm/ArrayBufferObject.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/SharedMem.h"
#include "vm/WrapperObject.h"

#include "gc/Nursery-inl.h"
#include "gc/StoreBuffer-inl.h"
#include "vm/ArrayBufferObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

using JS::CanonicalizeNaN;
using JS::ToInt32;
using mozilla::AssertedCast;
using mozilla::WrapToSigned;

DataViewObject* DataViewObject::create(
    JSContext* cx, uint32_t byteOffset, uint32_t byteLength,
    Handle<ArrayBufferObjectMaybeShared*> arrayBuffer, HandleObject proto) {
  if (arrayBuffer->isDetached()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return nullptr;
  }

  DataViewObject* obj = NewObjectWithClassProto<DataViewObject>(cx, proto);
  if (!obj || !obj->init(cx, arrayBuffer, byteOffset, byteLength,
                         /* bytesPerElement = */ 1)) {
    return nullptr;
  }

  return obj;
}

// ES2017 draft rev 931261ecef9b047b14daacf82884134da48dfe0f
// 24.3.2.1 DataView (extracted part of the main algorithm)
bool DataViewObject::getAndCheckConstructorArgs(JSContext* cx,
                                                HandleObject bufobj,
                                                const CallArgs& args,
                                                uint32_t* byteOffsetPtr,
                                                uint32_t* byteLengthPtr) {
  // Step 3.
  if (!IsArrayBufferMaybeShared(bufobj)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE, "DataView",
                              "ArrayBuffer", bufobj->getClass()->name);
    return false;
  }
  Rooted<ArrayBufferObjectMaybeShared*> buffer(
      cx, &AsArrayBufferMaybeShared(bufobj));

  // Step 4.
  uint64_t offset;
  if (!ToIndex(cx, args.get(1), &offset)) {
    return false;
  }

  // Step 5.
  if (buffer->isDetached()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return false;
  }

  // Step 6.
  uint32_t bufferByteLength = buffer->byteLength();

  // Step 7.
  if (offset > bufferByteLength) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_OFFSET_OUT_OF_BUFFER);
    return false;
  }
  MOZ_ASSERT(offset <= INT32_MAX);

  // Step 8.a
  uint64_t viewByteLength = bufferByteLength - offset;
  if (args.hasDefined(2)) {
    // Step 9.a.
    if (!ToIndex(cx, args.get(2), &viewByteLength)) {
      return false;
    }

    MOZ_ASSERT(offset + viewByteLength >= offset,
               "can't overflow: both numbers are less than "
               "DOUBLE_INTEGRAL_PRECISION_LIMIT");

    // Step 9.b.
    if (offset + viewByteLength > bufferByteLength) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_INVALID_DATA_VIEW_LENGTH);
      return false;
    }
  }
  MOZ_ASSERT(viewByteLength <= INT32_MAX);

  *byteOffsetPtr = AssertedCast<uint32_t>(offset);
  *byteLengthPtr = AssertedCast<uint32_t>(viewByteLength);
  return true;
}

bool DataViewObject::constructSameCompartment(JSContext* cx,
                                              HandleObject bufobj,
                                              const CallArgs& args) {
  MOZ_ASSERT(args.isConstructing());
  cx->check(bufobj);

  uint32_t byteOffset, byteLength;
  if (!getAndCheckConstructorArgs(cx, bufobj, args, &byteOffset, &byteLength)) {
    return false;
  }

  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_DataView, &proto)) {
    return false;
  }

  Rooted<ArrayBufferObjectMaybeShared*> buffer(
      cx, &AsArrayBufferMaybeShared(bufobj));
  JSObject* obj =
      DataViewObject::create(cx, byteOffset, byteLength, buffer, proto);
  if (!obj) {
    return false;
  }
  args.rval().setObject(*obj);
  return true;
}

// Create a DataView object in another compartment.
//
// ES6 supports creating a DataView in global A (using global A's DataView
// constructor) backed by an ArrayBuffer created in global B.
//
// Our DataViewObject implementation doesn't support a DataView in
// compartment A backed by an ArrayBuffer in compartment B. So in this case,
// we create the DataView in B (!) and return a cross-compartment wrapper.
//
// Extra twist: the spec says the new DataView's [[Prototype]] must be
// A's DataView.prototype. So even though we're creating the DataView in B,
// its [[Prototype]] must be (a cross-compartment wrapper for) the
// DataView.prototype in A.
bool DataViewObject::constructWrapped(JSContext* cx, HandleObject bufobj,
                                      const CallArgs& args) {
  MOZ_ASSERT(args.isConstructing());
  MOZ_ASSERT(bufobj->is<WrapperObject>());

  RootedObject unwrapped(cx, CheckedUnwrapStatic(bufobj));
  if (!unwrapped) {
    ReportAccessDenied(cx);
    return false;
  }

  // NB: This entails the IsArrayBuffer check
  uint32_t byteOffset, byteLength;
  if (!getAndCheckConstructorArgs(cx, unwrapped, args, &byteOffset,
                                  &byteLength)) {
    return false;
  }

  // Make sure to get the [[Prototype]] for the created view from this
  // compartment.
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_DataView, &proto)) {
    return false;
  }

  Rooted<GlobalObject*> global(cx, cx->realm()->maybeGlobal());
  if (!proto) {
    proto = GlobalObject::getOrCreateDataViewPrototype(cx, global);
    if (!proto) {
      return false;
    }
  }

  RootedObject dv(cx);
  {
    JSAutoRealm ar(cx, unwrapped);

    Rooted<ArrayBufferObjectMaybeShared*> buffer(cx);
    buffer = &unwrapped->as<ArrayBufferObjectMaybeShared>();

    RootedObject wrappedProto(cx, proto);
    if (!cx->compartment()->wrap(cx, &wrappedProto)) {
      return false;
    }

    dv = DataViewObject::create(cx, byteOffset, byteLength, buffer,
                                wrappedProto);
    if (!dv) {
      return false;
    }
  }

  if (!cx->compartment()->wrap(cx, &dv)) {
    return false;
  }

  args.rval().setObject(*dv);
  return true;
}

bool DataViewObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "DataView")) {
    return false;
  }

  RootedObject bufobj(cx);
  if (!GetFirstArgumentAsObject(cx, args, "DataView constructor", &bufobj)) {
    return false;
  }

  if (bufobj->is<WrapperObject>()) {
    return constructWrapped(cx, bufobj, args);
  }
  return constructSameCompartment(cx, bufobj, args);
}

template <typename NativeType>
/* static */
SharedMem<uint8_t*> DataViewObject::getDataPointer(JSContext* cx,
                                                   Handle<DataViewObject*> obj,
                                                   uint64_t offset,
                                                   bool* isSharedMemory) {
  const size_t TypeSize = sizeof(NativeType);
  if (offset > UINT32_MAX - TypeSize || offset + TypeSize > obj->byteLength()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_OFFSET_OUT_OF_DATAVIEW);
    return SharedMem<uint8_t*>::unshared(nullptr);
  }

  MOZ_ASSERT(offset < UINT32_MAX);
  *isSharedMemory = obj->isSharedMemory();
  return obj->dataPointerEither().cast<uint8_t*>() + uint32_t(offset);
}

static inline bool needToSwapBytes(bool littleEndian) {
#if MOZ_LITTLE_ENDIAN()
  return !littleEndian;
#else
  return littleEndian;
#endif
}

static inline uint8_t swapBytes(uint8_t x) { return x; }

static inline uint16_t swapBytes(uint16_t x) {
  return ((x & 0xff) << 8) | (x >> 8);
}

static inline uint32_t swapBytes(uint32_t x) {
  return ((x & 0xff) << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) |
         ((x & 0xff000000) >> 24);
}

static inline uint64_t swapBytes(uint64_t x) {
  uint32_t a = x & UINT32_MAX;
  uint32_t b = x >> 32;
  return (uint64_t(swapBytes(a)) << 32) | swapBytes(b);
}

template <typename DataType>
struct DataToRepType {
  using result = DataType;
};
template <>
struct DataToRepType<int8_t> {
  using result = uint8_t;
};
template <>
struct DataToRepType<uint8_t> {
  using result = uint8_t;
};
template <>
struct DataToRepType<int16_t> {
  using result = uint16_t;
};
template <>
struct DataToRepType<uint16_t> {
  using result = uint16_t;
};
template <>
struct DataToRepType<int32_t> {
  using result = uint32_t;
};
template <>
struct DataToRepType<uint32_t> {
  using result = uint32_t;
};
template <>
struct DataToRepType<int64_t> {
  using result = uint64_t;
};
template <>
struct DataToRepType<uint64_t> {
  using result = uint64_t;
};
template <>
struct DataToRepType<float> {
  using result = uint32_t;
};
template <>
struct DataToRepType<double> {
  using result = uint64_t;
};

static inline void Memcpy(uint8_t* dest, uint8_t* src, size_t nbytes) {
  memcpy(dest, src, nbytes);
}

static inline void Memcpy(uint8_t* dest, SharedMem<uint8_t*> src,
                          size_t nbytes) {
  jit::AtomicOperations::memcpySafeWhenRacy(dest, src, nbytes);
}

static inline void Memcpy(SharedMem<uint8_t*> dest, uint8_t* src,
                          size_t nbytes) {
  jit::AtomicOperations::memcpySafeWhenRacy(dest, src, nbytes);
}

template <typename DataType, typename BufferPtrType>
struct DataViewIO {
  using ReadWriteType = typename DataToRepType<DataType>::result;

  static constexpr auto alignMask =
      std::min<size_t>(MOZ_ALIGNOF(void*), sizeof(DataType)) - 1;

  static void fromBuffer(DataType* dest, BufferPtrType unalignedBuffer,
                         bool wantSwap) {
    MOZ_ASSERT((reinterpret_cast<uintptr_t>(dest) & alignMask) == 0);
    Memcpy((uint8_t*)dest, unalignedBuffer, sizeof(ReadWriteType));
    if (wantSwap) {
      ReadWriteType* rwDest = reinterpret_cast<ReadWriteType*>(dest);
      *rwDest = swapBytes(*rwDest);
    }
  }

  static void toBuffer(BufferPtrType unalignedBuffer, const DataType* src,
                       bool wantSwap) {
    MOZ_ASSERT((reinterpret_cast<uintptr_t>(src) & alignMask) == 0);
    ReadWriteType temp = *reinterpret_cast<const ReadWriteType*>(src);
    if (wantSwap) {
      temp = swapBytes(temp);
    }
    Memcpy(unalignedBuffer, (uint8_t*)&temp, sizeof(ReadWriteType));
  }
};

template <typename NativeType>
/* static */
bool DataViewObject::read(JSContext* cx, Handle<DataViewObject*> obj,
                          const CallArgs& args, NativeType* val) {
  // Steps 1-2. done by the caller
  // Step 3. unnecessary assert

  // Step 4.
  uint64_t getIndex;
  if (!ToIndex(cx, args.get(0), &getIndex)) {
    return false;
  }

  // Step 5.
  bool isLittleEndian = args.length() >= 2 && ToBoolean(args[1]);

  // Steps 6-7.
  if (obj->hasDetachedBuffer()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return false;
  }

  // Steps 8-12.
  bool isSharedMemory;
  SharedMem<uint8_t*> data = DataViewObject::getDataPointer<NativeType>(
      cx, obj, getIndex, &isSharedMemory);
  if (!data) {
    return false;
  }

  // Step 13.
  if (isSharedMemory) {
    DataViewIO<NativeType, SharedMem<uint8_t*>>::fromBuffer(
        val, data, needToSwapBytes(isLittleEndian));
  } else {
    DataViewIO<NativeType, uint8_t*>::fromBuffer(
        val, data.unwrapUnshared(), needToSwapBytes(isLittleEndian));
  }
  return true;
}

template <typename T>
static inline T WrappingConvert(int32_t value) {
  if (std::is_unsigned_v<T>) {
    return static_cast<T>(value);
  }

  return WrapToSigned(static_cast<typename std::make_unsigned_t<T>>(value));
}

template <typename NativeType>
static inline bool WebIDLCast(JSContext* cx, HandleValue value,
                              NativeType* out) {
  int32_t i;
  if (!ToInt32(cx, value, &i)) {
    return false;
  }

  *out = WrappingConvert<NativeType>(i);
  return true;
}

template <>
inline bool WebIDLCast<int64_t>(JSContext* cx, HandleValue value,
                                int64_t* out) {
  BigInt* bi = ToBigInt(cx, value);
  if (!bi) {
    return false;
  }
  *out = BigInt::toInt64(bi);
  return true;
}

template <>
inline bool WebIDLCast<uint64_t>(JSContext* cx, HandleValue value,
                                 uint64_t* out) {
  BigInt* bi = ToBigInt(cx, value);
  if (!bi) {
    return false;
  }
  *out = BigInt::toUint64(bi);
  return true;
}

template <>
inline bool WebIDLCast<float>(JSContext* cx, HandleValue value, float* out) {
  double temp;
  if (!ToNumber(cx, value, &temp)) {
    return false;
  }
  *out = static_cast<float>(temp);
  return true;
}

template <>
inline bool WebIDLCast<double>(JSContext* cx, HandleValue value, double* out) {
  return ToNumber(cx, value, out);
}

// https://tc39.github.io/ecma262/#sec-setviewvalue
// SetViewValue ( view, requestIndex, isLittleEndian, type, value )
template <typename NativeType>
/* static */
bool DataViewObject::write(JSContext* cx, Handle<DataViewObject*> obj,
                           const CallArgs& args) {
  // Steps 1-2. done by the caller
  // Step 3. unnecessary assert

  // Step 4.
  uint64_t getIndex;
  if (!ToIndex(cx, args.get(0), &getIndex)) {
    return false;
  }

  // Step 5. Extended by the BigInt proposal to call either ToBigInt or ToNumber
  NativeType value;
  if (!WebIDLCast(cx, args.get(1), &value)) {
    return false;
  }

#ifdef JS_MORE_DETERMINISTIC
  // See the comment in ElementSpecific::doubleToNative.
  if (TypeIsFloatingPoint<NativeType>()) {
    value = JS::CanonicalizeNaN(value);
  }
#endif

  // Step 6.
  bool isLittleEndian = args.length() >= 3 && ToBoolean(args[2]);

  // Steps 7-8.
  if (obj->hasDetachedBuffer()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return false;
  }

  // Steps 9-13.
  bool isSharedMemory;
  SharedMem<uint8_t*> data = DataViewObject::getDataPointer<NativeType>(
      cx, obj, getIndex, &isSharedMemory);
  if (!data) {
    return false;
  }

  // Step 14.
  if (isSharedMemory) {
    DataViewIO<NativeType, SharedMem<uint8_t*>>::toBuffer(
        data, &value, needToSwapBytes(isLittleEndian));
  } else {
    DataViewIO<NativeType, uint8_t*>::toBuffer(data.unwrapUnshared(), &value,
                                               needToSwapBytes(isLittleEndian));
  }
  return true;
}

bool DataViewObject::getInt8Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  int8_t val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }
  args.rval().setInt32(val);
  return true;
}

bool DataViewObject::fun_getInt8(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getInt8Impl>(cx, args);
}

bool DataViewObject::getUint8Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  uint8_t val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }
  args.rval().setInt32(val);
  return true;
}

bool DataViewObject::fun_getUint8(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getUint8Impl>(cx, args);
}

bool DataViewObject::getInt16Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  int16_t val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }
  args.rval().setInt32(val);
  return true;
}

bool DataViewObject::fun_getInt16(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getInt16Impl>(cx, args);
}

bool DataViewObject::getUint16Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  uint16_t val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }
  args.rval().setInt32(val);
  return true;
}

bool DataViewObject::fun_getUint16(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getUint16Impl>(cx, args);
}

bool DataViewObject::getInt32Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  int32_t val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }
  args.rval().setInt32(val);
  return true;
}

bool DataViewObject::fun_getInt32(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getInt32Impl>(cx, args);
}

bool DataViewObject::getUint32Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  uint32_t val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }
  args.rval().setNumber(val);
  return true;
}

bool DataViewObject::fun_getUint32(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getUint32Impl>(cx, args);
}

// BigInt proposal 7.26
// DataView.prototype.getBigInt64 ( byteOffset [ , littleEndian ] )
bool DataViewObject::getBigInt64Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  int64_t val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }

  BigInt* bi = BigInt::createFromInt64(cx, val);
  if (!bi) {
    return false;
  }
  args.rval().setBigInt(bi);
  return true;
}

bool DataViewObject::fun_getBigInt64(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getBigInt64Impl>(cx, args);
}

// BigInt proposal 7.27
// DataView.prototype.getBigUint64 ( byteOffset [ , littleEndian ] )
bool DataViewObject::getBigUint64Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  int64_t val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }

  BigInt* bi = BigInt::createFromUint64(cx, val);
  if (!bi) {
    return false;
  }
  args.rval().setBigInt(bi);
  return true;
}

bool DataViewObject::fun_getBigUint64(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getBigUint64Impl>(cx, args);
}

bool DataViewObject::getFloat32Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  float val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }

  args.rval().setDouble(CanonicalizeNaN(val));
  return true;
}

bool DataViewObject::fun_getFloat32(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getFloat32Impl>(cx, args);
}

bool DataViewObject::getFloat64Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  double val;
  if (!read(cx, thisView, args, &val)) {
    return false;
  }

  args.rval().setDouble(CanonicalizeNaN(val));
  return true;
}

bool DataViewObject::fun_getFloat64(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, getFloat64Impl>(cx, args);
}

bool DataViewObject::setInt8Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<int8_t>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setInt8(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setInt8Impl>(cx, args);
}

bool DataViewObject::setUint8Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<uint8_t>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setUint8(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setUint8Impl>(cx, args);
}

bool DataViewObject::setInt16Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<int16_t>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setInt16(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setInt16Impl>(cx, args);
}

bool DataViewObject::setUint16Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<uint16_t>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setUint16(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setUint16Impl>(cx, args);
}

bool DataViewObject::setInt32Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<int32_t>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setInt32(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setInt32Impl>(cx, args);
}

bool DataViewObject::setUint32Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<uint32_t>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setUint32(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setUint32Impl>(cx, args);
}

// BigInt proposal 7.28
// DataView.prototype.setBigInt64 ( byteOffset, value [ , littleEndian ] )
bool DataViewObject::setBigInt64Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<int64_t>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setBigInt64(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setBigInt64Impl>(cx, args);
}

// BigInt proposal 7.29
// DataView.prototype.setBigUint64 ( byteOffset, value [ , littleEndian ] )
bool DataViewObject::setBigUint64Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<uint64_t>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setBigUint64(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setBigUint64Impl>(cx, args);
}

bool DataViewObject::setFloat32Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<float>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setFloat32(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setFloat32Impl>(cx, args);
}

bool DataViewObject::setFloat64Impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  if (!write<double>(cx, thisView, args)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

bool DataViewObject::fun_setFloat64(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, setFloat64Impl>(cx, args);
}

bool DataViewObject::bufferGetterImpl(JSContext* cx, const CallArgs& args) {
  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());
  args.rval().set(DataViewObject::bufferValue(thisView));
  return true;
}

bool DataViewObject::bufferGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, bufferGetterImpl>(cx, args);
}

bool DataViewObject::byteLengthGetterImpl(JSContext* cx, const CallArgs& args) {
  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  // Step 6.
  if (thisView->hasDetachedBuffer()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return false;
  }

  // Step 7.
  args.rval().set(DataViewObject::byteLengthValue(thisView));
  return true;
}

bool DataViewObject::byteLengthGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, byteLengthGetterImpl>(cx, args);
}

bool DataViewObject::byteOffsetGetterImpl(JSContext* cx, const CallArgs& args) {
  Rooted<DataViewObject*> thisView(
      cx, &args.thisv().toObject().as<DataViewObject>());

  // Step 6.
  if (thisView->hasDetachedBuffer()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return false;
  }

  // Step 7.
  args.rval().set(DataViewObject::byteOffsetValue(thisView));
  return true;
}

bool DataViewObject::byteOffsetGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<is, byteOffsetGetterImpl>(cx, args);
}

JSObject* DataViewObject::CreatePrototype(JSContext* cx, JSProtoKey key) {
  return GlobalObject::createBlankPrototype(cx, cx->global(),
                                            &DataViewObject::protoClass_);
}

static const JSClassOps DataViewObjectClassOps = {
    nullptr,                       // addProperty
    nullptr,                       // delProperty
    nullptr,                       // enumerate
    nullptr,                       // newEnumerate
    nullptr,                       // resolve
    nullptr,                       // mayResolve
    nullptr,                       // finalize
    nullptr,                       // call
    nullptr,                       // hasInstance
    nullptr,                       // construct
    ArrayBufferViewObject::trace,  // trace
};

const ClassSpec DataViewObject::classSpec_ = {
    GenericCreateConstructor<DataViewObject::construct, 1,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<DataViewObject>,
    nullptr,
    nullptr,
    DataViewObject::methods,
    DataViewObject::properties};

const JSClass DataViewObject::class_ = {
    "DataView",
    JSCLASS_HAS_PRIVATE |
        JSCLASS_HAS_RESERVED_SLOTS(DataViewObject::RESERVED_SLOTS) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_DataView),
    &DataViewObjectClassOps, &DataViewObject::classSpec_};

const JSClass DataViewObject::protoClass_ = {
    js_Object_str, JSCLASS_HAS_CACHED_PROTO(JSProto_DataView),
    JS_NULL_CLASS_OPS, &DataViewObject::classSpec_};

const JSFunctionSpec DataViewObject::methods[] = {
    JS_FN("getInt8", DataViewObject::fun_getInt8, 1, 0),
    JS_FN("getUint8", DataViewObject::fun_getUint8, 1, 0),
    JS_FN("getInt16", DataViewObject::fun_getInt16, 1, 0),
    JS_FN("getUint16", DataViewObject::fun_getUint16, 1, 0),
    JS_FN("getInt32", DataViewObject::fun_getInt32, 1, 0),
    JS_FN("getUint32", DataViewObject::fun_getUint32, 1, 0),
    JS_FN("getFloat32", DataViewObject::fun_getFloat32, 1, 0),
    JS_FN("getFloat64", DataViewObject::fun_getFloat64, 1, 0),
    JS_FN("getBigInt64", DataViewObject::fun_getBigInt64, 1, 0),
    JS_FN("getBigUint64", DataViewObject::fun_getBigUint64, 1, 0),
    JS_FN("setInt8", DataViewObject::fun_setInt8, 2, 0),
    JS_FN("setUint8", DataViewObject::fun_setUint8, 2, 0),
    JS_FN("setInt16", DataViewObject::fun_setInt16, 2, 0),
    JS_FN("setUint16", DataViewObject::fun_setUint16, 2, 0),
    JS_FN("setInt32", DataViewObject::fun_setInt32, 2, 0),
    JS_FN("setUint32", DataViewObject::fun_setUint32, 2, 0),
    JS_FN("setFloat32", DataViewObject::fun_setFloat32, 2, 0),
    JS_FN("setFloat64", DataViewObject::fun_setFloat64, 2, 0),
    JS_FN("setBigInt64", DataViewObject::fun_setBigInt64, 2, 0),
    JS_FN("setBigUint64", DataViewObject::fun_setBigUint64, 2, 0),
    JS_FS_END};

const JSPropertySpec DataViewObject::properties[] = {
    JS_PSG("buffer", DataViewObject::bufferGetter, 0),
    JS_PSG("byteLength", DataViewObject::byteLengthGetter, 0),
    JS_PSG("byteOffset", DataViewObject::byteOffsetGetter, 0),
    JS_STRING_SYM_PS(toStringTag, "DataView", JSPROP_READONLY), JS_PS_END};

JS_FRIEND_API JSObject* JS_NewDataView(JSContext* cx, HandleObject buffer,
                                       uint32_t byteOffset,
                                       int32_t byteLength) {
  JSProtoKey key = JSProto_DataView;
  RootedObject constructor(cx, GlobalObject::getOrCreateConstructor(cx, key));
  if (!constructor) {
    return nullptr;
  }

  FixedConstructArgs<3> cargs(cx);

  cargs[0].setObject(*buffer);
  cargs[1].setNumber(byteOffset);
  cargs[2].setInt32(byteLength);

  RootedValue fun(cx, ObjectValue(*constructor));
  RootedObject obj(cx);
  if (!Construct(cx, fun, cargs, fun, &obj)) {
    return nullptr;
  }
  return obj;
}
