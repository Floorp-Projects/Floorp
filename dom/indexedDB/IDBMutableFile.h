/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbmutablefile_h__
#define mozilla_dom_idbmutablefile_h__

#include "js/TypeDecls.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/dom/MutableFileBase.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsTHashtable.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class DOMRequest;
class File;
class IDBDatabase;
class IDBFileHandle;

namespace indexedDB {
class BackgroundMutableFileChild;
}

class IDBMutableFile final
  : public DOMEventTargetHelper
  , public MutableFileBase
{
  RefPtr<IDBDatabase> mDatabase;

  nsTHashtable<nsPtrHashKey<IDBFileHandle>> mFileHandles;

  nsString mName;
  nsString mType;

  Atomic<bool> mInvalidated;

public:
  IDBMutableFile(IDBDatabase* aDatabase,
                 indexedDB::BackgroundMutableFileChild* aActor,
                 const nsAString& aName,
                 const nsAString& aType);

  void
  SetLazyData(const nsAString& aName,
              const nsAString& aType)
  {
    mName = aName;
    mType = aType;
  }

  int64_t
  GetFileId() const;

  void
  Invalidate();

  void
  RegisterFileHandle(IDBFileHandle* aFileHandle);

  void
  UnregisterFileHandle(IDBFileHandle* aFileHandle);

  void
  AbortFileHandles();

  // WebIDL
  nsPIDOMWindowInner*
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

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBMutableFile, DOMEventTargetHelper)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // MutableFileBase
  virtual const nsString&
  Name() const override;

  virtual const nsString&
  Type() const override;

  virtual bool
  IsInvalidated() override;

  virtual already_AddRefed<File>
  CreateFileFor(BlobImpl* aBlobImpl,
                FileHandleBase* aFileHandle) override;

private:
  ~IDBMutableFile();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_idbmutablefile_h__
