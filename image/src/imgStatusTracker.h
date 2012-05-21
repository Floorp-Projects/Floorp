/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef imgStatusTracker_h__
#define imgStatusTracker_h__

class imgIContainer;
class imgRequest;
class imgRequestProxy;
class imgStatusNotifyRunnable;
class imgRequestNotifyRunnable;
struct nsIntRect;
namespace mozilla {
namespace image {
class Image;
} // namespace image
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
  imgStatusTracker(mozilla::image::Image* aImage);
  imgStatusTracker(const imgStatusTracker& aOther);

  // Image-setter, for imgStatusTrackers created by imgRequest::Init, which
  // are created before their Image is created.  This method should only
  // be called once, and only on an imgStatusTracker that was initialized
  // without an image.
  void SetImage(mozilla::image::Image* aImage);

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
  void RecordImageIsAnimated();
  void SendImageIsAnimated(imgRequestProxy *aProxy);

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
  mozilla::image::Image* mImage;
  PRUint32 mState;
  nsresult mImageStatus;
  bool mHadLastPart;
};

#endif
