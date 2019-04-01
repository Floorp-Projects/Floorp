/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFileRequest.h"

#include "IDBFileHandle.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/IDBFileRequestBinding.h"
#include "mozilla/dom/ProgressEvent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/EventDispatcher.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsLiteralString.h"

namespace mozilla {
namespace dom {

using namespace mozilla::dom::indexedDB;

IDBFileRequest::IDBFileRequest(IDBFileHandle* aFileHandle,
                               bool aWrapAsDOMRequest)
    : DOMRequest(aFileHandle->GetOwnerGlobal()),
      mFileHandle(aFileHandle),
      mWrapAsDOMRequest(aWrapAsDOMRequest),
      mHasEncoding(false) {
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();
}

IDBFileRequest::~IDBFileRequest() { AssertIsOnOwningThread(); }

// static
already_AddRefed<IDBFileRequest> IDBFileRequest::Create(
    IDBFileHandle* aFileHandle, bool aWrapAsDOMRequest) {
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();

  RefPtr<IDBFileRequest> request =
      new IDBFileRequest(aFileHandle, aWrapAsDOMRequest);

  return request.forget();
}

void IDBFileRequest::FireProgressEvent(uint64_t aLoaded, uint64_t aTotal) {
  AssertIsOnOwningThread();

  if (NS_FAILED(CheckCurrentGlobalCorrectness())) {
    return;
  }

  ProgressEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mLengthComputable = false;
  init.mLoaded = aLoaded;
  init.mTotal = aTotal;

  RefPtr<ProgressEvent> event =
      ProgressEvent::Constructor(this, NS_LITERAL_STRING("progress"), init);
  DispatchTrustedEvent(event);
}

void IDBFileRequest::SetResultCallback(ResultCallback* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);

  AutoJSAPI autoJS;
  if (NS_WARN_IF(!autoJS.Init(GetOwnerGlobal()))) {
    FireError(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return;
  }

  JSContext* cx = autoJS.cx();

  JS::Rooted<JS::Value> result(cx);
  nsresult rv = aCallback->GetResult(cx, &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FireError(rv);
  } else {
    FireSuccess(result);
  }
}

NS_IMPL_ADDREF_INHERITED(IDBFileRequest, DOMRequest)
NS_IMPL_RELEASE_INHERITED(IDBFileRequest, DOMRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBFileRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_CYCLE_COLLECTION_INHERITED(IDBFileRequest, DOMRequest, mFileHandle)

void IDBFileRequest::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  AssertIsOnOwningThread();

  aVisitor.mCanHandle = true;
  aVisitor.SetParentTarget(mFileHandle, false);
}

// virtual
JSObject* IDBFileRequest::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  if (mWrapAsDOMRequest) {
    return DOMRequest::WrapObject(aCx, aGivenProto);
  }
  return IDBFileRequest_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
