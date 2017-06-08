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
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"

template <class> struct already_AddRefed;

namespace mozilla {

class EventChainPreVisitor;

namespace dom {

class IDBFileHandle;

class IDBFileRequest final
  : public DOMRequest
{
  RefPtr<IDBFileHandle> mFileHandle;

  nsString mEncoding;

  bool mWrapAsDOMRequest;
  bool mHasEncoding;

public:
  class ResultCallback;

  static already_AddRefed<IDBFileRequest>
  Create(IDBFileHandle* aFileHandle,
         bool aWrapAsDOMRequest);

  void
  SetEncoding(const nsAString& aEncoding)
  {
    mEncoding = aEncoding;
    mHasEncoding = true;
  }

  const nsAString&
  GetEncoding() const
  {
    return mEncoding;
  }

  bool
  HasEncoding() const
  {
    return mHasEncoding;
  }

  void
  FireProgressEvent(uint64_t aLoaded, uint64_t aTotal);

  void
  SetResultCallback(ResultCallback* aCallback);

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

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(IDBFileRequest);
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFileRequest, DOMRequest)

  // nsIDOMEventTarget
  virtual nsresult
  GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  IDBFileRequest(IDBFileHandle* aFileHandle,
                 bool aWrapAsDOMRequest);

  ~IDBFileRequest();
};

class NS_NO_VTABLE IDBFileRequest::ResultCallback
{
public:
  virtual nsresult
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult) = 0;

protected:
  ResultCallback()
  { }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_idbfilerequest_h__
