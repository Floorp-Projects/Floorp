/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileHelper.h"

#include "nsIFileStorage.h"

#include "nsNetError.h"
#include "nsProxyRelease.h"

#include "FileHandle.h"
#include "FileRequest.h"
#include "FileService.h"

USING_FILE_NAMESPACE

namespace {

LockedFile* gCurrentLockedFile = nsnull;

} // anonymous namespace

FileHelper::FileHelper(LockedFile* aLockedFile,
                       FileRequest* aFileRequest)
: mFileStorage(aLockedFile->mFileHandle->mFileStorage),
  mLockedFile(aLockedFile),
  mFileRequest(aFileRequest),
  mResultCode(NS_OK),
  mFinished(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

FileHelper::~FileHelper()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

NS_IMPL_THREADSAFE_ISUPPORTS1(FileHelper, nsIRequestObserver)

nsresult
FileHelper::Enqueue()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  FileService* service = FileService::GetOrCreate();
  NS_ENSURE_TRUE(service, NS_ERROR_FAILURE);

  nsresult rv = service->Enqueue(mLockedFile, this);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mLockedFile) {
    mLockedFile->OnNewRequest();
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
  if (mLockedFile->mRequestMode == LockedFile::PARALLEL) {
    rv = mLockedFile->CreateParallelStream(getter_AddRefs(stream));
  }
  else {
    rv = mLockedFile->GetOrCreateStream(getter_AddRefs(stream));
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
    mResultCode = NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
  }

  Finish();

  return NS_OK;
}

void
FileHelper::OnStreamProgress(PRUint64 aProgress, PRUint64 aProgressMax)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mLockedFile->IsAborted()) {
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
LockedFile*
FileHelper::GetCurrentLockedFile()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return gCurrentLockedFile;
}

nsresult
FileHelper::GetSuccessResult(JSContext* aCx,
                             jsval* aVal)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aVal = JSVAL_VOID;
  return NS_OK;
}

void
FileHelper::ReleaseObjects()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mFileStorage = nsnull;
  mLockedFile = nsnull;
  mFileRequest = nsnull;
  mListener = nsnull;
  mRequest = nsnull;
}

void
FileHelper::Finish()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mFinished) {
    return;
  }

  mFinished = true;

  if (mLockedFile->IsAborted()) {
    // Always fire a "error" event with ABORT_ERR if the transaction was
    // aborted, even if the request succeeded or failed with another error.
    mResultCode = NS_ERROR_DOM_FILEHANDLE_ABORT_ERR;
  }

  LockedFile* oldLockedFile = gCurrentLockedFile;
  gCurrentLockedFile = mLockedFile;

  if (mFileRequest) {
    nsresult rv = mFileRequest->NotifyHelperCompleted(this);
    if (NS_SUCCEEDED(mResultCode) && NS_FAILED(rv)) {
      mResultCode = rv;
    }
  }

  NS_ASSERTION(gCurrentLockedFile == mLockedFile, "Should be unchanged!");
  gCurrentLockedFile = oldLockedFile;

  mLockedFile->OnRequestFinished();

  mListener->OnFileHelperComplete(this);

  ReleaseObjects();

  NS_ASSERTION(!(mFileStorage || mLockedFile || mFileRequest || mListener ||
                 mRequest), "Subclass didn't call FileHelper::ReleaseObjects!");

}
