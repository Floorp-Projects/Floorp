/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileIOObject.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ProgressEvent.h"
#include "mozilla/EventDispatcher.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "nsIDOMEvent.h"

#define ERROR_STR "error"
#define ABORT_STR "abort"
#define PROGRESS_STR "progress"

namespace mozilla {
namespace dom {

const uint64_t kUnknownSize = uint64_t(-1);

NS_IMPL_ADDREF_INHERITED(FileIOObject, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FileIOObject, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileIOObject)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(FileIOObject)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FileIOObject,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mProgressNotifier)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FileIOObject,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mProgressNotifier)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_EVENT_HANDLER(FileIOObject, abort)
NS_IMPL_EVENT_HANDLER(FileIOObject, error)
NS_IMPL_EVENT_HANDLER(FileIOObject, progress)

FileIOObject::FileIOObject()
  : mProgressEventWasDelayed(false),
    mTimerIsActive(false),
    mReadyState(0),
    mTotal(0), mTransferred(0)
{}

FileIOObject::~FileIOObject()
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
    mError = new DOMError(GetOwner(), NS_LITERAL_STRING("NotFoundError"));
    break;
  case NS_ERROR_FILE_ACCESS_DENIED:
    mError = new DOMError(GetOwner(), NS_LITERAL_STRING("SecurityError"));
    break;
  default:
    mError = new DOMError(GetOwner(), NS_LITERAL_STRING("NotReadableError"));
    break;
  }

  // Dispatch error event to signify load failure
  DispatchProgressEvent(NS_LITERAL_STRING(ERROR_STR));
  DispatchProgressEvent(finalEvent);
}

nsresult
FileIOObject::DispatchProgressEvent(const nsAString& aType)
{
  ProgressEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mLoaded = mTransferred;

  if (mTotal != kUnknownSize) {
    init.mLengthComputable = true;
    init.mTotal = mTotal;
  } else {
    init.mLengthComputable = false;
    init.mTotal = 0;
  }
  nsRefPtr<ProgressEvent> event =
    ProgressEvent::Constructor(this, aType, init);
  event->SetTrusted(true);

  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);
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

// InputStreamCallback
NS_IMETHODIMP
FileIOObject::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  if (mReadyState != 1 || aStream != mAsyncStream) {
    return NS_OK;
  }

  uint64_t aCount;
  nsresult rv = aStream->Available(&aCount);

  if (NS_SUCCEEDED(rv) && aCount) {
    rv = DoReadData(aStream, aCount);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = DoAsyncWait(aStream);
  }

  if (NS_FAILED(rv) || !aCount) {
    if (rv == NS_BASE_STREAM_CLOSED) {
      rv = NS_OK;
    }
    return OnLoadEnd(rv);
  }

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

nsresult
FileIOObject::OnLoadEnd(nsresult aStatus)
{
  // Cancel the progress event timer
  ClearProgressEventTimer();

  // FileIOObject must be in DONE stage after an operation
  mReadyState = 2;

  nsString successEvent, termEvent;
  nsresult rv = DoOnLoadEnd(aStatus, successEvent, termEvent);
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

nsresult
FileIOObject::DoAsyncWait(nsIAsyncInputStream* aStream)
{
  return aStream->AsyncWait(this,
                            /* aFlags*/ 0,
                            /* aRequestedCount */ 0,
                            NS_GetCurrentThread());
}

void
FileIOObject::Abort(ErrorResult& aRv)
{
  if (mReadyState != 1) {
    // XXX The spec doesn't say this
    aRv.Throw(NS_ERROR_DOM_FILE_ABORT_ERR);
    return;
  }

  ClearProgressEventTimer();

  mReadyState = 2; // There are DONE constants on multiple interfaces,
                   // but they all have value 2.
  // XXX The spec doesn't say this
  mError = new DOMError(GetOwner(), NS_LITERAL_STRING("AbortError"));

  nsString finalEvent;
  DoAbort(finalEvent);

  // Dispatch the events
  DispatchProgressEvent(NS_LITERAL_STRING(ABORT_STR));
  DispatchProgressEvent(finalEvent);
}

} // namespace dom
} // namespace mozilla
