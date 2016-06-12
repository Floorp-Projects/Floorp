/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbfilerequest_h__
#define mozilla_dom_idbfilerequest_h__

#include "DOMRequest.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileRequestBase.h"
#include "nsCycleCollectionParticipant.h"

template <class> struct already_AddRefed;
class nsPIDOMWindowInner;

namespace mozilla {

class EventChainPreVisitor;

namespace dom {

class IDBFileHandle;

class IDBFileRequest final : public DOMRequest,
                             public FileRequestBase
{
  RefPtr<IDBFileHandle> mFileHandle;

  bool mWrapAsDOMRequest;

public:
  static already_AddRefed<IDBFileRequest>
  Create(nsPIDOMWindowInner* aOwner, IDBFileHandle* aFileHandle,
         bool aWrapAsDOMRequest);

  // WebIDL
  IDBFileHandle*
  GetFileHandle() const
  {
    AssertIsOnOwningThread();
    return mFileHandle;
  }

  IDBFileHandle*
  GetLockedFile() const
  {
    AssertIsOnOwningThread();
    return GetFileHandle();
  }

  IMPL_EVENT_HANDLER(progress)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFileRequest, DOMRequest)

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) override;

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // FileRequestBase
  virtual FileHandleBase*
  FileHandle() const override;

  virtual void
  OnProgress(uint64_t aProgress, uint64_t aProgressMax) override;

  virtual void
  SetResultCallback(ResultCallback* aCallback) override;

  virtual void
  SetError(nsresult aError) override;

private:
  IDBFileRequest(nsPIDOMWindowInner* aWindow,
                 IDBFileHandle* aFileHandle,
                 bool aWrapAsDOMRequest);

  ~IDBFileRequest();

  void
  FireProgressEvent(uint64_t aLoaded, uint64_t aTotal);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_idbfilerequest_h__
