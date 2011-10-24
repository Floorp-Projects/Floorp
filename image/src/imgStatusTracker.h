/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Drew <joe@drew.ca> (original author)
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

#ifndef imgStatusTracker_h__
#define imgStatusTracker_h__

class imgIContainer;
class imgRequest;
class imgRequestProxy;
class imgStatusNotifyRunnable;
class imgRequestNotifyRunnable;
struct nsIntRect;
namespace mozilla {
namespace imagelib {
class Image;
} // namespace imagelib
} // namespace mozilla


#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "prtypes.h"
#include "nscore.h"

enum {
  stateRequestStarted    = PR_BIT(0),
  stateHasSize           = PR_BIT(1),
  stateDecodeStarted     = PR_BIT(2),
  stateDecodeStopped     = PR_BIT(3),
  stateFrameStopped      = PR_BIT(4),
  stateRequestStopped    = PR_BIT(5)
};

/*
 * The image status tracker is a class that encapsulates all the loading and
 * decoding status about an Image, and makes it possible to send notifications
 * to imgRequestProxys, both synchronously (i.e., the status now) and
 * asynchronously (the status later).
 *
 * When a new proxy needs to be notified of the current state of an image, call
 * the Notify() method on this class with the relevant proxy as its argument,
 * and the notifications will be replayed to the proxy asynchronously.
 */

class imgStatusTracker
{
public:
  // aImage is the image that this status tracker will pass to the
  // imgRequestProxys in SyncNotify() and EmulateRequestFinished(), and must be
  // alive as long as this instance is, because we hold a weak reference to it.
  imgStatusTracker(mozilla::imagelib::Image* aImage);
  imgStatusTracker(const imgStatusTracker& aOther);

  // Image-setter, for imgStatusTrackers created by imgRequest::Init, which
  // are created before their Image is created.  This method should only
  // be called once, and only on an imgStatusTracker that was initialized
  // without an image.
  void SetImage(mozilla::imagelib::Image* aImage);

  // Schedule an asynchronous "replaying" of all the notifications that would
  // have to happen to put us in the current state.
  // We will also take note of any notifications that happen between the time
  // Notify() is called and when we call SyncNotify on |proxy|, and replay them
  // as well.
  void Notify(imgRequest* request, imgRequestProxy* proxy);

  // Schedule an asynchronous "replaying" of all the notifications that would
  // have to happen to put us in the state we are in right now.
  // Unlike Notify(), does *not* take into account future notifications.
  // This is only useful if you do not have an imgRequest, e.g., if you are a
  // static request returned from imgIRequest::GetStaticRequest().
  void NotifyCurrentState(imgRequestProxy* proxy);

  // "Replay" all of the notifications that would have to happen to put us in
  // the state we're currently in.
  // Only use this if you're already servicing an asynchronous call (e.g.
  // OnStartRequest).
  void SyncNotify(imgRequestProxy* proxy);

  // Send all notifications that would be necessary to make |proxy| believe the
  // request is finished downloading and decoding.
  // If aOnlySendStopRequest is true, we will only send OnStopRequest, and then
  // only if that is necessary.
  void EmulateRequestFinished(imgRequestProxy* proxy, nsresult aStatus,
                              bool aOnlySendStopRequest);

  // Returns whether we are in the process of loading; that is, whether we have
  // not received OnStopRequest.
  bool IsLoading() const;

  // Get the current image status (as in imgIRequest).
  PRUint32 GetImageStatus() const;

  // Following are all the notification methods. You must call the Record
  // variant on this status tracker, then call the Send variant for each proxy
  // you want to notify.

  // Call when the request is being cancelled.
  void RecordCancel();

  // Shorthand for recording all the load notifications: StartRequest,
  // StartContainer, StopRequest.
  void RecordLoaded();

  // Shorthand for recording all the decode notifications: StartDecode,
  // StartFrame, DataAvailable, StopFrame, StopContainer, StopDecode.
  void RecordDecoded();

  /* non-virtual imgIDecoderObserver methods */
  void RecordStartDecode();
  void SendStartDecode(imgRequestProxy* aProxy);
  void RecordStartContainer(imgIContainer* aContainer);
  void SendStartContainer(imgRequestProxy* aProxy, imgIContainer* aContainer);
  void RecordStartFrame(PRUint32 aFrame);
  void SendStartFrame(imgRequestProxy* aProxy, PRUint32 aFrame);
  void RecordDataAvailable(bool aCurrentFrame, const nsIntRect* aRect);
  void SendDataAvailable(imgRequestProxy* aProxy, bool aCurrentFrame, const nsIntRect* aRect);
  void RecordStopFrame(PRUint32 aFrame);
  void SendStopFrame(imgRequestProxy* aProxy, PRUint32 aFrame);
  void RecordStopContainer(imgIContainer* aContainer);
  void SendStopContainer(imgRequestProxy* aProxy, imgIContainer* aContainer);
  void RecordStopDecode(nsresult status, const PRUnichar* statusArg);
  void SendStopDecode(imgRequestProxy* aProxy, nsresult aStatus, const PRUnichar* statusArg);
  void RecordDiscard();
  void SendDiscard(imgRequestProxy* aProxy);

  /* non-virtual imgIContainerObserver methods */
  void RecordFrameChanged(imgIContainer* aContainer,
                          const nsIntRect* aDirtyRect);
  void SendFrameChanged(imgRequestProxy* aProxy, imgIContainer* aContainer,
                        const nsIntRect* aDirtyRect);

  /* non-virtual sort-of-nsIRequestObserver methods */
  void RecordStartRequest();
  void SendStartRequest(imgRequestProxy* aProxy);
  void RecordStopRequest(bool aLastPart, nsresult aStatus);
  void SendStopRequest(imgRequestProxy* aProxy, bool aLastPart, nsresult aStatus);

private:
  friend class imgStatusNotifyRunnable;
  friend class imgRequestNotifyRunnable;

  nsCOMPtr<nsIRunnable> mRequestRunnable;

  // A weak pointer to the Image, because it owns us, and we
  // can't create a cycle.
  mozilla::imagelib::Image* mImage;
  PRUint32 mState;
  nsresult mImageStatus;
  bool mHadLastPart;
};

#endif
