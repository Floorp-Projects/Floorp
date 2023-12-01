/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_FileUtils_h
#define mozilla_dom_cache_FileUtils_h

#include "CacheCommon.h"
#include "CacheCipherKeyManager.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/cache/Types.h"
#include "mozIStorageConnection.h"
#include "nsStreamUtils.h"
#include "nsTArrayForwardDeclare.h"

struct nsID;
class nsIFile;

namespace mozilla::dom::cache {

#define PADDING_FILE_NAME u".padding"
#define PADDING_TMP_FILE_NAME u".padding-tmp"

enum class DirPaddingFile { FILE, TMP_FILE };

nsresult BodyCreateDir(nsIFile& aBaseDir);

// Note that this function can only be used during the initialization of the
// database.  We're unlikely to be able to delete the DB successfully past
// that point due to the file being in use.
nsresult BodyDeleteDir(const CacheDirectoryMetadata& aDirectoryMetadata,
                       nsIFile& aBaseDir);

// Returns a Result with a success value with the body id and, optionally, the
// copy context.
Result<nsCOMPtr<nsISupports>, nsresult> BodyStartWriteStream(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aBaseDir,
    const nsID& aBodyId, Maybe<CipherKey> aMaybeCipherKey,
    nsIInputStream& aSource, void* aClosure, nsAsyncCopyCallbackFun aCallback);

void BodyCancelWrite(nsISupports& aCopyContext);

nsresult BodyFinalizeWrite(nsIFile& aBaseDir, const nsID& aId);

Result<MovingNotNull<nsCOMPtr<nsIInputStream>>, nsresult> BodyOpen(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aBaseDir,
    const nsID& aId, Maybe<CipherKey> aMaybeCipherKey);

nsresult BodyMaybeUpdatePaddingSize(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aBaseDir,
    const nsID& aId, uint32_t aPaddingInfo, int64_t* aPaddingSizeInOut);

nsresult BodyDeleteFiles(const CacheDirectoryMetadata& aDirectoryMetadata,
                         nsIFile& aBaseDir, const nsTArray<nsID>& aIdList);

nsresult BodyDeleteOrphanedFiles(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aBaseDir,
    const nsTArray<nsID>& aKnownBodyIdList);

// If aCanRemoveFiles is true, that means we are safe to touch the files which
// can be accessed in other threads.
// If it's not, that means we cannot remove the files which are possible to
// created by other threads. Note that if the files are not expected, we should
// be safe to remove them in any case.
template <typename Func>
nsresult BodyTraverseFiles(
    const Maybe<CacheDirectoryMetadata>& aDirectoryMetadata, nsIFile& aBodyDir,
    const Func& aHandleFileFunc, bool aCanRemoveFiles, bool aTrackQuota = true);

// XXX Remove this method when all callers properly wrap aClientMetadata with
// Some/Nothing
template <typename Func>
nsresult BodyTraverseFiles(const CacheDirectoryMetadata& aDirectoryMetadata,
                           nsIFile& aBodyDir, const Func& aHandleFileFunc,
                           bool aCanRemoveFiles, bool aTrackQuota = true) {
  return BodyTraverseFiles(Some(aDirectoryMetadata), aBodyDir, aHandleFileFunc,
                           aCanRemoveFiles, aTrackQuota);
}

nsresult CreateMarkerFile(const CacheDirectoryMetadata& aDirectoryMetadata);

nsresult DeleteMarkerFile(const CacheDirectoryMetadata& aDirectoryMetadata);

bool MarkerFileExists(const CacheDirectoryMetadata& aDirectoryMetadata);

nsresult RemoveNsIFileRecursively(
    const Maybe<CacheDirectoryMetadata>& aDirectoryMetadata, nsIFile& aFile,
    bool aTrackQuota = true);

// XXX Remove this method when all callers properly wrap aClientMetadata with
// Some/Nothing
inline nsresult RemoveNsIFileRecursively(
    const CacheDirectoryMetadata& aDirectoryMetadata, nsIFile& aFile,
    bool aTrackQuota = true) {
  return RemoveNsIFileRecursively(Some(aDirectoryMetadata), aFile, aTrackQuota);
}

// Delete a file that you think exists. If the file doesn't exist, an error
// will not be returned, but warning telemetry will be generated! So only call
// this on files that you know exist (idempotent usage, but it's not
// recommended).
nsresult RemoveNsIFile(const Maybe<CacheDirectoryMetadata>& aDirectoryMetadata,
                       nsIFile& aFile, bool aTrackQuota = true);

// XXX Remove this method when all callers properly wrap aClientMetadata with
// Some/Nothing
inline nsresult RemoveNsIFile(const CacheDirectoryMetadata& aDirectoryMetadata,
                              nsIFile& aFile, bool aTrackQuota = true) {
  return RemoveNsIFile(Some(aDirectoryMetadata), aFile, aTrackQuota);
}

void DecreaseUsageForDirectoryMetadata(
    const CacheDirectoryMetadata& aDirectoryMetadata, int64_t aUpdatingSize);

/**
 * This function is used to check if the directory padding file is existed.
 */
bool DirectoryPaddingFileExists(nsIFile& aBaseDir,
                                DirPaddingFile aPaddingFileType);

/**
 *
 * The functions below are used to read/write/delete the directory padding file
 * after acquiring the mutex lock. The mutex lock is held by
 * CacheQuotaClient to prevent multi-thread accessing issue. To avoid deadlock,
 * these functions should only access by static functions in
 * dom/cache/QuotaClient.cpp.
 *
 */

// Returns a Result with a success value denoting the padding size.
Result<int64_t, nsresult> DirectoryPaddingGet(nsIFile& aBaseDir);

nsresult DirectoryPaddingInit(nsIFile& aBaseDir);

nsresult UpdateDirectoryPaddingFile(nsIFile& aBaseDir,
                                    mozIStorageConnection& aConn,
                                    int64_t aIncreaseSize,
                                    int64_t aDecreaseSize,
                                    bool aTemporaryFileExist);

nsresult DirectoryPaddingFinalizeWrite(nsIFile& aBaseDir);

// Returns a Result with a success value denoting the padding size.
Result<int64_t, nsresult> DirectoryPaddingRestore(nsIFile& aBaseDir,
                                                  mozIStorageConnection& aConn,
                                                  bool aMustRestore);

nsresult DirectoryPaddingDeleteFile(nsIFile& aBaseDir,
                                    DirPaddingFile aPaddingFileType);
}  // namespace mozilla::dom::cache

#endif  // mozilla_dom_cache_FileUtils_h
