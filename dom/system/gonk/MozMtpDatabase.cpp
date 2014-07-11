/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozMtpDatabase.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Scoped.h"
#include "nsIFile.h"
#include "nsPrintfCString.h"
#include "prio.h"

#include <dirent.h>
#include <libgen.h>

using namespace android;
using namespace mozilla;

namespace mozilla {
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCloseDir, PRDir, PR_CloseDir)
}

BEGIN_MTP_NAMESPACE

static const char *
ObjectPropertyAsStr(MtpObjectProperty aProperty)
{
  switch (aProperty) {
    case MTP_PROPERTY_STORAGE_ID:       return "MTP_PROPERTY_STORAGE_ID";
    case MTP_PROPERTY_OBJECT_FORMAT:    return "MTP_PROPERTY_OBJECT_FORMAT";
    case MTP_PROPERTY_OBJECT_SIZE:      return "MTP_PROPERTY_OBJECT_SIZE";
    case MTP_PROPERTY_WIDTH:            return "MTP_PROPERTY_WIDTH";
    case MTP_PROPERTY_HEIGHT:           return "MTP_PROPERTY_HEIGHT";
    case MTP_PROPERTY_IMAGE_BIT_DEPTH:  return "MTP_PROPERTY_IMAGE_BIT_DEPTH";
    case MTP_PROPERTY_DISPLAY_NAME:     return "MTP_PROPERTY_DISPLAY_NAME";
  }
  return "MTP_PROPERTY_???";
}

MozMtpDatabase::MozMtpDatabase(const char *aDir)
{
  MTP_LOG("");

  // We use the index into the array as the handle. Since zero isn't a valid
  // index, we stick a dummy entry there.

  RefPtr<DbEntry> dummy;

  mDb.AppendElement(dummy);

  ReadVolume("sdcard", aDir);
}

//virtual
MozMtpDatabase::~MozMtpDatabase()
{
  MTP_LOG("");
}

void
MozMtpDatabase::AddEntry(DbEntry *entry)
{
  entry->mHandle = GetNextHandle();
  MOZ_ASSERT(mDb.Length() == entry->mHandle);
  mDb.AppendElement(entry);

  MTP_LOG("AddEntry: Handle: 0x%08x Parent: 0x%08x Path:'%s'",
          entry->mHandle, entry->mParent, entry->mPath.get());
}

TemporaryRef<MozMtpDatabase::DbEntry>
MozMtpDatabase::GetEntry(MtpObjectHandle aHandle)
{
  RefPtr<DbEntry> entry;

  if (aHandle > 0 && aHandle < mDb.Length()) {
    entry = mDb[aHandle];
  }
  return entry;
}

void
MozMtpDatabase::RemoveEntry(MtpObjectHandle aHandle)
{
  if (aHandle > 0 && aHandle < mDb.Length()) {
    mDb[aHandle] = nullptr;
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

void
MozMtpDatabase::ParseDirectory(const char *aDir, MtpObjectHandle aParent)
{
  ScopedCloseDir dir;

  if (!(dir = PR_OpenDir(aDir))) {
    MTP_ERR("Unable to open directory '%s'", aDir);
    return;
  }

  PRDirEntry* dirEntry;
  while ((dirEntry = PR_ReadDir(dir, PR_SKIP_BOTH))) {
    nsPrintfCString filename("%s/%s", aDir, dirEntry->name);
    PRFileInfo64 fileInfo;
    if (PR_GetFileInfo64(filename.get(), &fileInfo) != PR_SUCCESS) {
      MTP_ERR("Unable to retrieve file information for '%s'", filename.get());
      continue;
    }

    RefPtr<DbEntry> entry = new DbEntry;

    entry->mStorageID = MTP_STORAGE_FIXED_RAM;
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
      ParseDirectory(filename.get(), entry->mHandle);
    }
  }
}

void
MozMtpDatabase::ReadVolume(const char *volumeName, const char *aDir)
{
  //TODO: Add an assert re thread being run on

  PRFileInfo  fileInfo;

  if (PR_GetFileInfo(aDir, &fileInfo) != PR_SUCCESS) {
    MTP_ERR("'%s' doesn't exist", aDir);
    return;
  }
  if (fileInfo.type != PR_FILE_DIRECTORY) {
    MTP_ERR("'%s' isn't a directory", aDir);
    return;
  }

  RefPtr<DbEntry> entry = new DbEntry;

  entry->mStorageID = MTP_STORAGE_FIXED_RAM;
  entry->mParent = MTP_PARENT_ROOT;
  entry->mObjectName = volumeName;
  entry->mDisplayName = volumeName;
  entry->mPath = aDir;
  entry->mObjectFormat = MTP_FORMAT_ASSOCIATION;
  entry->mObjectSize = 0;

  AddEntry(entry);

  ParseDirectory(aDir, entry->mHandle);
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
  if (!aParent) {
    MTP_LOG("aParent is NULL");
    return kInvalidObjectHandle;
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
                            bool succeeded)
{
  MTP_LOG("Handle: 0x%08x Path: '%s'", aHandle, aPath);
  if (!succeeded) {
    RemoveEntry(aHandle);
  }
}

//virtual
MtpObjectHandleList*
MozMtpDatabase::getObjectList(MtpStorageID aStorageID,
                            MtpObjectFormat aFormat,
                            MtpObjectHandle aParent)
{
  MTP_LOG("StorageID: 0x%08x Format: 0x%04x Parent: 0x%08x",
          aStorageID, aFormat, aParent);

  //TODO: Optimize

  ScopedDeletePtr<MtpObjectHandleList> list;

  list = new MtpObjectHandleList();

  DbArray::size_type numEntries = mDb.Length();
  DbArray::index_type entryIndex;
  for (entryIndex = 1; entryIndex < numEntries; entryIndex++) {
    RefPtr<DbEntry> entry = mDb[entryIndex];
    if (entry->mParent == aParent) {
      list->push(entry->mHandle);
    }
  }
  return list.forget();
}

//virtual
int
MozMtpDatabase::getNumObjects(MtpStorageID aStorageID,
                            MtpObjectFormat aFormat,
                            MtpObjectHandle aParent)
{
  MTP_LOG("");

  return mDb.Length() - 1;
}

//virtual
MtpObjectFormatList*
MozMtpDatabase::getSupportedPlaybackFormats()
{
  static const uint16_t init_data[] = {MTP_FORMAT_PNG};

  MtpObjectFormatList *list = new MtpObjectFormatList();
  list->appendArray(init_data, MOZ_ARRAY_LENGTH(init_data));

  MTP_LOG("returning MTP_FORMAT_PNG");
  return list;
}

//virtual
MtpObjectFormatList*
MozMtpDatabase::getSupportedCaptureFormats()
{
  static const uint16_t init_data[] = {MTP_FORMAT_ASSOCIATION, MTP_FORMAT_PNG};

  MtpObjectFormatList *list = new MtpObjectFormatList();
  list->appendArray(init_data, MOZ_ARRAY_LENGTH(init_data));
  MTP_LOG("returning MTP_FORMAT_PNG");
  return list;
}

static const MtpObjectProperty sSupportedObjectProperties[] =
{
  MTP_PROPERTY_STORAGE_ID,
  MTP_PROPERTY_PARENT_OBJECT,
  MTP_PROPERTY_OBJECT_FORMAT,
  MTP_PROPERTY_OBJECT_SIZE,
  MTP_PROPERTY_OBJECT_FILE_NAME,    // just the filename - no directory
  MTP_PROPERTY_PROTECTION_STATUS,   // UINT16 - always 0
  MTP_PROPERTY_DATE_MODIFIED,
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
    case MTP_PROPERTY_OBJECT_FORMAT:  aPacket.putUInt32(entry->mObjectFormat); break;
    case MTP_PROPERTY_OBJECT_SIZE:    aPacket.putUInt32(entry->mObjectSize); break;
    case MTP_PROPERTY_DISPLAY_NAME:   aPacket.putString(entry->mDisplayName.get()); break;

    default:
      MTP_LOG("Invalid Property: 0x%08x", aProperty);
      return MTP_RESPONSE_INVALID_OBJECT_PROP_CODE;
  }

  return MTP_RESPONSE_OK;
}

//virtual
MtpResponseCode
MozMtpDatabase::setObjectPropertyValue(MtpObjectHandle aHandle,
                                     MtpObjectProperty aProperty,
                                     MtpDataPacket& aPacket)
{
  MTP_LOG("Handle: 0x%08x (NOT SUPPORTED)", aHandle);
  return MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
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
                             DbArray &result)
{
  DbArray::size_type numEntries = mDb.Length();
  DbArray::index_type entryIdx;
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
  MTP_LOG("Handle: 0x%08x Format: 0x%08x aProperty: 0x%08x aGroupCode: %d aDepth %d (NOT SUPPORTED)",
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

  DbArray result;
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

  DbArray::size_type numEntries = result.Length();
  DbArray::index_type entryIdx;

  aPacket.putUInt32(numEntries);
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

        case MTP_PROPERTY_OBJECT_FORMAT:
          aPacket.putUInt16(MTP_TYPE_UINT16);
          aPacket.putUInt16(entry->mObjectFormat);
          break;

        case MTP_PROPERTY_OBJECT_SIZE:
          aPacket.putUInt16(MTP_TYPE_UINT64);
          aPacket.putUInt64(entry->mObjectSize);
          break;

        case MTP_PROPERTY_OBJECT_FILE_NAME:
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
  aInfo.mCompressedSize = 0;
  aInfo.mThumbFormat = entry->mObjectFormat;
  aInfo.mThumbCompressedSize = 20*20*4;
  aInfo.mThumbPixWidth = 20;
  aInfo.mThumbPixHeight  =20;
  aInfo.mImagePixWidth = 20;
  aInfo.mImagePixHeight = 20;
  aInfo.mImagePixDepth = 4;
  aInfo.mParent = entry->mParent;
  aInfo.mAssociationType = 0;
  aInfo.mAssociationDesc = 0;
  aInfo.mSequenceNumber = 0;
  aInfo.mName = ::strdup(entry->mObjectName.get());
  aInfo.mDateCreated = 0;
  aInfo.mDateModified = 0;
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
  MTP_LOG("Handle: 0x%08x (NOT SUPPORTED)", aHandle);

  //TODO

  return MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
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

  // TODO: Perhaps Filesize should be 64-bit?

  MtpProperty* result = nullptr;
  switch (aProperty)
  {
    case MTP_PROPERTY_STORAGE_ID:       result = new MtpProperty(aProperty, MTP_TYPE_UINT32); break;
    case MTP_PROPERTY_OBJECT_FORMAT:    result = new MtpProperty(aProperty, MTP_TYPE_UINT32); break;
    case MTP_PROPERTY_OBJECT_SIZE:      result = new MtpProperty(aProperty, MTP_TYPE_UINT32); break;
    case MTP_PROPERTY_WIDTH:            result = new MtpProperty(aProperty, MTP_TYPE_UINT32); break;
    case MTP_PROPERTY_HEIGHT:           result = new MtpProperty(aProperty, MTP_TYPE_UINT32); break;
    case MTP_PROPERTY_IMAGE_BIT_DEPTH:  result = new MtpProperty(aProperty, MTP_TYPE_UINT32); break;
    case MTP_PROPERTY_DISPLAY_NAME:     result = new MtpProperty(aProperty, MTP_TYPE_STR); break;
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
