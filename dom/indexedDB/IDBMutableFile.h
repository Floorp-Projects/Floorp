/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbmutablefile_h__
#define mozilla_dom_indexeddb_idbmutablefile_h__

#include "js/TypeDecls.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/dom/indexedDB/FileInfo.h"
#include "mozilla/dom/MutableFile.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

class nsIDOMFile;
class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

class DOMRequest;

namespace indexedDB {

class IDBDatabase;
class IDBFileHandle;

class IDBMutableFile MOZ_FINAL : public DOMEventTargetHelper,
                                 public MutableFileBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBMutableFile, DOMEventTargetHelper)

  static already_AddRefed<IDBMutableFile>
  Create(const nsAString& aName, const nsAString& aType,
         IDBDatabase* aDatabase, already_AddRefed<FileInfo> aFileInfo);

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
  GetFileId() const
  {
    return mFileInfo->Id();
  }

  FileInfo*
  GetFileInfo() const
  {
    return mFileInfo;
  }

  virtual bool
  IsInvalid() MOZ_OVERRIDE;

  virtual nsIOfflineStorage*
  Storage() MOZ_OVERRIDE;

  virtual already_AddRefed<nsISupports>
  CreateStream(bool aReadOnly) MOZ_OVERRIDE;

  virtual void
  SetThreadLocals() MOZ_OVERRIDE;

  virtual void
  UnsetThreadLocals() MOZ_OVERRIDE;

  already_AddRefed<nsIDOMFile>
  CreateFileObject(IDBFileHandle* aFileHandle, uint32_t aFileSize);

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

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
  Database()
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    return mDatabase;
  }

  already_AddRefed<IDBFileHandle>
  Open(FileMode aMode, ErrorResult& aError);

  already_AddRefed<DOMRequest>
  GetFile(ErrorResult& aError);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)

private:
  explicit IDBMutableFile(IDBDatabase* aOwner);
  ~IDBMutableFile();

  nsString mName;
  nsString mType;

  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<FileInfo> mFileInfo;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbmutablefile_h__
