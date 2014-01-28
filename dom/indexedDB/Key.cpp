/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include "Key.h"
#include "ReportInternalError.h"

#include "jsfriendapi.h"
#include "nsAlgorithm.h"
#include "nsJSUtils.h"
#include "xpcpublic.h"
#include "mozilla/Endian.h"
#include <algorithm>

USING_INDEXEDDB_NAMESPACE

/*
 Here's how we encode keys:

 Basic strategy is the following

 Numbers: 1 n n n n n n n n    ("n"s are encoded 64bit float)
 Dates:   2 n n n n n n n n    ("n"s are encoded 64bit float)
 Strings: 3 s s s ... 0        ("s"s are encoded unicode bytes)
 Arrays:  4 i i i ... 0        ("i"s are encoded array items)


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


 When encoding Arrays, we use an additional trick. Rather than adding a byte
 containing the value '4' to indicate type, we instead add 4 to the next byte.
 This is usually the byte containing the type of the first item in the array.
 So simple examples are

 ["foo"]       7 s s s 0 0                              // 7 is 3 + 4
 [1, 2]        5 n n n n n n n n 1 n n n n n n n n 0    // 5 is 1 + 4

 Whe do this iteratively if the first item in the array is also an array

 [["foo"]]    11 s s s 0 0 0

 However, to avoid overflow in the byte, we only do this 3 times. If the first
 item in an array is an array, and that array also has an array as first item,
 we simply write out the total value accumulated so far and then follow the
 "normal" rules.

 [[["foo"]]]  12 3 s s s 0 0 0 0

 There is another edge case that can happen though, which is that the array
 doesn't have a first item to which we can add 4 to the type. Instead the
 next byte would normally be the array terminator (per basic-strategy table)
 so we simply add the 4 there.

 [[]]         8 0             // 8 is 4 + 4 + 0
 []           4               // 4 is 4 + 0
 [[], "foo"]  8 3 s s s 0 0   // 8 is 4 + 4 + 0

 Note that the max-3-times rule kicks in before we get a chance to add to the
 array terminator

 [[[]]]       12 0 0 0        // 12 is 4 + 4 + 4

 We could use a much higher number than 3 at no complexity or performance cost,
 however it seems unlikely that it'll make a practical difference, and the low
 limit makes testing eaiser.


 As a final optimization we do a post-encoding step which drops all 0s at the
 end of the encoded buffer.
 
 "foo"         // 3 s s s
 1             // 1 bf f0
 ["a", "b"]    // 7 s 3 s
 [1, 2]        // 5 bf f0 0 0 0 0 0 0 1 c0
 [[]]          // 8
*/

const int MaxArrayCollapse = 3;

const int MaxRecursionDepth = 256;

nsresult
Key::EncodeJSValInternal(JSContext* aCx, JS::Handle<JS::Value> aVal,
                         uint8_t aTypeOffset, uint16_t aRecursionDepth)
{
  NS_ENSURE_TRUE(aRecursionDepth < MaxRecursionDepth, NS_ERROR_DOM_INDEXEDDB_DATA_ERR);

  static_assert(eMaxType * MaxArrayCollapse < 256,
                "Unable to encode jsvals.");

  if (aVal.isString()) {
    nsDependentJSString str;
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
    if (JS_IsArrayObject(aCx, obj)) {
      aTypeOffset += eMaxType;

      if (aTypeOffset == eMaxType * MaxArrayCollapse) {
        mBuffer.Append(aTypeOffset);
        aTypeOffset = 0;
      }
      NS_ASSERTION((aTypeOffset % eMaxType) == 0 &&
                   aTypeOffset < (eMaxType * MaxArrayCollapse),
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

    if (JS_ObjectIsDate(aCx, obj)) {
      if (!js_DateIsValid(obj))  {
        return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
      }
      EncodeNumber(js_DateGetMsecSinceEpoch(obj), eDate + aTypeOffset);
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
  NS_ENSURE_TRUE(aRecursionDepth < MaxRecursionDepth, NS_ERROR_DOM_INDEXEDDB_DATA_ERR);

  if (*aPos - aTypeOffset >= eArray) {
    JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0, nullptr));
    if (!array) {
      NS_WARNING("Failed to make array!");
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    aTypeOffset += eMaxType;

    if (aTypeOffset == eMaxType * MaxArrayCollapse) {
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

      if (!JS_SetElement(aCx, array, index++, val)) {
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
    JSObject* date = JS_NewDateObjectMsec(aCx, msec);
    if (!date) {
      IDB_WARNING("Failed to make date!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    aVal.setObject(*date);
  }
  else if (*aPos - aTypeOffset == eFloat) {
    aVal.setDouble(DecodeNumber(aPos, aEnd));
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

void
Key::EncodeString(const nsAString& aString, uint8_t aTypeOffset)
{
  // First measure how long the encoded string will be.

  // The +2 is for initial 3 and trailing 0. We'll compensate for multi-byte
  // chars below.
  uint32_t size = aString.Length() + 2;
  
  const char16_t* start = aString.BeginReading();
  const char16_t* end = aString.EndReading();
  for (const char16_t* iter = start; iter < end; ++iter) {
    if (*iter > ONE_BYTE_LIMIT) {
      size += *iter > TWO_BYTE_LIMIT ? 2 : 1;
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
  *(buffer++) = eString + aTypeOffset;

  // Encode string
  for (const char16_t* iter = start; iter < end; ++iter) {
    if (*iter <= ONE_BYTE_LIMIT) {
      *(buffer++) = *iter + ONE_BYTE_ADJUST;
    }
    else if (*iter <= TWO_BYTE_LIMIT) {
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

union Float64Union {
  double d;
  uint64_t u;
}; 

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

  Float64Union pun;
  pun.d = aFloat;
  // Note: The subtraction from 0 below is necessary to fix
  // MSVC build warning C4146 (negating an unsigned value).
  uint64_t number = pun.u & PR_UINT64(0x8000000000000000) ?
                    (0 - pun.u) :
                    (pun.u | PR_UINT64(0x8000000000000000));

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

  Float64Union pun;
  // Note: The subtraction from 0 below is necessary to fix
  // MSVC build warning C4146 (negating an unsigned value).
  pun.u = number & PR_UINT64(0x8000000000000000) ?
          (number & ~PR_UINT64(0x8000000000000000)) :
          (0 - number);

  return pun.d;
}
