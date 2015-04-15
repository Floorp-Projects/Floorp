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
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::quota::FileInputStream;
using mozilla::dom::quota::FileOutputStream;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::unused;

// static
nsresult
FileUtils::BodyCreateDir(nsIFile* aBaseDir)
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
FileUtils::BodyDeleteDir(nsIFile* aBaseDir)
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
FileUtils::BodyGetCacheDir(nsIFile* aBaseDir, const nsID& aId,
                           nsIFile** aCacheDirOut)
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
FileUtils::BodyStartWriteStream(const QuotaInfo& aQuotaInfo,
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
FileUtils::BodyCancelWrite(nsIFile* aBaseDir, nsISupports* aCopyContext)
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
FileUtils::BodyFinalizeWrite(nsIFile* aBaseDir, const nsID& aId)
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
FileUtils::BodyOpen(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir,
                    const nsID& aId, nsIInputStream** aStreamOut)
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
FileUtils::BodyDeleteFiles(nsIFile* aBaseDir, const nsTArray<nsID>& aIdList)
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

// static
nsresult
FileUtils::BodyIdToFile(nsIFile* aBaseDir, const nsID& aId,
                        BodyFileType aType, nsIFile** aBodyFileOut)
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

} // namespace cache
} // namespace dom
} // namespace mozilla
