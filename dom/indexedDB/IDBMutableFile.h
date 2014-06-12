/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbmutablefile_h__
#define mozilla_dom_indexeddb_idbmutablefile_h__

#include "IndexedDatabase.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/indexedDB/FileInfo.h"
#include "mozilla/dom/MutableFile.h"
#include "nsCycleCollectionParticipant.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBDatabase;

class IDBMutableFile : public MutableFile
{
  typedef mozilla::dom::FileHandle FileHandle;

public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBMutableFile, MutableFile)

  static already_AddRefed<IDBMutableFile>
  Create(const nsAString& aName, const nsAString& aType,
         IDBDatabase* aDatabase, already_AddRefed<FileInfo> aFileInfo);


  virtual int64_t
  GetFileId() MOZ_OVERRIDE
  {
    return mFileInfo->Id();
  }

  virtual FileInfo*
  GetFileInfo() MOZ_OVERRIDE
  {
    return mFileInfo;
  }

  virtual bool
  IsShuttingDown() MOZ_OVERRIDE;

  virtual bool
  IsInvalid() MOZ_OVERRIDE;

  virtual nsIOfflineStorage*
  Storage() MOZ_OVERRIDE;

  virtual already_AddRefed<nsISupports>
  CreateStream(nsIFile* aFile, bool aReadOnly) MOZ_OVERRIDE;

  virtual void
  SetThreadLocals() MOZ_OVERRIDE;

  virtual void
  UnsetThreadLocals() MOZ_OVERRIDE;

  virtual already_AddRefed<nsIDOMFile>
  CreateFileObject(FileHandle* aFileHandle, uint32_t aFileSize) MOZ_OVERRIDE;

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  IDBDatabase*
  Database()
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    return mDatabase;
  }

private:
  IDBMutableFile(IDBDatabase* aOwner);

  ~IDBMutableFile()
  {
  }

  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<FileInfo> mFileInfo;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbmutablefile_h__
