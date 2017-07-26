/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "Key.h"

#include <algorithm>
#include "IndexedDatabaseManager.h"
#include "js/Date.h"
#include "js/Value.h"
#include "jsfriendapi.h"
#include "mozilla/Casting.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozIStorageStatement.h"
#include "mozIStorageValueArray.h"
#include "nsAlgorithm.h"
#include "nsJSUtils.h"
#include "ReportInternalError.h"
#include "xpcpublic.h"

#ifdef ENABLE_INTL_API
#include "unicode/ucol.h"
#endif

namespace mozilla {
namespace dom {
namespace indexedDB {

/*
 Here's how we encode keys:

 Basic strategy is the following

 Numbers:  0x10 n n n n n n n n    ("n"s are encoded 64bit float)
 Dates:    0x20 n n n n n n n n    ("n"s are encoded 64bit float)
 Strings:  0x30 s s s ... 0        ("s"s are encoded unicode bytes)
 Binaries: 0x40 s s s ... 0        ("s"s are encoded unicode bytes)
 Arrays:   0x50 i i i ... 0        ("i"s are encoded array items)


 When encoding floats, 64bit IEEE 754 are almost sortable, except that
 positive sort lower than negative, and negative sort descending. So we use
 the following encoding:

 value < 0 ?
   (-to64bitInt(value)) :
   (to64bitInt(value) | 0x8000000000000000)


 When encoding strings, we use variable-size encoding per the following table

 Chars 0         - 7E           are encoded as 0xxxxxxx with 1 added
 Chars 7F        - (3FFF+7F)    are encoded as 10xxxxxx xxxxxxxx with 7F subtracted
 Chars (3FFF+80) - FFFF         are encoded as 11xxxxxx xxxxxxxx xx000000

 This ensures that the first byte is never encoded as 0, which means that the
 string terminator (per basic-stategy table) sorts before any character.
 The reason that (3FFF+80) - FFFF is encoded "shifted up" 6 bits is to maximize
 the chance that the last character is 0. See below for why.

 When encoding binaries, the algorithm is the same to how strings are encoded.
 Since each octet in binary is in the range of [0-255], it'll take 1 to 2 encoded
 unicode bytes.

 When encoding Arrays, we use an additional trick. Rather than adding a byte
 containing the value 0x50 to indicate type, we instead add 0x50 to the next byte.
 This is usually the byte containing the type of the first item in the array.
 So simple examples are

 ["foo"]      0x80 s s s 0 0                              // 0x80 is 0x30 + 0x50
 [1, 2]       0x60 n n n n n n n n 1 n n n n n n n n 0    // 0x60 is 0x10 + 0x50

 Whe do this iteratively if the first item in the array is also an array

 [["foo"]]    0xA0 s s s 0 0 0

 However, to avoid overflow in the byte, we only do this 3 times. If the first
 item in an array is an array, and that array also has an array as first item,
 we simply write out the total value accumulated so far and then follow the
 "normal" rules.

 [[["foo"]]]  0xF0 0x30 s s s 0 0 0 0

 There is another edge case that can happen though, which is that the array
 doesn't have a first item to which we can add 0x50 to the type. Instead the
 next byte would normally be the array terminator (per basic-strategy table)
 so we simply add the 0x50 there.

 [[]]         0xA0 0                // 0xA0 is 0x50 + 0x50 + 0
 []           0x50                  // 0x50 is 0x50 + 0
 [[], "foo"]  0xA0 0x30 s s s 0 0   // 0xA0 is 0x50 + 0x50 + 0

 Note that the max-3-times rule kicks in before we get a chance to add to the
 array terminator

 [[[]]]       0xF0 0 0 0        // 0xF0 is 0x50 + 0x50 + 0x50

 As a final optimization we do a post-encoding step which drops all 0s at the
 end of the encoded buffer.

 "foo"         // 0x30 s s s
 1             // 0x10 bf f0
 ["a", "b"]    // 0x80 s 0 0x30 s
 [1, 2]        // 0x60 bf f0 0 0 0 0 0 0 0x10 c0
 [[]]          // 0x80
*/
#ifdef ENABLE_INTL_API
nsresult
Key::ToLocaleBasedKey(Key& aTarget, const nsCString& aLocale) const
{
  if (IsUnset()) {
    aTarget.Unset();
    return NS_OK;
  }

  if (IsFloat() || IsDate() || IsBinary()) {
    aTarget.mBuffer = mBuffer;
    return NS_OK;
  }

  aTarget.mBuffer.Truncate();
  aTarget.mBuffer.SetCapacity(mBuffer.Length());

  auto* it = reinterpret_cast<const unsigned char*>(mBuffer.BeginReading());
  auto* end = reinterpret_cast<const unsigned char*>(mBuffer.EndReading());

  // First we do a pass and see if there are any strings in this key. We only
  // want to copy/decode when necessary.
  bool canShareBuffers = true;
  while (it < end) {
    auto type = *it % eMaxType;
    if (type == eTerminator || type == eArray) {
      it++;
    } else if (type == eFloat || type == eDate) {
      it++;
      it += std::min(sizeof(uint64_t), size_t(end - it));
    } else {
      // We have a string!
      canShareBuffers = false;
      break;
    }
  }

  if (canShareBuffers) {
    MOZ_ASSERT(it == end);
    aTarget.mBuffer = mBuffer;
    return NS_OK;
  }

  // A string was found, so we need to copy the data we've read so far
  auto* start = reinterpret_cast<const unsigned char*>(mBuffer.BeginReading());
  if (it > start) {
    char* buffer;
    if (!aTarget.mBuffer.GetMutableData(&buffer, it-start)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    while (start < it) {
      *(buffer++) = *(start++);
    }
  }

  // Now continue decoding
  while (it < end) {
    char* buffer;
    uint32_t oldLen = aTarget.mBuffer.Length();
    auto type = *it % eMaxType;

    if (type == eTerminator || type == eArray) {
      // Copy array TypeID and terminator from raw key
      if (!aTarget.mBuffer.GetMutableData(&buffer, oldLen + 1)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      *(buffer + oldLen) = *(it++);
    } else if (type == eFloat || type == eDate) {
      // Copy number from raw key
      if (!aTarget.mBuffer.GetMutableData(&buffer,
                                          oldLen + 1 + sizeof(uint64_t))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      buffer += oldLen;
      *(buffer++) = *(it++);

      const size_t byteCount = std::min(sizeof(uint64_t), size_t(end - it));
      for (size_t count = 0; count < byteCount; count++) {
        *(buffer++) = (*it++);
      }
    } else {
      // Decode string and reencode
      uint8_t typeOffset = *it - eString;
      MOZ_ASSERT((typeOffset % eArray == 0) && (typeOffset / eArray <= 2));

      nsDependentString str;
      DecodeString(it, end, str);
      aTarget.EncodeLocaleString(str, typeOffset, aLocale);
    }
  }
  aTarget.TrimBuffer();
  return NS_OK;
}
#endif

nsresult
Key::EncodeJSValInternal(JSContext* aCx, JS::Handle<JS::Value> aVal,
                         uint8_t aTypeOffset, uint16_t aRecursionDepth)
{
  static_assert(eMaxType * kMaxArrayCollapse < 256,
                "Unable to encode jsvals.");

  if (NS_WARN_IF(aRecursionDepth == kMaxRecursionDepth)) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  if (aVal.isString()) {
    nsAutoJSString str;
    if (!str.init(aCx, aVal)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
    EncodeString(str, aTypeOffset);
    return NS_OK;
  }

  if (aVal.isNumber()) {
    double d = aVal.toNumber();
    if (mozilla::IsNaN(d)) {
      return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
    }
    EncodeNumber(d, eFloat + aTypeOffset);
    return NS_OK;
  }

  if (aVal.isObject()) {
    JS::Rooted<JSObject*> obj(aCx, &aVal.toObject());

    js::ESClass cls;
    if (!js::GetBuiltinClass(aCx, obj, &cls)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
    if (cls == js::ESClass::Array) {
      aTypeOffset += eMaxType;

      if (aTypeOffset == eMaxType * kMaxArrayCollapse) {
        mBuffer.Append(aTypeOffset);
        aTypeOffset = 0;
      }
      NS_ASSERTION((aTypeOffset % eMaxType) == 0 &&
                   aTypeOffset < (eMaxType * kMaxArrayCollapse),
                   "Wrong typeoffset");

      uint32_t length;
      if (!JS_GetArrayLength(aCx, obj, &length)) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      for (uint32_t index = 0; index < length; index++) {
        JS::Rooted<JS::Value> val(aCx);
        if (!JS_GetElement(aCx, obj, index, &val)) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        nsresult rv = EncodeJSValInternal(aCx, val, aTypeOffset,
                                          aRecursionDepth + 1);
        if (NS_FAILED(rv)) {
          return rv;
        }

        aTypeOffset = 0;
      }

      mBuffer.Append(eTerminator + aTypeOffset);

      return NS_OK;
    }

    if (cls == js::ESClass::Date) {
      bool valid;
      if (!js::DateIsValid(aCx, obj, &valid)) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
      if (!valid)  {
        return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
      }
      double t;
      if (!js::DateGetMsecSinceEpoch(aCx, obj, &t)) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
      EncodeNumber(t, eDate + aTypeOffset);
      return NS_OK;
    }

    if (JS_IsArrayBufferObject(obj)) {
      EncodeBinary(obj, /* aIsViewObject */ false, aTypeOffset);
      return NS_OK;
    }

    if (JS_IsArrayBufferViewObject(obj)) {
      EncodeBinary(obj, /* aIsViewObject */ true, aTypeOffset);
      return NS_OK;
    }
  }

  return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
}

// static
nsresult
Key::DecodeJSValInternal(const unsigned char*& aPos, const unsigned char* aEnd,
                         JSContext* aCx, uint8_t aTypeOffset, JS::MutableHandle<JS::Value> aVal,
                         uint16_t aRecursionDepth)
{
  if (NS_WARN_IF(aRecursionDepth == kMaxRecursionDepth)) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  if (*aPos - aTypeOffset >= eArray) {
    JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0));
    if (!array) {
      NS_WARNING("Failed to make array!");
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    aTypeOffset += eMaxType;

    if (aTypeOffset == eMaxType * kMaxArrayCollapse) {
      ++aPos;
      aTypeOffset = 0;
    }

    uint32_t index = 0;
    JS::Rooted<JS::Value> val(aCx);
    while (aPos < aEnd && *aPos - aTypeOffset != eTerminator) {
      nsresult rv = DecodeJSValInternal(aPos, aEnd, aCx, aTypeOffset,
                                        &val, aRecursionDepth + 1);
      NS_ENSURE_SUCCESS(rv, rv);

      aTypeOffset = 0;

      if (!JS_DefineElement(aCx, array, index++, val, JSPROP_ENUMERATE)) {
        NS_WARNING("Failed to set array element!");
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    NS_ASSERTION(aPos >= aEnd || (*aPos % eMaxType) == eTerminator,
                 "Should have found end-of-array marker");
    ++aPos;

    aVal.setObject(*array);
  }
  else if (*aPos - aTypeOffset == eString) {
    nsString key;
    DecodeString(aPos, aEnd, key);
    if (!xpc::StringToJsval(aCx, key, aVal)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }
  else if (*aPos - aTypeOffset == eDate) {
    double msec = static_cast<double>(DecodeNumber(aPos, aEnd));
    JS::ClippedTime time = JS::TimeClip(msec);
    MOZ_ASSERT(msec == time.toDouble(),
               "encoding from a Date object not containing an invalid date "
               "means we should always have clipped values");
    JSObject* date = JS::NewDateObject(aCx, time);
    if (!date) {
      IDB_WARNING("Failed to make date!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    aVal.setObject(*date);
  }
  else if (*aPos - aTypeOffset == eFloat) {
    aVal.setDouble(DecodeNumber(aPos, aEnd));
  }
  else if (*aPos - aTypeOffset == eBinary) {
    JSObject* binary = DecodeBinary(aPos, aEnd, aCx);
    if (!binary) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    aVal.setObject(*binary);
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }

  return NS_OK;
}

#define ONE_BYTE_LIMIT 0x7E
#define TWO_BYTE_LIMIT (0x3FFF+0x7F)

#define ONE_BYTE_ADJUST 1
#define TWO_BYTE_ADJUST (-0x7F)
#define THREE_BYTE_SHIFT 6

nsresult
Key::EncodeJSVal(JSContext* aCx,
                 JS::Handle<JS::Value> aVal,
                 uint8_t aTypeOffset)
{
  return EncodeJSValInternal(aCx, aVal, aTypeOffset, 0);
}

void
Key::EncodeString(const nsAString& aString, uint8_t aTypeOffset)
{
  const char16_t* start = aString.BeginReading();
  const char16_t* end = aString.EndReading();
  EncodeString(start, end, aTypeOffset);
}

template <typename T>
void
Key::EncodeString(const T* aStart, const T* aEnd, uint8_t aTypeOffset)
{
  EncodeAsString(aStart, aEnd, eString + aTypeOffset);
}

template <typename T>
void
Key::EncodeAsString(const T* aStart, const T* aEnd, uint8_t aType)
{
  // First measure how long the encoded string will be.

  // The +2 is for initial 3 and trailing 0. We'll compensate for multi-byte
  // chars below.
  uint32_t size = (aEnd - aStart) + 2;

  const T* start = aStart;
  const T* end = aEnd;
  for (const T* iter = start; iter < end; ++iter) {
    if (*iter > ONE_BYTE_LIMIT) {
      size += char16_t(*iter) > TWO_BYTE_LIMIT ? 2 : 1;
    }
  }

  // Allocate memory for the new size
  uint32_t oldLen = mBuffer.Length();
  char* buffer;
  if (!mBuffer.GetMutableData(&buffer, oldLen + size)) {
    return;
  }
  buffer += oldLen;

  // Write type marker
  *(buffer++) = aType;

  // Encode string
  for (const T* iter = start; iter < end; ++iter) {
    if (*iter <= ONE_BYTE_LIMIT) {
      *(buffer++) = *iter + ONE_BYTE_ADJUST;
    }
    else if (char16_t(*iter) <= TWO_BYTE_LIMIT) {
      char16_t c = char16_t(*iter) + TWO_BYTE_ADJUST + 0x8000;
      *(buffer++) = (char)(c >> 8);
      *(buffer++) = (char)(c & 0xFF);
    }
    else {
      uint32_t c = (uint32_t(*iter) << THREE_BYTE_SHIFT) | 0x00C00000;
      *(buffer++) = (char)(c >> 16);
      *(buffer++) = (char)(c >> 8);
      *(buffer++) = (char)c;
    }
  }

  // Write end marker
  *(buffer++) = eTerminator;

  NS_ASSERTION(buffer == mBuffer.EndReading(), "Wrote wrong number of bytes");
}

#ifdef ENABLE_INTL_API
nsresult
Key::EncodeLocaleString(const nsDependentString& aString, uint8_t aTypeOffset,
                        const nsCString& aLocale)
{
  const int length = aString.Length();
  if (length == 0) {
    return NS_OK;
  }
  const UChar* ustr = reinterpret_cast<const UChar*>(aString.BeginReading());

  UErrorCode uerror = U_ZERO_ERROR;
  UCollator* collator = ucol_open(aLocale.get(), &uerror);
  if (NS_WARN_IF(U_FAILURE(uerror))) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(collator);

  AutoTArray<uint8_t, 128> keyBuffer;
  int32_t sortKeyLength = ucol_getSortKey(collator, ustr, length,
                                          keyBuffer.Elements(),
                                          keyBuffer.Length());
  if (sortKeyLength > (int32_t)keyBuffer.Length()) {
    keyBuffer.SetLength(sortKeyLength);
    sortKeyLength = ucol_getSortKey(collator, ustr, length,
                                    keyBuffer.Elements(),
                                    sortKeyLength);
  }

  ucol_close(collator);
  if (NS_WARN_IF(sortKeyLength == 0)) {
    return NS_ERROR_FAILURE;
  }

  EncodeString(keyBuffer.Elements(),
               keyBuffer.Elements()+sortKeyLength,
               aTypeOffset);
  return NS_OK;
}
#endif

// static
nsresult
Key::DecodeJSVal(const unsigned char*& aPos,
                 const unsigned char* aEnd,
                 JSContext* aCx,
                 JS::MutableHandle<JS::Value> aVal)
{
  return DecodeJSValInternal(aPos, aEnd, aCx, 0, aVal, 0);
}

// static
void
Key::DecodeString(const unsigned char*& aPos, const unsigned char* aEnd,
                  nsString& aString)
{
  NS_ASSERTION(*aPos % eMaxType == eString, "Don't call me!");

  const unsigned char* buffer = aPos + 1;

  // First measure how big the decoded string will be.
  uint32_t size = 0;
  const unsigned char* iter;
  for (iter = buffer; iter < aEnd && *iter != eTerminator; ++iter) {
    if (*iter & 0x80) {
      iter += (*iter & 0x40) ? 2 : 1;
    }
    ++size;
  }

  // Set end so that we don't have to check for null termination in the loop
  // below
  if (iter < aEnd) {
    aEnd = iter;
  }

  char16_t* out;
  if (size && !aString.GetMutableData(&out, size)) {
    return;
  }

  for (iter = buffer; iter < aEnd;) {
    if (!(*iter & 0x80)) {
      *out = *(iter++) - ONE_BYTE_ADJUST;
    }
    else if (!(*iter & 0x40)) {
      char16_t c = (char16_t(*(iter++)) << 8);
      if (iter < aEnd) {
        c |= *(iter++);
      }
      *out = c - TWO_BYTE_ADJUST - 0x8000;
    }
    else {
      uint32_t c = uint32_t(*(iter++)) << (16 - THREE_BYTE_SHIFT);
      if (iter < aEnd) {
        c |= uint32_t(*(iter++)) << (8 - THREE_BYTE_SHIFT);
      }
      if (iter < aEnd) {
        c |= *(iter++) >> THREE_BYTE_SHIFT;
      }
      *out = (char16_t)c;
    }

    ++out;
  }

  NS_ASSERTION(!size || out == aString.EndReading(),
               "Should have written the whole string");

  aPos = iter + 1;
}

void
Key::EncodeNumber(double aFloat, uint8_t aType)
{
  // Allocate memory for the new size
  uint32_t oldLen = mBuffer.Length();
  char* buffer;
  if (!mBuffer.GetMutableData(&buffer, oldLen + 1 + sizeof(double))) {
    return;
  }
  buffer += oldLen;

  *(buffer++) = aType;

  uint64_t bits = BitwiseCast<uint64_t>(aFloat);
  // Note: The subtraction from 0 below is necessary to fix
  // MSVC build warning C4146 (negating an unsigned value).
  const uint64_t signbit = FloatingPoint<double>::kSignBit;
  uint64_t number = bits & signbit ? (0 - bits) : (bits | signbit);

  mozilla::BigEndian::writeUint64(buffer, number);
}

// static
double
Key::DecodeNumber(const unsigned char*& aPos, const unsigned char* aEnd)
{
  NS_ASSERTION(*aPos % eMaxType == eFloat ||
               *aPos % eMaxType == eDate, "Don't call me!");

  ++aPos;

  uint64_t number = 0;
  memcpy(&number, aPos, std::min<size_t>(sizeof(number), aEnd - aPos));
  number = mozilla::NativeEndian::swapFromBigEndian(number);

  aPos += sizeof(number);

  // Note: The subtraction from 0 below is necessary to fix
  // MSVC build warning C4146 (negating an unsigned value).
  const uint64_t signbit = FloatingPoint<double>::kSignBit;
  uint64_t bits = number & signbit ? (number & ~signbit) : (0 - number);

  return BitwiseCast<double>(bits);
}

void
Key::EncodeBinary(JSObject* aObject, bool aIsViewObject, uint8_t aTypeOffset)
{
  uint8_t* bufferData;
  uint32_t bufferLength;
  bool unused;

  if (aIsViewObject) {
    js::GetArrayBufferViewLengthAndData(aObject, &bufferLength, &unused, &bufferData);
  } else {
    js::GetArrayBufferLengthAndData(aObject, &bufferLength, &unused, &bufferData);
  }

  EncodeAsString(bufferData, bufferData + bufferLength, eBinary + aTypeOffset);
}

// static
JSObject*
Key::DecodeBinary(const unsigned char*& aPos,
                  const unsigned char* aEnd,
                  JSContext* aCx)
{
  MOZ_ASSERT(*aPos % eMaxType == eBinary, "Don't call me!");

  const unsigned char* buffer = ++aPos;

  // First measure how big the decoded array buffer will be.
  size_t size = 0;
  const unsigned char* iter;
  for (iter = buffer; iter < aEnd && *iter != eTerminator; ++iter) {
    if (*iter & 0x80) {
      iter++;
    }
    ++size;
  }

  if (!size) {
    return JS_NewArrayBuffer(aCx, 0);
  }

  uint8_t* out = static_cast<uint8_t*>(JS_malloc(aCx, size));
  if (NS_WARN_IF(!out)) {
    return nullptr;
  }

  uint8_t* pos = out;

  // Set end so that we don't have to check for null termination in the loop
  // below
  if (iter < aEnd) {
    aEnd = iter;
  }

  for (iter = buffer; iter < aEnd;) {
    if (!(*iter & 0x80)) {
      *pos = *(iter++) - ONE_BYTE_ADJUST;
    }
    else {
      uint16_t c = (uint16_t(*(iter++)) << 8);
      if (iter < aEnd) {
        c |= *(iter++);
      }
      *pos = static_cast<uint8_t>(c - TWO_BYTE_ADJUST - 0x8000);
    }

    ++pos;
  }

  aPos = iter + 1;

  MOZ_ASSERT(static_cast<size_t>(pos - out) == size,
             "Should have written the whole buffer");

  return JS_NewArrayBufferWithContents(aCx, size, out);
}

nsresult
Key::BindToStatement(mozIStorageStatement* aStatement,
                     const nsACString& aParamName) const
{
  nsresult rv;
  if (IsUnset()) {
    rv = aStatement->BindNullByName(aParamName);
  } else {
    rv = aStatement->BindBlobByName(aParamName,
      reinterpret_cast<const uint8_t*>(mBuffer.get()), mBuffer.Length());
  }

  return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
}

nsresult
Key::SetFromStatement(mozIStorageStatement* aStatement,
                      uint32_t aIndex)
{
  return SetFromSource(aStatement, aIndex);
}

nsresult
Key::SetFromValueArray(mozIStorageValueArray* aValues,
                      uint32_t aIndex)
{
  return SetFromSource(aValues, aIndex);
}

nsresult
Key::SetFromJSVal(JSContext* aCx,
                  JS::Handle<JS::Value> aVal)
{
  mBuffer.Truncate();

  if (aVal.isNull() || aVal.isUndefined()) {
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

nsresult
Key::ToJSVal(JSContext* aCx,
             JS::MutableHandle<JS::Value> aVal) const
{
  if (IsUnset()) {
    aVal.setUndefined();
    return NS_OK;
  }

  const unsigned char* pos = BufferStart();
  nsresult rv = DecodeJSVal(pos, BufferEnd(), aCx, aVal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(pos >= BufferEnd());

  return NS_OK;
}

nsresult
Key::ToJSVal(JSContext* aCx,
             JS::Heap<JS::Value>& aVal) const
{
  JS::Rooted<JS::Value> value(aCx);
  nsresult rv = ToJSVal(aCx, &value);
  if (NS_SUCCEEDED(rv)) {
    aVal = value;
  }
  return rv;
}

nsresult
Key::AppendItem(JSContext* aCx, bool aFirstOfArray, JS::Handle<JS::Value> aVal)
{
  nsresult rv = EncodeJSVal(aCx, aVal, aFirstOfArray ? eMaxType : 0);
  if (NS_FAILED(rv)) {
    Unset();
    return rv;
  }

  return NS_OK;
}

template <typename T>
nsresult
Key::SetFromSource(T* aSource, uint32_t aIndex)
{
  const uint8_t* data;
  uint32_t dataLength = 0;

  nsresult rv = aSource->GetSharedBlob(aIndex, &dataLength, &data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mBuffer.Assign(reinterpret_cast<const char*>(data), dataLength);

  return NS_OK;
}

#ifdef DEBUG

void
Key::Assert(bool aCondition) const
{
  MOZ_ASSERT(aCondition);
}

#endif // DEBUG

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
