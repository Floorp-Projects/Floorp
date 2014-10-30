/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPStorageParent.h"
#include "mozilla/SyncRunnable.h"
#include "plhash.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "GMPParent.h"
#include "gmp-storage.h"
#include "mozilla/unused.h"
#include "nsTHashtable.h"
#include "nsDataHashtable.h"
#include "prio.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsContentCID.h"
#include "nsServiceManagerUtils.h"

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

// We store the records in files in the profile dir.
// $profileDir/gmp/storage/$nodeId/
static nsresult
GetGMPStorageDir(nsIFile** aTempDir, const nsCString& aNodeId)
{
  if (NS_WARN_IF(!aTempDir)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (NS_WARN_IF(!mps)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = mps->GetStorageDir(getter_AddRefs(tmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->AppendNative(NS_LITERAL_CSTRING("storage"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->AppendNative(aNodeId);
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

enum OpenFileMode  { ReadWrite, Truncate };

nsresult
OpenStorageFile(const nsCString& aRecordName,
                const nsCString& aNodeId,
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

PLDHashOperator
CloseFile(const nsACString& key, PRFileDesc*& entry, void* cx)
{
  if (PR_Close(entry) != PR_SUCCESS) {
    NS_WARNING("GMPDiskStorage Failed to clsose file.");
  }
  return PL_DHASH_REMOVE;
}

class GMPDiskStorage : public GMPStorage {
public:
  explicit GMPDiskStorage(const nsCString& aNodeId)
    : mNodeId(aNodeId)
  {
  }
  ~GMPDiskStorage() {
    mFiles.Enumerate(CloseFile, nullptr);
    MOZ_ASSERT(!mFiles.Count());
  }

  virtual GMPErr Open(const nsCString& aRecordName) MOZ_OVERRIDE
  {
    MOZ_ASSERT(!IsOpen(aRecordName));
    PRFileDesc* fd = nullptr;
    if (NS_FAILED(OpenStorageFile(aRecordName, mNodeId, ReadWrite, &fd))) {
      NS_WARNING("Failed to open storage file.");
      return GMPGenericErr;
    }
    mFiles.Put(aRecordName, fd);
    return GMPNoErr;
  }

  virtual bool IsOpen(const nsCString& aRecordName) MOZ_OVERRIDE {
    return mFiles.Contains(aRecordName);
  }

  virtual GMPErr Read(const nsCString& aRecordName,
                      nsTArray<uint8_t>& aOutBytes) MOZ_OVERRIDE
  {
    PRFileDesc* fd = mFiles.Get(aRecordName);
    if (!fd) {
      return GMPGenericErr;
    }

    int32_t len = PR_Seek(fd, 0, PR_SEEK_END);
    PR_Seek(fd, 0, PR_SEEK_SET);

    if (len > GMP_MAX_RECORD_SIZE) {
      // Refuse to read big records.
      return GMPQuotaExceededErr;
    }
    aOutBytes.SetLength(len);
    auto bytesRead = PR_Read(fd, aOutBytes.Elements(), len);
    return (bytesRead == len) ? GMPNoErr : GMPGenericErr;
  }

  virtual GMPErr Write(const nsCString& aRecordName,
                       const nsTArray<uint8_t>& aBytes) MOZ_OVERRIDE
  {
    PRFileDesc* fd = mFiles.Get(aRecordName);
    if (!fd) {
      return GMPGenericErr;
    }

    // Write operations overwrite the entire record. So re-open the file
    // in truncate mode, to clear its contents.
    PR_Close(fd);
    mFiles.Remove(aRecordName);
    if (NS_FAILED(OpenStorageFile(aRecordName, mNodeId, Truncate, &fd))) {
      return GMPGenericErr;
    }
    mFiles.Put(aRecordName, fd);

    int32_t bytesWritten = PR_Write(fd, aBytes.Elements(), aBytes.Length());
    return (bytesWritten == (int32_t)aBytes.Length()) ? GMPNoErr : GMPGenericErr;
  }

  virtual void Close(const nsCString& aRecordName) MOZ_OVERRIDE
  {
    PRFileDesc* fd = mFiles.Get(aRecordName);
    if (fd) {
      if (PR_Close(fd) == PR_SUCCESS) {
        mFiles.Remove(aRecordName);
      } else {
        NS_WARNING("GMPDiskStorage Failed to clsose file.");
      }
    }
  }

private:
  nsDataHashtable<nsCStringHashKey, PRFileDesc*> mFiles;
  const nsAutoCString mNodeId;
};

class GMPMemoryStorage : public GMPStorage {
public:
  virtual GMPErr Open(const nsCString& aRecordName) MOZ_OVERRIDE
  {
    MOZ_ASSERT(!IsOpen(aRecordName));

    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      record = new Record();
      mRecords.Put(aRecordName, record);
    }
    record->mIsOpen = true;
    return GMPNoErr;
  }

  virtual bool IsOpen(const nsCString& aRecordName) MOZ_OVERRIDE {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return false;
    }
    return record->mIsOpen;
  }

  virtual GMPErr Read(const nsCString& aRecordName,
                      nsTArray<uint8_t>& aOutBytes) MOZ_OVERRIDE
  {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return GMPGenericErr;
    }
    aOutBytes = record->mData;
    return GMPNoErr;
  }

  virtual GMPErr Write(const nsCString& aRecordName,
                       const nsTArray<uint8_t>& aBytes) MOZ_OVERRIDE
  {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return GMPClosedErr;
    }
    record->mData = aBytes;
    return GMPNoErr;
  }

  virtual void Close(const nsCString& aRecordName) MOZ_OVERRIDE
  {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return;
    }
    if (!record->mData.Length()) {
      // Record is empty, delete.
      mRecords.Remove(aRecordName);
    } else {
      record->mIsOpen = false;
    }
  }

private:

  struct Record {
    Record() : mIsOpen(false) {}
    nsTArray<uint8_t> mData;
    bool mIsOpen;
  };

  nsClassHashtable<nsCStringHashKey, Record> mRecords;
};

GMPStorageParent::GMPStorageParent(const nsCString& aNodeId,
                                   GMPParent* aPlugin)
  : mNodeId(aNodeId)
  , mPlugin(aPlugin)
  , mShutdown(false)
{
}

nsresult
GMPStorageParent::Init()
{
  if (NS_WARN_IF(mNodeId.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (NS_WARN_IF(!mps)) {
    return NS_ERROR_FAILURE;
  }

  bool persistent = false;
  if (NS_WARN_IF(NS_FAILED(mps->IsPersistentStorageAllowed(mNodeId, &persistent)))) {
    return NS_ERROR_FAILURE;
  }
  if (persistent) {
    mStorage = MakeUnique<GMPDiskStorage>(mNodeId);
  } else {
    mStorage = MakeUnique<GMPMemoryStorage>();
  }

  return NS_OK;
}

bool
GMPStorageParent::RecvOpen(const nsCString& aRecordName)
{
  if (mShutdown) {
    return false;
  }

  if (mNodeId.EqualsLiteral("null")) {
    // Refuse to open storage if the page is opened from local disk,
    // or shared across origin.
    NS_WARNING("Refusing to open storage for null NodeId");
    unused << SendOpenComplete(aRecordName, GMPGenericErr);
    return true;
  }

  if (aRecordName.IsEmpty()) {
    unused << SendOpenComplete(aRecordName, GMPGenericErr);
    return true;
  }

  if (mStorage->IsOpen(aRecordName)) {
    unused << SendOpenComplete(aRecordName, GMPRecordInUse);
    return true;
  }

  auto err = mStorage->Open(aRecordName);
  MOZ_ASSERT(GMP_FAILED(err) || mStorage->IsOpen(aRecordName));
  unused << SendOpenComplete(aRecordName, err);

  return true;
}

bool
GMPStorageParent::RecvRead(const nsCString& aRecordName)
{
  LOGD(("%s::%s: %p record=%s", __CLASS__, __FUNCTION__, this, aRecordName.get()));

  if (mShutdown) {
    return false;
  }

  nsTArray<uint8_t> data;
  if (!mStorage->IsOpen(aRecordName)) {
    unused << SendReadComplete(aRecordName, GMPClosedErr, data);
  } else {
    unused << SendReadComplete(aRecordName, mStorage->Read(aRecordName, data), data);
  }

  return true;
}

bool
GMPStorageParent::RecvWrite(const nsCString& aRecordName,
                            const InfallibleTArray<uint8_t>& aBytes)
{
  LOGD(("%s::%s: %p record=%s", __CLASS__, __FUNCTION__, this, aRecordName.get()));

  if (mShutdown) {
    return false;
  }

  if (!mStorage->IsOpen(aRecordName)) {
    unused << SendWriteComplete(aRecordName, GMPClosedErr);
    return true;
  }

  if (aBytes.Length() > GMP_MAX_RECORD_SIZE) {
    unused << SendWriteComplete(aRecordName, GMPQuotaExceededErr);
    return true;
  }

  unused << SendWriteComplete(aRecordName, mStorage->Write(aRecordName, aBytes));

  return true;
}

bool
GMPStorageParent::RecvClose(const nsCString& aRecordName)
{
  LOGD(("%s::%s: %p record=%s", __CLASS__, __FUNCTION__, this, aRecordName.get()));

  if (mShutdown) {
    return true;
  }

  mStorage->Close(aRecordName);

  return true;
}

void
GMPStorageParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));
  Shutdown();
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

  mStorage = nullptr;

}

} // namespace gmp
} // namespace mozilla
