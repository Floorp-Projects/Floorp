/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPStorageParent.h"
#include "mozilla/SyncRunnable.h"
#include "plhash.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "GMPParent.h"
#include "gmp-storage.h"
#include "mozilla/unused.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetGMPLog();

#define LOGD(msg) PR_LOG(GetGMPLog(), PR_LOG_DEBUG, msg)
#define LOG(level, msg) PR_LOG(GetGMPLog(), (level), msg)
#else
#define LOGD(msg)
#define LOG(level, msg)
#endif

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPParent"

namespace gmp {

class GetTempDirTask : public nsRunnable
{
public:
  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIFile> tmpFile;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    tmpFile->GetPath(mPath);
    return NS_OK;
  }

  nsString mPath;
};

// We store the records in files in the system temp dir.
static nsresult
GetGMPStorageDir(nsIFile** aTempDir, const nsString& aOrigin)
{
  if (NS_WARN_IF(!aTempDir)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Directory service is main thread only...
  nsRefPtr<GetTempDirTask> task = new GetTempDirTask();
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  mozilla::SyncRunnable::DispatchToThread(mainThread, task);

  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = NS_NewLocalFile(task->mPath, false, getter_AddRefs(tmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->AppendNative(nsDependentCString("mozilla-gmp-storage"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // TODO: When aOrigin is the same node-id as the GMP sees in the child
  // process (a UUID or somesuch), we can just append it un-hashed here.
  // This should reduce the chance of hash collsions exposing data.
  nsAutoString nodeIdHash;
  nodeIdHash.AppendInt(HashString(aOrigin.get()));
  rv = tmpFile->Append(nodeIdHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  tmpFile.forget(aTempDir);

  return NS_OK;
}

GMPStorageParent::GMPStorageParent(const nsString& aOrigin,
                                   GMPParent* aPlugin)
  : mOrigin(aOrigin)
  , mPlugin(aPlugin)
  , mShutdown(false)
{
}

enum OpenFileMode  { ReadWrite, Truncate };

nsresult
OpenStorageFile(const nsCString& aRecordName,
                const nsString& aNodeId,
                const OpenFileMode aMode,
                PRFileDesc** aOutFD)
{
  MOZ_ASSERT(aOutFD);

  nsCOMPtr<nsIFile> f;
  nsresult rv = GetGMPStorageDir(getter_AddRefs(f), aNodeId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString recordNameHash;
  recordNameHash.AppendInt(HashString(aRecordName.get()));
  f->Append(recordNameHash);

  auto mode = PR_RDWR | PR_CREATE_FILE;
  if (aMode == Truncate) {
    mode |= PR_TRUNCATE;
  }

  return f->OpenNSPRFileDesc(mode, PR_IRWXU, aOutFD);
}

bool
GMPStorageParent::RecvOpen(const nsCString& aRecordName)
{
  if (mShutdown) {
    return true;
  }

  if (mOrigin.EqualsASCII("null")) {
    // Refuse to open storage if the page is the "null" origin; if the page
    // is opened from disk.
    NS_WARNING("Refusing to open storage for null origin");
    unused << SendOpenComplete(aRecordName, GMPGenericErr);
    return true;
  }

  if (aRecordName.IsEmpty() || mFiles.Contains(aRecordName)) {
    unused << SendOpenComplete(aRecordName, GMPRecordInUse);
    return true;
  }

  PRFileDesc* fd = nullptr;
  nsresult rv = OpenStorageFile(aRecordName, mOrigin, ReadWrite, &fd);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to open storage file.");
    unused << SendOpenComplete(aRecordName, GMPGenericErr);
    return true;
  }

  mFiles.Put(aRecordName, fd);
  unused << SendOpenComplete(aRecordName, GMPNoErr);

  return true;
}

bool
GMPStorageParent::RecvRead(const nsCString& aRecordName)
{
  LOGD(("%s::%s: %p record=%s", __CLASS__, __FUNCTION__, this, aRecordName.get()));

  if (mShutdown) {
    return true;
  }

  PRFileDesc* fd = mFiles.Get(aRecordName);
  nsTArray<uint8_t> data;
  if (!fd) {
    unused << SendReadComplete(aRecordName, GMPClosedErr, data);
    return true;
  }

  int32_t len = PR_Seek(fd, 0, PR_SEEK_END);
  PR_Seek(fd, 0, PR_SEEK_SET);

  if (len > GMP_MAX_RECORD_SIZE) {
    // Refuse to read big records.
    unused << SendReadComplete(aRecordName, GMPQuotaExceededErr, data);
    return true;
  }
  data.SetLength(len);
  auto bytesRead = PR_Read(fd, data.Elements(), len);
  auto res = (bytesRead == len) ? GMPNoErr : GMPGenericErr;
  unused << SendReadComplete(aRecordName, res, data);

  return true;
}

bool
GMPStorageParent::RecvWrite(const nsCString& aRecordName,
                            const InfallibleTArray<uint8_t>& aBytes)
{
  LOGD(("%s::%s: %p record=%s", __CLASS__, __FUNCTION__, this, aRecordName.get()));

  if (mShutdown) {
    return true;
  }
  if (aBytes.Length() > GMP_MAX_RECORD_SIZE) {
    unused << SendWriteComplete(aRecordName, GMPQuotaExceededErr);
    return true;
  }

  PRFileDesc* fd = mFiles.Get(aRecordName);
  if (!fd) {
    unused << SendWriteComplete(aRecordName, GMPGenericErr);
    return true;
  }

  // Write operations overwrite the entire record. So re-open the file
  // in truncate mode, to clear its contents.
  PR_Close(fd);
  mFiles.Remove(aRecordName);
  if (NS_FAILED(OpenStorageFile(aRecordName, mOrigin, Truncate, &fd))) {
    unused << SendWriteComplete(aRecordName, GMPGenericErr);
    return true;
  }
  mFiles.Put(aRecordName, fd);

  int32_t bytesWritten = PR_Write(fd, aBytes.Elements(), aBytes.Length());
  auto res = (bytesWritten == (int32_t)aBytes.Length()) ? GMPNoErr : GMPGenericErr;
  unused << SendWriteComplete(aRecordName, res);
  return true;
}

bool
GMPStorageParent::RecvClose(const nsCString& aRecordName)
{
  LOGD(("%s::%s: %p record=%s", __CLASS__, __FUNCTION__, this, aRecordName.get()));

  if (mShutdown) {
    return true;
  }

  PRFileDesc* fd = mFiles.Get(aRecordName);
  if (!fd) {
    return true;
  }
  PR_Close(fd);
  mFiles.Remove(aRecordName);
  return true;
}

void
GMPStorageParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));
  Shutdown();
}

PLDHashOperator
CloseFile(const nsACString& key, PRFileDesc*& entry, void* cx)
{
  PR_Close(entry);
  return PL_DHASH_REMOVE;
}

void
GMPStorageParent::Shutdown()
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));

  if (mShutdown) {
    return;
  }
  mShutdown = true;
  unused << SendShutdown();

  mFiles.Enumerate(CloseFile, nullptr);
  MOZ_ASSERT(!mFiles.Count());
}

} // namespace gmp
} // namespace mozilla
