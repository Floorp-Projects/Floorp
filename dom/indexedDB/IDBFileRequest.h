/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbfilerequest_h__
#define mozilla_dom_indexeddb_idbfilerequest_h__

#include "DOMRequest.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileRequest.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

class nsPIDOMWindow;

namespace mozilla {

class EventChainPreVisitor;

namespace dom {
namespace indexedDB {

class IDBFileHandle;

class IDBFileRequest final : public DOMRequest,
                             public FileRequestBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFileRequest, DOMRequest)

  static already_AddRefed<IDBFileRequest>
  Create(nsPIDOMWindow* aOwner, IDBFileHandle* aFileHandle,
         bool aWrapAsDOMRequest);

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) override;

  // FileRequest
  virtual void
  OnProgress(uint64_t aProgress, uint64_t aProgressMax) override;

  virtual nsresult
  NotifyHelperCompleted(FileHelper* aFileHelper) override;

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  IDBFileHandle*
  GetFileHandle() const;

  IDBFileHandle*
  GetLockedFile() const
  {
    return GetFileHandle();
  }

  IMPL_EVENT_HANDLER(progress)

private:
  explicit IDBFileRequest(nsPIDOMWindow* aWindow);
  ~IDBFileRequest();

  void
  FireProgressEvent(uint64_t aLoaded, uint64_t aTotal);

  nsRefPtr<IDBFileHandle> mFileHandle;

  bool mWrapAsDOMRequest;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbfilerequest_h__
