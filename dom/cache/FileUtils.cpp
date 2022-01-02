/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileUtilsImpl.h"

#include "DBSchema.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SnappyCompressOutputStream.h"
#include "mozilla/Unused.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIFile.h"
#include "nsIUUIDGenerator.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::cache {

using mozilla::dom::quota::Client;
using mozilla::dom::quota::CloneFileAndAppend;
using mozilla::dom::quota::FileInputStream;
using mozilla::dom::quota::FileOutputStream;
using mozilla::dom::quota::GetDirEntryKind;
using mozilla::dom::quota::nsIFileKind;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::QuotaObject;

namespace {

// Const variable for generate padding size.
// XXX This will be tweaked to something more meaningful in Bug 1383656.
const int64_t kRoundUpNumber = 20480;

enum BodyFileType { BODY_FILE_FINAL, BODY_FILE_TMP };

Result<NotNull<nsCOMPtr<nsIFile>>, nsresult> BodyIdToFile(nsIFile& aBaseDir,
                                                          const nsID& aId,
                                                          BodyFileType aType);

int64_t RoundUp(int64_t aX, int64_t aY);

// The alogrithm for generating padding refers to the mitigation approach in
// https://github.com/whatwg/storage/issues/31.
// First, generate a random number between 0 and 100kB.
// Next, round up the sum of random number and response size to the nearest
// 20kB.
// Finally, the virtual padding size will be the result minus the response size.
int64_t BodyGeneratePadding(int64_t aBodyFileSize, uint32_t aPaddingInfo);

nsresult DirectoryPaddingWrite(nsIFile& aBaseDir,
                               DirPaddingFile aPaddingFileType,
                               int64_t aPaddingSize);

const auto kMorgueDirectory = u"morgue"_ns;

bool IsFileNotFoundError(const nsresult aRv) {
  return aRv == NS_ERROR_FILE_NOT_FOUND ||
         aRv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
}

Result<NotNull<nsCOMPtr<nsIFile>>, nsresult> BodyGetCacheDir(nsIFile& aBaseDir,
                                                             const nsID& aId) {
  QM_TRY_UNWRAP(auto cacheDir, CloneFileAndAppend(aBaseDir, kMorgueDirectory));

  // Some file systems have poor performance when there are too many files
  // in a single directory.  Mitigate this issue by spreading the body
  // files out into sub-directories.  We use the last byte of the ID for
  // the name of the sub-directory.
  QM_TRY(MOZ_TO_RESULT(cacheDir->Append(IntToString(aId.m3[7]))));

  // Callers call this function without checking if the directory already
  // exists (idempotent usage). QM_OR_ELSE_WARN_IF is not used here since we
  // just want to log NS_ERROR_FILE_ALREADY_EXISTS result and not spam the
  // reports.
  QM_TRY(QM_OR_ELSE_LOG_VERBOSE_IF(
      // Expression.
      MOZ_TO_RESULT(cacheDir->Create(nsIFile::DIRECTORY_TYPE, 0755)),
      // Predicate.
      IsSpecificError<NS_ERROR_FILE_ALREADY_EXISTS>,
      // Fallback.
      ErrToDefaultOk<>));

  return WrapNotNullUnchecked(std::move(cacheDir));
}

}  // namespace

nsresult BodyCreateDir(nsIFile& aBaseDir) {
  QM_TRY_INSPECT(const auto& bodyDir,
                 CloneFileAndAppend(aBaseDir, kMorgueDirectory));

  // Callers call this function without checking if the directory already
  // exists (idempotent usage). QM_OR_ELSE_WARN_IF is not used here since we
  // just want to log NS_ERROR_FILE_ALREADY_EXISTS result and not spam the
  // reports.
  QM_TRY(QM_OR_ELSE_LOG_VERBOSE_IF(
      // Expression.
      MOZ_TO_RESULT(bodyDir->Create(nsIFile::DIRECTORY_TYPE, 0755)),
      // Predicate.
      IsSpecificError<NS_ERROR_FILE_ALREADY_EXISTS>,
      // Fallback.
      ErrToDefaultOk<>));

  return NS_OK;
}

nsresult BodyDeleteDir(const CacheDirectoryMetadata& aDirectoryMetadata,
                       nsIFile& aBaseDir) {
  QM_TRY_INSPECT(const auto& bodyDir,
                 CloneFileAndAppend(aBaseDir, kMorgueDirectory));

  QM_TRY(MOZ_TO_RESULT(RemoveNsIFileRecursively(aDirectoryMetadata, *bodyDir)));

  return NS_OK;
}

Result<std::pair<nsID, nsCOMPtr<nsISupports>>, nsresult> BodyStartWriteStream(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aBaseDir,
    nsIInputStream& aSource, void* aClosure, nsAsyncCopyCallbackFun aCallback) {
  MOZ_DIAGNOSTIC_ASSERT(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(aCallback);

  QM_TRY_INSPECT(const auto& idGen,
                 MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIUUIDGenerator>,
                                         MOZ_SELECT_OVERLOAD(do_GetService),
                                         "@mozilla.org/uuid-generator;1"));

  nsID id;
  QM_TRY(MOZ_TO_RESULT(idGen->GenerateUUIDInPlace(&id)));

  QM_TRY_INSPECT(const auto& finalFile,
                 BodyIdToFile(aBaseDir, id, BODY_FILE_FINAL));

  {
    QM_TRY_INSPECT(const bool& exists,
                   MOZ_TO_RESULT_INVOKE_MEMBER(*finalFile, Exists));

    QM_TRY(OkIf(!exists), Err(NS_ERROR_FILE_ALREADY_EXISTS));
  }

  QM_TRY_INSPECT(const auto& tmpFile,
                 BodyIdToFile(aBaseDir, id, BODY_FILE_TMP));

  QM_TRY_INSPECT(
      const auto& fileStream,
      CreateFileOutputStream(PERSISTENCE_TYPE_DEFAULT, aDirectoryMetadata,
                             Client::DOMCACHE, tmpFile.get()));

  const auto compressed =
      MakeRefPtr<SnappyCompressOutputStream>(fileStream.get());

  const nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

  nsCOMPtr<nsISupports> copyContext;
  QM_TRY(MOZ_TO_RESULT(
      NS_AsyncCopy(&aSource, compressed, target, NS_ASYNCCOPY_VIA_WRITESEGMENTS,
                   compressed->BlockSize(), aCallback, aClosure, true,
                   true,  // close streams
                   getter_AddRefs(copyContext))));

  return std::make_pair(id, std::move(copyContext));
}

void BodyCancelWrite(nsISupports& aCopyContext) {
  QM_WARNONLY_TRY(
      QM_TO_RESULT(NS_CancelAsyncCopy(&aCopyContext, NS_ERROR_ABORT)));

  // TODO The partially written file must be cleaned up after the async copy
  // makes its callback.
}

nsresult BodyFinalizeWrite(nsIFile& aBaseDir, const nsID& aId) {
  QM_TRY_INSPECT(const auto& tmpFile,
                 BodyIdToFile(aBaseDir, aId, BODY_FILE_TMP));

  QM_TRY_INSPECT(const auto& finalFile,
                 BodyIdToFile(aBaseDir, aId, BODY_FILE_FINAL));

  nsAutoString finalFileName;
  QM_TRY(MOZ_TO_RESULT(finalFile->GetLeafName(finalFileName)));

  // It's fine to not notify the QuotaManager that the path has been changed,
  // because its path will be updated and its size will be recalculated when
  // opening file next time.
  QM_TRY(MOZ_TO_RESULT(tmpFile->RenameTo(nullptr, finalFileName)));

  return NS_OK;
}

Result<NotNull<nsCOMPtr<nsIInputStream>>, nsresult> BodyOpen(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aBaseDir,
    const nsID& aId) {
  QM_TRY_INSPECT(const auto& finalFile,
                 BodyIdToFile(aBaseDir, aId, BODY_FILE_FINAL));

  QM_TRY_RETURN(
      CreateFileInputStream(PERSISTENCE_TYPE_DEFAULT, aDirectoryMetadata,
                            Client::DOMCACHE, finalFile.get())
          .map([](NotNull<RefPtr<FileInputStream>>&& stream) {
            return WrapNotNullUnchecked(nsCOMPtr<nsIInputStream>{stream.get()});
          }));
}

nsresult BodyMaybeUpdatePaddingSize(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aBaseDir,
    const nsID& aId, const uint32_t aPaddingInfo, int64_t* aPaddingSizeInOut) {
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSizeInOut);

  QM_TRY_INSPECT(const auto& bodyFile,
                 BodyIdToFile(aBaseDir, aId, BODY_FILE_TMP));

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(quotaManager);

  int64_t fileSize = 0;
  RefPtr<QuotaObject> quotaObject = quotaManager->GetQuotaObject(
      PERSISTENCE_TYPE_DEFAULT, aDirectoryMetadata, Client::DOMCACHE,
      bodyFile.get(), -1, &fileSize);
  MOZ_DIAGNOSTIC_ASSERT(quotaObject);
  MOZ_DIAGNOSTIC_ASSERT(fileSize >= 0);
  // XXXtt: bug: https://bugzilla.mozilla.org/show_bug.cgi?id=1422815
  if (!quotaObject) {
    return NS_ERROR_UNEXPECTED;
  }

  if (*aPaddingSizeInOut == InternalResponse::UNKNOWN_PADDING_SIZE) {
    *aPaddingSizeInOut = BodyGeneratePadding(fileSize, aPaddingInfo);
  }

  MOZ_DIAGNOSTIC_ASSERT(*aPaddingSizeInOut >= 0);

  if (!quotaObject->IncreaseSize(*aPaddingSizeInOut)) {
    return NS_ERROR_FILE_NO_DEVICE_SPACE;
  }

  return NS_OK;
}

nsresult BodyDeleteFiles(const CacheDirectoryMetadata& aDirectoryMetadata,
                         nsIFile& aBaseDir, const nsTArray<nsID>& aIdList) {
  for (const auto id : aIdList) {
    QM_TRY_INSPECT(const auto& bodyDir, BodyGetCacheDir(aBaseDir, id));

    const auto removeFileForId =
        [&aDirectoryMetadata, &id](
            nsIFile& bodyFile,
            const nsACString& leafName) -> Result<bool, nsresult> {
      nsID fileId;
      QM_TRY(OkIf(fileId.Parse(leafName.BeginReading())), true,
             ([&aDirectoryMetadata, &bodyFile](const auto) {
               DebugOnly<nsresult> result = RemoveNsIFile(
                   aDirectoryMetadata, bodyFile, /* aTrackQuota */ false);
               MOZ_ASSERT(NS_SUCCEEDED(result));
             }));

      if (id.Equals(fileId)) {
        DebugOnly<nsresult> result =
            RemoveNsIFile(aDirectoryMetadata, bodyFile);
        MOZ_ASSERT(NS_SUCCEEDED(result));
        return true;
      }

      return false;
    };
    QM_TRY(MOZ_TO_RESULT(BodyTraverseFiles(aDirectoryMetadata, *bodyDir,
                                           removeFileForId,
                                           /* aCanRemoveFiles */ false,
                                           /* aTrackQuota */ true)));
  }

  return NS_OK;
}

namespace {

Result<NotNull<nsCOMPtr<nsIFile>>, nsresult> BodyIdToFile(
    nsIFile& aBaseDir, const nsID& aId, const BodyFileType aType) {
  QM_TRY_UNWRAP(auto bodyFile, BodyGetCacheDir(aBaseDir, aId));

  char idString[NSID_LENGTH];
  aId.ToProvidedString(idString);

  NS_ConvertASCIItoUTF16 fileName(idString);

  if (aType == BODY_FILE_FINAL) {
    fileName.AppendLiteral(".final");
  } else {
    fileName.AppendLiteral(".tmp");
  }

  QM_TRY(MOZ_TO_RESULT(bodyFile->Append(fileName)));

  return bodyFile;
}

int64_t RoundUp(const int64_t aX, const int64_t aY) {
  MOZ_DIAGNOSTIC_ASSERT(aX >= 0);
  MOZ_DIAGNOSTIC_ASSERT(aY > 0);

  MOZ_DIAGNOSTIC_ASSERT(INT64_MAX - ((aX - 1) / aY) * aY >= aY);
  return aY + ((aX - 1) / aY) * aY;
}

int64_t BodyGeneratePadding(const int64_t aBodyFileSize,
                            const uint32_t aPaddingInfo) {
  // Generate padding
  int64_t randomSize = static_cast<int64_t>(aPaddingInfo);
  MOZ_DIAGNOSTIC_ASSERT(INT64_MAX - aBodyFileSize >= randomSize);
  randomSize += aBodyFileSize;

  return RoundUp(randomSize, kRoundUpNumber) - aBodyFileSize;
}

nsresult DirectoryPaddingWrite(nsIFile& aBaseDir,
                               DirPaddingFile aPaddingFileType,
                               int64_t aPaddingSize) {
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSize >= 0);

  QM_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, aPaddingFileType == DirPaddingFile::TMP_FILE
                                       ? nsLiteralString(PADDING_TMP_FILE_NAME)
                                       : nsLiteralString(PADDING_FILE_NAME)));

  QM_TRY_INSPECT(const auto& outputStream, NS_NewLocalFileOutputStream(file));

  nsCOMPtr<nsIObjectOutputStream> objectStream =
      NS_NewObjectOutputStream(outputStream);

  QM_TRY(MOZ_TO_RESULT(objectStream->Write64(aPaddingSize)));

  return NS_OK;
}

}  // namespace

nsresult BodyDeleteOrphanedFiles(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aBaseDir,
    const nsTArray<nsID>& aKnownBodyIdList) {
  // body files are stored in a directory structure like:
  //
  //  /morgue/01/{01fdddb2-884d-4c3d-95ba-0c8062f6c325}.final
  //  /morgue/02/{02fdddb2-884d-4c3d-95ba-0c8062f6c325}.tmp

  QM_TRY_INSPECT(const auto& dir,
                 CloneFileAndAppend(aBaseDir, kMorgueDirectory));

  // Iterate over all the intermediate morgue subdirs
  QM_TRY(quota::CollectEachFile(
      *dir,
      [&aDirectoryMetadata, &aKnownBodyIdList](
          const nsCOMPtr<nsIFile>& subdir) -> Result<Ok, nsresult> {
        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*subdir));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory: {
            const auto removeOrphanedFiles =
                [&aDirectoryMetadata, &aKnownBodyIdList](
                    nsIFile& bodyFile,
                    const nsACString& leafName) -> Result<bool, nsresult> {
              // Finally, parse the uuid out of the name.  If it fails to parse,
              // then ignore the file.
              auto cleanup = MakeScopeExit([&aDirectoryMetadata, &bodyFile] {
                DebugOnly<nsresult> result =
                    RemoveNsIFile(aDirectoryMetadata, bodyFile);
                MOZ_ASSERT(NS_SUCCEEDED(result));
              });

              nsID id;
              QM_TRY(OkIf(id.Parse(leafName.BeginReading())), true);

              if (!aKnownBodyIdList.Contains(id)) {
                return true;
              }

              cleanup.release();

              return false;
            };

            // QM_OR_ELSE_WARN_IF is not used here since we just want to log
            // NS_ERROR_FILE_FS_CORRUPTED result and not spam the reports (even
            // a warning in the reports is not desired).
            QM_TRY(QM_OR_ELSE_LOG_VERBOSE_IF(
                // Expression.
                MOZ_TO_RESULT(BodyTraverseFiles(aDirectoryMetadata, *subdir,
                                                removeOrphanedFiles,
                                                /* aCanRemoveFiles */ true,
                                                /* aTrackQuota */ true)),
                // Predicate.
                IsSpecificError<NS_ERROR_FILE_FS_CORRUPTED>,
                // Fallback. We treat NS_ERROR_FILE_FS_CORRUPTED as if the
                // directory did not exist at all.
                ErrToDefaultOk<>));
            break;
          }

          case nsIFileKind::ExistsAsFile: {
            // If a file got in here somehow, try to remove it and move on
            DebugOnly<nsresult> result =
                RemoveNsIFile(aDirectoryMetadata, *subdir,
                              /* aTrackQuota */ false);
            MOZ_ASSERT(NS_SUCCEEDED(result));
            break;
          }

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return Ok{};
      }));

  return NS_OK;
}

namespace {

Result<nsCOMPtr<nsIFile>, nsresult> GetMarkerFileHandle(
    const CacheDirectoryMetadata& aDirectoryMetadata) {
  QM_TRY_UNWRAP(auto marker,
                CloneFileAndAppend(*aDirectoryMetadata.mDir, u"cache"_ns));

  QM_TRY(MOZ_TO_RESULT(marker->Append(u"context_open.marker"_ns)));

  return marker;
}

}  // namespace

nsresult CreateMarkerFile(const CacheDirectoryMetadata& aDirectoryMetadata) {
  QM_TRY_INSPECT(const auto& marker, GetMarkerFileHandle(aDirectoryMetadata));

  // Callers call this function without checking if the file already exists
  // (idempotent usage). QM_OR_ELSE_WARN_IF is not used here since we just want
  // to log NS_ERROR_FILE_ALREADY_EXISTS result and not spam the reports.
  //
  // TODO: In theory if this file exists, then Context::~Context should have
  // cleaned it up, but obviously we can crash and not clean it up, which is
  // the whole point of the marker file. In that case, we'll realize the marker
  // file exists in SetupAction::RunSyncWithDBOnTarget and do some cleanup, but
  // we won't delete the marker file, so if we see this marker file, it is part
  // of our standard operating procedure to redundantly try and create the
  // marker here. We currently treat this as idempotent usage, but we could
  // make sure to delete the marker file when handling the existing marker
  // file in SetupAction::RunSyncWithDBOnTarget and change
  // QM_OR_ELSE_LOG_VERBOSE_IF to QM_OR_ELSE_WARN_IF in the end.
  QM_TRY(QM_OR_ELSE_LOG_VERBOSE_IF(
      // Expression.
      MOZ_TO_RESULT(marker->Create(nsIFile::NORMAL_FILE_TYPE, 0644)),
      // Predicate.
      IsSpecificError<NS_ERROR_FILE_ALREADY_EXISTS>,
      // Fallback.
      ErrToDefaultOk<>));

  // Note, we don't need to fsync here.  We only care about actually
  // writing the marker if later modifications to the Cache are
  // actually flushed to the disk.  If the OS crashes before the marker
  // is written then we are ensured no other changes to the Cache were
  // flushed either.

  return NS_OK;
}

nsresult DeleteMarkerFile(const CacheDirectoryMetadata& aDirectoryMetadata) {
  QM_TRY_INSPECT(const auto& marker, GetMarkerFileHandle(aDirectoryMetadata));

  DebugOnly<nsresult> result =
      RemoveNsIFile(aDirectoryMetadata, *marker, /* aTrackQuota */ false);
  MOZ_ASSERT(NS_SUCCEEDED(result));

  // Again, no fsync is necessary.  If the OS crashes before the file
  // removal is flushed, then the Cache will search for stale data on
  // startup.  This will cause the next Cache access to be a bit slow, but
  // it seems appropriate after an OS crash.

  return NS_OK;
}

bool MarkerFileExists(const CacheDirectoryMetadata& aDirectoryMetadata) {
  QM_TRY_INSPECT(const auto& marker, GetMarkerFileHandle(aDirectoryMetadata),
                 false);

  QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_MEMBER(marker, Exists), false);
}

nsresult RemoveNsIFileRecursively(
    const Maybe<CacheDirectoryMetadata>& aDirectoryMetadata, nsIFile& aFile,
    const bool aTrackQuota) {
  // XXX This assertion proves that we can remove aTrackQuota and just check
  // aClientMetadata
  MOZ_DIAGNOSTIC_ASSERT_IF(aTrackQuota, aDirectoryMetadata);

  QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(aFile));

  switch (dirEntryKind) {
    case nsIFileKind::ExistsAsDirectory:
      // Unfortunately, we need to traverse all the entries and delete files one
      // by
      // one to update their usages to the QuotaManager.
      QM_TRY(quota::CollectEachFile(
          aFile,
          [&aDirectoryMetadata, &aTrackQuota](
              const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
            QM_TRY(MOZ_TO_RESULT(RemoveNsIFileRecursively(aDirectoryMetadata,
                                                          *file, aTrackQuota)));

            return Ok{};
          }));

      // In the end, remove the folder
      QM_TRY(MOZ_TO_RESULT(aFile.Remove(/* recursive */ false)));

      break;

    case nsIFileKind::ExistsAsFile:
      return RemoveNsIFile(aDirectoryMetadata, aFile, aTrackQuota);

    case nsIFileKind::DoesNotExist:
      // Ignore files that got removed externally while iterating.
      break;
  }

  return NS_OK;
}

nsresult RemoveNsIFile(const Maybe<CacheDirectoryMetadata>& aDirectoryMetadata,
                       nsIFile& aFile, const bool aTrackQuota) {
  // XXX This assertion proves that we can remove aTrackQuota and just check
  // aClientMetadata
  MOZ_DIAGNOSTIC_ASSERT_IF(aTrackQuota, aDirectoryMetadata);

  int64_t fileSize = 0;
  if (aTrackQuota) {
    QM_TRY_INSPECT(
        const auto& maybeFileSize,
        QM_OR_ELSE_WARN_IF(
            // Expression.
            MOZ_TO_RESULT_INVOKE_MEMBER(aFile, GetFileSize).map(Some<int64_t>),
            // Predicate.
            IsFileNotFoundError,
            // Fallback.
            ErrToDefaultOk<Maybe<int64_t>>));

    if (!maybeFileSize) {
      return NS_OK;
    }

    fileSize = *maybeFileSize;
  }

  QM_TRY(QM_OR_ELSE_WARN_IF(
      // Expression.
      MOZ_TO_RESULT(aFile.Remove(/* recursive */ false)),
      // Predicate.
      IsFileNotFoundError,
      // Fallback.
      ErrToDefaultOk<>));

  if (fileSize > 0) {
    MOZ_ASSERT(aTrackQuota);
    DecreaseUsageForDirectoryMetadata(*aDirectoryMetadata, fileSize);
  }

  return NS_OK;
}

void DecreaseUsageForDirectoryMetadata(
    const CacheDirectoryMetadata& aDirectoryMetadata,
    const int64_t aUpdatingSize) {
  MOZ_DIAGNOSTIC_ASSERT(aUpdatingSize > 0);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(quotaManager);

  quotaManager->DecreaseUsageForClient(
      quota::ClientMetadata{aDirectoryMetadata, Client::DOMCACHE},
      aUpdatingSize);
}

bool DirectoryPaddingFileExists(nsIFile& aBaseDir,
                                DirPaddingFile aPaddingFileType) {
  QM_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, aPaddingFileType == DirPaddingFile::TMP_FILE
                                       ? nsLiteralString(PADDING_TMP_FILE_NAME)
                                       : nsLiteralString(PADDING_FILE_NAME)),
      false);

  QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_MEMBER(file, Exists), false);
}

Result<int64_t, nsresult> DirectoryPaddingGet(nsIFile& aBaseDir) {
  MOZ_DIAGNOSTIC_ASSERT(
      !DirectoryPaddingFileExists(aBaseDir, DirPaddingFile::TMP_FILE));

  QM_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, nsLiteralString(PADDING_FILE_NAME)));

  QM_TRY_UNWRAP(auto stream, NS_NewLocalFileInputStream(file));

  QM_TRY_INSPECT(const auto& bufferedStream,
                 NS_NewBufferedInputStream(stream.forget(), 512));

  const nsCOMPtr<nsIObjectInputStream> objectStream =
      NS_NewObjectInputStream(bufferedStream);

  QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_MEMBER(objectStream, Read64)
                    .map([](const uint64_t val) { return int64_t(val); }));
}

nsresult DirectoryPaddingInit(nsIFile& aBaseDir) {
  QM_TRY(
      MOZ_TO_RESULT(DirectoryPaddingWrite(aBaseDir, DirPaddingFile::FILE, 0)));

  return NS_OK;
}

nsresult UpdateDirectoryPaddingFile(nsIFile& aBaseDir,
                                    mozIStorageConnection& aConn,
                                    const int64_t aIncreaseSize,
                                    const int64_t aDecreaseSize,
                                    const bool aTemporaryFileExist) {
  MOZ_DIAGNOSTIC_ASSERT(aIncreaseSize >= 0);
  MOZ_DIAGNOSTIC_ASSERT(aDecreaseSize >= 0);

  const auto directoryPaddingGetResult =
      aTemporaryFileExist ? Maybe<int64_t>{} : [&aBaseDir] {
        QM_TRY_RETURN(QM_OR_ELSE_WARN_IF(
                          // Expression.
                          DirectoryPaddingGet(aBaseDir).map(Some<int64_t>),
                          // Predicate.
                          IsFileNotFoundError,
                          // Fallback.
                          ErrToDefaultOk<Maybe<int64_t>>),
                      Maybe<int64_t>{});
      }();

  QM_TRY_INSPECT(
      const int64_t& currentPaddingSize,
      ([directoryPaddingGetResult, &aBaseDir, &aConn, aIncreaseSize,
        aDecreaseSize]() -> Result<int64_t, nsresult> {
        if (!directoryPaddingGetResult) {
          // Fail to read padding size from the dir padding file, so try to
          // restore.

          // Not delete the temporary padding file here, because we're going
          // to overwrite it below anyway.
          QM_TRY(MOZ_TO_RESULT(
              DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE)));

          // We don't need to add the aIncreaseSize or aDecreaseSize here,
          // because it's already encompassed within the database.
          QM_TRY_RETURN(db::FindOverallPaddingSize(aConn));
        }

        int64_t currentPaddingSize = directoryPaddingGetResult.value();
        bool shouldRevise = false;

        if (aIncreaseSize > 0) {
          if (INT64_MAX - currentPaddingSize < aDecreaseSize) {
            shouldRevise = true;
          } else {
            currentPaddingSize += aIncreaseSize;
          }
        }

        if (aDecreaseSize > 0) {
          if (currentPaddingSize < aDecreaseSize) {
            shouldRevise = true;
          } else if (!shouldRevise) {
            currentPaddingSize -= aDecreaseSize;
          }
        }

        if (shouldRevise) {
          // If somehow runing into this condition, the tracking padding size is
          // incorrect.
          // Delete padding file to indicate the padding size is incorrect for
          // avoiding error happening in the following lines.
          QM_TRY(MOZ_TO_RESULT(
              DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE)));

          QM_TRY_UNWRAP(currentPaddingSize, db::FindOverallPaddingSize(aConn));

          // XXXtt: we should have an easy way to update (increase or
          // recalulate) padding size in the QM. For now, only correct the
          // padding size in padding file and make QM be able to get the correct
          // size in the next QM initialization. We still want to catch this in
          // the debug build.
          MOZ_ASSERT(false, "The padding size is unsync with QM");
        }

#ifdef DEBUG
        const int64_t lastPaddingSize = currentPaddingSize;
        QM_TRY_UNWRAP(currentPaddingSize, db::FindOverallPaddingSize(aConn));

        MOZ_DIAGNOSTIC_ASSERT(currentPaddingSize == lastPaddingSize);
#endif  // DEBUG

        return currentPaddingSize;
      }()));

  MOZ_DIAGNOSTIC_ASSERT(currentPaddingSize >= 0);

  QM_TRY(MOZ_TO_RESULT(DirectoryPaddingWrite(aBaseDir, DirPaddingFile::TMP_FILE,
                                             currentPaddingSize)));

  return NS_OK;
}

nsresult DirectoryPaddingFinalizeWrite(nsIFile& aBaseDir) {
  MOZ_DIAGNOSTIC_ASSERT(
      DirectoryPaddingFileExists(aBaseDir, DirPaddingFile::TMP_FILE));

  QM_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, nsLiteralString(PADDING_TMP_FILE_NAME)));

  QM_TRY(MOZ_TO_RESULT(
      file->RenameTo(nullptr, nsLiteralString(PADDING_FILE_NAME))));

  return NS_OK;
}

Result<int64_t, nsresult> DirectoryPaddingRestore(nsIFile& aBaseDir,
                                                  mozIStorageConnection& aConn,
                                                  const bool aMustRestore) {
  // The content of padding file is untrusted, so remove it here.
  QM_TRY(MOZ_TO_RESULT(
      DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE)));

  QM_TRY_INSPECT(const int64_t& paddingSize, db::FindOverallPaddingSize(aConn));
  MOZ_DIAGNOSTIC_ASSERT(paddingSize >= 0);

  QM_TRY(MOZ_TO_RESULT(DirectoryPaddingWrite(aBaseDir, DirPaddingFile::FILE,
                                             paddingSize)),
         (aMustRestore ? Err(tryTempError)
                       : Result<int64_t, nsresult>{paddingSize}));

  QM_TRY(MOZ_TO_RESULT(
      DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::TMP_FILE)));

  return paddingSize;
}

nsresult DirectoryPaddingDeleteFile(nsIFile& aBaseDir,
                                    DirPaddingFile aPaddingFileType) {
  QM_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, aPaddingFileType == DirPaddingFile::TMP_FILE
                                       ? nsLiteralString(PADDING_TMP_FILE_NAME)
                                       : nsLiteralString(PADDING_FILE_NAME)));

  QM_TRY(QM_OR_ELSE_WARN_IF(
      // Expression.
      MOZ_TO_RESULT(file->Remove(/* recursive */ false)),
      // Predicate.
      IsFileNotFoundError,
      // Fallback.
      ErrToDefaultOk<>));

  return NS_OK;
}
}  // namespace mozilla::dom::cache
