/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_key_h__
#define mozilla_dom_indexeddb_key_h__

#include "mozilla/dom/indexedDB/IDBResult.h"

class mozIStorageStatement;
class mozIStorageValueArray;

namespace IPC {

template <typename>
struct ParamTraits;

}  // namespace IPC

namespace mozilla::dom::indexedDB {

class Key {
  friend struct IPC::ParamTraits<Key>;

  nsCString mBuffer;
  CopyableTArray<uint32_t> mAutoIncrementKeyOffsets;

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

  explicit Key(nsCString aBuffer) : mBuffer(std::move(aBuffer)) {}

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

  void Unset() {
    mBuffer.SetIsVoid(true);
    mAutoIncrementKeyOffsets.Clear();
  }

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

  nsAutoString ToString() const {
    MOZ_ASSERT(IsString());
    const EncodedDataType* pos = BufferStart();
    auto res = DecodeString(pos, BufferEnd());
    MOZ_ASSERT(pos >= BufferEnd());
    return res;
  }

  Result<Ok, nsresult> SetFromString(const nsAString& aString);

  Result<Ok, nsresult> SetFromInteger(int64_t aInt) {
    mBuffer.Truncate();
    auto ret = EncodeNumber(double(aInt), eFloat);
    TrimBuffer();
    return ret;
  }

  // This function implements the standard algorithm "convert a value to a key".
  // A key return value is indicated by returning `true` whereas `false` means
  // either invalid (if `aRv.Failed()` is `false`) or an exception (otherwise).
  IDBResult<Ok, IDBSpecialValue::Invalid> SetFromJSVal(
      JSContext* aCx, JS::Handle<JS::Value> aVal);

  nsresult ToJSVal(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) const;

  nsresult ToJSVal(JSContext* aCx, JS::Heap<JS::Value>& aVal) const;

  // See SetFromJSVal() for the meaning of values returned by this function.
  IDBResult<Ok, IDBSpecialValue::Invalid> AppendItem(
      JSContext* aCx, bool aFirstOfArray, JS::Handle<JS::Value> aVal);

  Result<Key, nsresult> ToLocaleAwareKey(const nsCString& aLocale) const;

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

  void ReserveAutoIncrementKey(bool aFirstOfArray);

  void MaybeUpdateAutoIncrementKey(int64_t aKey);

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
  IDBResult<Ok, IDBSpecialValue::Invalid> EncodeJSVal(
      JSContext* aCx, JS::Handle<JS::Value> aVal, uint8_t aTypeOffset);

  Result<Ok, nsresult> EncodeString(const nsAString& aString,
                                    uint8_t aTypeOffset);

  template <typename T>
  Result<Ok, nsresult> EncodeString(Span<const T> aInput, uint8_t aTypeOffset);

  template <typename T>
  Result<Ok, nsresult> EncodeAsString(Span<const T> aInput, uint8_t aType);

  Result<Ok, nsresult> EncodeLocaleString(const nsAString& aString,
                                          uint8_t aTypeOffset,
                                          const nsCString& aLocale);

  Result<Ok, nsresult> EncodeNumber(double aFloat, uint8_t aType);

  // Decoding functions. aPos points into mBuffer and is adjusted to point
  // past the consumed value. (Note: this may be beyond aEnd).
  static nsresult DecodeJSVal(const EncodedDataType*& aPos,
                              const EncodedDataType* aEnd, JSContext* aCx,
                              JS::MutableHandle<JS::Value> aVal);

  static nsAutoString DecodeString(const EncodedDataType*& aPos,
                                   const EncodedDataType* aEnd);

  static double DecodeNumber(const EncodedDataType*& aPos,
                             const EncodedDataType* aEnd);

  static JSObject* GetArrayBufferObjectFromDataRange(
      const EncodedDataType*& aPos, const EncodedDataType* aEnd,
      JSContext* aCx);

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

  IDBResult<Ok, IDBSpecialValue::Invalid> EncodeJSValInternal(
      JSContext* aCx, JS::Handle<JS::Value> aVal, uint8_t aTypeOffset,
      uint16_t aRecursionDepth);

  static nsresult DecodeJSValInternal(const EncodedDataType*& aPos,
                                      const EncodedDataType* aEnd,
                                      JSContext* aCx, uint8_t aTypeOffset,
                                      JS::MutableHandle<JS::Value> aVal,
                                      uint16_t aRecursionDepth);

  template <typename T>
  nsresult SetFromSource(T* aSource, uint32_t aIndex);

  void WriteDoubleToUint64(char* aBuffer, double aValue);
};

}  // namespace mozilla::dom::indexedDB

#endif  // mozilla_dom_indexeddb_key_h__
