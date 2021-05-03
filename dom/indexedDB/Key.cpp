/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Key.h"

#include <algorithm>
#include <stdint.h>  // for UINT32_MAX, uintptr_t
#include "IndexedDBCommon.h"
#include "IndexedDatabase.h"
#include "IndexedDatabaseInlines.h"
#include "IndexedDatabaseManager.h"
#include "js/Array.h"        // JS::NewArrayObject
#include "js/ArrayBuffer.h"  // JS::{IsArrayBufferObject,NewArrayBuffer{,WithContents},GetArrayBufferLengthAndData}
#include "js/Date.h"
#include "js/experimental/TypedData.h"  // JS_IsArrayBufferViewObject, JS_GetObjectAsArrayBufferView
#include "js/MemoryFunctions.h"
#include "js/Object.h"  // JS::GetBuiltinClass
#include "js/Value.h"
#include "jsfriendapi.h"
#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ReverseIterator.h"
#include "mozIStorageStatement.h"
#include "mozIStorageValueArray.h"
#include "nsAlgorithm.h"
#include "nsJSUtils.h"
#include "ReportInternalError.h"
#include "unicode/ucol.h"
#include "xpcpublic.h"

namespace mozilla::dom::indexedDB {

namespace {
// Implementation of the array branch of step 3 of
// https://w3c.github.io/IndexedDB/#convert-value-to-key
template <typename ArrayConversionPolicy>
IDBResult<Ok, IDBSpecialValue::Invalid> ConvertArrayValueToKey(
    JSContext* const aCx, JS::HandleObject aObject,
    ArrayConversionPolicy&& aPolicy) {
  // 1. Let `len` be ? ToLength( ? Get(`input`, "length")).
  uint32_t len;
  if (!JS::GetArrayLength(aCx, aObject, &len)) {
    return Err(IDBException(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR));
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
      return Err(IDBException(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR));
    }

    // 1. Let `hop` be ? HasOwnProperty(`input`, `index`).
    bool hop;
    if (!JS_HasOwnPropertyById(aCx, aObject, indexId, &hop)) {
      return Err(IDBException(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR));
    }

    // 2. If `hop` is false, return invalid.
    if (!hop) {
      return Err(IDBError(SpecialValues::Invalid));
    }

    // 3. Let `entry` be ? Get(`input`, `index`).
    JS::RootedValue entry(aCx);
    if (!JS_GetPropertyById(aCx, aObject, indexId, &entry)) {
      return Err(IDBException(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR));
    }

    // 4. Let `key` be the result of running the steps to convert a value to a
    //    key with arguments `entry` and `seen`.
    // 5. ReturnIfAbrupt(`key`).
    // 6. If `key` is invalid abort these steps and return invalid.
    // 7. Append `key` to `keys`.
    auto result = aPolicy.ConvertSubkey(aCx, entry, index);
    if (result.isErr()) {
      return result;
    }

    // 8. Increase `index` by 1.
    index += 1;
  }

  // 6. Return a new array key with value `keys`.
  aPolicy.EndSubkeyList();
  return Ok();
}
}  // namespace

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
 Chars 7F        - (3FFF+7F)    are encoded as 10xxxxxx xxxxxxxx with 7F
                                subtracted
 Chars (3FFF+80) - FFFF         are encoded as 11xxxxxx xxxxxxxx xx000000

 This ensures that the first byte is never encoded as 0, which means that the
 string terminator (per basic-strategy table) sorts before any character.
 The reason that (3FFF+80) - FFFF is encoded "shifted up" 6 bits is to maximize
 the chance that the last character is 0. See below for why.

 When encoding binaries, the algorithm is the same to how strings are encoded.
 Since each octet in binary is in the range of [0-255], it'll take 1 to 2
 encoded unicode bytes.

 When encoding Arrays, we use an additional trick. Rather than adding a byte
 containing the value 0x50 to indicate type, we instead add 0x50 to the next
 byte. This is usually the byte containing the type of the first item in the
 array. So simple examples are

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

Result<Ok, nsresult> Key::SetFromString(const nsAString& aString) {
  mBuffer.Truncate();
  auto result = EncodeString(aString, 0);
  if (result.isOk()) {
    TrimBuffer();
  }
  return result;
}

// |aPos| should point to the type indicator.
// The returned length doesn't include the type indicator
// or the terminator.
// static
uint32_t Key::LengthOfEncodedBinary(const EncodedDataType* aPos,
                                    const EncodedDataType* aEnd) {
  MOZ_ASSERT(*aPos % Key::eMaxType == Key::eBinary, "Don't call me!");

  const auto* iter = aPos + 1;
  for (; iter < aEnd && *iter != eTerminator; ++iter) {
    if (*iter & 0x80) {
      ++iter;
      // XXX if iter == aEnd now, we got a bad enconding, should we report that
      // also in non-debug builds?
      MOZ_ASSERT(iter < aEnd);
    }
  }

  return iter - aPos - 1;
}

Result<Key, nsresult> Key::ToLocaleAwareKey(const nsCString& aLocale) const {
  Key res;

  if (IsUnset()) {
    return res;
  }

  if (IsFloat() || IsDate() || IsBinary()) {
    res.mBuffer = mBuffer;
    return res;
  }

  auto* it = BufferStart();
  auto* const end = BufferEnd();

  // First we do a pass and see if there are any strings in this key. We only
  // want to copy/decode when necessary.
  bool canShareBuffers = true;
  while (it < end) {
    const auto type = *it % eMaxType;
    if (type == eTerminator) {
      it++;
    } else if (type == eFloat || type == eDate) {
      it++;
      it += std::min(sizeof(uint64_t), size_t(end - it));
    } else if (type == eBinary) {
      // skip all binary data
      const auto binaryLength = LengthOfEncodedBinary(it, end);
      it++;
      it += binaryLength;
    } else {
      // We have a string!
      canShareBuffers = false;
      break;
    }
  }

  if (canShareBuffers) {
    MOZ_ASSERT(it == end);
    res.mBuffer = mBuffer;
    return res;
  }

  if (!res.mBuffer.SetCapacity(mBuffer.Length(), fallible)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  // A string was found, so we need to copy the data we've read so far
  auto* const start = BufferStart();
  if (it > start) {
    char* buffer;
    MOZ_ALWAYS_TRUE(res.mBuffer.GetMutableData(&buffer, it - start));
    std::copy(start, it, buffer);
  }

  // Now continue decoding
  while (it < end) {
    char* buffer;
    const uint32_t oldLen = res.mBuffer.Length();
    const auto type = *it % eMaxType;

    // Note: Do not modify |it| before calling |updateBufferAndIter|;
    // |byteCount| doesn't include the type indicator
    const auto updateBufferAndIter = [&](size_t byteCount) -> bool {
      if (!res.mBuffer.GetMutableData(&buffer, oldLen + 1 + byteCount)) {
        return false;
      }
      buffer += oldLen;

      // should also copy the type indicator at the begining
      std::copy_n(it, byteCount + 1, buffer);
      it += (byteCount + 1);
      return true;
    };

    if (type == eTerminator) {
      // Copy array TypeID and terminator from raw key
      if (!updateBufferAndIter(0)) {
        return Err(NS_ERROR_OUT_OF_MEMORY);
      }
    } else if (type == eFloat || type == eDate) {
      // Copy number from raw key
      const size_t byteCount = std::min(sizeof(uint64_t), size_t(end - it - 1));

      if (!updateBufferAndIter(byteCount)) {
        return Err(NS_ERROR_OUT_OF_MEMORY);
      }
    } else if (type == eBinary) {
      // skip all binary data
      const auto binaryLength = LengthOfEncodedBinary(it, end);

      if (!updateBufferAndIter(binaryLength)) {
        return Err(NS_ERROR_OUT_OF_MEMORY);
      }
    } else {
      // Decode string and reencode
      const uint8_t typeOffset = *it - eString;
      MOZ_ASSERT((typeOffset % eArray == 0) && (typeOffset / eArray <= 2));

      auto str = DecodeString(it, end);
      auto result = res.EncodeLocaleString(str, typeOffset, aLocale);
      if (NS_WARN_IF(result.isErr())) {
        return result.propagateErr();
      }
    }
  }
  res.TrimBuffer();
  return res;
}

class MOZ_STACK_CLASS Key::ArrayValueEncoder final {
 public:
  ArrayValueEncoder(Key& aKey, const uint8_t aTypeOffset,
                    const uint16_t aRecursionDepth)
      : mKey(aKey),
        mTypeOffset(aTypeOffset),
        mRecursionDepth(aRecursionDepth) {}

  void AddToSeenSet(JSContext* const aCx, JS::HandleObject) {
    ++mRecursionDepth;
  }

  void BeginSubkeyList() {
    mTypeOffset += Key::eMaxType;
    if (mTypeOffset == eMaxType * kMaxArrayCollapse) {
      mKey.mBuffer.Append(mTypeOffset);
      mTypeOffset = 0;
    }
    MOZ_ASSERT(mTypeOffset % eMaxType == 0,
               "Current type offset must indicate beginning of array");
    MOZ_ASSERT(mTypeOffset < eMaxType * kMaxArrayCollapse);
  }

  IDBResult<Ok, IDBSpecialValue::Invalid> ConvertSubkey(JSContext* const aCx,
                                                        JS::HandleValue aEntry,
                                                        const uint32_t aIndex) {
    auto result =
        mKey.EncodeJSValInternal(aCx, aEntry, mTypeOffset, mRecursionDepth);
    mTypeOffset = 0;
    return result;
  }

  void EndSubkeyList() const { mKey.mBuffer.Append(eTerminator + mTypeOffset); }

 private:
  Key& mKey;
  uint8_t mTypeOffset;
  uint16_t mRecursionDepth;
};

// Implements the following algorithm:
// https://w3c.github.io/IndexedDB/#convert-a-value-to-a-key
IDBResult<Ok, IDBSpecialValue::Invalid> Key::EncodeJSValInternal(
    JSContext* const aCx, JS::Handle<JS::Value> aVal, uint8_t aTypeOffset,
    const uint16_t aRecursionDepth) {
  static_assert(eMaxType * kMaxArrayCollapse < 256, "Unable to encode jsvals.");

  // 1. If `seen` was not given, let `seen` be a new empty set.
  // 2. If `input` is in `seen` return invalid.
  // Note: we replace this check with a simple recursion depth check.
  if (NS_WARN_IF(aRecursionDepth == kMaxRecursionDepth)) {
    return Err(IDBError(SpecialValues::Invalid));
  }

  // 3. Jump to the appropriate step below:
  // Note: some cases appear out of order to make the implementation more
  //       straightforward. This shouldn't affect observable behavior.

  // If Type(`input`) is Number
  if (aVal.isNumber()) {
    const auto number = aVal.toNumber();

    // 1. If `input` is NaN then return invalid.
    if (mozilla::IsNaN(number)) {
      return Err(IDBError(SpecialValues::Invalid));
    }

    // 2. Otherwise, return a new key with type `number` and value `input`.
    EncodeNumber(number, eFloat + aTypeOffset);
    return Ok();
  }

  // If Type(`input`) is String
  if (aVal.isString()) {
    // 1. Return a new key with type `string` and value `input`.
    nsAutoJSString string;
    if (!string.init(aCx, aVal)) {
      IDB_REPORT_INTERNAL_ERR();
      return Err(IDBException(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR));
    }
    return EncodeString(string, aTypeOffset);
  }

  if (aVal.isObject()) {
    JS::RootedObject object(aCx, &aVal.toObject());

    js::ESClass builtinClass;
    if (!JS::GetBuiltinClass(aCx, object, &builtinClass)) {
      IDB_REPORT_INTERNAL_ERR();
      return Err(IDBException(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR));
    }

    // If `input` is a Date (has a [[DateValue]] internal slot)
    if (builtinClass == js::ESClass::Date) {
      // 1. Let `ms` be the value of `input`’s [[DateValue]] internal slot.
      double ms;
      if (!js::DateGetMsecSinceEpoch(aCx, object, &ms)) {
        IDB_REPORT_INTERNAL_ERR();
        return Err(IDBException(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR));
      }

      // 2. If `ms` is NaN then return invalid.
      if (mozilla::IsNaN(ms)) {
        return Err(IDBError(SpecialValues::Invalid));
      }

      // 3. Otherwise, return a new key with type `date` and value `ms`.
      EncodeNumber(ms, eDate + aTypeOffset);
      return Ok();
    }

    // If `input` is a buffer source type
    if (JS::IsArrayBufferObject(object) || JS_IsArrayBufferViewObject(object)) {
      const bool isViewObject = JS_IsArrayBufferViewObject(object);
      return EncodeBinary(object, isViewObject, aTypeOffset);
    }

    // If IsArray(`input`)
    if (builtinClass == js::ESClass::Array) {
      return ConvertArrayValueToKey(
          aCx, object, ArrayValueEncoder{*this, aTypeOffset, aRecursionDepth});
    }
  }

  // Otherwise
  // Return invalid.
  return Err(IDBError(SpecialValues::Invalid));
}

// static
nsresult Key::DecodeJSValInternal(const EncodedDataType*& aPos,
                                  const EncodedDataType* aEnd, JSContext* aCx,
                                  uint8_t aTypeOffset,
                                  JS::MutableHandle<JS::Value> aVal,
                                  uint16_t aRecursionDepth) {
  if (NS_WARN_IF(aRecursionDepth == kMaxRecursionDepth)) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  if (*aPos - aTypeOffset >= eArray) {
    JS::Rooted<JSObject*> array(aCx, JS::NewArrayObject(aCx, 0));
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
      QM_TRY(DecodeJSValInternal(aPos, aEnd, aCx, aTypeOffset, &val,
                                 aRecursionDepth + 1));

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
  } else if (*aPos - aTypeOffset == eString) {
    auto key = DecodeString(aPos, aEnd);
    if (!xpc::StringToJsval(aCx, key, aVal)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  } else if (*aPos - aTypeOffset == eDate) {
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
  } else if (*aPos - aTypeOffset == eFloat) {
    aVal.setDouble(DecodeNumber(aPos, aEnd));
  } else if (*aPos - aTypeOffset == eBinary) {
    JSObject* binary = DecodeBinary(aPos, aEnd, aCx);
    if (!binary) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    aVal.setObject(*binary);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unknown key type!");
  }

  return NS_OK;
}

#define ONE_BYTE_LIMIT 0x7E
#define TWO_BYTE_LIMIT (0x3FFF + 0x7F)

#define ONE_BYTE_ADJUST 1
#define TWO_BYTE_ADJUST (-0x7F)
#define THREE_BYTE_SHIFT 6

IDBResult<Ok, IDBSpecialValue::Invalid> Key::EncodeJSVal(
    JSContext* aCx, JS::Handle<JS::Value> aVal, uint8_t aTypeOffset) {
  return EncodeJSValInternal(aCx, aVal, aTypeOffset, 0);
}

Result<Ok, nsresult> Key::EncodeString(const nsAString& aString,
                                       uint8_t aTypeOffset) {
  return EncodeString(Span{aString}, aTypeOffset);
}

template <typename T>
Result<Ok, nsresult> Key::EncodeString(const Span<const T> aInput,
                                       uint8_t aTypeOffset) {
  return EncodeAsString(aInput, eString + aTypeOffset);
}

template <typename T>
Result<Ok, nsresult> Key::EncodeAsString(const Span<const T> aInput,
                                         uint8_t aType) {
  // First measure how long the encoded string will be.
  if (NS_WARN_IF(UINT32_MAX - 2 < uintptr_t(aInput.Length()))) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  // The +2 is for initial aType and trailing 0. We'll compensate for multi-byte
  // chars below.
  CheckedUint32 size = 2;

  MOZ_ASSERT(size.isValid());

  // We construct a range over the raw pointers here because this loop is
  // time-critical.
  // XXX It might be good to encapsulate this in some function to make it less
  // error-prone and more expressive.
  const auto inputRange = mozilla::detail::IteratorRange(
      aInput.Elements(), aInput.Elements() + aInput.Length());

  CheckedUint32 payloadSize = aInput.Length();
  bool anyMultibyte = false;
  for (const auto val : inputRange) {
    if (val > ONE_BYTE_LIMIT) {
      anyMultibyte = true;
      payloadSize += char16_t(val) > TWO_BYTE_LIMIT ? 2 : 1;
      if (!payloadSize.isValid()) {
        IDB_REPORT_INTERNAL_ERR();
        return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      }
    }
  }

  size += payloadSize;

  // Allocate memory for the new size
  uint32_t oldLen = mBuffer.Length();
  size += oldLen;

  if (!size.isValid()) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  char* buffer;
  if (!mBuffer.GetMutableData(&buffer, size.value())) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  buffer += oldLen;

  // Write type marker
  *(buffer++) = aType;

  // Encode string
  if (anyMultibyte) {
    for (const auto val : inputRange) {
      if (val <= ONE_BYTE_LIMIT) {
        *(buffer++) = val + ONE_BYTE_ADJUST;
      } else if (char16_t(val) <= TWO_BYTE_LIMIT) {
        char16_t c = char16_t(val) + TWO_BYTE_ADJUST + 0x8000;
        *(buffer++) = (char)(c >> 8);
        *(buffer++) = (char)(c & 0xFF);
      } else {
        uint32_t c = (uint32_t(val) << THREE_BYTE_SHIFT) | 0x00C00000;
        *(buffer++) = (char)(c >> 16);
        *(buffer++) = (char)(c >> 8);
        *(buffer++) = (char)c;
      }
    }
  } else {
    // Optimization for the case where there are no multibyte characters.
    // This is ca. 13 resp. 5.8 times faster than the non-optimized version in
    // an -O2 build: https://quick-bench.com/q/v1oBpLGifs-3w_pkZG8alVSWVAw, for
    // the T==uint8_t resp. T==char16_t cases (for the char16_t case, copying
    // and then adjusting could even be slightly faster, but then we would need
    // another case distinction here)
    const auto inputLen = std::distance(inputRange.cbegin(), inputRange.cend());
    MOZ_ASSERT(inputLen == payloadSize);
    std::transform(inputRange.cbegin(), inputRange.cend(), buffer,
                   [](auto value) { return value + ONE_BYTE_ADJUST; });
    buffer += inputLen;
  }

  // Write end marker
  *(buffer++) = eTerminator;

  NS_ASSERTION(buffer == mBuffer.EndReading(), "Wrote wrong number of bytes");

  return Ok();
}

Result<Ok, nsresult> Key::EncodeLocaleString(const nsAString& aString,
                                             uint8_t aTypeOffset,
                                             const nsCString& aLocale) {
  const int length = aString.Length();
  if (length == 0) {
    return Ok();
  }
  const UChar* ustr = reinterpret_cast<const UChar*>(aString.BeginReading());

  UErrorCode uerror = U_ZERO_ERROR;
  UCollator* collator = ucol_open(aLocale.get(), &uerror);
  if (NS_WARN_IF(U_FAILURE(uerror))) {
    return Err(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(collator);

  AutoTArray<uint8_t, 128> keyBuffer;
  int32_t sortKeyLength = ucol_getSortKey(
      collator, ustr, length, keyBuffer.Elements(), keyBuffer.Length());
  if (sortKeyLength > (int32_t)keyBuffer.Length()) {
    if (!keyBuffer.SetLength(sortKeyLength, fallible)) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }
    sortKeyLength = ucol_getSortKey(collator, ustr, length,
                                    keyBuffer.Elements(), sortKeyLength);
  }

  ucol_close(collator);
  if (NS_WARN_IF(sortKeyLength == 0)) {
    return Err(NS_ERROR_FAILURE);
  }

  return EncodeString(Span{keyBuffer}.AsConst().First(sortKeyLength),
                      aTypeOffset);
}

// static
nsresult Key::DecodeJSVal(const EncodedDataType*& aPos,
                          const EncodedDataType* aEnd, JSContext* aCx,
                          JS::MutableHandle<JS::Value> aVal) {
  return DecodeJSValInternal(aPos, aEnd, aCx, 0, aVal, 0);
}

// static
template <typename T>
uint32_t Key::CalcDecodedStringySize(
    const EncodedDataType* const aBegin, const EncodedDataType* const aEnd,
    const EncodedDataType** aOutEncodedSectionEnd) {
  static_assert(sizeof(T) <= 2,
                "Only implemented for 1 and 2 byte decoded types");
  uint32_t decodedSize = 0;
  auto* iter = aBegin;
  for (; iter < aEnd && *iter != eTerminator; ++iter) {
    if (*iter & 0x80) {
      iter += (sizeof(T) > 1 && (*iter & 0x40)) ? 2 : 1;
    }
    ++decodedSize;
  }
  *aOutEncodedSectionEnd = std::min(aEnd, iter);
  return decodedSize;
}

// static
template <typename T>
void Key::DecodeAsStringy(const EncodedDataType* const aEncodedSectionBegin,
                          const EncodedDataType* const aEncodedSectionEnd,
                          const uint32_t aDecodedLength, T* const aOut) {
  static_assert(sizeof(T) <= 2,
                "Only implemented for 1 and 2 byte decoded types");
  T* decodedPos = aOut;
  for (const EncodedDataType* iter = aEncodedSectionBegin;
       iter < aEncodedSectionEnd;) {
    if (!(*iter & 0x80)) {
      *decodedPos = *(iter++) - ONE_BYTE_ADJUST;
    } else if (sizeof(T) == 1 || !(*iter & 0x40)) {
      auto c = static_cast<uint16_t>(*(iter++)) << 8;
      if (iter < aEncodedSectionEnd) {
        c |= *(iter++);
      }
      *decodedPos = static_cast<T>(c - TWO_BYTE_ADJUST - 0x8000);
    } else if (sizeof(T) > 1) {
      auto c = static_cast<uint32_t>(*(iter++)) << (16 - THREE_BYTE_SHIFT);
      if (iter < aEncodedSectionEnd) {
        c |= static_cast<uint32_t>(*(iter++)) << (8 - THREE_BYTE_SHIFT);
      }
      if (iter < aEncodedSectionEnd) {
        c |= *(iter++) >> THREE_BYTE_SHIFT;
      }
      *decodedPos = static_cast<T>(c);
    }
    ++decodedPos;
  }

  MOZ_ASSERT(static_cast<uint32_t>(decodedPos - aOut) == aDecodedLength,
             "Should have written the whole decoded area");
}

// static
template <Key::EncodedDataType TypeMask, typename T, typename AcquireBuffer,
          typename AcquireEmpty>
void Key::DecodeStringy(const EncodedDataType*& aPos,
                        const EncodedDataType* aEnd,
                        const AcquireBuffer& acquireBuffer,
                        const AcquireEmpty& acquireEmpty) {
  NS_ASSERTION(*aPos % eMaxType == TypeMask, "Don't call me!");

  // First measure how big the decoded stringy data will be.
  const EncodedDataType* const encodedSectionBegin = aPos + 1;
  const EncodedDataType* encodedSectionEnd;
  // decodedLength does not include the terminating 0 (in case of a string)
  const uint32_t decodedLength =
      CalcDecodedStringySize<T>(encodedSectionBegin, aEnd, &encodedSectionEnd);
  aPos = encodedSectionEnd + 1;

  if (!decodedLength) {
    acquireEmpty();
    return;
  }

  T* out;
  if (!acquireBuffer(&out, decodedLength)) {
    return;
  }

  DecodeAsStringy(encodedSectionBegin, encodedSectionEnd, decodedLength, out);
}

// static
nsAutoString Key::DecodeString(const EncodedDataType*& aPos,
                               const EncodedDataType* const aEnd) {
  nsAutoString res;
  DecodeStringy<eString, char16_t>(
      aPos, aEnd,
      [&res](char16_t** out, uint32_t decodedLength) {
        return 0 != res.GetMutableData(out, decodedLength);
      },
      [] {});
  return res;
}

void Key::EncodeNumber(double aFloat, uint8_t aType) {
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
double Key::DecodeNumber(const EncodedDataType*& aPos,
                         const EncodedDataType* aEnd) {
  NS_ASSERTION(*aPos % eMaxType == eFloat || *aPos % eMaxType == eDate,
               "Don't call me!");

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

Result<Ok, nsresult> Key::EncodeBinary(JSObject* aObject, bool aIsViewObject,
                                       uint8_t aTypeOffset) {
  uint8_t* bufferData;
  size_t bufferLength;

  // We must use JS::GetObjectAsArrayBuffer()/JS_GetObjectAsArrayBufferView()
  // instead of js::GetArrayBufferLengthAndData(). The object might be wrapped,
  // the former will handle the wrapped case, the later won't.
  if (aIsViewObject) {
    bool unused;
    JS_GetObjectAsArrayBufferView(aObject, &bufferLength, &unused, &bufferData);
  } else {
    JS::GetObjectAsArrayBuffer(aObject, &bufferLength, &bufferData);
  }

  return EncodeAsString(Span{bufferData, bufferLength}.AsConst(),
                        eBinary + aTypeOffset);
}

// static
JSObject* Key::DecodeBinary(const EncodedDataType*& aPos,
                            const EncodedDataType* aEnd, JSContext* aCx) {
  JS::RootedObject rv(aCx);
  DecodeStringy<eBinary, uint8_t>(
      aPos, aEnd,
      [&rv, aCx](uint8_t** out, uint32_t decodedSize) {
        *out = static_cast<uint8_t*>(JS_malloc(aCx, decodedSize));
        if (NS_WARN_IF(!*out)) {
          rv = nullptr;
          return false;
        }
        rv = JS::NewArrayBufferWithContents(aCx, decodedSize, *out);
        return true;
      },
      [&rv, aCx] { rv = JS::NewArrayBuffer(aCx, 0); });
  return rv;
}

nsresult Key::BindToStatement(mozIStorageStatement* aStatement,
                              const nsACString& aParamName) const {
  nsresult rv;
  if (IsUnset()) {
    rv = aStatement->BindNullByName(aParamName);
  } else {
    rv = aStatement->BindBlobByName(
        aParamName, reinterpret_cast<const uint8_t*>(mBuffer.get()),
        mBuffer.Length());
  }

  return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
}

nsresult Key::SetFromStatement(mozIStorageStatement* aStatement,
                               uint32_t aIndex) {
  return SetFromSource(aStatement, aIndex);
}

nsresult Key::SetFromValueArray(mozIStorageValueArray* aValues,
                                uint32_t aIndex) {
  return SetFromSource(aValues, aIndex);
}

IDBResult<Ok, IDBSpecialValue::Invalid> Key::SetFromJSVal(
    JSContext* aCx, JS::Handle<JS::Value> aVal) {
  mBuffer.Truncate();

  if (aVal.isNull() || aVal.isUndefined()) {
    Unset();
    return Ok();
  }

  auto result = EncodeJSVal(aCx, aVal, 0);
  if (result.isErr()) {
    Unset();
    return result;
  }
  TrimBuffer();
  return Ok();
}

nsresult Key::ToJSVal(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) const {
  if (IsUnset()) {
    aVal.setUndefined();
    return NS_OK;
  }

  const EncodedDataType* pos = BufferStart();
  nsresult rv = DecodeJSVal(pos, BufferEnd(), aCx, aVal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(pos >= BufferEnd());

  return NS_OK;
}

nsresult Key::ToJSVal(JSContext* aCx, JS::Heap<JS::Value>& aVal) const {
  JS::Rooted<JS::Value> value(aCx);
  nsresult rv = ToJSVal(aCx, &value);
  if (NS_SUCCEEDED(rv)) {
    aVal = value;
  }
  return rv;
}

IDBResult<Ok, IDBSpecialValue::Invalid> Key::AppendItem(
    JSContext* aCx, bool aFirstOfArray, JS::Handle<JS::Value> aVal) {
  auto result = EncodeJSVal(aCx, aVal, aFirstOfArray ? eMaxType : 0);
  if (result.isErr()) {
    Unset();
  }
  return result;
}

template <typename T>
nsresult Key::SetFromSource(T* aSource, uint32_t aIndex) {
  const uint8_t* data;
  uint32_t dataLength = 0;

  nsresult rv = aSource->GetSharedBlob(aIndex, &dataLength, &data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mBuffer.Assign(reinterpret_cast<const char*>(data), dataLength);

  return NS_OK;
}

}  // namespace mozilla::dom::indexedDB
