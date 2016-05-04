/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plhash.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "GMPParent.h"
#include "gmp-storage.h"
#include "mozilla/unused.h"
#include "mozilla/Endian.h"
#include "nsClassHashtable.h"
#include "prio.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsContentCID.h"
#include "nsServiceManagerUtils.h"
#include "nsISimpleEnumerator.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

namespace gmp {

// We store the records for a given GMP as files in the profile dir.
// $profileDir/gmp/$platform/$gmpName/storage/$nodeId/
static nsresult
GetGMPStorageDir(nsIFile** aTempDir,
                 const nsString& aGMPName,
                 const nsCString& aNodeId)
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

  rv = tmpFile->Append(aGMPName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
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

// Disk-backed GMP storage. Records are stored in files on disk in
// the profile directory. The record name is a hash of the filename,
// and we resolve hash collisions by just adding 1 to the hash code.
// The format of records on disk is:
//   4 byte, uint32_t $recordNameLength, in little-endian byte order,
//   record name (i.e. $recordNameLength bytes, no null terminator)
//   record bytes (entire remainder of file)
class GMPDiskStorage : public GMPStorage {
public:
  explicit GMPDiskStorage(const nsCString& aNodeId,
                          const nsString& aGMPName)
    : mNodeId(aNodeId)
    , mGMPName(aGMPName)
  {
  }

  ~GMPDiskStorage() {
    // Close all open file handles.
    for (auto iter = mRecords.ConstIter(); !iter.Done(); iter.Next()) {
      Record* record = iter.UserData();
      if (record->mFileDesc) {
        PR_Close(record->mFileDesc);
        record->mFileDesc = nullptr;
      }
    }
  }

  nsresult Init() {
    // Build our index of records on disk.
    nsCOMPtr<nsIFile> storageDir;
    nsresult rv = GetGMPStorageDir(getter_AddRefs(storageDir), mGMPName, mNodeId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_FAILURE;
    }

    DirectoryEnumerator iter(storageDir, DirectoryEnumerator::FilesAndDirs);
    for (nsCOMPtr<nsIFile> dirEntry; (dirEntry = iter.Next()) != nullptr;) {
      PRFileDesc* fd = nullptr;
      if (NS_FAILED(dirEntry->OpenNSPRFileDesc(PR_RDONLY, 0, &fd))) {
        continue;
      }
      int32_t recordLength = 0;
      nsCString recordName;
      nsresult err = ReadRecordMetadata(fd, recordLength, recordName);
      PR_Close(fd);
      if (NS_FAILED(err)) {
        // File is not a valid storage file. Don't index it. Delete the file,
        // to make our indexing faster in future.
        dirEntry->Remove(false);
        continue;
      }

      nsAutoString filename;
      rv = dirEntry->GetLeafName(filename);
      if (NS_FAILED(rv)) {
        continue;
      }

      mRecords.Put(recordName, new Record(filename, recordName));
    }

    return NS_OK;
  }

  GMPErr Open(const nsCString& aRecordName) override
  {
    MOZ_ASSERT(!IsOpen(aRecordName));
    nsresult rv;
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      // New file.
      nsAutoString filename;
      rv = GetUnusedFilename(aRecordName, filename);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return GMPGenericErr;
      }
      record = new Record(filename, aRecordName);
      mRecords.Put(aRecordName, record);
    }

    MOZ_ASSERT(record);
    if (record->mFileDesc) {
      NS_WARNING("Tried to open already open record");
      return GMPRecordInUse;
    }

    rv = OpenStorageFile(record->mFilename, ReadWrite, &record->mFileDesc);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return GMPGenericErr;
    }

    MOZ_ASSERT(IsOpen(aRecordName));

    return GMPNoErr;
  }

  bool IsOpen(const nsCString& aRecordName) const override {
    // We are open if we have a record indexed, and it has a valid
    // file descriptor.
    const Record* record = mRecords.Get(aRecordName);
    return record && !!record->mFileDesc;
  }

  GMPErr Read(const nsCString& aRecordName,
              nsTArray<uint8_t>& aOutBytes) override
  {
    if (!IsOpen(aRecordName)) {
      return GMPClosedErr;
    }

    Record* record = nullptr;
    mRecords.Get(aRecordName, &record);
    MOZ_ASSERT(record && !!record->mFileDesc); // IsOpen() guarantees this.

    // Our error strategy is to report records with invalid contents as
    // containing 0 bytes. Zero length records are considered "deleted" by
    // the GMPStorage API.
    aOutBytes.SetLength(0);

    int32_t recordLength = 0;
    nsCString recordName;
    nsresult err = ReadRecordMetadata(record->mFileDesc,
                                      recordLength,
                                      recordName);
    if (NS_FAILED(err) || recordLength == 0) {
      // We failed to read the record metadata. Or the record is 0 length.
      // Treat damaged records as empty.
      // ReadRecordMetadata() could fail if the GMP opened a new record and
      // tried to read it before anything was written to it..
      return GMPNoErr;
    }

    if (!aRecordName.Equals(recordName)) {
      NS_WARNING("Record file contains some other record's contents!");
      return GMPRecordCorrupted;
    }

    // After calling ReadRecordMetadata, we should be ready to read the
    // record data.
    if (PR_Available(record->mFileDesc) != recordLength) {
      NS_WARNING("Record file length mismatch!");
      return GMPRecordCorrupted;
    }

    aOutBytes.SetLength(recordLength);
    int32_t bytesRead = PR_Read(record->mFileDesc, aOutBytes.Elements(), recordLength);
    return (bytesRead == recordLength) ? GMPNoErr : GMPRecordCorrupted;
  }

  GMPErr Write(const nsCString& aRecordName,
               const nsTArray<uint8_t>& aBytes) override
  {
    if (!IsOpen(aRecordName)) {
      return GMPClosedErr;
    }

    Record* record = nullptr;
    mRecords.Get(aRecordName, &record);
    MOZ_ASSERT(record && !!record->mFileDesc); // IsOpen() guarantees this.

    // Write operations overwrite the entire record. So close it now.
    PR_Close(record->mFileDesc);
    record->mFileDesc = nullptr;

    // Writing 0 bytes means removing (deleting) the file.
    if (aBytes.Length() == 0) {
      nsresult rv = RemoveStorageFile(record->mFilename);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Could not delete file -> Continue with trying to erase the contents.
      } else {
        return GMPNoErr;
      }
    }

    // Write operations overwrite the entire record. So re-open the file
    // in truncate mode, to clear its contents.
    if (NS_FAILED(OpenStorageFile(record->mFilename,
                                  Truncate,
                                  &record->mFileDesc))) {
      return GMPGenericErr;
    }

    // Store the length of the record name followed by the record name
    // at the start of the file.
    int32_t bytesWritten = 0;
    char buf[sizeof(uint32_t)] = {0};
    LittleEndian::writeUint32(buf, aRecordName.Length());
    bytesWritten = PR_Write(record->mFileDesc, buf, MOZ_ARRAY_LENGTH(buf));
    if (bytesWritten != MOZ_ARRAY_LENGTH(buf)) {
      NS_WARNING("Failed to write GMPStorage record name length.");
      return GMPRecordCorrupted;
    }
    bytesWritten = PR_Write(record->mFileDesc,
                            aRecordName.get(),
                            aRecordName.Length());
    if (bytesWritten != (int32_t)aRecordName.Length()) {
      NS_WARNING("Failed to write GMPStorage record name.");
      return GMPRecordCorrupted;
    }

    bytesWritten = PR_Write(record->mFileDesc, aBytes.Elements(), aBytes.Length());
    if (bytesWritten != (int32_t)aBytes.Length()) {
      NS_WARNING("Failed to write GMPStorage record data.");
      return GMPRecordCorrupted;
    }

    // Try to sync the file to disk, so that in the event of a crash,
    // the record is less likely to be corrupted.
    PR_Sync(record->mFileDesc);

    return GMPNoErr;
  }

  GMPErr GetRecordNames(nsTArray<nsCString>& aOutRecordNames) const override
  {
    for (auto iter = mRecords.ConstIter(); !iter.Done(); iter.Next()) {
      aOutRecordNames.AppendElement(iter.UserData()->mRecordName);
    }
    return GMPNoErr;
  }

  void Close(const nsCString& aRecordName) override
  {
    Record* record = nullptr;
    mRecords.Get(aRecordName, &record);
    if (record && !!record->mFileDesc) {
      PR_Close(record->mFileDesc);
      record->mFileDesc = nullptr;
    }
    MOZ_ASSERT(!IsOpen(aRecordName));
  }

private:

  // We store records in a file which is a hash of the record name.
  // If there is a hash collision, we just keep adding 1 to the hash
  // code, until we find a free slot.
  nsresult GetUnusedFilename(const nsACString& aRecordName,
                             nsString& aOutFilename)
  {
    nsCOMPtr<nsIFile> storageDir;
    nsresult rv = GetGMPStorageDir(getter_AddRefs(storageDir), mGMPName, mNodeId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    uint64_t recordNameHash = HashString(PromiseFlatCString(aRecordName).get());
    for (int i = 0; i < 1000000; i++) {
      nsCOMPtr<nsIFile> f;
      rv = storageDir->Clone(getter_AddRefs(f));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      nsAutoString hashStr;
      hashStr.AppendInt(recordNameHash);
      rv = f->Append(hashStr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      bool exists = false;
      f->Exists(&exists);
      if (!exists) {
        // Filename not in use, we can write into this file.
        aOutFilename = hashStr;
        return NS_OK;
      } else {
        // Hash collision; just increment the hash name and try that again.
        ++recordNameHash;
        continue;
      }
    }
    // Somehow, we've managed to completely fail to find a vacant file name.
    // Give up.
    NS_WARNING("GetUnusedFilename had extreme hash collision!");
    return NS_ERROR_FAILURE;
  }

  enum OpenFileMode  { ReadWrite, Truncate };

  nsresult OpenStorageFile(const nsAString& aFileLeafName,
                           const OpenFileMode aMode,
                           PRFileDesc** aOutFD)
  {
    MOZ_ASSERT(aOutFD);

    nsCOMPtr<nsIFile> f;
    nsresult rv = GetGMPStorageDir(getter_AddRefs(f), mGMPName, mNodeId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    f->Append(aFileLeafName);

    auto mode = PR_RDWR | PR_CREATE_FILE;
    if (aMode == Truncate) {
      mode |= PR_TRUNCATE;
    }

    return f->OpenNSPRFileDesc(mode, PR_IRWXU, aOutFD);
  }

  nsresult ReadRecordMetadata(PRFileDesc* aFd,
                              int32_t& aOutRecordLength,
                              nsACString& aOutRecordName)
  {
    int32_t offset = PR_Seek(aFd, 0, PR_SEEK_END);
    PR_Seek(aFd, 0, PR_SEEK_SET);

    if (offset < 0 || offset > GMP_MAX_RECORD_SIZE) {
      // Refuse to read big records, or records where we can't get a length.
      return NS_ERROR_FAILURE;
    }
    const uint32_t fileLength = static_cast<uint32_t>(offset);

    // At the start of the file the length of the record name is stored in a
    // uint32_t (little endian byte order) followed by the record name at the
    // start of the file. The record name is not null terminated. The remainder
    // of the file is the record's data.

    if (fileLength < sizeof(uint32_t)) {
      // Record file doesn't have enough contents to store the record name
      // length. Fail.
      return NS_ERROR_FAILURE;
    }

    // Read length, and convert to host byte order.
    uint32_t recordNameLength = 0;
    char buf[sizeof(recordNameLength)] = { 0 };
    int32_t bytesRead = PR_Read(aFd, &buf, sizeof(recordNameLength));
    recordNameLength = LittleEndian::readUint32(buf);
    if (sizeof(recordNameLength) != bytesRead ||
        recordNameLength == 0 ||
        recordNameLength + sizeof(recordNameLength) > fileLength ||
        recordNameLength > GMP_MAX_RECORD_NAME_SIZE) {
      // Record file has invalid contents. Fail.
      return NS_ERROR_FAILURE;
    }

    nsCString recordName;
    recordName.SetLength(recordNameLength);
    bytesRead = PR_Read(aFd, recordName.BeginWriting(), recordNameLength);
    if ((uint32_t)bytesRead != recordNameLength) {
      // Read failed.
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(fileLength >= sizeof(recordNameLength) + recordNameLength);
    int32_t recordLength = fileLength - (sizeof(recordNameLength) + recordNameLength);

    aOutRecordLength = recordLength;
    aOutRecordName = recordName;

    // Read cursor should be positioned after the record name, before the record contents.
    if (PR_Seek(aFd, 0, PR_SEEK_CUR) != (int32_t)(sizeof(recordNameLength) + recordNameLength)) {
      NS_WARNING("Read cursor mismatch after ReadRecordMetadata()");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  nsresult RemoveStorageFile(const nsString& aFilename)
  {
    nsCOMPtr<nsIFile> f;
    nsresult rv = GetGMPStorageDir(getter_AddRefs(f), mGMPName, mNodeId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = f->Append(aFilename);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return f->Remove(/* bool recursive= */ false);
  }

  struct Record {
    Record(const nsAString& aFilename,
           const nsACString& aRecordName)
      : mFilename(aFilename)
      , mRecordName(aRecordName)
      , mFileDesc(0)
    {}
    ~Record() {
      MOZ_ASSERT(!mFileDesc);
    }
    nsString mFilename;
    nsCString mRecordName;
    PRFileDesc* mFileDesc;
  };

  // Hash record name to record data.
  nsClassHashtable<nsCStringHashKey, Record> mRecords;
  const nsCString mNodeId;
  const nsString mGMPName;
};

already_AddRefed<GMPStorage> CreateGMPDiskStorage(const nsCString& aNodeId,
                                                  const nsString& aGMPName)
{
  RefPtr<GMPDiskStorage> storage(new GMPDiskStorage(aNodeId, aGMPName));
  if (NS_FAILED(storage->Init())) {
    NS_WARNING("Failed to initialize on disk GMP storage");
    return nullptr;
  }
  return storage.forget();
}

} // namespace gmp
} // namespace mozilla
