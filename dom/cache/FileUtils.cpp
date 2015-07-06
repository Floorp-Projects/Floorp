/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/FileUtils.h"

#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/SnappyCompressOutputStream.h"
#include "mozilla/unused.h"
#include "nsIFile.h"
#include "nsIUUIDGenerator.h"
#include "nsNetCID.h"
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

namespace {

enum BodyFileType
{
  BODY_FILE_FINAL,
  BODY_FILE_TMP
};

nsresult
BodyIdToFile(nsIFile* aBaseDir, const nsID& aId, BodyFileType aType,
             nsIFile** aBodyFileOut);

} // anonymous namespace

// static
nsresult
BodyCreateDir(nsIFile* aBaseDir)
{
  MOZ_ASSERT(aBaseDir);

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
BodyDeleteDir(nsIFile* aBaseDir)
{
  MOZ_ASSERT(aBaseDir);

  nsCOMPtr<nsIFile> aBodyDir;
  nsresult rv = aBaseDir->Clone(getter_AddRefs(aBodyDir));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aBodyDir->Append(NS_LITERAL_STRING("morgue"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aBodyDir->Remove(/* recursive = */ true);
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    rv = NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
BodyGetCacheDir(nsIFile* aBaseDir, const nsID& aId, nsIFile** aCacheDirOut)
{
  MOZ_ASSERT(aBaseDir);
  MOZ_ASSERT(aCacheDirOut);

  *aCacheDirOut = nullptr;

  nsresult rv = aBaseDir->Clone(aCacheDirOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_ASSERT(*aCacheDirOut);

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
  MOZ_ASSERT(aBaseDir);
  MOZ_ASSERT(aSource);
  MOZ_ASSERT(aClosure);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(aIdOut);
  MOZ_ASSERT(aCopyContextOut);

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

  nsRefPtr<SnappyCompressOutputStream> compressed =
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
  MOZ_ASSERT(aBaseDir);
  MOZ_ASSERT(aCopyContext);

  nsresult rv = NS_CancelAsyncCopy(aCopyContext, NS_ERROR_ABORT);
  unused << NS_WARN_IF(NS_FAILED(rv));

  // The partially written file must be cleaned up after the async copy
  // makes its callback.
}

// static
nsresult
BodyFinalizeWrite(nsIFile* aBaseDir, const nsID& aId)
{
  MOZ_ASSERT(aBaseDir);

  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = BodyIdToFile(aBaseDir, aId, BODY_FILE_TMP, getter_AddRefs(tmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIFile> finalFile;
  rv = BodyIdToFile(aBaseDir, aId, BODY_FILE_FINAL, getter_AddRefs(finalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoString finalFileName;
  rv = finalFile->GetLeafName(finalFileName);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = tmpFile->RenameTo(nullptr, finalFileName);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
BodyOpen(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir, const nsID& aId,
         nsIInputStream** aStreamOut)
{
  MOZ_ASSERT(aBaseDir);
  MOZ_ASSERT(aStreamOut);

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
BodyDeleteFiles(nsIFile* aBaseDir, const nsTArray<nsID>& aIdList)
{
  nsresult rv = NS_OK;

  for (uint32_t i = 0; i < aIdList.Length(); ++i) {
    nsCOMPtr<nsIFile> tmpFile;
    rv = BodyIdToFile(aBaseDir, aIdList[i], BODY_FILE_TMP,
                      getter_AddRefs(tmpFile));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = tmpFile->Remove(false /* recursive */);
    if (rv == NS_ERROR_FILE_NOT_FOUND ||
        rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
      rv = NS_OK;
    }

    // Only treat file deletion as a hard failure in DEBUG builds.  Users
    // can unfortunately hit this on windows if anti-virus is scanning files,
    // etc.
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIFile> finalFile;
    rv = BodyIdToFile(aBaseDir, aIdList[i], BODY_FILE_FINAL,
                      getter_AddRefs(finalFile));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = finalFile->Remove(false /* recursive */);
    if (rv == NS_ERROR_FILE_NOT_FOUND ||
        rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
      rv = NS_OK;
    }

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
  MOZ_ASSERT(aBaseDir);
  MOZ_ASSERT(aBodyFileOut);

  *aBodyFileOut = nullptr;

  nsresult rv = BodyGetCacheDir(aBaseDir, aId, aBodyFileOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_ASSERT(*aBodyFileOut);

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

} // anonymous namespace

nsresult
BodyDeleteOrphanedFiles(nsIFile* aBaseDir, nsTArray<nsID>& aKnownBodyIdList)
{
  MOZ_ASSERT(aBaseDir);

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
      rv = subdir->Remove(false /* recursive */);
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
        rv = file->Remove(true /* recursive */);
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
        rv = file->Remove(true /* recursive */);
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
  MOZ_ASSERT(aFileOut);

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

} // anonymous namespace

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

  rv = marker->Remove(/* recursive = */ false);
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    rv = NS_OK;
  }

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

} // namespace cache
} // namespace dom
} // namespace mozilla
