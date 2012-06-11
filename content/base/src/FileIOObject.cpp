/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileIOObject.h"
#include "nsDOMFile.h"
#include "nsDOMError.h"
#include "nsIDOMEvent.h"
#include "nsIDOMProgressEvent.h"
#include "nsComponentManagerUtils.h"
#include "nsEventDispatcher.h"

#define ERROR_STR "error"
#define ABORT_STR "abort"
#define PROGRESS_STR "progress"

namespace mozilla {
namespace dom {

const PRUint64 kUnknownSize = PRUint64(-1);

NS_IMPL_ADDREF_INHERITED(FileIOObject, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FileIOObject, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileIOObject)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(FileIOObject)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FileIOObject,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mProgressNotifier)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(abort)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(error)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(progress)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FileIOObject,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mProgressNotifier)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(abort)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(error)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(progress)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

FileIOObject::FileIOObject()
  : mProgressEventWasDelayed(false),
    mTimerIsActive(false),
    mReadyState(0),
    mTotal(0), mTransferred(0)
{}

void
FileIOObject::StartProgressEventTimer()
{
  if (!mProgressNotifier) {
    mProgressNotifier = do_CreateInstance(NS_TIMER_CONTRACTID);
  }
  if (mProgressNotifier) {
    mProgressEventWasDelayed = false;
    mTimerIsActive = true;
    mProgressNotifier->Cancel();
    mProgressNotifier->InitWithCallback(this, NS_PROGRESS_EVENT_INTERVAL,
                                        nsITimer::TYPE_ONE_SHOT);
  }
}

void
FileIOObject::ClearProgressEventTimer()
{
  mProgressEventWasDelayed = false;
  mTimerIsActive = false;
  if (mProgressNotifier) {
    mProgressNotifier->Cancel();
  }
}

void
FileIOObject::DispatchError(nsresult rv, nsAString& finalEvent)
{
  // Set the status attribute, and dispatch the error event
  switch (rv) {
  case NS_ERROR_FILE_NOT_FOUND:
    mError = DOMError::CreateWithName(NS_LITERAL_STRING("NotFoundError"));
    break;
  case NS_ERROR_FILE_ACCESS_DENIED:
    mError = DOMError::CreateWithName(NS_LITERAL_STRING("SecurityError"));
    break;
  default:
    mError = DOMError::CreateWithName(NS_LITERAL_STRING("NotReadableError"));
    break;
  }

  // Dispatch error event to signify load failure
  DispatchProgressEvent(NS_LITERAL_STRING(ERROR_STR));
  DispatchProgressEvent(finalEvent);
}

nsresult
FileIOObject::DispatchProgressEvent(const nsAString& aType)
{
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = nsEventDispatcher::CreateEvent(nsnull, nsnull,
                                               NS_LITERAL_STRING("ProgressEvent"),
                                               getter_AddRefs(event));
  NS_ENSURE_SUCCESS(rv, rv);

  event->SetTrusted(true);
  nsCOMPtr<nsIDOMProgressEvent> progress = do_QueryInterface(event);
  NS_ENSURE_TRUE(progress, NS_ERROR_UNEXPECTED);

  bool known;
  PRUint64 size;
  if (mTotal != kUnknownSize) {
    known = true;
    size = mTotal;
  } else {
    known = false;
    size = 0;
  }
  rv = progress->InitProgressEvent(aType, false, false, known,
                                   mTransferred, size);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchDOMEvent(nsnull, event, nsnull, nsnull);
}

// nsITimerCallback
NS_IMETHODIMP
FileIOObject::Notify(nsITimer* aTimer)
{
  nsresult rv;
  mTimerIsActive = false;

  if (mProgressEventWasDelayed) {
    rv = DispatchProgressEvent(NS_LITERAL_STRING("progress"));
    NS_ENSURE_SUCCESS(rv, rv);

    StartProgressEventTimer();
  }

  return NS_OK;
}

// nsIStreamListener
NS_IMETHODIMP
FileIOObject::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  return DoOnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
FileIOObject::DoOnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP
FileIOObject::OnDataAvailable(nsIRequest *aRequest,
                              nsISupports *aContext,
                              nsIInputStream *aInputStream,
                              PRUint32 aOffset,
                              PRUint32 aCount)
{
  nsresult rv;
  rv = DoOnDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount);
  NS_ENSURE_SUCCESS(rv, rv);

  mTransferred += aCount;

  //Notify the timer is the appropriate timeframe has passed
  if (mTimerIsActive) {
    mProgressEventWasDelayed = true;
  } else {
    rv = DispatchProgressEvent(NS_LITERAL_STRING(PROGRESS_STR));
    NS_ENSURE_SUCCESS(rv, rv);

    StartProgressEventTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP
FileIOObject::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                            nsresult aStatus)
{
  // If we're here as a result of a call from Abort(),
  // simply ignore the request.
  if (aRequest != mChannel)
    return NS_OK;

  // Cancel the progress event timer
  ClearProgressEventTimer();

  // FileIOObject must be in DONE stage after an operation
  mReadyState = 2;

  nsString successEvent, termEvent;
  nsresult rv = DoOnStopRequest(aRequest, aContext, aStatus,
                                successEvent, termEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the status field as appropriate
  if (NS_FAILED(aStatus)) {
    DispatchError(aStatus, termEvent);
    return NS_OK;
  }

  // Dispatch event to signify end of a successful operation
  DispatchProgressEvent(successEvent);
  DispatchProgressEvent(termEvent);

  return NS_OK;
}

NS_IMETHODIMP
FileIOObject::Abort()
{
  if (mReadyState != 1) {
    // XXX The spec doesn't say this
    return NS_ERROR_DOM_FILE_ABORT_ERR;
  }

  ClearProgressEventTimer();

  mReadyState = 2; // There are DONE constants on multiple interfaces,
                   // but they all have value 2.
  // XXX The spec doesn't say this
  mError = DOMError::CreateWithName(NS_LITERAL_STRING("AbortError"));

  nsString finalEvent;
  nsresult rv = DoAbort(finalEvent);

  // Dispatch the events
  DispatchProgressEvent(NS_LITERAL_STRING(ABORT_STR));
  DispatchProgressEvent(finalEvent);

  return rv;
}

NS_IMETHODIMP
FileIOObject::GetReadyState(PRUint16 *aReadyState)
{
  *aReadyState = mReadyState;
  return NS_OK;
}

NS_IMETHODIMP
FileIOObject::GetError(nsIDOMDOMError** aError)
{
  NS_IF_ADDREF(*aError = mError);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
