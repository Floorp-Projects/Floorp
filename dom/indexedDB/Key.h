/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_key_h__
#define mozilla_dom_indexeddb_key_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozIStorageStatement.h"

BEGIN_INDEXEDDB_NAMESPACE

class Key
{
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

  Key& operator=(PRInt64 aInt)
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
    return !mBuffer.IsVoid() && mBuffer.First() == eFloat;
  }

  double ToFloat() const
  {
    NS_ASSERTION(IsFloat(), "Why'd you call this?");
    const unsigned char* pos = BufferStart();
    double res = DecodeNumber(pos, BufferEnd());
    NS_ASSERTION(pos >= BufferEnd(), "Should consume whole buffer");
    return res;
  }

  void SetFromString(const nsAString& aString)
  {
    mBuffer.Truncate();
    EncodeString(aString, 0);
    TrimBuffer();
  }

  void SetFromInteger(PRInt64 aInt)
  {
    mBuffer.Truncate();
    EncodeNumber(double(aInt), eFloat);
    TrimBuffer();
  }

  nsresult SetFromJSVal(JSContext* aCx,
                        const jsval aVal)
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
                   jsval* aVal) const
  {
    if (IsUnset()) {
      *aVal = JSVAL_VOID;
      return NS_OK;
    }

    const unsigned char* pos = BufferStart();
    nsresult rv = DecodeJSVal(pos, BufferEnd(), aCx, 0, aVal);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(pos >= BufferEnd(),
                 "Didn't consume whole buffer");

    return NS_OK;
  }

  nsresult AppendArrayItem(JSContext* aCx,
                           bool aFirst,
                           const jsval aVal)
  {
    if (aFirst) {
      Unset();
    }

    nsresult rv = EncodeJSVal(aCx, aVal, aFirst ? eMaxType : 0);
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
      reinterpret_cast<const PRUint8*>(mBuffer.get()), mBuffer.Length());

    return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult SetFromStatement(mozIStorageStatement* aStatement,
                            PRUint32 aIndex)
  {
    PRUint8* data;
    PRUint32 dataLength = 0;

    nsresult rv = aStatement->GetBlob(aIndex, &dataLength, &data);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    mBuffer.Adopt(
      reinterpret_cast<char*>(const_cast<PRUint8*>(data)), dataLength);

    return NS_OK;
  }

  static
  PRInt16 CompareKeys(Key& aFirst, Key& aSecond)
  {
    PRInt32 result = Compare(aFirst.mBuffer, aSecond.mBuffer);

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
  inline nsresult EncodeJSVal(JSContext* aCx, const jsval aVal,
                              PRUint8 aTypeOffset)
  {
    return EncodeJSValInternal(aCx, aVal, aTypeOffset, 0);
  }
  void EncodeString(const nsAString& aString, PRUint8 aTypeOffset);
  void EncodeNumber(double aFloat, PRUint8 aType);

  // Decoding functions. aPos points into mBuffer and is adjusted to point
  // past the consumed value.
  static inline nsresult DecodeJSVal(const unsigned char*& aPos,
                                     const unsigned char* aEnd, JSContext* aCx,
                                     PRUint8 aTypeOffset, jsval* aVal)
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
  nsresult EncodeJSValInternal(JSContext* aCx, const jsval aVal,
                               PRUint8 aTypeOffset, PRUint16 aRecursionDepth);

  static nsresult DecodeJSValInternal(const unsigned char*& aPos,
                                      const unsigned char* aEnd,
                                      JSContext* aCx, PRUint8 aTypeOffset,
                                      jsval* aVal, PRUint16 aRecursionDepth);
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_key_h__ */
