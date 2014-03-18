/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileRequest.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/FileRequestBinding.h"
#include "nsCxPusher.h"
#include "nsError.h"
#include "nsIDOMProgressEvent.h"
#include "nsDOMClassInfoID.h"
#include "FileHelper.h"
#include "LockedFile.h"

using namespace mozilla;

USING_FILE_NAMESPACE

FileRequest::FileRequest(nsPIDOMWindow* aWindow)
  : DOMRequest(aWindow), mWrapAsDOMRequest(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

FileRequest::~FileRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

// static
already_AddRefed<FileRequest>
FileRequest::Create(nsPIDOMWindow* aOwner, LockedFile* aLockedFile,
                    bool aWrapAsDOMRequest)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FileRequest> request = new FileRequest(aOwner);
  request->mLockedFile = aLockedFile;
  request->mWrapAsDOMRequest = aWrapAsDOMRequest;

  return request.forget();
}

nsresult
FileRequest::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mLockedFile;
  return NS_OK;
}

nsresult
FileRequest::NotifyHelperCompleted(FileHelper* aFileHelper)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = aFileHelper->mResultCode;

  // If the request failed then fire error event and return.
  if (NS_FAILED(rv)) {
    FireError(rv);
    return NS_OK;
  }

  // Otherwise we need to get the result from the helper.
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);

  AutoPushJSContext cx(sc->GetNativeContext());
  NS_ASSERTION(cx, "Failed to get a context!");

  JS::Rooted<JS::Value> result(cx);

  JS::Rooted<JSObject*> global(cx, sc->GetWindowProxy());
  NS_ASSERTION(global, "Failed to get global object!");

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

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(FileRequest, DOMRequest,
                                     mLockedFile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(FileRequest, DOMRequest)
NS_IMPL_RELEASE_INHERITED(FileRequest, DOMRequest)

// virtual
JSObject*
FileRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  if (mWrapAsDOMRequest) {
    return DOMRequest::WrapObject(aCx, aScope);
  }
  return FileRequestBinding::Wrap(aCx, aScope, this);
}

nsIDOMLockedFile*
FileRequest::GetLockedFile() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  return mLockedFile;
}

void
FileRequest::FireProgressEvent(uint64_t aLoaded, uint64_t aTotal)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = NS_NewDOMProgressEvent(getter_AddRefs(event), this,
                                       nullptr, nullptr);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIDOMProgressEvent> progress = do_QueryInterface(event);
  MOZ_ASSERT(progress);
  rv = progress->InitProgressEvent(NS_LITERAL_STRING("progress"), false, false,
                                   false, aLoaded, aTotal);
  if (NS_FAILED(rv)) {
    return;
  }

  DispatchTrustedEvent(event);
}
