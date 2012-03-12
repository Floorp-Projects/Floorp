/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozila.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Huey <me@kylehuey.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
