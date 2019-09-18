/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_key_h__
#define mozilla_dom_indexeddb_key_h__

#include "mozilla/dom/indexedDB/IDBResult.h"

#include "js/RootingAPI.h"
#include "jsapi.h"
#include "mozilla/ErrorResult.h"
#include "nsString.h"

class mozIStorageStatement;
class mozIStorageValueArray;

namespace IPC {

template <typename>
struct ParamTraits;

}  // namespace IPC

namespace mozilla {
namespace dom {
namespace indexedDB {

class Key {
  friend struct IPC::ParamTraits<Key>;

  nsCString mBuffer;

 public:
  enum {
    eTerminator = 0,
    eFloat = 0x10,
    eDate = 0x20,
    eString = 0x30,
    eBinary = 0x40,
    eArray = 0x50,
    eMaxType = eArray
  };

  static const uint8_t kMaxArrayCollapse = uint8_t(3);
  static const uint8_t kMaxRecursionDepth = uint8_t(64);

  Key() { Unset(); }

  explicit Key(const nsACString& aBuffer) : mBuffer(aBuffer) {}

  Key& operator=(int64_t aInt) {
    SetFromInteger(aInt);
    return *this;
  }

  bool operator==(const Key& aOther) const {
    MOZ_ASSERT(!mBuffer.IsVoid());
    MOZ_ASSERT(!aOther.mBuffer.IsVoid());

    return mBuffer.Equals(aOther.mBuffer);
  }

  bool operator!=(const Key& aOther) const {
    MOZ_ASSERT(!mBuffer.IsVoid());
    MOZ_ASSERT(!aOther.mBuffer.IsVoid());

    return !mBuffer.Equals(aOther.mBuffer);
  }

  bool operator<(const Key& aOther) const {
    MOZ_ASSERT(!mBuffer.IsVoid());
    MOZ_ASSERT(!aOther.mBuffer.IsVoid());

    return Compare(mBuffer, aOther.mBuffer) < 0;
  }

  bool operator>(const Key& aOther) const {
    MOZ_ASSERT(!mBuffer.IsVoid());
    MOZ_ASSERT(!aOther.mBuffer.IsVoid());

    return Compare(mBuffer, aOther.mBuffer) > 0;
  }

  bool operator<=(const Key& aOther) const {
    MOZ_ASSERT(!mBuffer.IsVoid());
    MOZ_ASSERT(!aOther.mBuffer.IsVoid());

    return Compare(mBuffer, aOther.mBuffer) <= 0;
  }

  bool operator>=(const Key& aOther) const {
    MOZ_ASSERT(!mBuffer.IsVoid());
    MOZ_ASSERT(!aOther.mBuffer.IsVoid());

    return Compare(mBuffer, aOther.mBuffer) >= 0;
  }

  void Unset() { mBuffer.SetIsVoid(true); }

  bool IsUnset() const { return mBuffer.IsVoid(); }

  bool IsFloat() const { return !IsUnset() && *BufferStart() == eFloat; }

  bool IsDate() const { return !IsUnset() && *BufferStart() == eDate; }

  bool IsString() const { return !IsUnset() && *BufferStart() == eString; }

  bool IsBinary() const { return !IsUnset() && *BufferStart() == eBinary; }

  bool IsArray() const { return !IsUnset() && *BufferStart() >= eArray; }

  double ToFloat() const {
    MOZ_ASSERT(IsFloat());
    const EncodedDataType* pos = BufferStart();
    double res = DecodeNumber(pos, BufferEnd());
    MOZ_ASSERT(pos >= BufferEnd());
    return res;
  }

  double ToDateMsec() const {
    MOZ_ASSERT(IsDate());
    const EncodedDataType* pos = BufferStart();
    double res = DecodeNumber(pos, BufferEnd());
    MOZ_ASSERT(pos >= BufferEnd());
    return res;
  }

  void ToString(nsString& aString) const {
    MOZ_ASSERT(IsString());
    const EncodedDataType* pos = BufferStart();
    DecodeString(pos, BufferEnd(), aString);
    MOZ_ASSERT(pos >= BufferEnd());
  }

  IDBResult<void, IDBSpecialValue::Invalid> SetFromString(
      const nsAString& aString, ErrorResult& aRv);

  void SetFromInteger(int64_t aInt) {
    mBuffer.Truncate();
    EncodeNumber(double(aInt), eFloat);
    TrimBuffer();
  }

  // This function implements the standard algorithm "convert a value to a key".
  // A key return value is indicated by returning `true` whereas `false` means
  // either invalid (if `aRv.Failed()` is `false`) or an exception (otherwise).
  IDBResult<void, IDBSpecialValue::Invalid> SetFromJSVal(
      JSContext* aCx, JS::Handle<JS::Value> aVal, ErrorResult& aRv);

  nsresult ToJSVal(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) const;

  nsresult ToJSVal(JSContext* aCx, JS::Heap<JS::Value>& aVal) const;

  // See SetFromJSVal() for the meaning of values returned by this function.
  IDBResult<void, IDBSpecialValue::Invalid> AppendItem(
      JSContext* aCx, bool aFirstOfArray, JS::Handle<JS::Value> aVal,
      ErrorResult& aRv);

  IDBResult<void, IDBSpecialValue::Invalid> ToLocaleBasedKey(
      Key& aTarget, const nsCString& aLocale, ErrorResult& aRv) const;

  void FinishArray() { TrimBuffer(); }

  const nsCString& GetBuffer() const { return mBuffer; }

  nsresult BindToStatement(mozIStorageStatement* aStatement,
                           const nsACString& aParamName) const;

  nsresult SetFromStatement(mozIStorageStatement* aStatement, uint32_t aIndex);

  nsresult SetFromValueArray(mozIStorageValueArray* aValues, uint32_t aIndex);

  static int16_t CompareKeys(const Key& aFirst, const Key& aSecond) {
    int32_t result = Compare(aFirst.mBuffer, aSecond.mBuffer);

    if (result < 0) {
      return -1;
    }

    if (result > 0) {
      return 1;
    }

    return 0;
  }

  // Implementation of the array branch of step 3 of
  // https://w3c.github.io/IndexedDB/#convert-value-to-key
  template <typename ArrayConversionPolicy>
  static IDBResult<void, IDBSpecialValue::Invalid> ConvertArrayValueToKey(
      JSContext* const aCx, JS::HandleObject aObject,
      ArrayConversionPolicy&& aPolicy, ErrorResult& aRv) {
    // 1. Let `len` be ? ToLength( ? Get(`input`, "length")).
    uint32_t len;
    if (!JS_GetArrayLength(aCx, aObject, &len)) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return Exception;
    }

    // 2. Add `input` to `seen`.
    aPolicy.AddToSeenSet(aCx, aObject);

    // 3. Let `keys` be a new empty list.
    aPolicy.BeginSubkeyList();

    // 4. Let `index` be 0.
    uint32_t index = 0;

    // 5. While `index` is less than `len`:
    while (index < len) {
      JS::RootedId indexId(aCx);
      if (!JS_IndexToId(aCx, index, &indexId)) {
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
        return Exception;
      }

      // 1. Let `hop` be ? HasOwnProperty(`input`, `index`).
      bool hop;
      if (!JS_HasOwnPropertyById(aCx, aObject, indexId, &hop)) {
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
        return Exception;
      }

      // 2. If `hop` is false, return invalid.
      if (!hop) {
        return Invalid;
      }

      // 3. Let `entry` be ? Get(`input`, `index`).
      JS::RootedValue entry(aCx);
      if (!JS_GetPropertyById(aCx, aObject, indexId, &entry)) {
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
        return Exception;
      }

      // 4. Let `key` be the result of running the steps to convert a value to a
      //    key with arguments `entry` and `seen`.
      // 5. ReturnIfAbrupt(`key`).
      // 6. If `key` is invalid abort these steps and return invalid.
      // 7. Append `key` to `keys`.
      auto result = aPolicy.ConvertSubkey(aCx, entry, index, aRv);
      if (!result.Is(Ok, aRv)) {
        return result;
      }

      // 8. Increase `index` by 1.
      index += 1;
    }

    // 6. Return a new array key with value `keys`.
    aPolicy.EndSubkeyList();
    return Ok();
  }

 private:
  class MOZ_STACK_CLASS ArrayValueEncoder;

  using EncodedDataType = unsigned char;

  const EncodedDataType* BufferStart() const {
    // TODO it would be nicer if mBuffer was also using EncodedDataType
    return reinterpret_cast<const EncodedDataType*>(mBuffer.BeginReading());
  }

  const EncodedDataType* BufferEnd() const {
    return reinterpret_cast<const EncodedDataType*>(mBuffer.EndReading());
  }

  // Encoding helper. Trims trailing zeros off of mBuffer as a post-processing
  // step.
  void TrimBuffer() {
    const char* end = mBuffer.EndReading() - 1;
    while (!*end) {
      --end;
    }

    mBuffer.Truncate(end + 1 - mBuffer.BeginReading());
  }

  // Encoding functions. These append the encoded value to the end of mBuffer
  IDBResult<void, IDBSpecialValue::Invalid> EncodeJSVal(
      JSContext* aCx, JS::Handle<JS::Value> aVal, uint8_t aTypeOffset,
      ErrorResult& aRv);

  IDBResult<void, IDBSpecialValue::Invalid> EncodeString(
      const nsAString& aString, uint8_t aTypeOffset, ErrorResult& aRv);

  template <typename T>
  IDBResult<void, IDBSpecialValue::Invalid> EncodeString(const T* aStart,
                                                         const T* aEnd,
                                                         uint8_t aTypeOffset,
                                                         ErrorResult& aRv);

  template <typename T>
  IDBResult<void, IDBSpecialValue::Invalid> EncodeAsString(const T* aStart,
                                                           const T* aEnd,
                                                           uint8_t aType,
                                                           ErrorResult& aRv);

  IDBResult<void, IDBSpecialValue::Invalid> EncodeLocaleString(
      const nsDependentString& aString, uint8_t aTypeOffset,
      const nsCString& aLocale, ErrorResult& aRv);

  void EncodeNumber(double aFloat, uint8_t aType);

  IDBResult<void, IDBSpecialValue::Invalid> EncodeBinary(JSObject* aObject,
                                                         bool aIsViewObject,
                                                         uint8_t aTypeOffset,
                                                         ErrorResult& aRv);

  // Decoding functions. aPos points into mBuffer and is adjusted to point
  // past the consumed value. (Note: this may be beyond aEnd).
  static nsresult DecodeJSVal(const EncodedDataType*& aPos,
                              const EncodedDataType* aEnd, JSContext* aCx,
                              JS::MutableHandle<JS::Value> aVal);

  static void DecodeString(const EncodedDataType*& aPos,
                           const EncodedDataType* aEnd, nsString& aString);

  static double DecodeNumber(const EncodedDataType*& aPos,
                             const EncodedDataType* aEnd);

  static JSObject* DecodeBinary(const EncodedDataType*& aPos,
                                const EncodedDataType* aEnd, JSContext* aCx);

  // Returns the size of the decoded data for stringy (string or binary),
  // excluding a null terminator.
  // On return, aOutSectionEnd points to the last byte behind the current
  // encoded section, i.e. either aEnd, or the eTerminator.
  // T is the base type for the decoded data.
  template <typename T>
  static uint32_t CalcDecodedStringySize(
      const EncodedDataType* aBegin, const EncodedDataType* aEnd,
      const EncodedDataType** aOutEncodedSectionEnd);

  static uint32_t LengthOfEncodedBinary(const EncodedDataType* aPos,
                                        const EncodedDataType* aEnd);

  template <typename T>
  static void DecodeAsStringy(const EncodedDataType* aEncodedSectionBegin,
                              const EncodedDataType* aEncodedSectionEnd,
                              uint32_t aDecodedLength, T* aOut);

  template <EncodedDataType TypeMask, typename T, typename AcquireBuffer,
            typename AcquireEmpty>
  static void DecodeStringy(const EncodedDataType*& aPos,
                            const EncodedDataType* aEnd,
                            const AcquireBuffer& acquireBuffer,
                            const AcquireEmpty& acquireEmpty);

  IDBResult<void, IDBSpecialValue::Invalid> EncodeJSValInternal(
      JSContext* aCx, JS::Handle<JS::Value> aVal, uint8_t aTypeOffset,
      uint16_t aRecursionDepth, ErrorResult& aRv);

  static nsresult DecodeJSValInternal(const EncodedDataType*& aPos,
                                      const EncodedDataType* aEnd,
                                      JSContext* aCx, uint8_t aTypeOffset,
                                      JS::MutableHandle<JS::Value> aVal,
                                      uint16_t aRecursionDepth);

  template <typename T>
  nsresult SetFromSource(T* aSource, uint32_t aIndex);
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_key_h__
