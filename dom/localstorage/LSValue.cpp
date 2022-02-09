/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/LSValue.h"

#include "mozIStorageStatement.h"
#include "mozilla/dom/SnappyUtils.h"
#include "mozilla/fallible.h"
#include "mozilla/TextUtils.h"
#include "nsDebug.h"
#include "nsError.h"

namespace mozilla::dom {

namespace {

bool PutStringBytesToCString(const nsAString& aSrc, nsCString& aDest) {
  const char16_t* bufferData;
  const size_t byteLength = sizeof(char16_t) * aSrc.GetData(&bufferData);

  char* destDataPtr;
  const auto newLength = aDest.GetMutableData(&destDataPtr, byteLength);
  if (newLength != byteLength) {
    return false;
  }
  std::memcpy(static_cast<void*>(destDataPtr),
              static_cast<const void*>(bufferData), byteLength);

  return true;
}

template <class T>
using TypeBufferResult = Result<std::pair<T, nsCString>, nsresult>;

}  // namespace

bool PutCStringBytesToString(const nsACString& aSrc, nsString& aDest) {
  const char* bufferData;
  const size_t byteLength = aSrc.GetData(&bufferData);
  const size_t shortLength = byteLength / sizeof(char16_t);

  char16_t* destDataPtr;
  const auto newLength = aDest.GetMutableData(&destDataPtr, shortLength);
  if (newLength != shortLength) {
    return false;
  }

  std::memcpy(static_cast<void*>(destDataPtr),
              static_cast<const void*>(bufferData), byteLength);
  return true;
}

LSValue::Converter::Converter(const LSValue& aValue) {
  using ConversionType = LSValue::ConversionType;
  using CompressionType = LSValue::CompressionType;

  if (aValue.mBuffer.IsVoid()) {
    mBuffer.SetIsVoid(true);
    return;
  }

  const CompressionType compressionType = aValue.GetCompressionType();
  const ConversionType conversionType = aValue.GetConversionType();

  const nsCString uncompressed = [compressionType, &aValue]() {
    if (CompressionType::UNCOMPRESSED != compressionType) {
      nsCString buffer;
      MOZ_ASSERT(CompressionType::SNAPPY == compressionType);
      if (NS_WARN_IF(!SnappyUncompress(aValue.mBuffer, buffer))) {
        buffer.Truncate();
      }
      return buffer;
    }

    return aValue.mBuffer;
  }();

  if (ConversionType::NONE != conversionType) {
    MOZ_ASSERT(ConversionType::UTF16_UTF8 == conversionType);
    if (NS_WARN_IF(!CopyUTF8toUTF16(uncompressed, mBuffer, fallible))) {
      mBuffer.SetIsVoid(true);
    }
    return;
  }

  if (NS_WARN_IF(!PutCStringBytesToString(uncompressed, mBuffer))) {
    mBuffer.SetIsVoid(true);
  }
}

bool LSValue::InitFromString(const nsAString& aBuffer) {
  MOZ_ASSERT(mBuffer.IsVoid());
  MOZ_ASSERT(!mUTF16Length);
  MOZ_ASSERT(ConversionType::NONE == mConversionType);
  MOZ_ASSERT(CompressionType::UNCOMPRESSED == mCompressionType);

  if (aBuffer.IsVoid()) {
    return true;
  }

  const uint32_t utf16Length = aBuffer.Length();

  const auto conversionRes = [&aBuffer]() -> TypeBufferResult<ConversionType> {
    nsCString converted;

    if (Utf16ValidUpTo(aBuffer) == aBuffer.Length()) {
      if (NS_WARN_IF(!CopyUTF16toUTF8(aBuffer, converted, fallible))) {
        return Err(NS_ERROR_OUT_OF_MEMORY);
      }
      return std::pair{ConversionType::UTF16_UTF8, std::move(converted)};
    }

    if (NS_WARN_IF(!PutStringBytesToCString(aBuffer, converted))) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }
    return std::pair{ConversionType::NONE, std::move(converted)};
  }();

  if (conversionRes.isErr()) {
    return false;
  }

  const auto& [conversionType, converted] = conversionRes.inspect();

  const auto compressionRes =
      [&converted = converted]() -> TypeBufferResult<CompressionType> {
    nsCString compressed;
    if (NS_WARN_IF(!SnappyCompress(converted, compressed))) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }
    if (!compressed.IsVoid()) {
      return std::pair{CompressionType::SNAPPY, std::move(compressed)};
    }

    compressed = converted;
    return std::pair{CompressionType::UNCOMPRESSED, std::move(compressed)};
  }();

  if (compressionRes.isErr()) {
    return false;
  }

  const auto& [compressionType, compressed] = compressionRes.inspect();

  mBuffer = compressed;
  mUTF16Length = utf16Length;
  mConversionType = conversionType;
  mCompressionType = compressionType;

  return true;
}

nsresult LSValue::InitFromStatement(mozIStorageStatement* aStatement,
                                    uint32_t aIndex) {
  MOZ_ASSERT(aStatement);
  MOZ_ASSERT(mBuffer.IsVoid());
  MOZ_ASSERT(!mUTF16Length);
  MOZ_ASSERT(ConversionType::NONE == mConversionType);
  MOZ_ASSERT(CompressionType::UNCOMPRESSED == mCompressionType);

  int32_t utf16Length;
  nsresult rv = aStatement->GetInt32(aIndex, &utf16Length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t conversionType;
  rv = aStatement->GetInt32(aIndex + 1, &conversionType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t compressionType;
  rv = aStatement->GetInt32(aIndex + 2, &compressionType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString buffer;
  rv = aStatement->GetBlobAsUTF8String(aIndex + 3, buffer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mBuffer = buffer;
  mUTF16Length = static_cast<uint32_t>(utf16Length);
  mConversionType = static_cast<decltype(mConversionType)>(conversionType);
  mCompressionType = static_cast<decltype(mCompressionType)>(compressionType);

  return NS_OK;
}

const LSValue& VoidLSValue() {
  static const LSValue sVoidLSValue;

  return sVoidLSValue;
}

}  // namespace mozilla::dom
