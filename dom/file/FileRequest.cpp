/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileRequest.h"

#include "nsIJSContextStack.h"

#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsDOMError.h"
#include "nsDOMProgressEvent.h"
#include "nsDOMClassInfoID.h"
#include "FileHelper.h"
#include "LockedFile.h"

USING_FILE_NAMESPACE

FileRequest::FileRequest(nsIDOMWindow* aWindow)
: DOMRequest(aWindow), mIsFileRequest(true)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

FileRequest::~FileRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

// static
already_AddRefed<FileRequest>
FileRequest::Create(nsIDOMWindow* aOwner,
                    LockedFile* aLockedFile,
                    bool aIsFileRequest)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FileRequest> request = new FileRequest(aOwner);
  request->mLockedFile = aLockedFile;
  request->mIsFileRequest = aIsFileRequest;

  return request.forget();
}

nsresult
FileRequest::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
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
  jsval result;

  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);

  JSContext* cx = sc->GetNativeContext();
  NS_ASSERTION(cx, "Failed to get a context!");

  JSObject* global = sc->GetNativeGlobal();
  NS_ASSERTION(global, "Failed to get global object!");

  JSAutoRequest ar(cx);
  JSAutoEnterCompartment ac;
  if (ac.enter(cx, global)) {
    rv = aFileHelper->GetSuccessResult(cx, &result);
    if (NS_FAILED(rv)) {
      NS_WARNING("GetSuccessResult failed!");
    }
  }
  else {
    NS_WARNING("Failed to enter correct compartment!");
    rv = NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
  }

  if (NS_SUCCEEDED(rv)) {
    FireSuccess(result);
  }
  else {
    FireError(rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
FileRequest::GetLockedFile(nsIDOMLockedFile** aLockedFile)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIDOMLockedFile> lockedFile(mLockedFile);
  lockedFile.forget(aLockedFile);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(FileRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FileRequest, DOMRequest)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS because
  // nsDOMEventTargetHelper does it for us.
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(progress)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mLockedFile,
                                                       nsIDOMLockedFile)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FileRequest, DOMRequest)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(progress)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLockedFile)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileRequest)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMFileRequest, mIsFileRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(FileRequest, mIsFileRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(DOMRequest, !mIsFileRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(FileRequest, DOMRequest)
NS_IMPL_RELEASE_INHERITED(FileRequest, DOMRequest)

DOMCI_DATA(FileRequest, FileRequest)

NS_IMPL_EVENT_HANDLER(FileRequest, progress)

void
FileRequest::FireProgressEvent(PRUint64 aLoaded, PRUint64 aTotal)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  nsRefPtr<nsDOMProgressEvent> event = new nsDOMProgressEvent(nullptr, nullptr);
  nsresult rv = event->InitProgressEvent(NS_LITERAL_STRING("progress"),
                                         false, false, false, aLoaded, aTotal);
  if (NS_FAILED(rv)) {
    return;
  }

  rv = event->SetTrusted(true);
  if (NS_FAILED(rv)) {
    return;
  }

  bool dummy;
  rv = DispatchEvent(static_cast<nsIDOMProgressEvent*>(event), &dummy);
  if (NS_FAILED(rv)) {
    return;
  }
}

void
FileRequest::RootResultVal()
{
  NS_ASSERTION(!mRooted, "Don't call me if already rooted!");
  nsXPCOMCycleCollectionParticipant *participant;
  CallQueryInterface(this, &participant);
  nsContentUtils::HoldJSObjects(NS_CYCLE_COLLECTION_UPCAST(this, DOMRequest),
                                participant);
  mRooted = true;
}
