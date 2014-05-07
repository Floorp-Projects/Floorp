/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileHandle_h
#define mozilla_dom_FileHandle_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"

class nsIDOMFile;
class nsIFile;
class nsIFileStorage;
class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

class DOMRequest;
class FileService;
class LockedFile;
class FinishHelper;
class FileHelper;

namespace indexedDB {
class FileInfo;
} // namespace indexedDB

/**
 * This class provides a default FileHandle implementation, but it can be also
 * subclassed. The subclass can override implementation of GetFileId,
 * GetFileInfo, CreateStream and CreateFileObject.
 * (for example IDBFileHandle provides IndexedDB specific implementation).
 */
class FileHandle : public DOMEventTargetHelper
{
  friend class FileService;
  friend class LockedFile;
  friend class FinishHelper;
  friend class FileHelper;

public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileHandle, DOMEventTargetHelper)

  static already_AddRefed<FileHandle>
  Create(nsPIDOMWindow* aWindow,
         nsIFileStorage* aFileStorage,
         nsIFile* aFile);

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

  virtual int64_t
  GetFileId()
  {
    return -1;
  }

  virtual mozilla::dom::indexedDB::FileInfo*
  GetFileInfo()
  {
    return nullptr;
  }

  virtual already_AddRefed<nsISupports>
  CreateStream(nsIFile* aFile, bool aReadOnly);

  virtual already_AddRefed<nsIDOMFile>
  CreateFileObject(LockedFile* aLockedFile, uint32_t aFileSize);

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

  already_AddRefed<LockedFile>
  Open(FileMode aMode, ErrorResult& aError);

  already_AddRefed<DOMRequest>
  GetFile(ErrorResult& aError);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)

protected:
  FileHandle(nsPIDOMWindow* aWindow);

  FileHandle(DOMEventTargetHelper* aOwner);

  virtual ~FileHandle();

  nsCOMPtr<nsIFileStorage> mFileStorage;

  nsString mName;
  nsString mType;

  nsCOMPtr<nsIFile> mFile;
  nsString mFileName;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileHandle_h
