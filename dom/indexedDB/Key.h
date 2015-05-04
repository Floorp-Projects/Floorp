/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_key_h__
#define mozilla_dom_indexeddb_key_h__

#include "js/RootingAPI.h"
#include "nsString.h"

class mozIStorageStatement;
class mozIStorageValueArray;

namespace IPC {

template <typename> struct ParamTraits;

} // namespace IPC

namespace mozilla {
namespace dom {
namespace indexedDB {

class Key
{
  friend struct IPC::ParamTraits<Key>;

  nsCString mBuffer;

public:
  enum {
    eTerminator = 0,
    eFloat = 0x10,
    eDate = 0x20,
    eString = 0x30,
    eArray = 0x50,
    eMaxType = eArray
  };

  static const uint8_t kMaxArrayCollapse = uint8_t(3);
  static const uint8_t kMaxRecursionDepth = uint8_t(64);

  Key()
  {
    Unset();
  }

  explicit
  Key(const nsACString& aBuffer)
    : mBuffer(aBuffer)
  { }

  Key&
  operator=(const nsAString& aString)
  {
    SetFromString(aString);
    return *this;
  }

  Key&
  operator=(int64_t aInt)
  {
    SetFromInteger(aInt);
    return *this;
  }

  bool
  operator==(const Key& aOther) const
  {
    Assert(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid());

    return mBuffer.Equals(aOther.mBuffer);
  }

  bool
  operator!=(const Key& aOther) const
  {
    Assert(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid());

    return !mBuffer.Equals(aOther.mBuffer);
  }

  bool
  operator<(const Key& aOther) const
  {
    Assert(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid());

    return Compare(mBuffer, aOther.mBuffer) < 0;
  }

  bool
  operator>(const Key& aOther) const
  {
    Assert(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid());

    return Compare(mBuffer, aOther.mBuffer) > 0;
  }

  bool
  operator<=(const Key& aOther) const
  {
    Assert(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid());

    return Compare(mBuffer, aOther.mBuffer) <= 0;
  }

  bool
  operator>=(const Key& aOther) const
  {
    Assert(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid());

    return Compare(mBuffer, aOther.mBuffer) >= 0;
  }

  void
  Unset()
  {
    mBuffer.SetIsVoid(true);
  }

  bool
  IsUnset() const
  {
    return mBuffer.IsVoid();
  }

  bool
  IsFloat() const
  {
    return !IsUnset() && *BufferStart() == eFloat;
  }

  bool
  IsDate() const
  {
    return !IsUnset() && *BufferStart() == eDate;
  }

  bool
  IsString() const
  {
    return !IsUnset() && *BufferStart() == eString;
  }

  bool
  IsArray() const
  {
    return !IsUnset() && *BufferStart() >= eArray;
  }

  double
  ToFloat() const
  {
    Assert(IsFloat());
    const unsigned char* pos = BufferStart();
    double res = DecodeNumber(pos, BufferEnd());
    Assert(pos >= BufferEnd());
    return res;
  }

  double
  ToDateMsec() const
  {
    Assert(IsDate());
    const unsigned char* pos = BufferStart();
    double res = DecodeNumber(pos, BufferEnd());
    Assert(pos >= BufferEnd());
    return res;
  }

  void
  ToString(nsString& aString) const
  {
    Assert(IsString());
    const unsigned char* pos = BufferStart();
    DecodeString(pos, BufferEnd(), aString);
    Assert(pos >= BufferEnd());
  }

  void
  SetFromString(const nsAString& aString)
  {
    mBuffer.Truncate();
    EncodeString(aString, 0);
    TrimBuffer();
  }

  void
  SetFromInteger(int64_t aInt)
  {
    mBuffer.Truncate();
    EncodeNumber(double(aInt), eFloat);
    TrimBuffer();
  }

  nsresult
  SetFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aVal);

  nsresult
  ToJSVal(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) const;

  nsresult
  ToJSVal(JSContext* aCx, JS::Heap<JS::Value>& aVal) const;

  nsresult
  AppendItem(JSContext* aCx, bool aFirstOfArray, JS::Handle<JS::Value> aVal);

  void
  FinishArray()
  {
    TrimBuffer();
  }

  const nsCString&
  GetBuffer() const
  {
    return mBuffer;
  }

  nsresult
  BindToStatement(mozIStorageStatement* aStatement,
                  const nsACString& aParamName) const;

  nsresult
  SetFromStatement(mozIStorageStatement* aStatement, uint32_t aIndex);

  nsresult
  SetFromValueArray(mozIStorageValueArray* aValues, uint32_t aIndex);

  static int16_t
  CompareKeys(Key& aFirst, Key& aSecond)
  {
    int32_t result = Compare(aFirst.mBuffer, aSecond.mBuffer);

    if (result < 0) {
      return -1;
    }

    if (result > 0) {
      return 1;
    }

    return 0;
  }

private:
  const unsigned char*
  BufferStart() const
  {
    return reinterpret_cast<const unsigned char*>(mBuffer.BeginReading());
  }

  const unsigned char*
  BufferEnd() const
  {
    return reinterpret_cast<const unsigned char*>(mBuffer.EndReading());
  }

  // Encoding helper. Trims trailing zeros off of mBuffer as a post-processing
  // step.
  void
  TrimBuffer()
  {
    const char* end = mBuffer.EndReading() - 1;
    while (!*end) {
      --end;
    }

    mBuffer.Truncate(end + 1 - mBuffer.BeginReading());
  }

  // Encoding functions. These append the encoded value to the end of mBuffer
  nsresult
  EncodeJSVal(JSContext* aCx, JS::Handle<JS::Value> aVal, uint8_t aTypeOffset);

  void
  EncodeString(const nsAString& aString, uint8_t aTypeOffset);

  void
  EncodeNumber(double aFloat, uint8_t aType);

  // Decoding functions. aPos points into mBuffer and is adjusted to point
  // past the consumed value.
  static nsresult
  DecodeJSVal(const unsigned char*& aPos,
              const unsigned char* aEnd,
              JSContext* aCx,
              JS::MutableHandle<JS::Value> aVal);

  static void
  DecodeString(const unsigned char*& aPos,
               const unsigned char* aEnd,
               nsString& aString);

  static double
  DecodeNumber(const unsigned char*& aPos, const unsigned char* aEnd);

  nsresult
  EncodeJSValInternal(JSContext* aCx,
                      JS::Handle<JS::Value> aVal,
                      uint8_t aTypeOffset,
                      uint16_t aRecursionDepth);

  static nsresult
  DecodeJSValInternal(const unsigned char*& aPos,
                      const unsigned char* aEnd,
                      JSContext* aCx,
                      uint8_t aTypeOffset,
                      JS::MutableHandle<JS::Value> aVal,
                      uint16_t aRecursionDepth);

  template <typename T>
  nsresult
  SetFromSource(T* aSource, uint32_t aIndex);

  void
  Assert(bool aCondition) const
#ifdef DEBUG
  ;
#else
  { }
#endif
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_key_h__
