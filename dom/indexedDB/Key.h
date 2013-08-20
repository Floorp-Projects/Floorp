/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_key_h__
#define mozilla_dom_indexeddb_key_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozIStorageStatement.h"

#include "js/Value.h"

namespace IPC {
template <typename T> struct ParamTraits;
} // namespace IPC

BEGIN_INDEXEDDB_NAMESPACE

class Key
{
  friend struct IPC::ParamTraits<Key>;

public:
  Key()
  {
    Unset();
  }

  Key& operator=(const nsAString& aString)
  {
    SetFromString(aString);
    return *this;
  }

  Key& operator=(int64_t aInt)
  {
    SetFromInteger(aInt);
    return *this;
  }

  bool operator==(const Key& aOther) const
  {
    NS_ASSERTION(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid(),
                 "Don't compare unset keys!");

    return mBuffer.Equals(aOther.mBuffer);
  }

  bool operator!=(const Key& aOther) const
  {
    NS_ASSERTION(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid(),
                 "Don't compare unset keys!");

    return !mBuffer.Equals(aOther.mBuffer);
  }

  bool operator<(const Key& aOther) const
  {
    NS_ASSERTION(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid(),
                 "Don't compare unset keys!");

    return Compare(mBuffer, aOther.mBuffer) < 0;
  }

  bool operator>(const Key& aOther) const
  {
    NS_ASSERTION(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid(),
                 "Don't compare unset keys!");

    return Compare(mBuffer, aOther.mBuffer) > 0;
  }

  bool operator<=(const Key& aOther) const
  {
    NS_ASSERTION(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid(),
                 "Don't compare unset keys!");

    return Compare(mBuffer, aOther.mBuffer) <= 0;
  }

  bool operator>=(const Key& aOther) const
  {
    NS_ASSERTION(!mBuffer.IsVoid() && !aOther.mBuffer.IsVoid(),
                 "Don't compare unset keys!");

    return Compare(mBuffer, aOther.mBuffer) >= 0;
  }

  void
  Unset()
  {
    mBuffer.SetIsVoid(true);
  }

  bool IsUnset() const
  {
    return mBuffer.IsVoid();
  }

  bool IsFloat() const
  {
    return !IsUnset() && mBuffer.First() == eFloat;
  }

  bool IsDate() const
  {
    return !IsUnset() && mBuffer.First() == eDate;
  }

  bool IsString() const
  {
    return !IsUnset() && mBuffer.First() == eString;
  }

  bool IsArray() const
  {
    return !IsUnset() && mBuffer.First() >= eArray;
  }

  double ToFloat() const
  {
    NS_ASSERTION(IsFloat(), "Why'd you call this?");
    const unsigned char* pos = BufferStart();
    double res = DecodeNumber(pos, BufferEnd());
    NS_ASSERTION(pos >= BufferEnd(), "Should consume whole buffer");
    return res;
  }

  double ToDateMsec() const
  {
    NS_ASSERTION(IsDate(), "Why'd you call this?");
    const unsigned char* pos = BufferStart();
    double res = DecodeNumber(pos, BufferEnd());
    NS_ASSERTION(pos >= BufferEnd(), "Should consume whole buffer");
    return res;
  }

  void ToString(nsString& aString) const
  {
    NS_ASSERTION(IsString(), "Why'd you call this?");
    const unsigned char* pos = BufferStart();
    DecodeString(pos, BufferEnd(), aString);
    NS_ASSERTION(pos >= BufferEnd(), "Should consume whole buffer");
  }

  void SetFromString(const nsAString& aString)
  {
    mBuffer.Truncate();
    EncodeString(aString, 0);
    TrimBuffer();
  }

  void SetFromInteger(int64_t aInt)
  {
    mBuffer.Truncate();
    EncodeNumber(double(aInt), eFloat);
    TrimBuffer();
  }

  nsresult SetFromJSVal(JSContext* aCx,
                        const JS::Value aVal)
  {
    mBuffer.Truncate();

    if (JSVAL_IS_NULL(aVal) || JSVAL_IS_VOID(aVal)) {
      Unset();
      return NS_OK;
    }

    nsresult rv = EncodeJSVal(aCx, aVal, 0);
    if (NS_FAILED(rv)) {
      Unset();
      return rv;
    }
    TrimBuffer();

    return NS_OK;
  }

  nsresult ToJSVal(JSContext* aCx,
                   JS::MutableHandle<JS::Value> aVal) const
  {
    if (IsUnset()) {
      aVal.set(JSVAL_VOID);
      return NS_OK;
    }

    const unsigned char* pos = BufferStart();
    nsresult rv = DecodeJSVal(pos, BufferEnd(), aCx, 0, aVal);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(pos >= BufferEnd(),
                 "Didn't consume whole buffer");

    return NS_OK;
  }

  nsresult ToJSVal(JSContext* aCx,
                   JS::Heap<JS::Value>& aVal) const
  {
    JS::Rooted<JS::Value> value(aCx);
    nsresult rv = ToJSVal(aCx, &value);
    if (NS_SUCCEEDED(rv)) {
      aVal = value;
    }
    return rv;
  }

  nsresult AppendItem(JSContext* aCx,
                      bool aFirstOfArray,
                      const JS::Value aVal)
  {
    nsresult rv = EncodeJSVal(aCx, aVal, aFirstOfArray ? eMaxType : 0);
    if (NS_FAILED(rv)) {
      Unset();
      return rv;
    }

    return NS_OK;
  }

  void FinishArray()
  {
    TrimBuffer();
  }

  const nsCString& GetBuffer() const
  {
    return mBuffer;
  }

  nsresult BindToStatement(mozIStorageStatement* aStatement,
                           const nsACString& aParamName) const
  {
    nsresult rv = aStatement->BindBlobByName(aParamName,
      reinterpret_cast<const uint8_t*>(mBuffer.get()), mBuffer.Length());

    return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult SetFromStatement(mozIStorageStatement* aStatement,
                            uint32_t aIndex)
  {
    uint8_t* data;
    uint32_t dataLength = 0;

    nsresult rv = aStatement->GetBlob(aIndex, &dataLength, &data);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    mBuffer.Adopt(
      reinterpret_cast<char*>(const_cast<uint8_t*>(data)), dataLength);

    return NS_OK;
  }

  static
  int16_t CompareKeys(Key& aFirst, Key& aSecond)
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
  const unsigned char* BufferStart() const
  {
    return reinterpret_cast<const unsigned char*>(mBuffer.BeginReading());
  }

  const unsigned char* BufferEnd() const
  {
    return reinterpret_cast<const unsigned char*>(mBuffer.EndReading());
  }

  enum {
    eTerminator = 0,
    eFloat = 1,
    eDate = 2,
    eString = 3,
    eArray = 4,
    eMaxType = eArray
  };

  // Encoding helper. Trims trailing zeros off of mBuffer as a post-processing
  // step.
  void TrimBuffer()
  {
    const char* end = mBuffer.EndReading() - 1;
    while (!*end) {
      --end;
    }

    mBuffer.Truncate(end + 1 - mBuffer.BeginReading());
  }

  // Encoding functions. These append the encoded value to the end of mBuffer
  inline nsresult EncodeJSVal(JSContext* aCx, const JS::Value aVal,
                              uint8_t aTypeOffset)
  {
    return EncodeJSValInternal(aCx, aVal, aTypeOffset, 0);
  }
  void EncodeString(const nsAString& aString, uint8_t aTypeOffset);
  void EncodeNumber(double aFloat, uint8_t aType);

  // Decoding functions. aPos points into mBuffer and is adjusted to point
  // past the consumed value.
  static inline nsresult DecodeJSVal(const unsigned char*& aPos,
                                     const unsigned char* aEnd, JSContext* aCx,
                                     uint8_t aTypeOffset, JS::MutableHandle<JS::Value> aVal)
  {
    return DecodeJSValInternal(aPos, aEnd, aCx, aTypeOffset, aVal, 0);
  }

  static void DecodeString(const unsigned char*& aPos,
                           const unsigned char* aEnd,
                           nsString& aString);
  static double DecodeNumber(const unsigned char*& aPos,
                             const unsigned char* aEnd);

  nsCString mBuffer;

private:
  nsresult EncodeJSValInternal(JSContext* aCx, const JS::Value aVal,
                               uint8_t aTypeOffset, uint16_t aRecursionDepth);

  static nsresult DecodeJSValInternal(const unsigned char*& aPos,
                                      const unsigned char* aEnd,
                                      JSContext* aCx, uint8_t aTypeOffset,
                                      JS::MutableHandle<JS::Value> aVal, uint16_t aRecursionDepth);
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_key_h__ */
