/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbmutablefile_h__
#define mozilla_dom_indexeddb_idbmutablefile_h__

#include "js/TypeDecls.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/dom/MutableFile.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

class DOMRequest;
class File;
class MetadataParameters;

namespace indexedDB {

class FileInfo;
class IDBDatabase;
class IDBFileHandle;

class IDBMutableFile final
  : public DOMEventTargetHelper
  , public MutableFileBase
{
  typedef mozilla::dom::MetadataParameters MetadataParameters;
  typedef mozilla::dom::quota::PersistenceType PersistenceType;

  nsString mName;
  nsString mType;

  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<FileInfo> mFileInfo;

  const nsCString mGroup;
  const nsCString mOrigin;
  const PersistenceType mPersistenceType;

  Atomic<bool> mInvalidated;

public:
  static already_AddRefed<IDBMutableFile>
  Create(IDBDatabase* aDatabase,
         const nsAString& aName,
         const nsAString& aType,
         already_AddRefed<FileInfo> aFileInfo);

  const nsAString&
  Name() const
  {
    return mName;
  }

  const nsAString&
  Type() const
  {
    return mType;
  }

  int64_t
  GetFileId() const;

  FileInfo*
  GetFileInfo() const
  {
    return mFileInfo;
  }

  already_AddRefed<File>
  CreateFileObject(IDBFileHandle* aFileHandle,
                   MetadataParameters* aMetadataParams);

  void
  Invalidate();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBMutableFile, DOMEventTargetHelper)

  virtual bool
  IsInvalid() override;

  virtual nsIOfflineStorage*
  Storage() override;

  virtual already_AddRefed<nsISupports>
  CreateStream(bool aReadOnly) override;

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  void
  GetName(nsString& aName) const
  {
    aName = mName;
  }

  void
  GetType(nsString& aType) const
  {
    aType = mType;
  }

  IDBDatabase*
  Database() const;

  already_AddRefed<IDBFileHandle>
  Open(FileMode aMode, ErrorResult& aError);

  already_AddRefed<DOMRequest>
  GetFile(ErrorResult& aError);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)

private:
  IDBMutableFile(IDBDatabase* aDatabase,
                 const nsAString& aName,
                 const nsAString& aType,
                 already_AddRefed<FileInfo> aFileInfo,
                 const nsACString& aGroup,
                 const nsACString& aOrigin,
                 const nsACString& aStorageId,
                 PersistenceType aPersistenceType,
                 already_AddRefed<nsIFile> aFile);

  ~IDBMutableFile();
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbmutablefile_h__
