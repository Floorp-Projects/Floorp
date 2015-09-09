/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbfilehandle_h__
#define mozilla_dom_indexeddb_idbfilehandle_h__

#include "IDBFileRequest.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileHandleBase.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIRunnable.h"
#include "nsWeakReference.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

struct IDBFileMetadataParameters;

namespace indexedDB {

class IDBFileRequest;
class IDBMutableFile;

class IDBFileHandle final
  : public DOMEventTargetHelper
  , public nsIRunnable
  , public FileHandleBase
  , public nsSupportsWeakReference
{
  nsRefPtr<IDBMutableFile> mMutableFile;

public:
  static already_AddRefed<IDBFileHandle>
  Create(IDBMutableFile* aMutableFile,
         FileMode aMode);

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    AssertIsOnOwningThread();
    return GetOwner();
  }

  IDBMutableFile*
  GetMutableFile() const
  {
    AssertIsOnOwningThread();
    return mMutableFile;
  }

  IDBMutableFile*
  GetFileHandle() const
  {
    AssertIsOnOwningThread();
    return GetMutableFile();
  }

  already_AddRefed<IDBFileRequest>
  GetMetadata(const IDBFileMetadataParameters& aParameters, ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  ReadAsArrayBuffer(uint64_t aSize, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return Read(aSize, false, NullString(), aRv).downcast<IDBFileRequest>();
  }

  already_AddRefed<IDBFileRequest>
  ReadAsText(uint64_t aSize, const nsAString& aEncoding, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return Read(aSize, true, aEncoding, aRv).downcast<IDBFileRequest>();
  }

  already_AddRefed<IDBFileRequest>
  Write(const StringOrArrayBufferOrArrayBufferViewOrBlob& aValue,
        ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return WriteOrAppend(aValue, false, aRv).downcast<IDBFileRequest>();
  }

  already_AddRefed<IDBFileRequest>
  Append(const StringOrArrayBufferOrArrayBufferViewOrBlob& aValue,
         ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return WriteOrAppend(aValue, true, aRv).downcast<IDBFileRequest>();
  }

  already_AddRefed<IDBFileRequest>
  Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return FileHandleBase::Truncate(aSize, aRv).downcast<IDBFileRequest>();
  }

  already_AddRefed<IDBFileRequest>
  Flush(ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return FileHandleBase::Flush(aRv).downcast<IDBFileRequest>();
  }

  IMPL_EVENT_HANDLER(complete)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFileHandle, DOMEventTargetHelper)

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) override;

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // FileHandleBase
  virtual MutableFileBase*
  MutableFile() const override;

  virtual void
  HandleCompleteOrAbort(bool aAborted) override;

private:
  IDBFileHandle(FileMode aMode,
                IDBMutableFile* aMutableFile);
  ~IDBFileHandle();

  // FileHandleBase
  virtual bool
  CheckWindow() override;

  virtual already_AddRefed<FileRequestBase>
  GenerateFileRequest() override;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbfilehandle_h__
