/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbfilehandle_h__
#define mozilla_dom_indexeddb_idbfilehandle_h__

#include "IDBFileRequest.h"
#include "js/TypeDecls.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileHandle.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

struct IDBFileMetadataParameters;

namespace indexedDB {

class IDBMutableFile;

class IDBFileHandle MOZ_FINAL : public DOMEventTargetHelper,
                                public nsIRunnable,
                                public FileHandleBase,
                                public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFileHandle, DOMEventTargetHelper)

  static already_AddRefed<IDBFileHandle>
  Create(FileMode aMode,
         RequestMode aRequestMode,
         IDBMutableFile* aMutableFile);

  virtual MutableFileBase*
  MutableFile() const MOZ_OVERRIDE;

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  IDBMutableFile*
  GetMutableFile() const
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    return mMutableFile;
  }

  IDBMutableFile*
  GetFileHandle() const
  {
    return GetMutableFile();
  }

  already_AddRefed<IDBFileRequest>
  GetMetadata(const IDBFileMetadataParameters& aParameters, ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  ReadAsArrayBuffer(uint64_t aSize, ErrorResult& aRv)
  {
    return Read(aSize, false, NullString(), aRv).downcast<IDBFileRequest>();
  }

  already_AddRefed<IDBFileRequest>
  ReadAsText(uint64_t aSize, const nsAString& aEncoding, ErrorResult& aRv)
  {
    return Read(aSize, true, aEncoding, aRv).downcast<IDBFileRequest>();
  }

  template<class T>
  already_AddRefed<IDBFileRequest>
  Write(const T& aValue, ErrorResult& aRv)
  {
    return
      WriteOrAppend(aValue, false, aRv).template downcast<IDBFileRequest>();
  }

  template<class T>
  already_AddRefed<IDBFileRequest>
  Append(const T& aValue, ErrorResult& aRv)
  {
    return WriteOrAppend(aValue, true, aRv).template downcast<IDBFileRequest>();
  }

  already_AddRefed<IDBFileRequest>
  Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv)
  {
    return FileHandleBase::Truncate(aSize, aRv).downcast<IDBFileRequest>();
  }

  already_AddRefed<IDBFileRequest>
  Flush(ErrorResult& aRv)
  {
    return FileHandleBase::Flush(aRv).downcast<IDBFileRequest>();
  }

  IMPL_EVENT_HANDLER(complete)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)

private:
  IDBFileHandle(FileMode aMode,
                RequestMode aRequestMode,
                IDBMutableFile* aMutableFile);
  ~IDBFileHandle();

  virtual nsresult
  OnCompleteOrAbort(bool aAborted) MOZ_OVERRIDE;

  virtual bool
  CheckWindow() MOZ_OVERRIDE;

  virtual already_AddRefed<FileRequestBase>
  GenerateFileRequest() MOZ_OVERRIDE;

  nsRefPtr<IDBMutableFile> mMutableFile;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbfilehandle_h__
