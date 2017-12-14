/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/FileUtils.h"

#include "DBSchema.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/SnappyCompressOutputStream.h"
#include "mozilla/Unused.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIFile.h"
#include "nsIUUIDGenerator.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsISimpleEnumerator.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::quota::FileInputStream;
using mozilla::dom::quota::FileOutputStream;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::QuotaObject;

namespace {

// Const variable for generate padding size.
// XXX This will be tweaked to something more meaningful in Bug 1383656.
const int64_t kRoundUpNumber = 20480;

enum BodyFileType
{
  BODY_FILE_FINAL,
  BODY_FILE_TMP
};

nsresult
BodyIdToFile(nsIFile* aBaseDir, const nsID& aId, BodyFileType aType,
             nsIFile** aBodyFileOut);

int64_t
RoundUp(const int64_t aX, const int64_t aY);

// The alogrithm for generating padding refers to the mitigation approach in
// https://github.com/whatwg/storage/issues/31.
// First, generate a random number between 0 and 100kB.
// Next, round up the sum of random number and response size to the nearest
// 20kB.
// Finally, the virtual padding size will be the result minus the response size.
int64_t
BodyGeneratePadding(const int64_t aBodyFileSize, const uint32_t aPaddingInfo);

nsresult
LockedDirectoryPaddingWrite(nsIFile* aBaseDir, DirPaddingFile aPaddingFileType,
                            int64_t aPaddingSize);

} // namespace

// static
nsresult
BodyCreateDir(nsIFile* aBaseDir)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  nsCOMPtr<nsIFile> aBodyDir;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(aBodyDir));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aBodyDir->Append(NS_LITERAL_STRING("morgue"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aBodyDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
  if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
BodyDeleteDir(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  nsCOMPtr<nsIFile> aBodyDir;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(aBodyDir));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aBodyDir->Append(NS_LITERAL_STRING("morgue"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = RemoveNsIFileRecursively(aQuotaInfo, aBodyDir);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
BodyGetCacheDir(nsIFile* aBaseDir, const nsID& aId, nsIFile** aCacheDirOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aCacheDirOut);

  *aCacheDirOut = nullptr;

  nsresult rv = aBaseDir->Clone(aCacheDirOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_DIAGNOSTIC_ASSERT(*aCacheDirOut);

  rv = (*aCacheDirOut)->Append(NS_LITERAL_STRING("morgue"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Some file systems have poor performance when there are too many files
  // in a single directory.  Mitigate this issue by spreading the body
  // files out into sub-directories.  We use the last byte of the ID for
  // the name of the sub-directory.
  nsAutoString subDirName;
  subDirName.AppendInt(aId.m3[7]);
  rv = (*aCacheDirOut)->Append(subDirName);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = (*aCacheDirOut)->Create(nsIFile::DIRECTORY_TYPE, 0755);
  if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
BodyStartWriteStream(const QuotaInfo& aQuotaInfo,
                     nsIFile* aBaseDir, nsIInputStream* aSource,
                     void* aClosure,
                     nsAsyncCopyCallbackFun aCallback, nsID* aIdOut,
                     nsISupports** aCopyContextOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aSource);
  MOZ_DIAGNOSTIC_ASSERT(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(aCallback);
  MOZ_DIAGNOSTIC_ASSERT(aIdOut);
  MOZ_DIAGNOSTIC_ASSERT(aCopyContextOut);

  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> idGen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = idGen->GenerateUUIDInPlace(aIdOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIFile> finalFile;
  rv = BodyIdToFile(aBaseDir, *aIdOut, BODY_FILE_FINAL,
                    getter_AddRefs(finalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool exists;
  rv = finalFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  if (NS_WARN_IF(exists)) { return NS_ERROR_FILE_ALREADY_EXISTS; }

  nsCOMPtr<nsIFile> tmpFile;
  rv = BodyIdToFile(aBaseDir, *aIdOut, BODY_FILE_TMP, getter_AddRefs(tmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = tmpFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  if (NS_WARN_IF(exists)) { return NS_ERROR_FILE_ALREADY_EXISTS; }

  nsCOMPtr<nsIOutputStream> fileStream =
    FileOutputStream::Create(PERSISTENCE_TYPE_DEFAULT, aQuotaInfo.mGroup,
                             aQuotaInfo.mOrigin, tmpFile);
  if (NS_WARN_IF(!fileStream)) { return NS_ERROR_UNEXPECTED; }

  RefPtr<SnappyCompressOutputStream> compressed =
    new SnappyCompressOutputStream(fileStream);

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

  rv = NS_AsyncCopy(aSource, compressed, target, NS_ASYNCCOPY_VIA_WRITESEGMENTS,
                    compressed->BlockSize(), aCallback, aClosure,
                    true, true, // close streams
                    aCopyContextOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
void
BodyCancelWrite(nsIFile* aBaseDir, nsISupports* aCopyContext)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aCopyContext);

  nsresult rv = NS_CancelAsyncCopy(aCopyContext, NS_ERROR_ABORT);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  // The partially written file must be cleaned up after the async copy
  // makes its callback.
}

// static
nsresult
BodyFinalizeWrite(nsIFile* aBaseDir, const nsID& aId)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = BodyIdToFile(aBaseDir, aId, BODY_FILE_TMP, getter_AddRefs(tmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIFile> finalFile;
  rv = BodyIdToFile(aBaseDir, aId, BODY_FILE_FINAL, getter_AddRefs(finalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoString finalFileName;
  rv = finalFile->GetLeafName(finalFileName);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // It's fine to not notify the QuotaManager that the path has been changed,
  // because its path will be updated and its size will be recalculated when
  // opening file next time.
  rv = tmpFile->RenameTo(nullptr, finalFileName);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
BodyOpen(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir, const nsID& aId,
         nsIInputStream** aStreamOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aStreamOut);

  nsCOMPtr<nsIFile> finalFile;
  nsresult rv = BodyIdToFile(aBaseDir, aId, BODY_FILE_FINAL,
                             getter_AddRefs(finalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool exists;
  rv = finalFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  if (NS_WARN_IF(!exists)) { return NS_ERROR_FILE_NOT_FOUND; }

  nsCOMPtr<nsIInputStream> fileStream =
    FileInputStream::Create(PERSISTENCE_TYPE_DEFAULT, aQuotaInfo.mGroup,
                            aQuotaInfo.mOrigin, finalFile);
  if (NS_WARN_IF(!fileStream)) { return NS_ERROR_UNEXPECTED; }

  fileStream.forget(aStreamOut);

  return rv;
}

// static
nsresult
BodyMaybeUpdatePaddingSize(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir,
                           const nsID& aId, const uint32_t aPaddingInfo,
                           int64_t* aPaddingSizeOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSizeOut);

  nsCOMPtr<nsIFile> bodyFile;
  nsresult rv =
    BodyIdToFile(aBaseDir, aId, BODY_FILE_TMP, getter_AddRefs(bodyFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  MOZ_DIAGNOSTIC_ASSERT(bodyFile);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(quotaManager);

  int64_t fileSize = 0;
  RefPtr<QuotaObject> quotaObject =
    quotaManager->GetQuotaObject(PERSISTENCE_TYPE_DEFAULT, aQuotaInfo.mGroup,
                                 aQuotaInfo.mOrigin, bodyFile, &fileSize);
  MOZ_DIAGNOSTIC_ASSERT(quotaObject);
  MOZ_DIAGNOSTIC_ASSERT(fileSize >= 0);
  // XXXtt: bug: https://bugzilla.mozilla.org/show_bug.cgi?id=1422815
  if (!quotaObject) { return NS_ERROR_UNEXPECTED; }

  if (*aPaddingSizeOut == InternalResponse::UNKNOWN_PADDING_SIZE) {
    *aPaddingSizeOut = BodyGeneratePadding(fileSize, aPaddingInfo);
  }

  MOZ_DIAGNOSTIC_ASSERT(*aPaddingSizeOut >= 0);

  if (!quotaObject->IncreaseSize(*aPaddingSizeOut)) {
    return NS_ERROR_FILE_NO_DEVICE_SPACE;
  }

  return rv;
}

// static
nsresult
BodyDeleteFiles(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir,
                const nsTArray<nsID>& aIdList)
{
  nsresult rv = NS_OK;

  for (uint32_t i = 0; i < aIdList.Length(); ++i) {
    nsCOMPtr<nsIFile> tmpFile;
    rv = BodyIdToFile(aBaseDir, aIdList[i], BODY_FILE_TMP,
                      getter_AddRefs(tmpFile));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = RemoveNsIFile(aQuotaInfo, tmpFile);
    // Only treat file deletion as a hard failure in DEBUG builds.  Users
    // can unfortunately hit this on windows if anti-virus is scanning files,
    // etc.
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIFile> finalFile;
    rv = BodyIdToFile(aBaseDir, aIdList[i], BODY_FILE_FINAL,
                      getter_AddRefs(finalFile));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = RemoveNsIFile(aQuotaInfo, finalFile);
    // Again, only treat removal as hard failure in debug build.
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  return NS_OK;
}

namespace {

nsresult
BodyIdToFile(nsIFile* aBaseDir, const nsID& aId, BodyFileType aType,
             nsIFile** aBodyFileOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aBodyFileOut);

  *aBodyFileOut = nullptr;

  nsresult rv = BodyGetCacheDir(aBaseDir, aId, aBodyFileOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_DIAGNOSTIC_ASSERT(*aBodyFileOut);

  char idString[NSID_LENGTH];
  aId.ToProvidedString(idString);

  NS_ConvertASCIItoUTF16 fileName(idString);

  if (aType == BODY_FILE_FINAL) {
    fileName.AppendLiteral(".final");
  } else {
    fileName.AppendLiteral(".tmp");
  }

  rv = (*aBodyFileOut)->Append(fileName);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

int64_t
RoundUp(const int64_t aX, const int64_t aY)
{
  MOZ_DIAGNOSTIC_ASSERT(aX >= 0);
  MOZ_DIAGNOSTIC_ASSERT(aY > 0);

  MOZ_DIAGNOSTIC_ASSERT(INT64_MAX - ((aX - 1) / aY) * aY >= aY);
  return aY + ((aX - 1) / aY) * aY;
}

int64_t
BodyGeneratePadding(const int64_t aBodyFileSize, const uint32_t aPaddingInfo)
{
  // Generate padding
  int64_t randomSize = static_cast<int64_t>(aPaddingInfo);
  MOZ_DIAGNOSTIC_ASSERT(INT64_MAX - aBodyFileSize >= randomSize);
  randomSize += aBodyFileSize;

  return RoundUp(randomSize, kRoundUpNumber) - aBodyFileSize;
}

nsresult
LockedDirectoryPaddingWrite(nsIFile* aBaseDir, DirPaddingFile aPaddingFileType,
                            int64_t aPaddingSize)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSize >= 0);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (aPaddingFileType == DirPaddingFile::TMP_FILE) {
    rv = file->Append(NS_LITERAL_STRING(PADDING_TMP_FILE_NAME));
  } else {
    rv = file->Append(NS_LITERAL_STRING(PADDING_FILE_NAME));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIObjectOutputStream> objectStream =
    NS_NewObjectOutputStream(outputStream);

  rv = objectStream->Write64(aPaddingSize);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

} // namespace

nsresult
BodyDeleteOrphanedFiles(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir,
                        nsTArray<nsID>& aKnownBodyIdList)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  // body files are stored in a directory structure like:
  //
  //  /morgue/01/{01fdddb2-884d-4c3d-95ba-0c8062f6c325}.final
  //  /morgue/02/{02fdddb2-884d-4c3d-95ba-0c8062f6c325}.tmp

  nsCOMPtr<nsIFile> dir;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(dir));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Add the root morgue directory
  rv = dir->Append(NS_LITERAL_STRING("morgue"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = dir->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Iterate over all the intermediate morgue subdirs
  bool hasMore = false;
  while (NS_SUCCEEDED(rv = entries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    nsCOMPtr<nsIFile> subdir = do_QueryInterface(entry);

    bool isDir = false;
    rv = subdir->IsDirectory(&isDir);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    // If a file got in here somehow, try to remove it and move on
    if (NS_WARN_IF(!isDir)) {
      rv = RemoveNsIFile(aQuotaInfo, subdir);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      continue;
    }

    nsCOMPtr<nsISimpleEnumerator> subEntries;
    rv = subdir->GetDirectoryEntries(getter_AddRefs(subEntries));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    // Now iterate over all the files in the subdir
    bool subHasMore = false;
    while(NS_SUCCEEDED(rv = subEntries->HasMoreElements(&subHasMore)) &&
          subHasMore) {
      nsCOMPtr<nsISupports> subEntry;
      rv = subEntries->GetNext(getter_AddRefs(subEntry));
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      nsCOMPtr<nsIFile> file = do_QueryInterface(subEntry);

      nsAutoCString leafName;
      rv = file->GetNativeLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      // Delete all tmp files regardless of known bodies.  These are
      // all considered orphans.
      if (StringEndsWith(leafName, NS_LITERAL_CSTRING(".tmp"))) {
        // remove recursively in case its somehow a directory
        rv = RemoveNsIFileRecursively(aQuotaInfo, file);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        continue;
      }

      nsCString suffix(NS_LITERAL_CSTRING(".final"));

      // Otherwise, it must be a .final file.  If its not, then just
      // skip it.
      if (NS_WARN_IF(!StringEndsWith(leafName, suffix) ||
                     leafName.Length() != NSID_LENGTH - 1 + suffix.Length())) {
        continue;
      }

      // Finally, parse the uuid out of the name.  If its fails to parse,
      // the ignore the file.
      nsID id;
      if (NS_WARN_IF(!id.Parse(leafName.BeginReading()))) {
        continue;
      }

      if (!aKnownBodyIdList.Contains(id)) {
        // remove recursively in case its somehow a directory
        rv = RemoveNsIFileRecursively(aQuotaInfo, file);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
    }
  }

  return rv;
}

namespace {

nsresult
GetMarkerFileHandle(const QuotaInfo& aQuotaInfo, nsIFile** aFileOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aFileOut);

  nsCOMPtr<nsIFile> marker;
  nsresult rv = aQuotaInfo.mDir->Clone(getter_AddRefs(marker));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = marker->Append(NS_LITERAL_STRING("cache"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = marker->Append(NS_LITERAL_STRING("context_open.marker"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  marker.forget(aFileOut);

  return rv;
}

} // namespace

nsresult
CreateMarkerFile(const QuotaInfo& aQuotaInfo)
{
  nsCOMPtr<nsIFile> marker;
  nsresult rv = GetMarkerFileHandle(aQuotaInfo, getter_AddRefs(marker));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = marker->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    rv = NS_OK;
  }

  // Note, we don't need to fsync here.  We only care about actually
  // writing the marker if later modifications to the Cache are
  // actually flushed to the disk.  If the OS crashes before the marker
  // is written then we are ensured no other changes to the Cache were
  // flushed either.

  return rv;
}

nsresult
DeleteMarkerFile(const QuotaInfo& aQuotaInfo)
{
  nsCOMPtr<nsIFile> marker;
  nsresult rv = GetMarkerFileHandle(aQuotaInfo, getter_AddRefs(marker));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = RemoveNsIFile(aQuotaInfo, marker);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Again, no fsync is necessary.  If the OS crashes before the file
  // removal is flushed, then the Cache will search for stale data on
  // startup.  This will cause the next Cache access to be a bit slow, but
  // it seems appropriate after an OS crash.

  return NS_OK;
}

bool
MarkerFileExists(const QuotaInfo& aQuotaInfo)
{
  nsCOMPtr<nsIFile> marker;
  nsresult rv = GetMarkerFileHandle(aQuotaInfo, getter_AddRefs(marker));
  if (NS_WARN_IF(NS_FAILED(rv))) { return false; }

  bool exists = false;
  rv = marker->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) { return false; }

  return exists;
}

// static
nsresult
RemoveNsIFileRecursively(const QuotaInfo& aQuotaInfo, nsIFile* aFile)
{
  MOZ_DIAGNOSTIC_ASSERT(aFile);

  bool isDirectory = false;
  nsresult rv = aFile->IsDirectory(&isDirectory);
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (!isDirectory) {
    return RemoveNsIFile(aQuotaInfo, aFile);
  }

  // Unfortunately, we need to traverse all the entries and delete files one by
  // one to update their usages to the QuotaManager.
  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = aFile->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMore = false;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    MOZ_ASSERT(file);

    rv = RemoveNsIFileRecursively(aQuotaInfo, file);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // In the end, remove the folder
  rv = aFile->Remove(/* recursive */ false);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
RemoveNsIFile(const QuotaInfo& aQuotaInfo, nsIFile* aFile)
{
  MOZ_DIAGNOSTIC_ASSERT(aFile);

  int64_t fileSize = 0;
  nsresult rv = aFile->GetFileSize(&fileSize);
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aFile->Remove( /* recursive */ false);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (fileSize > 0) {
    DecreaseUsageForQuotaInfo(aQuotaInfo, fileSize);
  }

  return rv;
}

// static
void
DecreaseUsageForQuotaInfo(const QuotaInfo& aQuotaInfo,
                          const int64_t& aUpdatingSize)
{
  MOZ_DIAGNOSTIC_ASSERT(aUpdatingSize > 0);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(quotaManager);

  quotaManager->DecreaseUsageForOrigin(PERSISTENCE_TYPE_DEFAULT,
                                       aQuotaInfo.mGroup, aQuotaInfo.mOrigin,
                                       aUpdatingSize);
}

// static
bool
DirectoryPaddingFileExists(nsIFile* aBaseDir, DirPaddingFile aPaddingFileType)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) { return false; }

  nsString fileName;
  if (aPaddingFileType == DirPaddingFile::TMP_FILE) {
    fileName = NS_LITERAL_STRING(PADDING_TMP_FILE_NAME);
  } else {
    fileName = NS_LITERAL_STRING(PADDING_FILE_NAME);
  }

  rv = file->Append(fileName);
  if (NS_WARN_IF(NS_FAILED(rv))) { return false; }

  bool exists = false;
  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) { return false; }

  return exists;
}

// static
nsresult
LockedDirectoryPaddingGet(nsIFile* aBaseDir, int64_t* aPaddingSizeOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSizeOut);
  MOZ_DIAGNOSTIC_ASSERT(!DirectoryPaddingFileExists(aBaseDir,
                                                    DirPaddingFile::TMP_FILE));

  nsCOMPtr<nsIFile> file;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = file->Append(NS_LITERAL_STRING(PADDING_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIInputStream> bufferedStream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream.forget(),
                                 512);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIObjectInputStream> objectStream =
    NS_NewObjectInputStream(bufferedStream);

  uint64_t paddingSize = 0;
  rv = objectStream->Read64(&paddingSize);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  *aPaddingSizeOut = paddingSize;

  return rv;
}

// static
nsresult
LockedDirectoryPaddingInit(nsIFile* aBaseDir)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  nsresult rv = LockedDirectoryPaddingWrite(aBaseDir, DirPaddingFile::FILE, 0);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return rv;
}

// static
nsresult
LockedUpdateDirectoryPaddingFile(nsIFile* aBaseDir,
                                 mozIStorageConnection* aConn,
                                 const int64_t aIncreaseSize,
                                 const int64_t aDecreaseSize,
                                 const bool aTemporaryFileExist)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aConn);
  MOZ_DIAGNOSTIC_ASSERT(aIncreaseSize >= 0);
  MOZ_DIAGNOSTIC_ASSERT(aDecreaseSize >= 0);

  int64_t currentPaddingSize = 0;
  nsresult rv = NS_OK;
  if (aTemporaryFileExist ||
      NS_WARN_IF(NS_FAILED(rv =
        LockedDirectoryPaddingGet(aBaseDir, &currentPaddingSize)))) {
    // Fail to read padding size from the dir padding file, so try to restore.
    if (rv != NS_ERROR_FILE_NOT_FOUND &&
        rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
      // Not delete the temporary padding file here, because we're going to
      // overwrite it below anyway.
      rv = LockedDirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    }

    // We don't need to add the aIncreaseSize or aDecreaseSize here, because
    // it's already encompassed within the database.
    rv = db::FindOverallPaddingSize(aConn, &currentPaddingSize);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  } else {
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
      } else if(!shouldRevise) {
        currentPaddingSize -= aDecreaseSize;
      }
    }

    if (shouldRevise) {
      // If somehow runing into this condition, the tracking padding size is
      // incorrect.
      // Delete padding file to indicate the padding size is incorrect for
      // avoiding error happening in the following lines.
      rv = LockedDirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::FILE);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      int64_t paddingSizeFromDB = 0;
      rv = db::FindOverallPaddingSize(aConn, &paddingSizeFromDB);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
      currentPaddingSize = paddingSizeFromDB;

      // XXXtt: we should have an easy way to update (increase or recalulate)
      // padding size in the QM. For now, only correct the padding size in
      // padding file and make QM be able to get the correct size in the next QM
      // initialization.
      // We still want to catch this in the debug build.
      MOZ_ASSERT(false, "The padding size is unsync with QM");
    }

#ifdef DEBUG
    int64_t paddingSizeFromDB = 0;
    rv = db::FindOverallPaddingSize(aConn, &paddingSizeFromDB);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    MOZ_DIAGNOSTIC_ASSERT(paddingSizeFromDB == currentPaddingSize);
#endif // DEBUG
  }

  MOZ_DIAGNOSTIC_ASSERT(currentPaddingSize >= 0);

  rv = LockedDirectoryPaddingTemporaryWrite(aBaseDir, currentPaddingSize);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
LockedDirectoryPaddingTemporaryWrite(nsIFile* aBaseDir, int64_t aPaddingSize)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSize >= 0);

  nsresult rv = LockedDirectoryPaddingWrite(aBaseDir, DirPaddingFile::TMP_FILE,
                                            aPaddingSize);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
LockedDirectoryPaddingFinalizeWrite(nsIFile* aBaseDir)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(DirectoryPaddingFileExists(aBaseDir,
                                                   DirPaddingFile::TMP_FILE));

  nsCOMPtr<nsIFile> file;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = file->Append(NS_LITERAL_STRING(PADDING_TMP_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = file->RenameTo(nullptr, NS_LITERAL_STRING(PADDING_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
LockedDirectoryPaddingRestore(nsIFile* aBaseDir, mozIStorageConnection* aConn,
                              bool aMustRestore, int64_t* aPaddingSizeOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aConn);
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSizeOut);

  // The content of padding file is untrusted, so remove it here.
  nsresult rv = LockedDirectoryPaddingDeleteFile(aBaseDir,
                                                 DirPaddingFile::FILE);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int64_t paddingSize = 0;
  rv = db::FindOverallPaddingSize(aConn, &paddingSize);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  MOZ_DIAGNOSTIC_ASSERT(paddingSize >= 0);
  *aPaddingSizeOut = paddingSize;

  rv = LockedDirectoryPaddingWrite(aBaseDir, DirPaddingFile::FILE, paddingSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If we cannot write the correct padding size to file, just keep the
    // temporary file and let the padding size to be recalculate in the next
    // action
    return aMustRestore ? rv : NS_OK;
  }

  rv = LockedDirectoryPaddingDeleteFile(aBaseDir, DirPaddingFile::TMP_FILE);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return rv;
}

// static
nsresult
LockedDirectoryPaddingDeleteFile(nsIFile* aBaseDir,
                                 DirPaddingFile aPaddingFileType)
{
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (aPaddingFileType == DirPaddingFile::TMP_FILE) {
    rv = file->Append(NS_LITERAL_STRING(PADDING_TMP_FILE_NAME));
  } else {
    rv = file->Append(NS_LITERAL_STRING(PADDING_FILE_NAME));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = file->Remove( /* recursive */ false);
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}
} // namespace cache
} // namespace dom
} // namespace mozilla
