/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_indexeddatabase_h__
#define mozilla_dom_indexeddb_indexeddatabase_h__

#include "nsIProgrammingLanguage.h"

#include "mozilla/Attributes.h"
#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsDOMError.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

#define BEGIN_INDEXEDDB_NAMESPACE \
  namespace mozilla { namespace dom { namespace indexedDB {

#define END_INDEXEDDB_NAMESPACE \
  } /* namespace indexedDB */ } /* namepsace dom */ } /* namespace mozilla */

#define USING_INDEXEDDB_NAMESPACE \
  using namespace mozilla::dom::indexedDB;

class nsIDOMBlob;
class nsIInputStream;

BEGIN_INDEXEDDB_NAMESPACE

class FileInfo;
class IDBDatabase;
class IDBTransaction;

enum FactoryPrivilege {
  Content,
  Chrome
};

template <class T>
void SwapData(T& aData1, T& aData2)
{
  T temp = aData2;
  aData2 = aData1;
  aData1 = temp;
}

struct StructuredCloneFile
{
  bool operator==(const StructuredCloneFile& aOther) const
  {
    return this->mFile == aOther.mFile &&
           this->mFileInfo == aOther.mFileInfo &&
           this->mInputStream == aOther.mInputStream;
  }

  nsCOMPtr<nsIDOMBlob> mFile;
  nsRefPtr<FileInfo> mFileInfo;
  nsCOMPtr<nsIInputStream> mInputStream;
};

struct SerializedStructuredCloneReadInfo;

struct StructuredCloneReadInfo
{
  // In IndexedDatabaseInlines.h
  inline StructuredCloneReadInfo();

  void Swap(StructuredCloneReadInfo& aCloneReadInfo)
  {
    mCloneBuffer.swap(aCloneReadInfo.mCloneBuffer);
    mFiles.SwapElements(aCloneReadInfo.mFiles);
    SwapData(mDatabase, aCloneReadInfo.mDatabase);
  }

  // In IndexedDatabaseInlines.h
  inline bool
  SetFromSerialized(const SerializedStructuredCloneReadInfo& aOther);

  JSAutoStructuredCloneBuffer mCloneBuffer;
  nsTArray<StructuredCloneFile> mFiles;
  IDBDatabase* mDatabase;
};

struct SerializedStructuredCloneReadInfo
{
  SerializedStructuredCloneReadInfo()
  : data(nullptr), dataLength(0)
  { }

  bool
  operator==(const SerializedStructuredCloneReadInfo& aOther) const
  {
    return this->data == aOther.data &&
           this->dataLength == aOther.dataLength;
  }

  SerializedStructuredCloneReadInfo&
  operator=(const StructuredCloneReadInfo& aOther)
  {
    data = aOther.mCloneBuffer.data();
    dataLength = aOther.mCloneBuffer.nbytes();
    return *this;
  }

  // Make sure to update ipc/SerializationHelpers.h when changing members here!
  uint64_t* data;
  size_t dataLength;
};

struct SerializedStructuredCloneWriteInfo;

struct StructuredCloneWriteInfo
{
  // In IndexedDatabaseInlines.h
  inline StructuredCloneWriteInfo();

  void Swap(StructuredCloneWriteInfo& aCloneWriteInfo)
  {
    mCloneBuffer.swap(aCloneWriteInfo.mCloneBuffer);
    mFiles.SwapElements(aCloneWriteInfo.mFiles);
    SwapData(mTransaction, aCloneWriteInfo.mTransaction);
    SwapData(mOffsetToKeyProp, aCloneWriteInfo.mOffsetToKeyProp);
  }

  bool operator==(const StructuredCloneWriteInfo& aOther) const
  {
    return this->mCloneBuffer.nbytes() == aOther.mCloneBuffer.nbytes() &&
           this->mCloneBuffer.data() == aOther.mCloneBuffer.data() &&
           this->mFiles == aOther.mFiles &&
           this->mTransaction == aOther.mTransaction &&
           this->mOffsetToKeyProp == aOther.mOffsetToKeyProp;
  }

  // In IndexedDatabaseInlines.h
  inline bool
  SetFromSerialized(const SerializedStructuredCloneWriteInfo& aOther);

  JSAutoStructuredCloneBuffer mCloneBuffer;
  nsTArray<StructuredCloneFile> mFiles;
  IDBTransaction* mTransaction;
  PRUint64 mOffsetToKeyProp;
};

struct SerializedStructuredCloneWriteInfo
{
  SerializedStructuredCloneWriteInfo()
  : data(nullptr), dataLength(0), offsetToKeyProp(0)
  { }

  bool
  operator==(const SerializedStructuredCloneWriteInfo& aOther) const
  {
    return this->data == aOther.data &&
           this->dataLength == aOther.dataLength &&
           this->offsetToKeyProp == aOther.offsetToKeyProp;
  }

  SerializedStructuredCloneWriteInfo&
  operator=(const StructuredCloneWriteInfo& aOther)
  {
    data = aOther.mCloneBuffer.data();
    dataLength = aOther.mCloneBuffer.nbytes();
    offsetToKeyProp = aOther.mOffsetToKeyProp;
    return *this;
  }

  // Make sure to update ipc/SerializationHelpers.h when changing members here!
  uint64_t* data;
  size_t dataLength;
  uint64_t offsetToKeyProp;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_indexeddatabase_h__
