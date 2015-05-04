/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileHelper.h"

#include "FileHandle.h"
#include "FileRequest.h"
#include "FileService.h"
#include "js/Value.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "MutableFile.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIRequest.h"

namespace mozilla {
namespace dom {

namespace {

FileHandleBase* gCurrentFileHandle = nullptr;

} // anonymous namespace

FileHelper::FileHelper(FileHandleBase* aFileHandle,
                       FileRequestBase* aFileRequest)
: mMutableFile(aFileHandle->MutableFile()),
  mFileHandle(aFileHandle),
  mFileRequest(aFileRequest),
  mResultCode(NS_OK),
  mFinished(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

FileHelper::~FileHelper()
{
  MOZ_ASSERT(!mMutableFile && !mFileHandle && !mFileRequest && !mListener &&
             !mRequest, "Should have cleared this!");
}

NS_IMPL_ISUPPORTS(FileHelper, nsIRequestObserver)

nsresult
FileHelper::Enqueue()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  FileService* service = FileService::GetOrCreate();
  NS_ENSURE_TRUE(service, NS_ERROR_FAILURE);

  nsresult rv = service->Enqueue(mFileHandle, this);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mFileHandle) {
    mFileHandle->OnNewRequest();
  }

  return NS_OK;
}

nsresult
FileHelper::AsyncRun(FileHelperListener* aListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Assign the listener early, so we can use it synchronously if the code
  // below fails.
  mListener = aListener;

  nsresult rv;

  nsCOMPtr<nsISupports> stream;
  if (mFileHandle->mRequestMode == FileHandleBase::PARALLEL) {
    rv = mFileHandle->CreateParallelStream(getter_AddRefs(stream));
  }
  else {
    rv = mFileHandle->GetOrCreateStream(getter_AddRefs(stream));
  }

  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(stream, "This should never be null!");

    rv = DoAsyncRun(stream);
  }

  if (NS_FAILED(rv)) {
    mResultCode = NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    Finish();
  }

  return NS_OK;
}

NS_IMETHODIMP
FileHelper::OnStartRequest(nsIRequest* aRequest, nsISupports* aCtxt)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return NS_OK;
}

NS_IMETHODIMP
FileHelper::OnStopRequest(nsIRequest* aRequest, nsISupports* aCtxt,
                          nsresult aStatus)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (NS_FAILED(aStatus)) {
    if (aStatus == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      mResultCode = NS_ERROR_DOM_FILEHANDLE_QUOTA_ERR;
    }
    else {
      mResultCode = NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }
  }

  Finish();

  return NS_OK;
}

void
FileHelper::OnStreamProgress(uint64_t aProgress, uint64_t aProgressMax)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mFileHandle->IsAborted()) {
    NS_ASSERTION(mRequest, "Should have a request!\n");

    nsresult rv = mRequest->Cancel(NS_BINDING_ABORTED);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to cancel the request!");
    }

    return;
  }

  if (mFileRequest) {
    mFileRequest->OnProgress(aProgress, aProgressMax);
  }
}

// static
FileHandleBase*
FileHelper::GetCurrentFileHandle()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return gCurrentFileHandle;
}

nsresult
FileHelper::GetSuccessResult(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aVal)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aVal.setUndefined();
  return NS_OK;
}

void
FileHelper::ReleaseObjects()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mMutableFile = nullptr;
  mFileHandle = nullptr;
  mFileRequest = nullptr;
  mListener = nullptr;
  mRequest = nullptr;
}

void
FileHelper::Finish()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mFinished) {
    return;
  }

  mFinished = true;

  if (mFileHandle->IsAborted()) {
    // Always fire a "error" event with ABORT_ERR if the transaction was
    // aborted, even if the request succeeded or failed with another error.
    mResultCode = NS_ERROR_DOM_FILEHANDLE_ABORT_ERR;
  }

  FileHandleBase* oldFileHandle = gCurrentFileHandle;
  gCurrentFileHandle = mFileHandle;

  if (mFileRequest) {
    nsresult rv = mFileRequest->NotifyHelperCompleted(this);
    if (NS_SUCCEEDED(mResultCode) && NS_FAILED(rv)) {
      mResultCode = rv;
    }
  }

  MOZ_ASSERT(gCurrentFileHandle == mFileHandle, "Should be unchanged!");
  gCurrentFileHandle = oldFileHandle;

  mFileHandle->OnRequestFinished();

  mListener->OnFileHelperComplete(this);

  ReleaseObjects();

  MOZ_ASSERT(!(mMutableFile || mFileHandle || mFileRequest || mListener ||
               mRequest), "Subclass didn't call FileHelper::ReleaseObjects!");

}

void
FileHelper::OnStreamClose()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  Finish();
}

void
FileHelper::OnStreamDestroy()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  Finish();
}

} // namespace dom
} // namespace mozilla
