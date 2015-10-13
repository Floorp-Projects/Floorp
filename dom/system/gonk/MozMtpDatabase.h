/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_mozmtpdatabase_h__
#define mozilla_system_mozmtpdatabase_h__

#include "MozMtpCommon.h"

#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIThread.h"
#include "nsTArray.h"

class DeviceStorageFile;

BEGIN_MTP_NAMESPACE // mozilla::system::mtp

class RefCountedMtpServer;

using namespace android;

class MozMtpDatabase final : public MtpDatabase
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MozMtpDatabase)

  MozMtpDatabase();

  // called from SendObjectInfo to reserve a database entry for the incoming file
  virtual MtpObjectHandle beginSendObject(const char* aPath,
                                          MtpObjectFormat aFormat,
                                          MtpObjectHandle aParent,
                                          MtpStorageID aStorageID,
                                          uint64_t aSize,
                                          time_t aModified);

  // called to report success or failure of the SendObject file transfer
  // success should signal a notification of the new object's creation,
  // failure should remove the database entry created in beginSendObject
  virtual void endSendObject(const char* aPath,
                             MtpObjectHandle aHandle,
                             MtpObjectFormat aFormat,
                             bool aSucceeded);

  virtual MtpObjectHandleList* getObjectList(MtpStorageID aStorageID,
                                             MtpObjectFormat aFormat,
                                             MtpObjectHandle aParent);

  virtual int getNumObjects(MtpStorageID aStorageID,
                            MtpObjectFormat aFormat,
                            MtpObjectHandle aParent);

  virtual MtpObjectFormatList* getSupportedPlaybackFormats();

  virtual MtpObjectFormatList* getSupportedCaptureFormats();

  virtual MtpObjectPropertyList* getSupportedObjectProperties(MtpObjectFormat aFormat);

  virtual MtpDevicePropertyList* getSupportedDeviceProperties();

  virtual MtpResponseCode getObjectPropertyValue(MtpObjectHandle aHandle,
                                                 MtpObjectProperty aProperty,
                                                 MtpDataPacket& aPacket);

  virtual MtpResponseCode setObjectPropertyValue(MtpObjectHandle aHandle,
                                                 MtpObjectProperty aProperty,
                                                 MtpDataPacket& aPacket);

  virtual MtpResponseCode getDevicePropertyValue(MtpDeviceProperty aProperty,
                                                 MtpDataPacket& aPacket);

  virtual MtpResponseCode setDevicePropertyValue(MtpDeviceProperty aProperty,
                                                 MtpDataPacket& aPacket);

  virtual MtpResponseCode resetDeviceProperty(MtpDeviceProperty aProperty);

  virtual MtpResponseCode getObjectPropertyList(MtpObjectHandle aHandle,
                                                uint32_t aFormat,
                                                uint32_t aProperty,
                                                int aGroupCode,
                                                int aDepth,
                                                MtpDataPacket& aPacket);

  virtual MtpResponseCode getObjectInfo(MtpObjectHandle aHandle,
                                        MtpObjectInfo& aInfo);

  virtual void* getThumbnail(MtpObjectHandle aHandle, size_t& aOutThumbSize);

  virtual MtpResponseCode getObjectFilePath(MtpObjectHandle aHandle,
                                            MtpString& aOutFilePath,
                                            int64_t& aOutFileLength,
                                            MtpObjectFormat& aOutFormat);

  virtual MtpResponseCode deleteFile(MtpObjectHandle aHandle);

  virtual MtpObjectHandleList* getObjectReferences(MtpObjectHandle aHandle);

  virtual MtpResponseCode setObjectReferences(MtpObjectHandle aHandle,
                                              MtpObjectHandleList* aReferences);

  virtual MtpProperty* getObjectPropertyDesc(MtpObjectProperty aProperty,
                                             MtpObjectFormat aFormat);

  virtual MtpProperty* getDevicePropertyDesc(MtpDeviceProperty aProperty);

  virtual void sessionStarted();

  virtual void sessionEnded();

  void AddStorage(MtpStorageID aStorageID, const char* aPath, const char *aName);
  void RemoveStorage(MtpStorageID aStorageID);

  void MtpWatcherUpdate(RefCountedMtpServer* aMtpServer,
                        DeviceStorageFile* aFile,
                        const nsACString& aEventType);

protected:
  virtual ~MozMtpDatabase();

private:

  struct DbEntry final
  {
    DbEntry()
      : mHandle(0),
        mStorageID(0),
        mObjectFormat(MTP_FORMAT_DEFINED),
        mParent(0),
        mObjectSize(0),
        mDateCreated(0),
        mDateModified(0),
        mDateAdded(0) {}

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DbEntry)

    MtpObjectHandle mHandle;        // uint32_t
    MtpStorageID    mStorageID;     // uint32_t
    nsCString       mObjectName;
    MtpObjectFormat mObjectFormat;  // uint16_t
    MtpObjectHandle mParent;        // uint32_t
    uint64_t        mObjectSize;
    nsCString       mDisplayName;
    nsCString       mPath;
    time_t          mDateCreated;
    time_t          mDateModified;
    time_t          mDateAdded;

  protected:
    ~DbEntry() {}
  };

  template<class T>
  class ProtectedTArray : private nsTArray<T>
  {
  public:
    typedef T elem_type;
    typedef typename nsTArray<T>::size_type size_type;
    typedef typename nsTArray<T>::index_type index_type;
    typedef nsTArray<T> base_type;

    static const index_type NoIndex = base_type::NoIndex;

    ProtectedTArray(mozilla::Mutex& aMutex)
      : mMutex(aMutex)
    {}

    size_type Length() const
    {
      // GRR - This assert prints to stderr and won't show up in logcat.
      mMutex.AssertCurrentThreadOwns();
      return base_type::Length();
    }

    template <class Item>
    elem_type* AppendElement(const Item& aItem)
    {
      mMutex.AssertCurrentThreadOwns();
      return base_type::AppendElement(aItem);
    }

    void Clear()
    {
      mMutex.AssertCurrentThreadOwns();
      base_type::Clear();
    }

    void RemoveElementAt(index_type aIndex)
    {
      mMutex.AssertCurrentThreadOwns();
      base_type::RemoveElementAt(aIndex);
    }

    elem_type& operator[](index_type aIndex)
    {
      mMutex.AssertCurrentThreadOwns();
      return base_type::ElementAt(aIndex);
    }

    const elem_type& operator[](index_type aIndex) const
    {
      mMutex.AssertCurrentThreadOwns();
      return base_type::ElementAt(aIndex);
    }

  private:
    mozilla::Mutex& mMutex;
  };
  typedef nsTArray<mozilla::RefPtr<DbEntry> > UnprotectedDbArray;
  typedef ProtectedTArray<mozilla::RefPtr<DbEntry> > ProtectedDbArray;

  struct StorageEntry final
  {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(StorageEntry)

    MtpStorageID  mStorageID;
    nsCString     mStoragePath;
    nsCString     mStorageName;

  protected:
    ~StorageEntry() {}
  };
  typedef ProtectedTArray<mozilla::RefPtr<StorageEntry> > StorageArray;

  enum MatchType
  {
    MatchAll,
    MatchHandle,
    MatchParent,
    MatchFormat,
    MatchHandleFormat,
    MatchParentFormat,
  };

  bool IsValidHandle(MtpObjectHandle aHandle)
  {
    return aHandle > 0 && aHandle < mDb.Length();
  }

  void AddEntry(DbEntry* aEntry);
  void AddEntryAndNotify(DbEntry* aEntr, RefCountedMtpServer* aMtpServer);
  void DumpEntries(const char* aLabel);
  MtpObjectHandle FindEntryByPath(const nsACString& aPath);
  already_AddRefed<DbEntry> GetEntry(MtpObjectHandle aHandle);
  void RemoveEntry(MtpObjectHandle aHandle);
  void RemoveEntryAndNotify(MtpObjectHandle aHandle, RefCountedMtpServer* aMtpServer);
  void UpdateEntry(MtpObjectHandle aHandle, DeviceStorageFile* aFile);
  void UpdateEntryAndNotify(MtpObjectHandle aHandle, DeviceStorageFile* aFile,
                            RefCountedMtpServer* aMtpServer);
  void QueryEntries(MatchType aMatchType, uint32_t aMatchField1,
                    uint32_t aMatchField2, UnprotectedDbArray& aResult);

  nsCString BaseName(const nsCString& aPath);


  MtpObjectHandle GetNextHandle()
  {
    return mDb.Length();
  }

  void AddDirectory(MtpStorageID aStorageID, const char *aPath, MtpObjectHandle aParent);

  void CreateEntryForFileAndNotify(const nsACString& aPath,
                                   DeviceStorageFile* aFile,
                                   RefCountedMtpServer* aMtpServer);

  StorageArray::index_type FindStorage(MtpStorageID aStorageID);
  MtpStorageID FindStorageIDFor(const nsACString& aPath, nsCSubstring& aRemainder);
  void MtpWatcherNotify(DbEntry* aEntry, const char* aEventType);

  // We need a mutex to protext mDb and mStorage. The MTP server runs on a
  // dedicated thread, and it updates/accesses mDb. When files are updated
  // through DeviceStorage, we need to update/access mDb and mStorage as well
  // (from a non-MTP server thread).
  mozilla::Mutex mMutex;
  ProtectedDbArray mDb;
  StorageArray mStorage;

  bool mBeginSendObjectCalled;
};

END_MTP_NAMESPACE

#endif // mozilla_system_mozmtpdatabase_h__
