/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbfilehandle_h__
#define mozilla_dom_indexeddb_idbfilehandle_h__

#include "IndexedDatabase.h"

#include "mozilla/dom/file/FileHandle.h"
#include "mozilla/dom/indexedDB/FileInfo.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBDatabase;

class IDBFileHandle : public file::FileHandle
{
  typedef mozilla::dom::file::LockedFile LockedFile;

public:
  static already_AddRefed<IDBFileHandle>
  Create(IDBDatabase* aDatabase, const nsAString& aName,
         const nsAString& aType, already_AddRefed<FileInfo> aFileInfo);


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

  virtual already_AddRefed<nsISupports>
  CreateStream(nsIFile* aFile, bool aReadOnly) MOZ_OVERRIDE;

  virtual already_AddRefed<nsIDOMFile>
  CreateFileObject(LockedFile* aLockedFile, uint32_t aFileSize) MOZ_OVERRIDE;

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  IDBDatabase*
  Database();

private:
  IDBFileHandle(IDBDatabase* aOwner);

  ~IDBFileHandle()
  {
  }

  nsRefPtr<FileInfo> mFileInfo;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbfilehandle_h__
