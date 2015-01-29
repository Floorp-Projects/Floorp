/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozMtpDatabase.h"
#include "MozMtpServer.h"

#include "base/message_loop.h"
#include "DeviceStorage.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Scoped.h"
#include "mozilla/Services.h"
#include "nsAutoPtr.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "prio.h"

#include <dirent.h>
#include <libgen.h>

using namespace android;
using namespace mozilla;

namespace mozilla {
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCloseDir, PRDir, PR_CloseDir)
}

BEGIN_MTP_NAMESPACE

#if 0
// Some debug code for figuring out deadlocks, if you happen to run into
// that scenario

class DebugMutexAutoLock: public MutexAutoLock
{
public:
  DebugMutexAutoLock(mozilla::Mutex& aMutex)
    : MutexAutoLock(aMutex)
  {
    MTP_LOG("Mutex acquired");
  }

  ~DebugMutexAutoLock()
  {
    MTP_LOG("Releasing mutex");
  }
};
#define MutexAutoLock  MTP_LOG("About to enter mutex"); DebugMutexAutoLock

#endif

static const char *
ObjectPropertyAsStr(MtpObjectProperty aProperty)
{
  switch (aProperty) {
    case MTP_PROPERTY_STORAGE_ID:         return "MTP_PROPERTY_STORAGE_ID";
    case MTP_PROPERTY_OBJECT_FORMAT:      return "MTP_PROPERTY_OBJECT_FORMAT";
    case MTP_PROPERTY_PROTECTION_STATUS:  return "MTP_PROPERTY_PROTECTION_STATUS";
    case MTP_PROPERTY_OBJECT_SIZE:        return "MTP_PROPERTY_OBJECT_SIZE";
    case MTP_PROPERTY_OBJECT_FILE_NAME:   return "MTP_PROPERTY_OBJECT_FILE_NAME";
    case MTP_PROPERTY_DATE_MODIFIED:      return "MTP_PROPERTY_DATE_MODIFIED";
    case MTP_PROPERTY_PARENT_OBJECT:      return "MTP_PROPERTY_PARENT_OBJECT";
    case MTP_PROPERTY_PERSISTENT_UID:     return "MTP_PROPERTY_PERSISTENT_UID";
    case MTP_PROPERTY_NAME:               return "MTP_PROPERTY_NAME";
    case MTP_PROPERTY_DATE_ADDED:         return "MTP_PROPERTY_DATE_ADDED";
    case MTP_PROPERTY_WIDTH:              return "MTP_PROPERTY_WIDTH";
    case MTP_PROPERTY_HEIGHT:             return "MTP_PROPERTY_HEIGHT";
    case MTP_PROPERTY_IMAGE_BIT_DEPTH:    return "MTP_PROPERTY_IMAGE_BIT_DEPTH";
    case MTP_PROPERTY_DISPLAY_NAME:       return "MTP_PROPERTY_DISPLAY_NAME";
  }
  return "MTP_PROPERTY_???";
}

MozMtpDatabase::MozMtpDatabase()
  : mMutex("MozMtpDatabase::mMutex"),
    mDb(mMutex),
    mStorage(mMutex),
    mBeginSendObjectCalled(false)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  // We use the index into the array as the handle. Since zero isn't a valid
  // index, we stick a dummy entry there.

  RefPtr<DbEntry> dummy;

  MutexAutoLock lock(mMutex);
  mDb.AppendElement(dummy);
}

//virtual
MozMtpDatabase::~MozMtpDatabase()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
}

void
MozMtpDatabase::AddEntry(DbEntry *entry)
{
  MutexAutoLock lock(mMutex);

  entry->mHandle = GetNextHandle();
  MOZ_ASSERT(mDb.Length() == entry->mHandle);
  mDb.AppendElement(entry);

  MTP_DBG("Handle: 0x%08x Parent: 0x%08x Path:'%s'",
          entry->mHandle, entry->mParent, entry->mPath.get());
}

void
MozMtpDatabase::AddEntryAndNotify(DbEntry* entry, RefCountedMtpServer* aMtpServer)
{
  AddEntry(entry);
  aMtpServer->sendObjectAdded(entry->mHandle);
}

void
MozMtpDatabase::DumpEntries(const char* aLabel)
{
  MutexAutoLock lock(mMutex);

  ProtectedDbArray::size_type numEntries = mDb.Length();
  MTP_LOG("%s: numEntries = %d", aLabel, numEntries);
  ProtectedDbArray::index_type entryIndex;
  for (entryIndex = 1; entryIndex < numEntries; entryIndex++) {
    RefPtr<DbEntry> entry = mDb[entryIndex];
    if (entry) {
      MTP_LOG("%s: mDb[%d]: mHandle: 0x%08x mParent: 0x%08x StorageID: 0x%08x path: '%s'",
              aLabel, entryIndex, entry->mHandle, entry->mParent, entry->mStorageID, entry->mPath.get());
    } else {
      MTP_LOG("%s: mDb[%2d]: entry is NULL", aLabel, entryIndex);
    }
  }
}

MtpObjectHandle
MozMtpDatabase::FindEntryByPath(const nsACString& aPath)
{
  MutexAutoLock lock(mMutex);

  ProtectedDbArray::size_type numEntries = mDb.Length();
  ProtectedDbArray::index_type entryIndex;
  for (entryIndex = 1; entryIndex < numEntries; entryIndex++) {
    RefPtr<DbEntry> entry = mDb[entryIndex];
    if (entry && entry->mPath.Equals(aPath)) {
      return entryIndex;
    }
  }
  return 0;
}

TemporaryRef<MozMtpDatabase::DbEntry>
MozMtpDatabase::GetEntry(MtpObjectHandle aHandle)
{
  MutexAutoLock lock(mMutex);

  RefPtr<DbEntry> entry;

  if (aHandle > 0 && aHandle < mDb.Length()) {
    entry = mDb[aHandle];
  }
  return entry;
}

void
MozMtpDatabase::RemoveEntry(MtpObjectHandle aHandle)
{
  MutexAutoLock lock(mMutex);
  if (!IsValidHandle(aHandle)) {
    return;
  }

  RefPtr<DbEntry> removedEntry = mDb[aHandle];
  mDb[aHandle] = nullptr;
  MTP_DBG("0x%08x removed", aHandle);
  // if the entry is not a folder, just return.
  if (removedEntry->mObjectFormat != MTP_FORMAT_ASSOCIATION) {
    return;
  }

  // Find out and remove the children of aHandle.
  // Since the index for a directory will always be less than the index of any of its children,
  // we can remove the entire subtree in one pass.
  ProtectedDbArray::size_type numEntries = mDb.Length();
  ProtectedDbArray::index_type entryIndex;
  for (entryIndex = aHandle+1; entryIndex < numEntries; entryIndex++) {
    RefPtr<DbEntry> entry = mDb[entryIndex];
    if (entry && IsValidHandle(entry->mParent) && !mDb[entry->mParent]) {
      mDb[entryIndex] = nullptr;
      MTP_DBG("0x%08x removed", aHandle);
    }
  }
}

void
MozMtpDatabase::RemoveEntryAndNotify(MtpObjectHandle aHandle, RefCountedMtpServer* aMtpServer)
{
  RemoveEntry(aHandle);
  aMtpServer->sendObjectRemoved(aHandle);
}

void
MozMtpDatabase::UpdateEntryAndNotify(MtpObjectHandle aHandle, DeviceStorageFile* aFile, RefCountedMtpServer* aMtpServer)
{
  UpdateEntry(aHandle, aFile);
  aMtpServer->sendObjectAdded(aHandle);
}


void
MozMtpDatabase::UpdateEntry(MtpObjectHandle aHandle, DeviceStorageFile* aFile)
{
  MutexAutoLock lock(mMutex);

  RefPtr<DbEntry> entry = mDb[aHandle];

  int64_t fileSize = 0;
  aFile->mFile->GetFileSize(&fileSize);
  entry->mObjectSize = fileSize;
  aFile->mFile->GetLastModifiedTime(&entry->mDateCreated);
  entry->mDateModified = entry->mDateCreated;
  MTP_DBG("UpdateEntry (0x%08x file %s)", entry->mHandle, entry->mPath.get());
}


class FileWatcherNotifyRunnable MOZ_FINAL : public nsRunnable
{
public:
  FileWatcherNotifyRunnable(nsACString& aStorageName,
                            nsACString& aPath,
                            const char* aEventType)
    : mStorageName(aStorageName),
      mPath(aPath),
      mEventType(aEventType)
  {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    NS_ConvertUTF8toUTF16 storageName(mStorageName);
    NS_ConvertUTF8toUTF16 path(mPath);

    nsRefPtr<DeviceStorageFile> dsf(
      new DeviceStorageFile(NS_LITERAL_STRING(DEVICESTORAGE_SDCARD),
                            storageName, path));
    NS_ConvertUTF8toUTF16 eventType(mEventType);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();

    MTP_DBG("Sending file-watcher-notify %s %s %s",
            mEventType.get(), mStorageName.get(), mPath.get());

    obs->NotifyObservers(dsf, "file-watcher-notify", eventType.get());
    return NS_OK;
  }

private:
  nsCString mStorageName;
  nsCString mPath;
  nsCString mEventType;
};

// FileWatcherNotify is used to tell DeviceStorage when a file was changed
// through the MTP server.
void
MozMtpDatabase::FileWatcherNotify(DbEntry* aEntry, const char* aEventType)
{
  // This function gets called from the MozMtpServer::mServerThread
  MOZ_ASSERT(!NS_IsMainThread());

  MTP_DBG("file: %s %s", aEntry->mPath.get(), aEventType);

  // Tell interested parties that a file was created, deleted, or modified.

  RefPtr<StorageEntry> storageEntry;
  {
    MutexAutoLock lock(mMutex);

    // FindStorage and the mStorage[] access both need to have the mutex held.
    StorageArray::index_type storageIndex = FindStorage(aEntry->mStorageID);
    if (storageIndex == StorageArray::NoIndex) {
      return;
    }
    storageEntry = mStorage[storageIndex];
  }

  // DeviceStorage wants the storageName and the path relative to the root
  // of the storage area, so we need to strip off the storagePath

  nsAutoCString relPath(Substring(aEntry->mPath,
                                  storageEntry->mStoragePath.Length() + 1));

  nsRefPtr<FileWatcherNotifyRunnable> r =
    new FileWatcherNotifyRunnable(storageEntry->mStorageName, relPath, aEventType);
  DebugOnly<nsresult> rv = NS_DispatchToMainThread(r);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

// Called to tell the MTP server about new or deleted files,
void
MozMtpDatabase::FileWatcherUpdate(RefCountedMtpServer* aMtpServer,
                                  DeviceStorageFile* aFile,
                                  const nsACString& aEventType)
{
  // Runs on the FileWatcherUpdate->mIOThread (see MozMtpServer.cpp)
  MOZ_ASSERT(!NS_IsMainThread());

  // Figure out which storage the belongs to (if any)

  if (!aFile->mFile) {
    // No path - don't bother looking.
    return;
  }
  nsString wideFilePath;
  aFile->mFile->GetPath(wideFilePath);
  NS_ConvertUTF16toUTF8 filePath(wideFilePath);

  nsCString evtType(aEventType);
  MTP_LOG("file %s %s", filePath.get(), evtType.get());

  MtpObjectHandle entryHandle = FindEntryByPath(filePath);

  if (aEventType.EqualsLiteral("modified")) {
    // To update the file information to the newest, we remove the entry for
    // the existing file, then re-add the entry for the file.

    if (entryHandle != 0) {
      // Update entry for the file and tell MTP.
      MTP_LOG("About to update handle 0x%08x file %s", entryHandle, filePath.get());
      UpdateEntryAndNotify(entryHandle, aFile, aMtpServer);
    }
    else {
      // Create entry for the file and tell MTP.
      CreateEntryForFileAndNotify(filePath, aFile, aMtpServer);
    }
    return;
  }

  if (aEventType.EqualsLiteral("deleted")) {
    if (entryHandle == 0) {
      // The entry has already been removed. We can't tell MTP.
      return;
    }
    MTP_LOG("About to call sendObjectRemoved Handle 0x%08x file %s", entryHandle, filePath.get());
    RemoveEntryAndNotify(entryHandle, aMtpServer);
    return;
  }
}

nsCString
MozMtpDatabase::BaseName(const nsCString& path)
{
  nsCOMPtr<nsIFile> file;
  NS_NewNativeLocalFile(path, false, getter_AddRefs(file));
  if (file) {
    nsCString leafName;
    file->GetNativeLeafName(leafName);
    return leafName;
  }
  return path;
}

static nsCString
GetPathWithoutFileName(const nsCString& aFullPath)
{
  nsCString path;

  int32_t offset = aFullPath.RFindChar('/');
  if (offset != kNotFound) {
    // The trailing slash will be as part of 'path'
    path = StringHead(aFullPath, offset + 1);
  }

  MTP_LOG("returning '%s'", path.get());

  return path;
}

void
MozMtpDatabase::CreateEntryForFileAndNotify(const nsACString& aPath,
                                            DeviceStorageFile* aFile,
                                            RefCountedMtpServer* aMtpServer)
{
  // Find the StorageID that this path corresponds to.

  nsCString remainder;
  MtpStorageID storageID = FindStorageIDFor(aPath, remainder);
  if (storageID == 0) {
    // The path in question isn't for a storage area we're monitoring.
    nsCString path(aPath);
    return;
  }

  bool exists = false;
  aFile->mFile->Exists(&exists);
  if (!exists) {
    // File doesn't exist, no sense telling MTP about it.
    // This could happen if Device Storage created and deleted a file right
    // away. Since the notifications wind up being async, the file might
    // not exist any more.
    return;
  }

  // Now walk the remaining directories, finding or creating as required.

  MtpObjectHandle parent = MTP_PARENT_ROOT;
  bool doFind = true;
  int32_t offset = aPath.Length() - remainder.Length();
  int32_t slash;

  do {
    nsDependentCSubstring component;
    slash = aPath.FindChar('/', offset);
    if (slash == kNotFound) {
      component.Rebind(aPath, 0, aPath.Length());
    } else {
      component.Rebind(aPath, 0 , slash);
    }
    if (doFind) {
      MtpObjectHandle entryHandle = FindEntryByPath(component);
      if (entryHandle != 0) {
        // We found an entry.
        parent = entryHandle;
        offset = slash + 1 ;
        continue;
      }
    }

    // We've got a directory component that doesn't exist. This means that all
    // further subdirectories won't exist either, so we can skip searching
    // for them.
    doFind = false;

    // This directory and the file don't exist, create them

    RefPtr<DbEntry> entry = new DbEntry;

    entry->mStorageID = storageID;
    entry->mObjectName = Substring(aPath, offset, slash - offset);
    entry->mParent = parent;
    entry->mDisplayName = entry->mObjectName;
    entry->mPath = component;

    if (slash == kNotFound) {
      // No slash - this is the file component
      entry->mObjectFormat = MTP_FORMAT_DEFINED;

      int64_t fileSize = 0;
      aFile->mFile->GetFileSize(&fileSize);
      entry->mObjectSize = fileSize;

      aFile->mFile->GetLastModifiedTime(&entry->mDateCreated);
    } else {
      // Found a slash, this makes this a directory component
      entry->mObjectFormat = MTP_FORMAT_ASSOCIATION;
      entry->mObjectSize = 0;
      entry->mDateCreated = PR_Now();
    }
    entry->mDateModified = entry->mDateCreated;

    AddEntryAndNotify(entry, aMtpServer);
    MTP_LOG("About to call sendObjectAdded Handle 0x%08x file %s", entry->mHandle, entry->mPath.get());

    parent = entry->mHandle;
    offset = slash + 1;
  } while (slash != kNotFound);

  return;
}

void
MozMtpDatabase::AddDirectory(MtpStorageID aStorageID,
                             const char* aPath,
                             MtpObjectHandle aParent)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  ScopedCloseDir dir;

  if (!(dir = PR_OpenDir(aPath))) {
    MTP_ERR("Unable to open directory '%s'", aPath);
    return;
  }

  PRDirEntry* dirEntry;
  while ((dirEntry = PR_ReadDir(dir, PR_SKIP_BOTH))) {
    nsPrintfCString filename("%s/%s", aPath, dirEntry->name);
    PRFileInfo64 fileInfo;
    if (PR_GetFileInfo64(filename.get(), &fileInfo) != PR_SUCCESS) {
      MTP_ERR("Unable to retrieve file information for '%s'", filename.get());
      continue;
    }

    RefPtr<DbEntry> entry = new DbEntry;

    entry->mStorageID = aStorageID;
    entry->mParent = aParent;
    entry->mObjectName = dirEntry->name;
    entry->mDisplayName = dirEntry->name;
    entry->mPath = filename;
    entry->mDateCreated = fileInfo.creationTime;
    entry->mDateModified = fileInfo.modifyTime;

    if (fileInfo.type == PR_FILE_FILE) {
      entry->mObjectFormat = MTP_FORMAT_DEFINED;
      //TODO: Check how 64-bit filesize are dealt with
      entry->mObjectSize = fileInfo.size;
      AddEntry(entry);
    } else if (fileInfo.type == PR_FILE_DIRECTORY) {
      entry->mObjectFormat = MTP_FORMAT_ASSOCIATION;
      entry->mObjectSize = 0;
      AddEntry(entry);
      AddDirectory(aStorageID, filename.get(), entry->mHandle);
    }
  }
}

MozMtpDatabase::StorageArray::index_type
MozMtpDatabase::FindStorage(MtpStorageID aStorageID)
{
  // Currently, this routine is called from MozMtpDatabase::RemoveStorage
  // and MozMtpDatabase::FileWatcherNotify, which both hold mMutex.

  StorageArray::size_type numStorages = mStorage.Length();
  StorageArray::index_type storageIndex;

  for (storageIndex = 0; storageIndex < numStorages; storageIndex++) {
    RefPtr<StorageEntry> storage = mStorage[storageIndex];
    if (storage->mStorageID == aStorageID) {
      return storageIndex;
    }
  }
  return StorageArray::NoIndex;
}

// Find the storage ID for the storage area that contains aPath.
MtpStorageID
MozMtpDatabase::FindStorageIDFor(const nsACString& aPath, nsCSubstring& aRemainder)
{
  MutexAutoLock lock(mMutex);

  aRemainder.Truncate();

  StorageArray::size_type numStorages = mStorage.Length();
  StorageArray::index_type storageIndex;

  for (storageIndex = 0; storageIndex < numStorages; storageIndex++) {
    RefPtr<StorageEntry> storage = mStorage[storageIndex];
    if (StringHead(aPath, storage->mStoragePath.Length()).Equals(storage->mStoragePath)) {
      if (aPath.Length() == storage->mStoragePath.Length()) {
        return storage->mStorageID;
      }
      if (aPath[storage->mStoragePath.Length()] == '/') {
        aRemainder = Substring(aPath, storage->mStoragePath.Length() + 1);
        return storage->mStorageID;
      }
    }
  }
  return 0;
}

void
MozMtpDatabase::AddStorage(MtpStorageID aStorageID,
                           const char* aPath,
                           const char* aName)
{
  // This is called on the IOThread from MozMtpStorage::StorageAvailable
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  MTP_DBG("StorageID: 0x%08x aPath: '%s' aName: '%s'",
          aStorageID, aPath, aName);

  PRFileInfo  fileInfo;
  if (PR_GetFileInfo(aPath, &fileInfo) != PR_SUCCESS) {
    MTP_ERR("'%s' doesn't exist", aPath);
    return;
  }
  if (fileInfo.type != PR_FILE_DIRECTORY) {
    MTP_ERR("'%s' isn't a directory", aPath);
    return;
  }

  nsRefPtr<StorageEntry> storageEntry = new StorageEntry;

  storageEntry->mStorageID = aStorageID;
  storageEntry->mStoragePath = aPath;
  storageEntry->mStorageName = aName;
  {
    MutexAutoLock lock(mMutex);
    mStorage.AppendElement(storageEntry);
  }

  AddDirectory(aStorageID, aPath, MTP_PARENT_ROOT);
  {
    MutexAutoLock lock(mMutex);
    MTP_LOG("added %d items from tree '%s'", mDb.Length(), aPath);
  }
}

void
MozMtpDatabase::RemoveStorage(MtpStorageID aStorageID)
{
  MutexAutoLock lock(mMutex);

  // This is called on the IOThread from MozMtpStorage::StorageAvailable
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  ProtectedDbArray::size_type numEntries = mDb.Length();
  ProtectedDbArray::index_type entryIndex;
  for (entryIndex = 1; entryIndex < numEntries; entryIndex++) {
    RefPtr<DbEntry> entry = mDb[entryIndex];
    if (entry && entry->mStorageID == aStorageID) {
      mDb[entryIndex] = nullptr;
    }
  }
  StorageArray::index_type storageIndex = FindStorage(aStorageID);
  if (storageIndex != StorageArray::NoIndex) {
    mStorage.RemoveElementAt(storageIndex);
  }
}

// called from SendObjectInfo to reserve a database entry for the incoming file
//virtual
MtpObjectHandle
MozMtpDatabase::beginSendObject(const char* aPath,
                                MtpObjectFormat aFormat,
                                MtpObjectHandle aParent,
                                MtpStorageID aStorageID,
                                uint64_t aSize,
                                time_t aModified)
{
  // If MtpServer::doSendObjectInfo receives a request with a parent of
  // MTP_PARENT_ROOT, then it fills in aPath with the fully qualified path
  // and then passes in a parent of zero.

  if (aParent == 0) {
    // Undo what doSendObjectInfo did
    aParent = MTP_PARENT_ROOT;
  }

  RefPtr<DbEntry> entry = new DbEntry;

  entry->mStorageID = aStorageID;
  entry->mParent = aParent;
  entry->mPath = aPath;
  entry->mObjectName = BaseName(entry->mPath);
  entry->mDisplayName = entry->mObjectName;
  entry->mObjectFormat = aFormat;
  entry->mObjectSize = aSize;

  AddEntry(entry);

  MTP_LOG("Handle: 0x%08x Parent: 0x%08x Path: '%s'", entry->mHandle, aParent, aPath);

  mBeginSendObjectCalled = true;
  return entry->mHandle;
}

// called to report success or failure of the SendObject file transfer
// success should signal a notification of the new object's creation,
// failure should remove the database entry created in beginSendObject

//virtual
void
MozMtpDatabase::endSendObject(const char* aPath,
                              MtpObjectHandle aHandle,
                              MtpObjectFormat aFormat,
                              bool aSucceeded)
{
  MTP_LOG("Handle: 0x%08x Path: '%s'", aHandle, aPath);

  if (aSucceeded) {
    RefPtr<DbEntry> entry = GetEntry(aHandle);
    if (entry) {
      FileWatcherNotify(entry, "modified");
    }
  } else {
    RemoveEntry(aHandle);
  }
  mBeginSendObjectCalled = false;
}

//virtual
MtpObjectHandleList*
MozMtpDatabase::getObjectList(MtpStorageID aStorageID,
                              MtpObjectFormat aFormat,
                              MtpObjectHandle aParent)
{
  MTP_LOG("StorageID: 0x%08x Format: 0x%04x Parent: 0x%08x",
          aStorageID, aFormat, aParent);

  // aStorageID == 0xFFFFFFFF for all storage
  // aFormat    == 0          for all formats
  // aParent    == 0xFFFFFFFF for objects with no parents
  // aParent    == 0          for all objects

  //TODO: Optimize

  ScopedDeletePtr<MtpObjectHandleList> list;

  list = new MtpObjectHandleList();

  MutexAutoLock lock(mMutex);

  ProtectedDbArray::size_type numEntries = mDb.Length();
  ProtectedDbArray::index_type entryIndex;
  for (entryIndex = 1; entryIndex < numEntries; entryIndex++) {
    RefPtr<DbEntry> entry = mDb[entryIndex];
    if (entry &&
        (aStorageID == 0xFFFFFFFF || entry->mStorageID == aStorageID) &&
        (aFormat == 0 || entry->mObjectFormat == aFormat) &&
        (aParent == 0 || entry->mParent == aParent)) {
      list->push(entry->mHandle);
    }
  }
  MTP_LOG("  returning %d items", list->size());
  return list.forget();
}

//virtual
int
MozMtpDatabase::getNumObjects(MtpStorageID aStorageID,
                              MtpObjectFormat aFormat,
                              MtpObjectHandle aParent)
{
  MTP_LOG("");

  // aStorageID == 0xFFFFFFFF for all storage
  // aFormat    == 0          for all formats
  // aParent    == 0xFFFFFFFF for objects with no parents
  // aParent    == 0          for all objects

  int count = 0;

  MutexAutoLock lock(mMutex);

  ProtectedDbArray::size_type numEntries = mDb.Length();
  ProtectedDbArray::index_type entryIndex;
  for (entryIndex = 1; entryIndex < numEntries; entryIndex++) {
    RefPtr<DbEntry> entry = mDb[entryIndex];
    if (entry &&
        (aStorageID == 0xFFFFFFFF || entry->mStorageID == aStorageID) &&
        (aFormat == 0 || entry->mObjectFormat == aFormat) &&
        (aParent == 0 || entry->mParent == aParent)) {
      count++;
    }
  }

  MTP_LOG("  returning %d items", count);
  return count;
}

//virtual
MtpObjectFormatList*
MozMtpDatabase::getSupportedPlaybackFormats()
{
  static const uint16_t init_data[] = {MTP_FORMAT_UNDEFINED, MTP_FORMAT_ASSOCIATION,
                                       MTP_FORMAT_TEXT, MTP_FORMAT_HTML, MTP_FORMAT_WAV,
                                       MTP_FORMAT_MP3, MTP_FORMAT_MPEG, MTP_FORMAT_EXIF_JPEG,
                                       MTP_FORMAT_TIFF_EP, MTP_FORMAT_BMP, MTP_FORMAT_GIF,
                                       MTP_FORMAT_PNG, MTP_FORMAT_TIFF, MTP_FORMAT_WMA,
                                       MTP_FORMAT_OGG, MTP_FORMAT_AAC, MTP_FORMAT_MP4_CONTAINER,
                                       MTP_FORMAT_MP2, MTP_FORMAT_3GP_CONTAINER, MTP_FORMAT_FLAC};

  MtpObjectFormatList *list = new MtpObjectFormatList();
  list->appendArray(init_data, MOZ_ARRAY_LENGTH(init_data));

  MTP_LOG("returning Supported Playback Formats");
  return list;
}

//virtual
MtpObjectFormatList*
MozMtpDatabase::getSupportedCaptureFormats()
{
  static const uint16_t init_data[] = {MTP_FORMAT_ASSOCIATION, MTP_FORMAT_PNG};

  MtpObjectFormatList *list = new MtpObjectFormatList();
  list->appendArray(init_data, MOZ_ARRAY_LENGTH(init_data));
  MTP_LOG("returning MTP_FORMAT_ASSOCIATION, MTP_FORMAT_PNG");
  return list;
}

static const MtpObjectProperty sSupportedObjectProperties[] =
{
  MTP_PROPERTY_STORAGE_ID,
  MTP_PROPERTY_OBJECT_FORMAT,
  MTP_PROPERTY_PROTECTION_STATUS,   // UINT16 - always 0
  MTP_PROPERTY_OBJECT_SIZE,
  MTP_PROPERTY_OBJECT_FILE_NAME,    // just the filename - no directory
  MTP_PROPERTY_NAME,
  MTP_PROPERTY_DATE_MODIFIED,
  MTP_PROPERTY_PARENT_OBJECT,
  MTP_PROPERTY_PERSISTENT_UID,
  MTP_PROPERTY_DATE_ADDED,
};

//virtual
MtpObjectPropertyList*
MozMtpDatabase::getSupportedObjectProperties(MtpObjectFormat aFormat)
{
  MTP_LOG("");
  MtpObjectPropertyList *list = new MtpObjectPropertyList();
  list->appendArray(sSupportedObjectProperties,
                    MOZ_ARRAY_LENGTH(sSupportedObjectProperties));
  return list;
}

//virtual
MtpDevicePropertyList*
MozMtpDatabase::getSupportedDeviceProperties()
{
  MTP_LOG("");
  static const uint16_t init_data[] = { MTP_DEVICE_PROPERTY_UNDEFINED };

  MtpDevicePropertyList *list = new MtpDevicePropertyList();
  list->appendArray(init_data, MOZ_ARRAY_LENGTH(init_data));
  return list;
}

//virtual
MtpResponseCode
MozMtpDatabase::getObjectPropertyValue(MtpObjectHandle aHandle,
                                       MtpObjectProperty aProperty,
                                       MtpDataPacket& aPacket)
{
  RefPtr<DbEntry> entry = GetEntry(aHandle);
  if (!entry) {
    MTP_ERR("Invalid Handle: 0x%08x", aHandle);
    return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
  }

  MTP_LOG("Handle: 0x%08x '%s' Property: %s 0x%08x",
          aHandle, entry->mDisplayName.get(), ObjectPropertyAsStr(aProperty), aProperty);

  switch (aProperty)
  {
    case MTP_PROPERTY_STORAGE_ID:     aPacket.putUInt32(entry->mStorageID); break;
    case MTP_PROPERTY_PARENT_OBJECT:  aPacket.putUInt32(entry->mParent); break;
    case MTP_PROPERTY_OBJECT_FORMAT:  aPacket.putUInt16(entry->mObjectFormat); break;
    case MTP_PROPERTY_OBJECT_SIZE:    aPacket.putUInt64(entry->mObjectSize); break;
    case MTP_PROPERTY_DISPLAY_NAME:   aPacket.putString(entry->mDisplayName.get()); break;
    case MTP_PROPERTY_PERSISTENT_UID:
      // the same as aPacket.putUInt128
      aPacket.putUInt64(entry->mHandle);
      aPacket.putUInt64(entry->mStorageID);
      break;
    case MTP_PROPERTY_NAME:           aPacket.putString(entry->mDisplayName.get()); break;

    default:
      MTP_LOG("Invalid Property: 0x%08x", aProperty);
      return MTP_RESPONSE_INVALID_OBJECT_PROP_CODE;
  }

  return MTP_RESPONSE_OK;
}

static int
GetTypeOfObjectProp(MtpObjectProperty aProperty)
{
  struct PropertyTableEntry {
    MtpObjectProperty property;
    int type;
  };

  static const PropertyTableEntry kObjectPropertyTable[] = {
    {MTP_PROPERTY_STORAGE_ID,        MTP_TYPE_UINT32  },
    {MTP_PROPERTY_OBJECT_FORMAT,     MTP_TYPE_UINT16  },
    {MTP_PROPERTY_PROTECTION_STATUS, MTP_TYPE_UINT16  },
    {MTP_PROPERTY_OBJECT_SIZE,       MTP_TYPE_UINT64  },
    {MTP_PROPERTY_OBJECT_FILE_NAME,  MTP_TYPE_STR     },
    {MTP_PROPERTY_DATE_MODIFIED,     MTP_TYPE_STR     },
    {MTP_PROPERTY_PARENT_OBJECT,     MTP_TYPE_UINT32  },
    {MTP_PROPERTY_DISPLAY_NAME,      MTP_TYPE_STR     },
    {MTP_PROPERTY_NAME,              MTP_TYPE_STR     },
    {MTP_PROPERTY_PERSISTENT_UID,    MTP_TYPE_UINT128 },
    {MTP_PROPERTY_DATE_ADDED,        MTP_TYPE_STR     },
  };

  int count = sizeof(kObjectPropertyTable) / sizeof(kObjectPropertyTable[0]);
  const PropertyTableEntry* entryProp = kObjectPropertyTable;
  int type = 0;

  for (int i = 0; i < count; ++i, ++entryProp) {
    if (entryProp->property == aProperty) {
      type = entryProp->type;
      break;
    }
  }

  return type;
}

//virtual
MtpResponseCode
MozMtpDatabase::setObjectPropertyValue(MtpObjectHandle aHandle,
                                       MtpObjectProperty aProperty,
                                       MtpDataPacket& aPacket)
{
  MTP_LOG("Handle: 0x%08x Property: 0x%08x", aHandle, aProperty);

  // Only support file name change
  if (aProperty != MTP_PROPERTY_OBJECT_FILE_NAME) {
    MTP_ERR("property 0x%x not supported", aProperty);
    return  MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED;
  }

  if (GetTypeOfObjectProp(aProperty) != MTP_TYPE_STR) {
    MTP_ERR("property type 0x%x not supported", GetTypeOfObjectProp(aProperty));
    return MTP_RESPONSE_GENERAL_ERROR;
  }

  RefPtr<DbEntry> entry = GetEntry(aHandle);
  if (!entry) {
    MTP_ERR("Invalid Handle: 0x%08x", aHandle);
    return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
  }

  MtpStringBuffer buf;
  aPacket.getString(buf);

  nsDependentCString newFileName(buf);
  nsCString newFileFullPath(GetPathWithoutFileName(entry->mPath) + newFileName);

  if (PR_Rename(entry->mPath.get(), newFileFullPath.get()) != PR_SUCCESS) {
    MTP_ERR("Failed to rename '%s' to '%s'",
            entry->mPath.get(), newFileFullPath.get());
    return MTP_RESPONSE_GENERAL_ERROR;
  }

  MTP_LOG("renamed '%s' to '%s'", entry->mPath.get(), newFileFullPath.get());

  entry->mPath = newFileFullPath;
  entry->mObjectName = BaseName(entry->mPath);
  entry->mDisplayName = entry->mObjectName;

  return MTP_RESPONSE_OK;
}

//virtual
MtpResponseCode
MozMtpDatabase::getDevicePropertyValue(MtpDeviceProperty aProperty,
                                       MtpDataPacket& aPacket)
{
  MTP_LOG("(GENERAL ERROR)");
  return MTP_RESPONSE_GENERAL_ERROR;
}

//virtual
MtpResponseCode
MozMtpDatabase::setDevicePropertyValue(MtpDeviceProperty aProperty,
                                       MtpDataPacket& aPacket)
{
  MTP_LOG("(NOT SUPPORTED)");
  return MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
}

//virtual
MtpResponseCode
MozMtpDatabase::resetDeviceProperty(MtpDeviceProperty aProperty)
{
  MTP_LOG("(NOT SUPPORTED)");
  return MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
}

void
MozMtpDatabase::QueryEntries(MozMtpDatabase::MatchType aMatchType,
                             uint32_t aMatchField1,
                             uint32_t aMatchField2,
                             UnprotectedDbArray &result)
{
  MutexAutoLock lock(mMutex);

  ProtectedDbArray::size_type numEntries = mDb.Length();
  ProtectedDbArray::index_type entryIdx;
  RefPtr<DbEntry> entry;

  result.Clear();

  switch (aMatchType) {

    case MatchAll:
      for (entryIdx = 0; entryIdx < numEntries; entryIdx++) {
        if (mDb[entryIdx]) {
          result.AppendElement(mDb[entryIdx]);
        }
      }
      break;

    case MatchHandle:
      for (entryIdx = 0; entryIdx < numEntries; entryIdx++) {
        entry = mDb[entryIdx];
        if (entry && entry->mHandle == aMatchField1) {
          result.AppendElement(entry);
          // Handles are unique - return the one that we found.
          return;
        }
      }
      break;

    case MatchParent:
      for (entryIdx = 0; entryIdx < numEntries; entryIdx++) {
        entry = mDb[entryIdx];
        if (entry && entry->mParent == aMatchField1) {
          result.AppendElement(entry);
        }
      }
      break;

    case MatchFormat:
      for (entryIdx = 0; entryIdx < numEntries; entryIdx++) {
        entry = mDb[entryIdx];
        if (entry && entry->mObjectFormat == aMatchField1) {
          result.AppendElement(entry);
        }
      }
      break;

    case MatchHandleFormat:
      for (entryIdx = 0; entryIdx < numEntries; entryIdx++) {
        entry = mDb[entryIdx];
        if (entry && entry->mHandle == aMatchField1) {
          if (entry->mObjectFormat == aMatchField2) {
            result.AppendElement(entry);
          }
          // Only 1 entry can match my aHandle. So we can return early.
          return;
        }
      }
      break;

    case MatchParentFormat:
      for (entryIdx = 0; entryIdx < numEntries; entryIdx++) {
        entry = mDb[entryIdx];
        if (entry && entry->mParent == aMatchField1 && entry->mObjectFormat == aMatchField2) {
          result.AppendElement(entry);
        }
      }
      break;

    default:
      MOZ_ASSERT(!"Invalid MatchType");
  }
}

//virtual
MtpResponseCode
MozMtpDatabase::getObjectPropertyList(MtpObjectHandle aHandle,
                                      uint32_t aFormat,
                                      uint32_t aProperty,
                                      int aGroupCode,
                                      int aDepth,
                                      MtpDataPacket& aPacket)
{
  MTP_LOG("Handle: 0x%08x Format: 0x%08x aProperty: 0x%08x aGroupCode: %d aDepth %d",
          aHandle, aFormat, aProperty, aGroupCode, aDepth);

  if (aDepth > 1) {
    return MTP_RESPONSE_SPECIFICATION_BY_DEPTH_UNSUPPORTED;
  }
  if (aGroupCode != 0) {
    return MTP_RESPONSE_SPECIFICATION_BY_GROUP_UNSUPPORTED;
  }

  MatchType matchType = MatchAll;
  uint32_t matchField1 = 0;
  uint32_t matchField2 = 0;

  // aHandle == 0 implies all objects at the root level
  // further specificed by aFormat and/or aDepth

  if (aFormat == 0) {
    if (aHandle == 0xffffffff) {
      // select all objects
      matchType = MatchAll;
    } else {
      if (aDepth == 1) {
        // select objects whose Parent matches aHandle
        matchType = MatchParent;
        matchField1 = aHandle;
      } else {
        // select object whose handle matches aHandle
        matchType = MatchHandle;
        matchField1 = aHandle;
      }
    }
  } else {
    if (aHandle == 0xffffffff) {
      // select all objects whose format matches aFormat
      matchType = MatchFormat;
      matchField1 = aFormat;
    } else {
      if (aDepth == 1) {
        // select objects whose Parent is aHandle and format matches aFormat
        matchType = MatchParentFormat;
        matchField1 = aHandle;
        matchField2 = aFormat;
      } else {
        // select objects whose handle is aHandle and format matches aFormat
        matchType = MatchHandleFormat;
        matchField1 = aHandle;
        matchField2 = aFormat;
      }
    }
  }

  UnprotectedDbArray result;
  QueryEntries(matchType, matchField1, matchField2, result);

  const MtpObjectProperty *objectPropertyList;
  size_t numObjectProperties = 0;
  MtpObjectProperty objectProperty;

  if (aProperty == 0xffffffff) {
    // return all supported properties
    numObjectProperties = MOZ_ARRAY_LENGTH(sSupportedObjectProperties);
    objectPropertyList = sSupportedObjectProperties;
  } else {
    // return property indicated by aProperty
    numObjectProperties = 1;
    objectProperty = aProperty;
    objectPropertyList = &objectProperty;
  }

  UnprotectedDbArray::size_type numEntries = result.Length();
  UnprotectedDbArray::index_type entryIdx;

  aPacket.putUInt32(numObjectProperties * numEntries);
  for (entryIdx = 0; entryIdx < numEntries; entryIdx++) {
    RefPtr<DbEntry> entry = result[entryIdx];

    for (size_t propertyIdx = 0; propertyIdx < numObjectProperties; propertyIdx++) {
      aPacket.putUInt32(entry->mHandle);
      MtpObjectProperty prop = objectPropertyList[propertyIdx];
      aPacket.putUInt16(prop);
      switch (prop) {

        case MTP_PROPERTY_STORAGE_ID:
          aPacket.putUInt16(MTP_TYPE_UINT32);
          aPacket.putUInt32(entry->mStorageID);
          break;

        case MTP_PROPERTY_PARENT_OBJECT:
          aPacket.putUInt16(MTP_TYPE_UINT32);
          aPacket.putUInt32(entry->mParent);
          break;

        case MTP_PROPERTY_PERSISTENT_UID:
          aPacket.putUInt16(MTP_TYPE_UINT128);
          // the same as aPacket.putUInt128
          aPacket.putUInt64(entry->mHandle);
          aPacket.putUInt64(entry->mStorageID);
          break;

        case MTP_PROPERTY_OBJECT_FORMAT:
          aPacket.putUInt16(MTP_TYPE_UINT16);
          aPacket.putUInt16(entry->mObjectFormat);
          break;

        case MTP_PROPERTY_OBJECT_SIZE:
          aPacket.putUInt16(MTP_TYPE_UINT64);
          aPacket.putUInt64(entry->mObjectSize);
          break;

        case MTP_PROPERTY_OBJECT_FILE_NAME:
        case MTP_PROPERTY_NAME:
          aPacket.putUInt16(MTP_TYPE_STR);
          aPacket.putString(entry->mObjectName.get());
          break;

        case MTP_PROPERTY_PROTECTION_STATUS:
          aPacket.putUInt16(MTP_TYPE_UINT16);
          aPacket.putUInt16(0); // 0 = No Protection
          break;

        case MTP_PROPERTY_DATE_MODIFIED: {
          aPacket.putUInt16(MTP_TYPE_STR);
          PRExplodedTime explodedTime;
          PR_ExplodeTime(entry->mDateModified, PR_LocalTimeParameters, &explodedTime);
          char dateStr[20];
          PR_FormatTime(dateStr, sizeof(dateStr), "%Y%m%dT%H%M%S", &explodedTime);
          aPacket.putString(dateStr);
          break;
        }

        case MTP_PROPERTY_DATE_ADDED: {
          aPacket.putUInt16(MTP_TYPE_STR);
          PRExplodedTime explodedTime;
          PR_ExplodeTime(entry->mDateCreated, PR_LocalTimeParameters, &explodedTime);
          char dateStr[20];
          PR_FormatTime(dateStr, sizeof(dateStr), "%Y%m%dT%H%M%S", &explodedTime);
          aPacket.putString(dateStr);
          break;
        }

        default:
          MTP_ERR("Unrecognized property code: %u", prop);
          return MTP_RESPONSE_GENERAL_ERROR;
      }
    }
  }
  return MTP_RESPONSE_OK;
}

//virtual
MtpResponseCode
MozMtpDatabase::getObjectInfo(MtpObjectHandle aHandle,
                              MtpObjectInfo& aInfo)
{
  RefPtr<DbEntry> entry = GetEntry(aHandle);
  if (!entry) {
    MTP_ERR("Handle 0x%08x is invalid", aHandle);
    return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
  }

  MTP_LOG("Handle: 0x%08x Display:'%s' Object:'%s'", aHandle, entry->mDisplayName.get(), entry->mObjectName.get());

  aInfo.mHandle = aHandle;
  aInfo.mStorageID = entry->mStorageID;
  aInfo.mFormat = entry->mObjectFormat;
  aInfo.mProtectionStatus = 0x0;

  if (entry->mObjectSize > 0xFFFFFFFFuLL) {
    aInfo.mCompressedSize = 0xFFFFFFFFuLL;
  } else {
    aInfo.mCompressedSize = entry->mObjectSize;
  }

  aInfo.mThumbFormat = MTP_FORMAT_UNDEFINED;
  aInfo.mThumbCompressedSize = 0;
  aInfo.mThumbPixWidth = 0;
  aInfo.mThumbPixHeight  = 0;
  aInfo.mImagePixWidth = 0;
  aInfo.mImagePixHeight = 0;
  aInfo.mImagePixDepth = 0;
  aInfo.mParent = entry->mParent;
  aInfo.mAssociationType = 0;
  aInfo.mAssociationDesc = 0;
  aInfo.mSequenceNumber = 0;
  aInfo.mName = ::strdup(entry->mObjectName.get());
  aInfo.mDateCreated = entry->mDateCreated;
  aInfo.mDateModified = entry->mDateModified;
  aInfo.mKeywords = ::strdup("fxos,touch");

  return MTP_RESPONSE_OK;
}

//virtual
void*
MozMtpDatabase::getThumbnail(MtpObjectHandle aHandle, size_t& aOutThumbSize)
{
  MTP_LOG("Handle: 0x%08x (returning nullptr)", aHandle);

  aOutThumbSize = 0;

  return nullptr;
}

//virtual
MtpResponseCode
MozMtpDatabase::getObjectFilePath(MtpObjectHandle aHandle,
                                  MtpString& aOutFilePath,
                                  int64_t& aOutFileLength,
                                  MtpObjectFormat& aOutFormat)
{
  RefPtr<DbEntry> entry = GetEntry(aHandle);
  if (!entry) {
    MTP_ERR("Handle 0x%08x is invalid", aHandle);
    return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
  }

  MTP_LOG("Handle: 0x%08x FilePath: '%s'", aHandle, entry->mPath.get());

  aOutFilePath = entry->mPath.get();
  aOutFileLength = entry->mObjectSize;
  aOutFormat = entry->mObjectFormat;

  return MTP_RESPONSE_OK;
}

//virtual
MtpResponseCode
MozMtpDatabase::deleteFile(MtpObjectHandle aHandle)
{
  RefPtr<DbEntry> entry = GetEntry(aHandle);
  if (!entry) {
    MTP_ERR("Invalid Handle: 0x%08x", aHandle);
    return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
  }

  MTP_LOG("Handle: 0x%08x '%s'", aHandle, entry->mPath.get());

  // File deletion will happen in lower level implementation.
  // The only thing we need to do is removing the entry from the db.
  RemoveEntry(aHandle);

  // Tell Device Storage that the file is gone.
  FileWatcherNotify(entry, "deleted");

  return MTP_RESPONSE_OK;
}

#if 0
//virtual
MtpResponseCode
MozMtpDatabase::moveFile(MtpObjectHandle aHandle, MtpObjectHandle aNewParent)
{
  MTP_LOG("Handle: 0x%08x NewParent: 0x%08x", aHandle, aNewParent);

  // change parent

  return MTP_RESPONSE_OK
}

//virtual
MtpResponseCode
MozMtpDatabase::copyFile(MtpObjectHandle aHandle, MtpObjectHandle aNewParent)
{
  MTP_LOG("Handle: 0x%08x NewParent: 0x%08x", aHandle, aNewParent);

  // duplicate DbEntry
  // change parent

  return MTP_RESPONSE_OK
}
#endif

//virtual
MtpObjectHandleList*
MozMtpDatabase::getObjectReferences(MtpObjectHandle aHandle)
{
  MTP_LOG("Handle: 0x%08x (returning nullptr)", aHandle);
  return nullptr;
}

//virtual
MtpResponseCode
MozMtpDatabase::setObjectReferences(MtpObjectHandle aHandle,
                                    MtpObjectHandleList* aReferences)
{
  MTP_LOG("Handle: 0x%08x (NOT SUPPORTED)", aHandle);
  return MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
}

//virtual
MtpProperty*
MozMtpDatabase::getObjectPropertyDesc(MtpObjectProperty aProperty,
                                      MtpObjectFormat aFormat)
{
  MTP_LOG("Property: %s 0x%08x", ObjectPropertyAsStr(aProperty), aProperty);

  MtpProperty* result = nullptr;
  switch (aProperty)
  {
    case MTP_PROPERTY_PROTECTION_STATUS:
      result = new MtpProperty(aProperty, MTP_TYPE_UINT16);
      break;
    case MTP_PROPERTY_OBJECT_FORMAT:
      result = new MtpProperty(aProperty, MTP_TYPE_UINT16, false, aFormat);
      break;
    case MTP_PROPERTY_STORAGE_ID:
    case MTP_PROPERTY_PARENT_OBJECT:
    case MTP_PROPERTY_WIDTH:
    case MTP_PROPERTY_HEIGHT:
    case MTP_PROPERTY_IMAGE_BIT_DEPTH:
      result = new MtpProperty(aProperty, MTP_TYPE_UINT32);
      break;
    case MTP_PROPERTY_OBJECT_SIZE:
      result = new MtpProperty(aProperty, MTP_TYPE_UINT64);
      break;
    case MTP_PROPERTY_DISPLAY_NAME:
    case MTP_PROPERTY_NAME:
      result = new MtpProperty(aProperty, MTP_TYPE_STR);
      break;
    case MTP_PROPERTY_OBJECT_FILE_NAME:
      result = new MtpProperty(aProperty, MTP_TYPE_STR, true);
      break;
    case MTP_PROPERTY_DATE_MODIFIED:
    case MTP_PROPERTY_DATE_ADDED:
      result = new MtpProperty(aProperty, MTP_TYPE_STR);
      result->setFormDateTime();
      break;
    case MTP_PROPERTY_PERSISTENT_UID:
      result = new MtpProperty(aProperty, MTP_TYPE_UINT128);
      break;
    default:
      break;
  }

  return result;
}

//virtual
MtpProperty*
MozMtpDatabase::getDevicePropertyDesc(MtpDeviceProperty aProperty)
{
  MTP_LOG("(returning MTP_DEVICE_PROPERTY_UNDEFINED)");
  return new MtpProperty(MTP_DEVICE_PROPERTY_UNDEFINED, MTP_TYPE_UNDEFINED);
}

//virtual
void
MozMtpDatabase::sessionStarted()
{
  MTP_LOG("");
}

//virtual
void
MozMtpDatabase::sessionEnded()
{
  MTP_LOG("");
}

END_MTP_NAMESPACE
