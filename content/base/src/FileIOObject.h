/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FileIOObject_h__
#define FileIOObject_h__

#include "nsIDOMEventTarget.h"
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
namespace dom {

extern const PRUint64 kUnknownSize;

// A common base class for FileReader and FileSaver

class FileIOObject : public nsDOMEventTargetHelper,
                     public nsIStreamListener,
                     public nsITimerCallback
{
public:
  FileIOObject();

  NS_DECL_ISUPPORTS_INHERITED

  // Common methods
  NS_METHOD Abort();
  NS_METHOD GetReadyState(PRUint16* aReadyState);
  NS_METHOD GetError(nsIDOMDOMError** aError);

  NS_DECL_AND_IMPL_EVENT_HANDLER(abort);
  NS_DECL_AND_IMPL_EVENT_HANDLER(error);
  NS_DECL_AND_IMPL_EVENT_HANDLER(progress);

  NS_DECL_NSITIMERCALLBACK

  NS_DECL_NSISTREAMLISTENER

  NS_DECL_NSIREQUESTOBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileIOObject,
                                           nsDOMEventTargetHelper)

protected:
  // Implemented by the derived class to do whatever it needs to do for abort
  NS_IMETHOD DoAbort(nsAString& aEvent) = 0;
  // for onStartRequest (this has a default impl since FileReader doesn't need
  // special handling
  NS_IMETHOD DoOnStartRequest(nsIRequest *aRequest, nsISupports *aContext);
  // for onStopRequest
  NS_IMETHOD DoOnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                             nsresult aStatus, nsAString& aSuccessEvent,
                             nsAString& aTerminationEvent) = 0;
  // and for onDataAvailable
  NS_IMETHOD DoOnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                               nsIInputStream *aInputStream, PRUint32 aOffset,
                               PRUint32 aCount) = 0;

  void StartProgressEventTimer();
  void ClearProgressEventTimer();
  void DispatchError(nsresult rv, nsAString& finalEvent);
  nsresult DispatchProgressEvent(const nsAString& aType);

  nsCOMPtr<nsITimer> mProgressNotifier;
  bool mProgressEventWasDelayed;
  bool mTimerIsActive;

  nsCOMPtr<nsIDOMDOMError> mError;
  nsCOMPtr<nsIChannel> mChannel;

  PRUint16 mReadyState;

  PRUint64 mTotal;
  PRUint64 mTransferred;
};

} // namespace dom
} // namespace mozilla

#endif
