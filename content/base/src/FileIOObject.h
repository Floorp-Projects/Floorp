/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FileIOObject_h__
#define FileIOObject_h__

#include "nsDOMEventTargetHelper.h"
#include "nsIChannel.h"
#include "nsIFile.h"
#include "nsIDOMFile.h"
#include "nsIStreamListener.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"

#include "mozilla/dom/DOMError.h"

#define NS_PROGRESS_EVENT_INTERVAL 50

namespace mozilla {

class ErrorResult;

namespace dom {

extern const uint64_t kUnknownSize;

// A common base class for FileReader and FileSaver

class FileIOObject : public nsDOMEventTargetHelper,
                     public nsIStreamListener,
                     public nsITimerCallback
{
public:
  FileIOObject();

  NS_DECL_ISUPPORTS_INHERITED

  // Common methods
  void Abort(ErrorResult& aRv);
  uint16_t ReadyState() const
  {
    return mReadyState;
  }
  DOMError* GetError() const
  {
    return mError;
  }

  NS_METHOD GetOnabort(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);
  NS_METHOD SetOnabort(JSContext* aCx, JS::Handle<JS::Value> aValue);
  NS_METHOD GetOnerror(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);
  NS_METHOD SetOnerror(JSContext* aCx, JS::Handle<JS::Value> aValue);
  NS_METHOD GetOnprogress(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);
  NS_METHOD SetOnprogress(JSContext* aCx, JS::Handle<JS::Value> aValue);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(progress)

  NS_DECL_NSITIMERCALLBACK

  NS_DECL_NSISTREAMLISTENER

  NS_DECL_NSIREQUESTOBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileIOObject,
                                           nsDOMEventTargetHelper)

protected:
  // Implemented by the derived class to do whatever it needs to do for abort
  virtual void DoAbort(nsAString& aEvent) = 0;
  // for onStartRequest (this has a default impl since FileReader doesn't need
  // special handling
  NS_IMETHOD DoOnStartRequest(nsIRequest *aRequest, nsISupports *aContext);
  // for onStopRequest
  NS_IMETHOD DoOnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                             nsresult aStatus, nsAString& aSuccessEvent,
                             nsAString& aTerminationEvent) = 0;
  // and for onDataAvailable
  NS_IMETHOD DoOnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                               nsIInputStream *aInputStream, uint64_t aOffset,
                               uint32_t aCount) = 0;

  void StartProgressEventTimer();
  void ClearProgressEventTimer();
  void DispatchError(nsresult rv, nsAString& finalEvent);
  nsresult DispatchProgressEvent(const nsAString& aType);

  nsCOMPtr<nsITimer> mProgressNotifier;
  bool mProgressEventWasDelayed;
  bool mTimerIsActive;

  nsRefPtr<DOMError> mError;
  nsCOMPtr<nsIChannel> mChannel;

  uint16_t mReadyState;

  uint64_t mTotal;
  uint64_t mTransferred;
};

} // namespace dom
} // namespace mozilla

#endif
