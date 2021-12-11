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
#include "mozilla/dom/ScriptSettings.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"

namespace mozilla {

class EventChainPreVisitor;

namespace dom {

class IDBFileHandle;

class IDBFileRequest final : public DOMRequest {
  RefPtr<IDBFileHandle> mFileHandle;

  nsString mEncoding;

  bool mWrapAsDOMRequest;
  bool mHasEncoding;

 public:
  [[nodiscard]] static RefPtr<IDBFileRequest> Create(IDBFileHandle* aFileHandle,
                                                     bool aWrapAsDOMRequest);

  void SetEncoding(const nsAString& aEncoding) {
    mEncoding = aEncoding;
    mHasEncoding = true;
  }

  const nsAString& GetEncoding() const { return mEncoding; }

  bool HasEncoding() const { return mHasEncoding; }

  void FireProgressEvent(uint64_t aLoaded, uint64_t aTotal);

  template <typename ResultCallback>
  void SetResult(const ResultCallback& aCallback) {
    AssertIsOnOwningThread();

    AutoJSAPI autoJS;
    if (NS_WARN_IF(!autoJS.Init(GetOwnerGlobal()))) {
      FireError(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
      return;
    }

    JSContext* cx = autoJS.cx();

    JS::Rooted<JS::Value> result(cx);
    nsresult rv = aCallback(cx, &result);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FireError(rv);
    } else {
      FireSuccess(result);
    }
  }

  // WebIDL
  IDBFileHandle* GetFileHandle() const {
    AssertIsOnOwningThread();
    return mFileHandle;
  }

  IDBFileHandle* GetLockedFile() const {
    AssertIsOnOwningThread();
    return GetFileHandle();
  }

  IMPL_EVENT_HANDLER(progress)

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(IDBFileRequest);
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFileRequest, DOMRequest)

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  IDBFileRequest(IDBFileHandle* aFileHandle, bool aWrapAsDOMRequest);

  ~IDBFileRequest();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbfilerequest_h__
