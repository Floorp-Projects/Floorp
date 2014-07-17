/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFileRequest.h"

#include "IDBFileHandle.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/FileHelper.h"
#include "mozilla/dom/IDBFileRequestBinding.h"
#include "mozilla/dom/ProgressEvent.h"
#include "mozilla/EventDispatcher.h"
#include "nsCOMPtr.h"
#include "nsCxPusher.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIDOMEvent.h"
#include "nsIScriptContext.h"
#include "nsLiteralString.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

IDBFileRequest::IDBFileRequest(nsPIDOMWindow* aWindow)
  : DOMRequest(aWindow), mWrapAsDOMRequest(false)
{
}

IDBFileRequest::~IDBFileRequest()
{
}

// static
already_AddRefed<IDBFileRequest>
IDBFileRequest::Create(nsPIDOMWindow* aOwner, IDBFileHandle* aFileHandle,
                       bool aWrapAsDOMRequest)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBFileRequest> request = new IDBFileRequest(aOwner);
  request->mFileHandle = aFileHandle;
  request->mWrapAsDOMRequest = aWrapAsDOMRequest;

  return request.forget();
}

nsresult
IDBFileRequest::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mFileHandle;
  return NS_OK;
}

void
IDBFileRequest::OnProgress(uint64_t aProgress, uint64_t aProgressMax)
{
  FireProgressEvent(aProgress, aProgressMax);
}

nsresult
IDBFileRequest::NotifyHelperCompleted(FileHelper* aFileHelper)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = aFileHelper->ResultCode();

  // If the request failed then fire error event and return.
  if (NS_FAILED(rv)) {
    FireError(rv);
    return NS_OK;
  }

  // Otherwise we need to get the result from the helper.
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);

  AutoJSContext cx;
  MOZ_ASSERT(cx, "Failed to get a context!");

  JS::Rooted<JS::Value> result(cx);

  JS::Rooted<JSObject*> global(cx, sc->GetWindowProxy());
  MOZ_ASSERT(global, "Failed to get global object!");

  JSAutoCompartment ac(cx, global);

  rv = aFileHelper->GetSuccessResult(cx, &result);
  if (NS_FAILED(rv)) {
    NS_WARNING("GetSuccessResult failed!");
  }

  if (NS_SUCCEEDED(rv)) {
    FireSuccess(result);
  }
  else {
    FireError(rv);
  }
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(IDBFileRequest, DOMRequest,
                                   mFileHandle)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBFileRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(IDBFileRequest, DOMRequest)
NS_IMPL_RELEASE_INHERITED(IDBFileRequest, DOMRequest)

// virtual
JSObject*
IDBFileRequest::WrapObject(JSContext* aCx)
{
  if (mWrapAsDOMRequest) {
    return DOMRequest::WrapObject(aCx);
  }
  return IDBFileRequestBinding::Wrap(aCx, this);
}


IDBFileHandle*
IDBFileRequest::GetFileHandle() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  return static_cast<IDBFileHandle*>(mFileHandle.get());
}

void
IDBFileRequest::FireProgressEvent(uint64_t aLoaded, uint64_t aTotal)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  ProgressEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mLengthComputable = false;
  init.mLoaded = aLoaded;
  init.mTotal = aTotal;

  nsRefPtr<ProgressEvent> event =
    ProgressEvent::Constructor(this, NS_LITERAL_STRING("progress"), init);
  DispatchTrustedEvent(event);
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
