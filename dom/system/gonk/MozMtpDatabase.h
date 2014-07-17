/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_mozmtpdatabase_h__
#define mozilla_system_mozmtpdatabase_h__

#include "MozMtpCommon.h"

#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"

BEGIN_MTP_NAMESPACE // mozilla::system::mtp

using namespace android;

class MozMtpDatabase : public MtpDatabase
{
public:
  MozMtpDatabase(const char *aDir);
  virtual ~MozMtpDatabase();

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

private:

  struct DbEntry
  {
    NS_INLINE_DECL_REFCOUNTING(DbEntry)

    MtpObjectHandle mHandle;        // uint32_t
    MtpStorageID    mStorageID;     // uint32_t
    nsCString       mObjectName;
    MtpObjectFormat mObjectFormat;  // uint16_t
    MtpObjectHandle mParent;        // uint32_t
    uint64_t        mObjectSize;
    nsCString       mDisplayName;
    nsCString       mPath;
    PRTime          mDateCreated;
    PRTime          mDateModified;
  };
  typedef nsTArray<mozilla::RefPtr<DbEntry> > DbArray;

  DbArray mDb;

  enum MatchType
  {
    MatchAll,
    MatchHandle,
    MatchParent,
    MatchFormat,
    MatchHandleFormat,
    MatchParentFormat,
  };


  void AddEntry(DbEntry *aEntry);
  mozilla::TemporaryRef<DbEntry> GetEntry(MtpObjectHandle aHandle);
  void RemoveEntry(MtpObjectHandle aHandle);
  void QueryEntries(MatchType aMatchType, uint32_t aMatchField1,
                    uint32_t aMatchField2, DbArray& aResult);

  nsCString BaseName(const nsCString& aPath);


  MtpObjectHandle GetNextHandle()
  {
    return mDb.Length();
  }

  void ParseDirectory(const char *aDir, MtpObjectHandle aParent);
  void ReadVolume(const char *aVolumeName, const char *aDir);
};

END_MTP_NAMESPACE

#endif // mozilla_system_mozmtpdatabase_h__
