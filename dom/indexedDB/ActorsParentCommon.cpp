/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParentCommon.h"

// local includes
#include "FileInfoFwd.h"  // for FileInfo
#include "FileManager.h"
#include "IndexedDatabase.h"  // for StructuredCloneFile...
#include "IndexedDatabaseManager.h"
#include "IndexedDBCommon.h"
#include "ReportInternalError.h"

// global includes
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <numeric>
#include <type_traits>
#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "SafeRefPtr.h"
#include "js/RootingAPI.h"
#include "js/StructuredClone.h"
#include "mozIStorageStatement.h"
#include "mozIStorageValueArray.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/JSObjectHolder.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/SnappyUncompressInputStream.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryScalarEnums.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/fallible.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIInputStream.h"
#include "nsIPrincipal.h"
#include "nsIXPConnect.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsXULAppAPI.h"
#include "snappy/snappy.h"

class nsIFile;

namespace mozilla::dom::indexedDB {

using mozilla::ipc::IsOnBackgroundThread;

namespace {
bool TokenizerIgnoreNothing(char16_t /* aChar */) { return false; }

constexpr StructuredCloneFileBase::FileType ToStructuredCloneFileType(
    const char16_t aTag) {
  switch (aTag) {
    case char16_t('-'):
      return StructuredCloneFileBase::eMutableFile;

    case char16_t('.'):
      return StructuredCloneFileBase::eStructuredClone;

    case char16_t('/'):
      return StructuredCloneFileBase::eWasmBytecode;

    case char16_t('\\'):
      return StructuredCloneFileBase::eWasmCompiled;

    default:
      return StructuredCloneFileBase::eBlob;
  }
}

int32_t ToInteger(const nsAString& aStr, nsresult* const aRv) {
  return aStr.ToInteger(aRv);
}

Result<StructuredCloneFileParent, nsresult> DeserializeStructuredCloneFile(
    const FileManager& aFileManager, const nsDependentSubstring& aText) {
  MOZ_ASSERT(!aText.IsEmpty());

  const StructuredCloneFileBase::FileType type =
      ToStructuredCloneFileType(aText.First());

  IDB_TRY_VAR(
      const auto id,
      ToResultGet<int32_t>(
          ToInteger, type == StructuredCloneFileBase::eBlob
                         ? aText
                         : static_cast<const nsAString&>(Substring(aText, 1))));

  SafeRefPtr<FileInfo> fileInfo = aFileManager.GetFileInfo(id);
  MOZ_ASSERT(fileInfo);
  // XXX In bug 1432133, for some reasons FileInfo object cannot be got. This
  // is just a short-term fix, and we are working on finding the real cause
  // in bug 1519859.
  if (!fileInfo) {
    IDB_WARNING(
        "Corrupt structured clone data detected in IndexedDB. Failing the "
        "database request. Bug 1519859 will address this problem.");
    Telemetry::ScalarAdd(Telemetry::ScalarID::IDB_FAILURE_FILEINFO_ERROR, 1);

    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  return StructuredCloneFileParent{type, std::move(fileInfo)};
}

// This class helps to create only 1 sandbox.
class SandboxHolder final {
 public:
  NS_INLINE_DECL_REFCOUNTING(SandboxHolder)

 private:
  friend JSObject* mozilla::dom::indexedDB::GetSandbox(JSContext* aCx);

  ~SandboxHolder() = default;

  static SandboxHolder* GetOrCreate() {
    MOZ_ASSERT(XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());

    static StaticRefPtr<SandboxHolder> sHolder;
    if (!sHolder) {
      sHolder = new SandboxHolder();
      ClearOnShutdown(&sHolder);
    }
    return sHolder;
  }

  JSObject* GetSandboxInternal(JSContext* aCx) {
    if (!mSandbox) {
      nsIXPConnect* const xpc = nsContentUtils::XPConnect();
      MOZ_ASSERT(xpc, "This should never be null!");

      // Let's use a null principal.
      const nsCOMPtr<nsIPrincipal> principal =
          NullPrincipal::CreateWithoutOriginAttributes();

      JS::Rooted<JSObject*> sandbox(aCx);
      nsresult rv = xpc->CreateSandbox(aCx, principal, sandbox.address());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      mSandbox = new JSObjectHolder(aCx, sandbox);
    }

    return mSandbox->GetJSObject();
  }

  RefPtr<JSObjectHolder> mSandbox;
};

uint32_t CompressedByteCountForNumber(uint64_t aNumber) {
  // All bytes have 7 bits available.
  uint32_t count = 1;
  while ((aNumber >>= 7)) {
    count++;
  }

  return count;
}

uint32_t CompressedByteCountForIndexId(IndexOrObjectStoreId aIndexId) {
  MOZ_ASSERT(aIndexId);
  MOZ_ASSERT(UINT64_MAX - uint64_t(aIndexId) >= uint64_t(aIndexId),
             "Overflow!");

  return CompressedByteCountForNumber(uint64_t(aIndexId * 2));
}

void WriteCompressedNumber(uint64_t aNumber, uint8_t** aIterator) {
  MOZ_ASSERT(aIterator);
  MOZ_ASSERT(*aIterator);

  uint8_t*& buffer = *aIterator;

#ifdef DEBUG
  const uint8_t* const bufferStart = buffer;
  const uint64_t originalNumber = aNumber;
#endif

  while (true) {
    uint64_t shiftedNumber = aNumber >> 7;
    if (shiftedNumber) {
      *buffer++ = uint8_t(0x80 | (aNumber & 0x7f));
      aNumber = shiftedNumber;
    } else {
      *buffer++ = uint8_t(aNumber);
      break;
    }
  }

  MOZ_ASSERT(buffer > bufferStart);
  MOZ_ASSERT(uint32_t(buffer - bufferStart) ==
             CompressedByteCountForNumber(originalNumber));
}

void WriteCompressedIndexId(IndexOrObjectStoreId aIndexId, bool aUnique,
                            uint8_t** aIterator) {
  MOZ_ASSERT(aIndexId);
  MOZ_ASSERT(UINT64_MAX - uint64_t(aIndexId) >= uint64_t(aIndexId),
             "Overflow!");
  MOZ_ASSERT(aIterator);
  MOZ_ASSERT(*aIterator);

  const uint64_t indexId = (uint64_t(aIndexId * 2) | (aUnique ? 1 : 0));
  WriteCompressedNumber(indexId, aIterator);
}

// aOutIndexValues is an output parameter, since its storage is reused.
nsresult ReadCompressedIndexDataValuesFromBlob(
    const Span<const uint8_t> aBlobData,
    nsTArray<IndexDataValue>& aOutIndexValues) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!aBlobData.IsEmpty());
  MOZ_ASSERT(aOutIndexValues.IsEmpty());

  AUTO_PROFILER_LABEL("ReadCompressedIndexDataValuesFromBlob", DOM);

  // XXX Is this check still necessary with a Span? Or should it rather be moved
  // to the caller?
  if (uintptr_t(aBlobData.Elements()) > UINTPTR_MAX - aBlobData.LengthBytes()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  for (auto remainder = aBlobData; !remainder.IsEmpty();) {
    IDB_TRY_VAR((const auto [indexId, unique, remainderAfterIndexId]),
                ReadCompressedIndexId(remainder));

    if (NS_WARN_IF(remainderAfterIndexId.IsEmpty())) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    // Read key buffer length.
    IDB_TRY_VAR((const auto [keyBufferLength, remainderAfterKeyBufferLength]),
                ReadCompressedNumber(remainderAfterIndexId));

    if (NS_WARN_IF(remainderAfterKeyBufferLength.IsEmpty()) ||
        NS_WARN_IF(keyBufferLength > uint64_t(UINT32_MAX)) ||
        NS_WARN_IF(keyBufferLength > remainderAfterKeyBufferLength.Length())) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    const auto [keyBuffer, remainderAfterKeyBuffer] =
        remainderAfterKeyBufferLength.SplitAt(keyBufferLength);
    auto idv =
        IndexDataValue{indexId, unique, Key{nsCString{AsChars(keyBuffer)}}};

    // Read sort key buffer length.
    IDB_TRY_VAR(
        (const auto [sortKeyBufferLength, remainderAfterSortKeyBufferLength]),
        ReadCompressedNumber(remainderAfterKeyBuffer));

    remainder = remainderAfterSortKeyBufferLength;
    if (sortKeyBufferLength > 0) {
      if (NS_WARN_IF(remainder.IsEmpty()) ||
          NS_WARN_IF(sortKeyBufferLength > uint64_t(UINT32_MAX)) ||
          NS_WARN_IF(sortKeyBufferLength > remainder.Length())) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_FILE_CORRUPTED;
      }

      const auto [sortKeyBuffer, remainderAfterSortKeyBuffer] =
          remainder.SplitAt(sortKeyBufferLength);
      idv.mLocaleAwarePosition = Key{nsCString{AsChars(sortKeyBuffer)}};
      remainder = remainderAfterSortKeyBuffer;
    }

    if (NS_WARN_IF(!aOutIndexValues.AppendElement(std::move(idv), fallible))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  aOutIndexValues.Sort();

  return NS_OK;
}

// aOutIndexValues is an output parameter, since its storage is reused.
template <typename T>
nsresult ReadCompressedIndexDataValuesFromSource(
    T& aSource, uint32_t aColumnIndex,
    nsTArray<IndexDataValue>& aOutIndexValues) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aOutIndexValues.IsEmpty());

  int32_t columnType;
  nsresult rv = aSource.GetTypeOfIndex(aColumnIndex, &columnType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (columnType == mozIStorageStatement::VALUE_TYPE_NULL) {
    return NS_OK;
  }

  MOZ_ASSERT(columnType == mozIStorageStatement::VALUE_TYPE_BLOB);

  const uint8_t* blobData;
  uint32_t blobDataLength;
  rv = aSource.GetSharedBlob(aColumnIndex, &blobDataLength, &blobData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!blobDataLength)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  rv = ReadCompressedIndexDataValuesFromBlob(Span(blobData, blobDataLength),
                                             aOutIndexValues);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

Result<StructuredCloneReadInfoParent, nsresult>
GetStructuredCloneReadInfoFromBlob(const uint8_t* aBlobData,
                                   uint32_t aBlobDataLength,
                                   const FileManager& aFileManager,
                                   const nsAString& aFileIds) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  AUTO_PROFILER_LABEL("GetStructuredCloneReadInfoFromBlob", DOM);

  const char* const compressed = reinterpret_cast<const char*>(aBlobData);
  const size_t compressedLength = size_t(aBlobDataLength);

  size_t uncompressedLength;
  if (NS_WARN_IF(!snappy::GetUncompressedLength(compressed, compressedLength,
                                                &uncompressedLength))) {
    return Err(NS_ERROR_FILE_CORRUPTED);
  }

  AutoTArray<uint8_t, 512> uncompressed;
  if (NS_WARN_IF(!uncompressed.SetLength(uncompressedLength, fallible))) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  char* const uncompressedBuffer =
      reinterpret_cast<char*>(uncompressed.Elements());

  if (NS_WARN_IF(!snappy::RawUncompress(compressed, compressedLength,
                                        uncompressedBuffer))) {
    return Err(NS_ERROR_FILE_CORRUPTED);
  }

  JSStructuredCloneData data(JS::StructuredCloneScope::DifferentProcess);
  if (!data.AppendBytes(uncompressedBuffer, uncompressed.Length())) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  nsTArray<StructuredCloneFileParent> files;
  if (!aFileIds.IsVoid()) {
    IDB_TRY_VAR(files, DeserializeStructuredCloneFiles(aFileManager, aFileIds));
  }

  return StructuredCloneReadInfoParent{std::move(data), std::move(files),
                                       false};
}

Result<StructuredCloneReadInfoParent, nsresult>
GetStructuredCloneReadInfoFromExternalBlob(uint64_t aIntData,
                                           const FileManager& aFileManager,
                                           const nsAString& aFileIds) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  AUTO_PROFILER_LABEL("GetStructuredCloneReadInfoFromExternalBlob", DOM);

  nsresult rv;

  nsTArray<StructuredCloneFileParent> files;
  if (!aFileIds.IsVoid()) {
    IDB_TRY_VAR(files, DeserializeStructuredCloneFiles(aFileManager, aFileIds));
  }

  // Higher and lower 32 bits described
  // in ObjectStoreAddOrPutRequestOp::DoDatabaseWork.
  const uint32_t index = uint32_t(aIntData & 0xFFFFFFFF);

  if (index >= files.Length()) {
    MOZ_ASSERT(false, "Bad index value!");
    return Err(NS_ERROR_UNEXPECTED);
  }

  if (IndexedDatabaseManager::PreprocessingEnabled()) {
    return StructuredCloneReadInfoParent{
        JSStructuredCloneData{JS::StructuredCloneScope::DifferentProcess},
        std::move(files), true};
  }

  // XXX Why can there be multiple files, but we use only a single one here?
  const StructuredCloneFileParent& file = files[index];
  MOZ_ASSERT(file.Type() == StructuredCloneFileBase::eStructuredClone);

  const nsCOMPtr<nsIFile> nativeFile = file.FileInfo().GetFileForFileInfo();
  if (NS_WARN_IF(!nativeFile)) {
    return Err(NS_ERROR_FAILURE);
  }

  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), nativeFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  const auto snappyInputStream =
      MakeRefPtr<SnappyUncompressInputStream>(fileInputStream);

  auto data = JSStructuredCloneData{JS::StructuredCloneScope::DifferentProcess};
  do {
    char buffer[kFileCopyBufferSize];

    uint32_t numRead;
    rv = snappyInputStream->Read(buffer, sizeof(buffer), &numRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }

    if (!numRead) {
      break;
    }

    if (NS_WARN_IF(!data.AppendBytes(buffer, numRead))) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }
  } while (true);

  return StructuredCloneReadInfoParent{std::move(data), std::move(files),
                                       false};
}

template <typename T>
Result<StructuredCloneReadInfoParent, nsresult>
GetStructuredCloneReadInfoFromSource(T* aSource, uint32_t aDataIndex,
                                     uint32_t aFileIdsIndex,
                                     const FileManager& aFileManager) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aSource);

  int32_t columnType;
  nsresult rv = aSource->GetTypeOfIndex(aDataIndex, &columnType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  MOZ_ASSERT(columnType == mozIStorageStatement::VALUE_TYPE_BLOB ||
             columnType == mozIStorageStatement::VALUE_TYPE_INTEGER);

  bool isNull;
  rv = aSource->GetIsNull(aFileIdsIndex, &isNull);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  nsString fileIds;

  if (isNull) {
    fileIds.SetIsVoid(true);
  } else {
    rv = aSource->GetString(aFileIdsIndex, fileIds);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }
  }

  if (columnType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
    uint64_t intData;
    rv = aSource->GetInt64(aDataIndex, reinterpret_cast<int64_t*>(&intData));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }

    return GetStructuredCloneReadInfoFromExternalBlob(intData, aFileManager,
                                                      fileIds);
  }

  const uint8_t* blobData;
  uint32_t blobDataLength;
  rv = aSource->GetSharedBlob(aDataIndex, &blobDataLength, &blobData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  return GetStructuredCloneReadInfoFromBlob(blobData, blobDataLength,
                                            aFileManager, fileIds);
}

}  // namespace

IndexDataValue::IndexDataValue() : mIndexId(0), mUnique(false) {
  MOZ_COUNT_CTOR(IndexDataValue);
}

#ifdef NS_BUILD_REFCNT_LOGGING
IndexDataValue::IndexDataValue(IndexDataValue&& aOther)
    : mIndexId(aOther.mIndexId),
      mPosition(std::move(aOther.mPosition)),
      mLocaleAwarePosition(std::move(aOther.mLocaleAwarePosition)),
      mUnique(aOther.mUnique) {
  MOZ_ASSERT(!aOther.mPosition.IsUnset());

  MOZ_COUNT_CTOR(IndexDataValue);
}
#endif

IndexDataValue::IndexDataValue(IndexOrObjectStoreId aIndexId, bool aUnique,
                               const Key& aPosition)
    : mIndexId(aIndexId), mPosition(aPosition), mUnique(aUnique) {
  MOZ_ASSERT(!aPosition.IsUnset());

  MOZ_COUNT_CTOR(IndexDataValue);
}

IndexDataValue::IndexDataValue(IndexOrObjectStoreId aIndexId, bool aUnique,
                               const Key& aPosition,
                               const Key& aLocaleAwarePosition)
    : mIndexId(aIndexId),
      mPosition(aPosition),
      mLocaleAwarePosition(aLocaleAwarePosition),
      mUnique(aUnique) {
  MOZ_ASSERT(!aPosition.IsUnset());

  MOZ_COUNT_CTOR(IndexDataValue);
}

bool IndexDataValue::operator==(const IndexDataValue& aOther) const {
  if (mIndexId != aOther.mIndexId) {
    return false;
  }
  if (mLocaleAwarePosition.IsUnset()) {
    return mPosition == aOther.mPosition;
  }
  return mLocaleAwarePosition == aOther.mLocaleAwarePosition;
}

bool IndexDataValue::operator<(const IndexDataValue& aOther) const {
  if (mIndexId == aOther.mIndexId) {
    if (mLocaleAwarePosition.IsUnset()) {
      return mPosition < aOther.mPosition;
    }
    return mLocaleAwarePosition < aOther.mLocaleAwarePosition;
  }

  return mIndexId < aOther.mIndexId;
}

JSObject* GetSandbox(JSContext* aCx) {
  SandboxHolder* holder = SandboxHolder::GetOrCreate();
  return holder->GetSandboxInternal(aCx);
}

Result<std::pair<UniqueFreePtr<uint8_t>, uint32_t>, nsresult>
MakeCompressedIndexDataValues(const nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  AUTO_PROFILER_LABEL("MakeCompressedIndexDataValues", DOM);

  const uint32_t arrayLength = aIndexValues.Length();
  if (!arrayLength) {
    return std::pair{UniqueFreePtr<uint8_t>{}, 0u};
  }

  // First calculate the size of the final buffer.
  const auto blobDataLength = std::accumulate(
      aIndexValues.cbegin(), aIndexValues.cend(), CheckedUint32(0),
      [](CheckedUint32 sum, const IndexDataValue& info) {
        const nsCString& keyBuffer = info.mPosition.GetBuffer();
        const nsCString& sortKeyBuffer = info.mLocaleAwarePosition.GetBuffer();
        const uint32_t keyBufferLength = keyBuffer.Length();
        const uint32_t sortKeyBufferLength = sortKeyBuffer.Length();

        MOZ_ASSERT(!keyBuffer.IsEmpty());

        return sum + CompressedByteCountForIndexId(info.mIndexId) +
               CompressedByteCountForNumber(keyBufferLength) +
               CompressedByteCountForNumber(sortKeyBufferLength) +
               keyBufferLength + sortKeyBufferLength;
      });

  if (NS_WARN_IF(!blobDataLength.isValid())) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  UniqueFreePtr<uint8_t> blobData(
      static_cast<uint8_t*>(malloc(blobDataLength.value())));
  if (NS_WARN_IF(!blobData)) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  uint8_t* blobDataIter = blobData.get();

  for (const IndexDataValue& info : aIndexValues) {
    const nsCString& keyBuffer = info.mPosition.GetBuffer();
    const nsCString& sortKeyBuffer = info.mLocaleAwarePosition.GetBuffer();
    const uint32_t keyBufferLength = keyBuffer.Length();
    const uint32_t sortKeyBufferLength = sortKeyBuffer.Length();

    WriteCompressedIndexId(info.mIndexId, info.mUnique, &blobDataIter);
    WriteCompressedNumber(keyBufferLength, &blobDataIter);

    memcpy(blobDataIter, keyBuffer.get(), keyBufferLength);
    blobDataIter += keyBufferLength;

    WriteCompressedNumber(sortKeyBufferLength, &blobDataIter);

    memcpy(blobDataIter, sortKeyBuffer.get(), sortKeyBufferLength);
    blobDataIter += sortKeyBufferLength;
  }

  MOZ_ASSERT(blobDataIter == blobData.get() + blobDataLength.value());

  return std::pair{std::move(blobData), blobDataLength.value()};
}

nsresult ReadCompressedIndexDataValues(
    mozIStorageStatement& aStatement, uint32_t aColumnIndex,
    nsTArray<IndexDataValue>& aOutIndexValues) {
  return ReadCompressedIndexDataValuesFromSource(aStatement, aColumnIndex,
                                                 aOutIndexValues);
}

using IndexDataValuesAutoArray = AutoTArray<IndexDataValue, 32>;
Result<IndexDataValuesAutoArray, nsresult> ReadCompressedIndexDataValues(
    mozIStorageValueArray& aValues, uint32_t aColumnIndex) {
  IndexDataValuesAutoArray result;
  const nsresult rv =
      ReadCompressedIndexDataValuesFromSource(aValues, aColumnIndex, result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  return result;
}

Result<std::tuple<IndexOrObjectStoreId, bool, Span<const uint8_t>>, nsresult>
ReadCompressedIndexId(const Span<const uint8_t> aData) {
  IDB_TRY_VAR((const auto [indexId, remainder]), ReadCompressedNumber(aData));

  MOZ_ASSERT(UINT64_MAX / 2 >= uint64_t(indexId), "Bad index id!");

  return std::tuple{IndexOrObjectStoreId(indexId >> 1), indexId % 2 == 1,
                    remainder};
}

Result<std::pair<uint64_t, mozilla::Span<const uint8_t>>, nsresult>
ReadCompressedNumber(const Span<const uint8_t> aSpan) {
  uint8_t shiftCounter = 0;
  uint64_t result = 0;

  const auto end = aSpan.cend();

  const auto newPos =
      std::find_if(aSpan.cbegin(), end, [&result, &shiftCounter](uint8_t byte) {
        MOZ_ASSERT(shiftCounter <= 56, "Shifted too many bits!");

        result += (uint64_t(byte & 0x7f) << shiftCounter);
        shiftCounter += 7;

        return !(byte & 0x80);
      });

  if (NS_WARN_IF(newPos == end)) {
    MOZ_ASSERT(false);
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_FILE_CORRUPTED);
  }

  return std::pair{result, Span{newPos + 1, end}};
}

Result<StructuredCloneReadInfoParent, nsresult>
GetStructuredCloneReadInfoFromValueArray(mozIStorageValueArray* aValues,
                                         uint32_t aDataIndex,
                                         uint32_t aFileIdsIndex,
                                         const FileManager& aFileManager) {
  return GetStructuredCloneReadInfoFromSource(aValues, aDataIndex,
                                              aFileIdsIndex, aFileManager);
}

Result<StructuredCloneReadInfoParent, nsresult>
GetStructuredCloneReadInfoFromStatement(mozIStorageStatement* aStatement,
                                        uint32_t aDataIndex,
                                        uint32_t aFileIdsIndex,
                                        const FileManager& aFileManager) {
  return GetStructuredCloneReadInfoFromSource(aStatement, aDataIndex,
                                              aFileIdsIndex, aFileManager);
}

Result<nsTArray<StructuredCloneFileParent>, nsresult>
DeserializeStructuredCloneFiles(const FileManager& aFileManager,
                                const nsAString& aText) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  nsCharSeparatedTokenizerTemplate<TokenizerIgnoreNothing> tokenizer(aText,
                                                                     ' ');

  nsTArray<StructuredCloneFileParent> result;
  while (tokenizer.hasMoreTokens()) {
    const auto& token = tokenizer.nextToken();
    MOZ_ASSERT(!token.IsEmpty());

    IDB_TRY_VAR(auto structuredCloneFile,
                DeserializeStructuredCloneFile(aFileManager, token));

    result.EmplaceBack(std::move(structuredCloneFile));
  }

  return result;
}

}  // namespace mozilla::dom::indexedDB
