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

template <typename SuccessType = Ok>
Result<SuccessType, nsresult> MapNotFoundToDefault(const nsresult aRv) {
  return (aRv == NS_ERROR_FILE_NOT_FOUND ||
          aRv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
             ? Result<SuccessType, nsresult>{std::in_place}
             : Err(aRv);
}

Result<Ok, nsresult> MapAlreadyExistsToDefault(const nsresult aRv) {
  return (aRv == NS_ERROR_FILE_ALREADY_EXISTS)
             ? Result<Ok, nsresult>{std::in_place}
             : Err(aRv);
}

Result<NotNull<nsCOMPtr<nsIFile>>, nsresult> BodyGetCacheDir(nsIFile& aBaseDir,
                                                             const nsID& aId) {
  CACHE_TRY_UNWRAP(auto cacheDir,
                   CloneFileAndAppend(aBaseDir, kMorgueDirectory));

  // Some file systems have poor performance when there are too many files
  // in a single directory.  Mitigate this issue by spreading the body
  // files out into sub-directories.  We use the last byte of the ID for
  // the name of the sub-directory.
  CACHE_TRY(cacheDir->Append(IntToString(aId.m3[7])));

  // Callers call this function without checking if the directory already
  // exists (idempotent usage). QM_OR_ELSE_WARN is not used here since we want
  // to ignore NS_ERROR_FILE_ALREADY_EXISTS completely.
  QM_TRY(ToResult(cacheDir->Create(nsIFile::DIRECTORY_TYPE, 0755))
             .orElse(ErrToDefaultOkOrErr<NS_ERROR_FILE_ALREADY_EXISTS>));

  return WrapNotNullUnchecked(std::move(cacheDir));
}

}  // namespace

nsresult BodyCreateDir(nsIFile& aBaseDir) {
  CACHE_TRY_INSPECT(const auto& bodyDir,
                    CloneFileAndAppend(aBaseDir, kMorgueDirectory));

  // Callers call this function without checking if the directory already
  // exists (idempotent usage). QM_OR_ELSE_WARN is not used here since we want
  // to ignore NS_ERROR_FILE_ALREADY_EXISTS completely.
  QM_TRY(ToResult(bodyDir->Create(nsIFile::DIRECTORY_TYPE, 0755))
             .orElse(ErrToDefaultOkOrErr<NS_ERROR_FILE_ALREADY_EXISTS>));

  return NS_OK;
}

nsresult BodyDeleteDir(const QuotaInfo& aQuotaInfo, nsIFile& aBaseDir) {
  CACHE_TRY_INSPECT(const auto& bodyDir,
                    CloneFileAndAppend(aBaseDir, kMorgueDirectory));

  CACHE_TRY(RemoveNsIFileRecursively(aQuotaInfo, *bodyDir));

  return NS_OK;
}

Result<std::pair<nsID, nsCOMPtr<nsISupports>>, nsresult> BodyStartWriteStream(
    const QuotaInfo& aQuotaInfo, nsIFile& aBaseDir, nsIInputStream& aSource,
    void* aClosure, nsAsyncCopyCallbackFun aCallback) {
  MOZ_DIAGNOSTIC_ASSERT(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(aCallback);

  CACHE_TRY_INSPECT(const auto& idGen, ToResultGet<nsCOMPtr<nsIUUIDGenerator>>(
                                           MOZ_SELECT_OVERLOAD(do_GetService),
                                           "@mozilla.org/uuid-generator;1"));

  nsID id;
  CACHE_TRY(idGen->GenerateUUIDInPlace(&id));

  CACHE_TRY_INSPECT(const auto& finalFile,
                    BodyIdToFile(aBaseDir, id, BODY_FILE_FINAL));

  {
    CACHE_TRY_INSPECT(const bool& exists,
                      MOZ_TO_RESULT_INVOKE(*finalFile, Exists));

    CACHE_TRY(OkIf(!exists), Err(NS_ERROR_FILE_ALREADY_EXISTS));
  }

  CACHE_TRY_INSPECT(const auto& tmpFile,
                    BodyIdToFile(aBaseDir, id, BODY_FILE_TMP));

  CACHE_TRY_INSPECT(const auto& fileStream,
                    CreateFileOutputStream(PERSISTENCE_TYPE_DEFAULT, aQuotaInfo,
                                           Client::DOMCACHE, tmpFile.get()));

  const auto compressed =
      MakeRefPtr<SnappyCompressOutputStream>(fileStream.get());

  const nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

  nsCOMPtr<nsISupports> copyContext;
  CACHE_TRY(NS_AsyncCopy(&aSource, compressed, target,
                         NS_ASYNCCOPY_VIA_WRITESEGMENTS,
                         compressed->BlockSize(), aCallback, aClosure, true,
                         true,  // close streams
                         getter_AddRefs(copyContext)));

  return std::make_pair(id, std::move(copyContext));
}

void BodyCancelWrite(nsISupports& aCopyContext) {
  QM_WARNONLY_TRY(NS_CancelAsyncCopy(&aCopyContext, NS_ERROR_ABORT));

  // TODO The partially written file must be cleaned up after the async copy
  // makes its callback.
}

nsresult BodyFinalizeWrite(nsIFile& aBaseDir, const nsID& aId) {
  CACHE_TRY_INSPECT(const auto& tmpFile,
                    BodyIdToFile(aBaseDir, aId, BODY_FILE_TMP));

  CACHE_TRY_INSPECT(const auto& finalFile,
                    BodyIdToFile(aBaseDir, aId, BODY_FILE_FINAL));

  nsAutoString finalFileName;
  CACHE_TRY(finalFile->GetLeafName(finalFileName));

  // It's fine to not notify the QuotaManager that the path has been changed,
  // because its path will be updated and its size will be recalculated when
  // opening file next time.
  CACHE_TRY(tmpFile->RenameTo(nullptr, finalFileName));

  return NS_OK;
}

Result<NotNull<nsCOMPtr<nsIInputStream>>, nsresult> BodyOpen(
    const QuotaInfo& aQuotaInfo, nsIFile& aBaseDir, const nsID& aId) {
  CACHE_TRY_INSPECT(const auto& finalFile,
                    BodyIdToFile(aBaseDir, aId, BODY_FILE_FINAL));

  CACHE_TRY_RETURN(CreateFileInputStream(PERSISTENCE_TYPE_DEFAULT, aQuotaInfo,
                                         Client::DOMCACHE, finalFile.get())
                       .map([](NotNull<RefPtr<FileInputStream>>&& stream) {
                         return WrapNotNullUnchecked(
                             nsCOMPtr<nsIInputStream>{stream.get()});
                       }));
}

nsresult BodyMaybeUpdatePaddingSize(const QuotaInfo& aQuotaInfo,
                                    nsIFile& aBaseDir, const nsID& aId,
                                    const uint32_t aPaddingInfo,
                                    int64_t* aPaddingSizeInOut) {
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSizeInOut);

  CACHE_TRY_INSPECT(const auto& bodyFile,
                    BodyIdToFile(aBaseDir, aId, BODY_FILE_TMP));

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(quotaManager);

  int64_t fileSize = 0;
  RefPtr<QuotaObject> quotaObject = quotaManager->GetQuotaObject(
      PERSISTENCE_TYPE_DEFAULT, aQuotaInfo, Client::DOMCACHE, bodyFile.get(),
      -1, &fileSize);
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

nsresult BodyDeleteFiles(const QuotaInfo& aQuotaInfo, nsIFile& aBaseDir,
                         const nsTArray<nsID>& aIdList) {
  for (const auto id : aIdList) {
    CACHE_TRY_INSPECT(const auto& bodyDir, BodyGetCacheDir(aBaseDir, id));

    const auto removeFileForId =
        [&aQuotaInfo, &id](
            nsIFile& bodyFile,
            const nsACString& leafName) -> Result<bool, nsresult> {
      nsID fileId;
      CACHE_TRY(OkIf(fileId.Parse(leafName.BeginReading())), true,
                ([&aQuotaInfo, &bodyFile](const auto) {
                  DebugOnly<nsresult> result = RemoveNsIFile(
                      aQuotaInfo, bodyFile, /* aTrackQuota */ false);
                  MOZ_ASSERT(NS_SUCCEEDED(result));
                }));

      if (id.Equals(fileId)) {
        DebugOnly<nsresult> result = RemoveNsIFile(aQuotaInfo, bodyFile);
        MOZ_ASSERT(NS_SUCCEEDED(result));
        return true;
      }

      return false;
    };
    CACHE_TRY(BodyTraverseFiles(aQuotaInfo, *bodyDir, removeFileForId,
                                /* aCanRemoveFiles */ false,
                                /* aTrackQuota */ true));
  }

  return NS_OK;
}

namespace {

Result<NotNull<nsCOMPtr<nsIFile>>, nsresult> BodyIdToFile(
    nsIFile& aBaseDir, const nsID& aId, const BodyFileType aType) {
  CACHE_TRY_UNWRAP(auto bodyFile, BodyGetCacheDir(aBaseDir, aId));

  char idString[NSID_LENGTH];
  aId.ToProvidedString(idString);

  NS_ConvertASCIItoUTF16 fileName(idString);

  if (aType == BODY_FILE_FINAL) {
    fileName.AppendLiteral(".final");
  } else {
    fileName.AppendLiteral(".tmp");
  }

  CACHE_TRY(bodyFile->Append(fileName));

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

  CACHE_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, aPaddingFileType == DirPaddingFile::TMP_FILE
                                       ? nsLiteralString(PADDING_TMP_FILE_NAME)
                                       : nsLiteralString(PADDING_FILE_NAME)));

  CACHE_TRY_INSPECT(const auto& outputStream,
                    NS_NewLocalFileOutputStream(file));

  nsCOMPtr<nsIObjectOutputStream> objectStream =
      NS_NewObjectOutputStream(outputStream);

  CACHE_TRY(objectStream->Write64(aPaddingSize));

  return NS_OK;
}

}  // namespace

nsresult BodyDeleteOrphanedFiles(const QuotaInfo& aQuotaInfo, nsIFile& aBaseDir,
                                 const nsTArray<nsID>& aKnownBodyIdList) {
  // body files are stored in a directory structure like:
  //
  //  /morgue/01/{01fdddb2-884d-4c3d-95ba-0c8062f6c325}.final
  //  /morgue/02/{02fdddb2-884d-4c3d-95ba-0c8062f6c325}.tmp

  CACHE_TRY_INSPECT(const auto& dir,
                    CloneFileAndAppend(aBaseDir, kMorgueDirectory));

  // Iterate over all the intermediate morgue subdirs
  CACHE_TRY(quota::CollectEachFile(
      *dir,
      [&aQuotaInfo, &aKnownBodyIdList](
          const nsCOMPtr<nsIFile>& subdir) -> Result<Ok, nsresult> {
        CACHE_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*subdir));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory: {
            const auto removeOrphanedFiles =
                [&aQuotaInfo, &aKnownBodyIdList](
                    nsIFile& bodyFile,
                    const nsACString& leafName) -> Result<bool, nsresult> {
              // Finally, parse the uuid out of the name.  If it fails to parse,
              // then ignore the file.
              auto cleanup = MakeScopeExit([&aQuotaInfo, &bodyFile] {
                DebugOnly<nsresult> result =
                    RemoveNsIFile(aQuotaInfo, bodyFile);
                MOZ_ASSERT(NS_SUCCEEDED(result));
              });

              nsID id;
              CACHE_TRY(OkIf(id.Parse(leafName.BeginReading())), true);

              if (!aKnownBodyIdList.Contains(id)) {
                return true;
              }

              cleanup.release();

              return false;
            };

            // QM_OR_ELSE_WARN is not used here since we want ignore
            // NS_ERROR_FILE_FS_CORRUPTED completely (even a warning is not
            // desired).
            CACHE_TRY(
                ToResult(BodyTraverseFiles(aQuotaInfo, *subdir,
                                           removeOrphanedFiles,
                                           /* aCanRemoveFiles */ true,
                                           /* aTrackQuota */ true))
                    .orElse([](const nsresult rv) -> Result<Ok, nsresult> {
                      // We treat NS_ERROR_FILE_FS_CORRUPTED as if the
                      // directory did not exist at all.
                      if (rv == NS_ERROR_FILE_FS_CORRUPTED) {
                        return Ok{};
                      }

                      return Err(rv);
                    }));
            break;
          }

          case nsIFileKind::ExistsAsFile: {
            // If a file got in here somehow, try to remove it and move on
            DebugOnly<nsresult> result =
                RemoveNsIFile(aQuotaInfo, *subdir, /* aTrackQuota */ false);
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
    const QuotaInfo& aQuotaInfo) {
  CACHE_TRY_UNWRAP(auto marker,
                   CloneFileAndAppend(*aQuotaInfo.mDir, u"cache"_ns));

  CACHE_TRY(marker->Append(u"context_open.marker"_ns));

  return marker;
}

}  // namespace

nsresult CreateMarkerFile(const QuotaInfo& aQuotaInfo) {
  CACHE_TRY_INSPECT(const auto& marker, GetMarkerFileHandle(aQuotaInfo));

  QM_TRY(
      QM_OR_ELSE_WARN(ToResult(marker->Create(nsIFile::NORMAL_FILE_TYPE, 0644)),
                      MapAlreadyExistsToDefault));

  // Note, we don't need to fsync here.  We only care about actually
  // writing the marker if later modifications to the Cache are
  // actually flushed to the disk.  If the OS crashes before the marker
  // is written then we are ensured no other changes to the Cache were
  // flushed either.

  return NS_OK;
}

nsresult DeleteMarkerFile(const QuotaInfo& aQuotaInfo) {
  CACHE_TRY_INSPECT(const auto& marker, GetMarkerFileHandle(aQuotaInfo));

  DebugOnly<nsresult> result =
      RemoveNsIFile(aQuotaInfo, *marker, /* aTrackQuota */ false);
  MOZ_ASSERT(NS_SUCCEEDED(result));

  // Again, no fsync is necessary.  If the OS crashes before the file
  // removal is flushed, then the Cache will search for stale data on
  // startup.  This will cause the next Cache access to be a bit slow, but
  // it seems appropriate after an OS crash.

  return NS_OK;
}

bool MarkerFileExists(const QuotaInfo& aQuotaInfo) {
  CACHE_TRY_INSPECT(const auto& marker, GetMarkerFileHandle(aQuotaInfo), false);

  CACHE_TRY_RETURN(MOZ_TO_RESULT_INVOKE(marker, Exists), false);
}

nsresult RemoveNsIFileRecursively(const QuotaInfo& aQuotaInfo, nsIFile& aFile,
                                  const bool aTrackQuota) {
  CACHE_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(aFile));

  switch (dirEntryKind) {
    case nsIFileKind::ExistsAsDirectory:
      // Unfortunately, we need to traverse all the entries and delete files one
      // by
      // one to update their usages to the QuotaManager.
      CACHE_TRY(quota::CollectEachFile(
          aFile,
          [&aQuotaInfo, &aTrackQuota](
              const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
            CACHE_TRY(RemoveNsIFileRecursively(aQuotaInfo, *file, aTrackQuota));

            return Ok{};
          }));

      // In the end, remove the folder
      CACHE_TRY(aFile.Remove(/* recursive */ false));

      break;

    case nsIFileKind::ExistsAsFile:
      return RemoveNsIFile(aQuotaInfo, aFile, aTrackQuota);

    case nsIFileKind::DoesNotExist:
      // Ignore files that got removed externally while iterating.
      break;
  }

  return NS_OK;
}

nsresult RemoveNsIFile(const QuotaInfo& aQuotaInfo, nsIFile& aFile,
                       const bool aTrackQuota) {
  int64_t fileSize = 0;
  if (aTrackQuota) {
    CACHE_TRY_INSPECT(
        const auto& maybeFileSize,
        QM_OR_ELSE_WARN(
            MOZ_TO_RESULT_INVOKE(aFile, GetFileSize).map(Some<int64_t>),
            MapNotFoundToDefault<Maybe<int64_t>>));

    if (!maybeFileSize) {
      return NS_OK;
    }

    fileSize = *maybeFileSize;
  }

  QM_TRY(QM_OR_ELSE_WARN(ToResult(aFile.Remove(/* recursive */ false)),
                         MapNotFoundToDefault<>));

  if (fileSize > 0) {
    MOZ_ASSERT(aTrackQuota);
    DecreaseUsageForQuotaInfo(aQuotaInfo, fileSize);
  }

  return NS_OK;
}

void DecreaseUsageForQuotaInfo(const QuotaInfo& aQuotaInfo,
                               const int64_t aUpdatingSize) {
  MOZ_DIAGNOSTIC_ASSERT(aUpdatingSize > 0);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(quotaManager);

  quotaManager->DecreaseUsageForClient(
      quota::ClientMetadata{aQuotaInfo, Client::DOMCACHE}, aUpdatingSize);
}

bool DirectoryPaddingFileExists(nsIFile& aBaseDir,
                                DirPaddingFile aPaddingFileType) {
  CACHE_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, aPaddingFileType == DirPaddingFile::TMP_FILE
                                       ? nsLiteralString(PADDING_TMP_FILE_NAME)
                                       : nsLiteralString(PADDING_FILE_NAME)),
      false);

  CACHE_TRY_RETURN(MOZ_TO_RESULT_INVOKE(file, Exists), false);
}

Result<int64_t, nsresult> DirectoryPaddingGet(nsIFile& aBaseDir) {
  MOZ_DIAGNOSTIC_ASSERT(
      !DirectoryPaddingFileExists(aBaseDir, DirPaddingFile::TMP_FILE));

  CACHE_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, nsLiteralString(PADDING_FILE_NAME)));

  CACHE_TRY_UNWRAP(auto stream, NS_NewLocalFileInputStream(file));

  CACHE_TRY_INSPECT(const auto& bufferedStream,
                    NS_NewBufferedInputStream(stream.forget(), 512));

  const nsCOMPtr<nsIObjectInputStream> objectStream =
      NS_NewObjectInputStream(bufferedStream);

  CACHE_TRY_RETURN(
      MOZ_TO_RESULT_INVOKE(objectStream, Read64).map([](const uint64_t val) {
        return int64_t(val);
      }));
}

nsresult DirectoryPaddingInit(nsIFile& aBaseDir) {
  CACHE_TRY(DirectoryPaddingWrite(aBaseDir, DirPaddingFile::FILE, 0));

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
        CACHE_TRY_RETURN(
            QM_OR_ELSE_WARN(DirectoryPaddingGet(aBaseDir).map(Some<int64_t>),
                            MapNotFoundToDefault<Maybe<int64_t>>),
            Maybe<int64_t>{});
      }();

  CACHE_TRY_INSPECT(
      const int64_t& currentPaddingSize,
      ([directoryPaddingGetResult, &aBaseDir, &aConn, aIncreaseSize,
        aDecreaseSize]() -> Result<int64_t, nsresult> {
        if (!directoryPaddingGetResult) {
          // Fail to read padding size from the dir padding file, so try to
          // restore.

          // Not delete the temporary padding file here, because we're going
          // to overwrite it below anyway.
          CACHE_TRY(DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE));

          // We don't need to add the aIncreaseSize or aDecreaseSize here,
          // because it's already encompassed within the database.
          CACHE_TRY_RETURN(db::FindOverallPaddingSize(aConn));
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
          CACHE_TRY(DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE));

          CACHE_TRY_UNWRAP(currentPaddingSize,
                           db::FindOverallPaddingSize(aConn));

          // XXXtt: we should have an easy way to update (increase or
          // recalulate) padding size in the QM. For now, only correct the
          // padding size in padding file and make QM be able to get the correct
          // size in the next QM initialization. We still want to catch this in
          // the debug build.
          MOZ_ASSERT(false, "The padding size is unsync with QM");
        }

#ifdef DEBUG
        const int64_t lastPaddingSize = currentPaddingSize;
        CACHE_TRY_UNWRAP(currentPaddingSize, db::FindOverallPaddingSize(aConn));

        MOZ_DIAGNOSTIC_ASSERT(currentPaddingSize == lastPaddingSize);
#endif  // DEBUG

        return currentPaddingSize;
      }()));

  MOZ_DIAGNOSTIC_ASSERT(currentPaddingSize >= 0);

  CACHE_TRY(DirectoryPaddingWrite(aBaseDir, DirPaddingFile::TMP_FILE,
                                  currentPaddingSize));

  return NS_OK;
}

nsresult DirectoryPaddingFinalizeWrite(nsIFile& aBaseDir) {
  MOZ_DIAGNOSTIC_ASSERT(
      DirectoryPaddingFileExists(aBaseDir, DirPaddingFile::TMP_FILE));

  CACHE_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, nsLiteralString(PADDING_TMP_FILE_NAME)));

  CACHE_TRY(file->RenameTo(nullptr, nsLiteralString(PADDING_FILE_NAME)));

  return NS_OK;
}

Result<int64_t, nsresult> DirectoryPaddingRestore(nsIFile& aBaseDir,
                                                  mozIStorageConnection& aConn,
                                                  const bool aMustRestore) {
  // The content of padding file is untrusted, so remove it here.
  CACHE_TRY(DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE));

  CACHE_TRY_INSPECT(const int64_t& paddingSize,
                    db::FindOverallPaddingSize(aConn));
  MOZ_DIAGNOSTIC_ASSERT(paddingSize >= 0);

  CACHE_TRY(DirectoryPaddingWrite(aBaseDir, DirPaddingFile::FILE, paddingSize),
            (aMustRestore ? Err(tryTempError)
                          : Result<int64_t, nsresult>{paddingSize}));

  CACHE_TRY(DirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::TMP_FILE));

  return paddingSize;
}

nsresult DirectoryPaddingDeleteFile(nsIFile& aBaseDir,
                                    DirPaddingFile aPaddingFileType) {
  CACHE_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aBaseDir, aPaddingFileType == DirPaddingFile::TMP_FILE
                                       ? nsLiteralString(PADDING_TMP_FILE_NAME)
                                       : nsLiteralString(PADDING_FILE_NAME)));

  QM_TRY(QM_OR_ELSE_WARN(ToResult(file->Remove(/* recursive */ false)),
                         MapNotFoundToDefault<>));

  return NS_OK;
}
}  // namespace mozilla::dom::cache
