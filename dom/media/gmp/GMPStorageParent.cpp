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
#include "nsISimpleEnumerator.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern PRLogModuleInfo* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPStorageParent"

namespace gmp {

// We store the records in files in the profile dir.
// $profileDir/gmp/storage/$nodeId/
static nsresult
GetGMPStorageDir(nsIFile** aTempDir, const nsCString& aNodeId)
{
  if (NS_WARN_IF(!aTempDir)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIGeckoMediaPluginChromeService> mps =
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

static nsresult
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

static nsresult
RemoveStorageFile(const nsCString& aRecordName,
                  const nsCString& aNodeId)
{
  nsCOMPtr<nsIFile> f;
  nsresult rv = GetGMPStorageDir(getter_AddRefs(f), aNodeId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString recordNameHash;
  recordNameHash.AppendInt(HashString(aRecordName.get()));
  f->Append(recordNameHash);

  return f->Remove(/* bool recursive= */ false);
}

PLDHashOperator
CloseFile(const nsACString& key, PRFileDesc*& entry, void* cx)
{
  if (PR_Close(entry) != PR_SUCCESS) {
    NS_WARNING("GMPDiskStorage failed to close file.");
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

  virtual GMPErr Open(const nsCString& aRecordName) override
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

  virtual bool IsOpen(const nsCString& aRecordName) override {
    return mFiles.Contains(aRecordName);
  }

  static
  GMPErr ReadRecordMetadata(PRFileDesc* aFd,
                            int32_t& aOutFileLength,
                            int32_t& aOutRecordLength,
                            nsACString& aOutRecordName)
  {
    int32_t fileLength = PR_Seek(aFd, 0, PR_SEEK_END);
    PR_Seek(aFd, 0, PR_SEEK_SET);

    if (fileLength > GMP_MAX_RECORD_SIZE) {
      // Refuse to read big records.
      return GMPQuotaExceededErr;
    }
    aOutFileLength = fileLength;
    aOutRecordLength = 0;

    // At the start of the file the length of the record name is stored in a
    // size_t (host byte order) followed by the record name at the start of
    // the file. The record name is not null terminated. The remainder of the
    // file is the record's data.

    size_t recordNameLength = 0;
    if (fileLength == 0 || sizeof(recordNameLength) >= (size_t)fileLength) {
      // Record file is empty, or doesn't even have enough contents to
      // store the record name length and/or record name. Report record
      // as empty.
      return GMPNoErr;
    }

    int32_t bytesRead = PR_Read(aFd, &recordNameLength, sizeof(recordNameLength));
    if (sizeof(recordNameLength) != bytesRead ||
        recordNameLength > fileLength - sizeof(recordNameLength)) {
      // Record file has invalid contents. Report record as empty.
      return GMPNoErr;
    }

    nsCString recordName;
    recordName.SetLength(recordNameLength);
    bytesRead = PR_Read(aFd, recordName.BeginWriting(), recordNameLength);
    if (bytesRead != (int32_t)recordNameLength) {
      // Record file has invalid contents. Report record as empty.
      return GMPGenericErr;
    }

    MOZ_ASSERT(fileLength > 0 && (size_t)fileLength >= sizeof(recordNameLength) + recordNameLength);
    int32_t recordLength = fileLength - (sizeof(recordNameLength) + recordNameLength);

    aOutRecordLength = recordLength;
    aOutRecordName = recordName;

    return GMPNoErr;
  }

  virtual GMPErr Read(const nsCString& aRecordName,
                      nsTArray<uint8_t>& aOutBytes) override
  {
    // Our error strategy is to report records with invalid contents as
    // containing 0 bytes. Zero length records are considered "deleted" by
    // the GMPStorage API.
    aOutBytes.SetLength(0);

    PRFileDesc* fd = mFiles.Get(aRecordName);
    if (!fd) {
      return GMPGenericErr;
    }

    int32_t fileLength = 0;
    int32_t recordLength = 0;
    nsCString recordName;
    GMPErr err = ReadRecordMetadata(fd,
                                    fileLength,
                                    recordLength,
                                    recordName);
    if (NS_WARN_IF(GMP_FAILED(err))) {
      return err;
    }

    if (recordLength == 0) {
      // Record is empoty but not invalid, or it's invalid and we're going to
      // just act like it's empty and let the client overwrite it.
      return GMPNoErr;
    }

    if (!aRecordName.Equals(recordName)) {
      NS_WARNING("Hash collision in GMPStorage");
      return GMPGenericErr;
    }

    // After calling ReadRecordMetadata, we should be ready to read the
    // record data.
    MOZ_ASSERT(PR_Available(fd) == recordLength);

    aOutBytes.SetLength(recordLength);
    int32_t bytesRead = PR_Read(fd, aOutBytes.Elements(), recordLength);
    return (bytesRead == recordLength) ? GMPNoErr : GMPGenericErr;
  }

  virtual GMPErr Write(const nsCString& aRecordName,
                       const nsTArray<uint8_t>& aBytes) override
  {
    PRFileDesc* fd = mFiles.Get(aRecordName);
    if (!fd) {
      return GMPGenericErr;
    }

    // Write operations overwrite the entire record. So close it now.
    PR_Close(fd);
    mFiles.Remove(aRecordName);

    // Writing 0 bytes means removing (deleting) the file.
    if (aBytes.Length() == 0) {
      nsresult rv = RemoveStorageFile(aRecordName, mNodeId);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Could not delete file -> Continue with trying to erase the contents.
      } else {
        return GMPNoErr;
      }
    }

    // Write operations overwrite the entire record. So re-open the file
    // in truncate mode, to clear its contents.
    if (NS_FAILED(OpenStorageFile(aRecordName, mNodeId, Truncate, &fd))) {
      return GMPGenericErr;
    }
    mFiles.Put(aRecordName, fd);

    // Store the length of the record name followed by the record name
    // at the start of the file.
    int32_t bytesWritten = 0;
    if (aBytes.Length() > 0) {
      size_t recordNameLength = aRecordName.Length();
      bytesWritten = PR_Write(fd, &recordNameLength, sizeof(recordNameLength));
      if (NS_WARN_IF(bytesWritten != sizeof(recordNameLength))) {
        return GMPGenericErr;
      }
      bytesWritten = PR_Write(fd, aRecordName.get(), recordNameLength);
      if (NS_WARN_IF(bytesWritten != (int32_t)recordNameLength)) {
        return GMPGenericErr;
      }
    }

    bytesWritten = PR_Write(fd, aBytes.Elements(), aBytes.Length());
    return (bytesWritten == (int32_t)aBytes.Length()) ? GMPNoErr : GMPGenericErr;
  }

  virtual GMPErr GetRecordNames(nsTArray<nsCString>& aOutRecordNames) override
  {
    nsCOMPtr<nsIFile> storageDir;
    nsresult rv = GetGMPStorageDir(getter_AddRefs(storageDir), mNodeId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return GMPGenericErr;
    }

    nsCOMPtr<nsISimpleEnumerator> iter;
    rv = storageDir->GetDirectoryEntries(getter_AddRefs(iter));
    if (NS_FAILED(rv)) {
      return GMPGenericErr;
    }

    bool hasMore;
    while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> supports;
      rv = iter->GetNext(getter_AddRefs(supports));
      if (NS_FAILED(rv)) {
        continue;
      }
      nsCOMPtr<nsIFile> dirEntry(do_QueryInterface(supports, &rv));
      if (NS_FAILED(rv)) {
        continue;
      }

      nsAutoCString leafName;
      rv = dirEntry->GetNativeLeafName(leafName);
      if (NS_FAILED(rv)) {
        continue;
      }

      PRFileDesc* fd = nullptr;
      if (NS_FAILED(dirEntry->OpenNSPRFileDesc(PR_RDONLY, 0, &fd))) {
        continue;
      }
      int32_t fileLength = 0;
      int32_t recordLength = 0;
      nsCString recordName;
      GMPErr err = ReadRecordMetadata(fd,
                                      fileLength,
                                      recordLength,
                                      recordName);
      PR_Close(fd);
      if (NS_WARN_IF(GMP_FAILED(err))) {
        return err;
      }

      if (recordName.IsEmpty() || recordLength == 0) {
        continue;
      }

      // Ensure the file name is the hash of the record name stored in the
      // record file. Otherwise it's not a valid record.
      nsAutoCString recordNameHash;
      recordNameHash.AppendInt(HashString(recordName.get()));
      if (!recordNameHash.Equals(leafName)) {
        continue;
      }

      aOutRecordNames.AppendElement(recordName);
    }

    return GMPNoErr;
  }

  virtual void Close(const nsCString& aRecordName) override
  {
    PRFileDesc* fd = mFiles.Get(aRecordName);
    if (fd) {
      if (PR_Close(fd) == PR_SUCCESS) {
        mFiles.Remove(aRecordName);
      } else {
        NS_WARNING("GMPDiskStorage failed to close file.");
      }
    }
  }

private:
  nsDataHashtable<nsCStringHashKey, PRFileDesc*> mFiles;
  const nsAutoCString mNodeId;
};

class GMPMemoryStorage : public GMPStorage {
public:
  virtual GMPErr Open(const nsCString& aRecordName) override
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

  virtual bool IsOpen(const nsCString& aRecordName) override {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return false;
    }
    return record->mIsOpen;
  }

  virtual GMPErr Read(const nsCString& aRecordName,
                      nsTArray<uint8_t>& aOutBytes) override
  {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return GMPGenericErr;
    }
    aOutBytes = record->mData;
    return GMPNoErr;
  }

  virtual GMPErr Write(const nsCString& aRecordName,
                       const nsTArray<uint8_t>& aBytes) override
  {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return GMPClosedErr;
    }
    record->mData = aBytes;
    return GMPNoErr;
  }

  virtual GMPErr GetRecordNames(nsTArray<nsCString>& aOutRecordNames) override
  {
    mRecords.EnumerateRead(EnumRecordNames, &aOutRecordNames);
    return GMPNoErr;
  }

  virtual void Close(const nsCString& aRecordName) override
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

  static PLDHashOperator
  EnumRecordNames(const nsACString& aKey,
                  Record* aRecord,
                  void* aUserArg)
  {
    nsTArray<nsCString>* names = reinterpret_cast<nsTArray<nsCString>*>(aUserArg);
    names->AppendElement(aKey);
    return PL_DHASH_NEXT;
  }

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
  nsCOMPtr<mozIGeckoMediaPluginChromeService> mps =
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
                            InfallibleTArray<uint8_t>&& aBytes)
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
GMPStorageParent::RecvGetRecordNames()
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));

  if (mShutdown) {
    return true;
  }

  nsTArray<nsCString> recordNames;
  GMPErr status = mStorage->GetRecordNames(recordNames);
  unused << SendRecordNames(recordNames, status);

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
