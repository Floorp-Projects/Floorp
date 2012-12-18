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
class imgStatusTrackerObserver;
struct nsIntRect;
namespace mozilla {
namespace image {
class Image;
} // namespace image
} // namespace mozilla


#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsTObserverArray.h"
#include "nsIRunnable.h"
#include "nscore.h"
#include "imgDecoderObserver.h"

enum {
  stateRequestStarted    = 1u << 0,
  stateHasSize           = 1u << 1,
  stateDecodeStopped     = 1u << 3,
  stateFrameStopped      = 1u << 4,
  stateRequestStopped    = 1u << 5,
  stateBlockingOnload    = 1u << 6
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
  imgStatusTracker(mozilla::image::Image* aImage, imgRequest* aRequest);
  imgStatusTracker(const imgStatusTracker& aOther);
  ~imgStatusTracker();

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

  // Send some notifications that would be necessary to make |proxy| believe
  // the request is finished downloading and decoding.  We only send
  // OnStopRequest and UnblockOnload, and only if necessary.
  void EmulateRequestFinished(imgRequestProxy* proxy, nsresult aStatus);

  // We manage a set of consumers that are using an image and thus concerned
  // with its status. Weak pointers.
  void AddConsumer(imgRequestProxy* aConsumer);
  bool RemoveConsumer(imgRequestProxy* aConsumer, nsresult aStatus);
  size_t ConsumerCount() const { return mConsumers.Length(); }

  // This is intentionally non-general because its sole purpose is to support an
  // some obscure network priority logic in imgRequest. That stuff could probably
  // be improved, but it's too scary to mess with at the moment.
  bool FirstConsumerIs(imgRequestProxy* aConsumer) {
    return mConsumers.SafeElementAt(0, nullptr) == aConsumer;
  }

  void AdoptConsumers(imgStatusTracker* aTracker) { mConsumers = aTracker->mConsumers; }

  // Returns whether we are in the process of loading; that is, whether we have
  // not received OnStopRequest.
  bool IsLoading() const;

  // Get the current image status (as in imgIRequest).
  uint32_t GetImageStatus() const;

  // Following are all the notification methods. You must call the Record
  // variant on this status tracker, then call the Send variant for each proxy
  // you want to notify.

  // Call when the request is being cancelled.
  void RecordCancel();

  // Shorthand for recording all the load notifications: StartRequest,
  // StartContainer, StopRequest.
  void RecordLoaded();

  // Shorthand for recording all the decode notifications: StartDecode,
  // StartFrame, DataAvailable, StopFrame, StopDecode.
  void RecordDecoded();

  /* non-virtual imgDecoderObserver methods */
  void RecordStartContainer(imgIContainer* aContainer);
  void SendStartContainer(imgRequestProxy* aProxy);
  void RecordDataAvailable();
  void SendDataAvailable(imgRequestProxy* aProxy, const nsIntRect* aRect);
  void RecordFrameChanged(const nsIntRect* aDirtyRect);
  void SendFrameChanged(imgRequestProxy* aProxy, const nsIntRect* aDirtyRect);
  void RecordStopFrame();
  void SendStopFrame(imgRequestProxy* aProxy);
  void RecordStopDecode(nsresult statusg);
  void SendStopDecode(imgRequestProxy* aProxy, nsresult aStatus);
  void RecordDiscard();
  void SendDiscard(imgRequestProxy* aProxy);
  void RecordImageIsAnimated();
  void SendImageIsAnimated(imgRequestProxy *aProxy);

  /* non-virtual sort-of-nsIRequestObserver methods */
  void RecordStartRequest();
  void SendStartRequest(imgRequestProxy* aProxy);
  void RecordStopRequest(bool aLastPart, nsresult aStatus);
  void SendStopRequest(imgRequestProxy* aProxy, bool aLastPart, nsresult aStatus);

  void OnStartRequest();
  void OnDataAvailable();
  void OnStopRequest(bool aLastPart, nsresult aStatus);

  /* non-virtual imgIOnloadBlocker methods */
  // NB: If UnblockOnload is sent, and then we are asked to replay the
  // notifications, we will not send a BlockOnload/UnblockOnload pair.  This
  // is different from all the other notifications.
  void RecordBlockOnload();
  void SendBlockOnload(imgRequestProxy* aProxy);
  void RecordUnblockOnload();
  void SendUnblockOnload(imgRequestProxy* aProxy);

  void MaybeUnblockOnload();

  // Null out any reference to an associated image request
  void ClearRequest();

  // Weak pointer getters - no AddRefs.
  inline mozilla::image::Image* GetImage() const { return mImage; }
  inline imgRequest* GetRequest() const { return mRequest; }

  inline imgDecoderObserver* GetDecoderObserver() { return mTrackerObserver.get(); }

private:
  friend class imgStatusNotifyRunnable;
  friend class imgRequestNotifyRunnable;
  friend class imgStatusTrackerObserver;

  nsCOMPtr<nsIRunnable> mRequestRunnable;

  // Weak pointers to the image and request. The request owns the image, and
  // the image (or the request, if there's no image) owns the status tracker.
  mozilla::image::Image* mImage;
  imgRequest* mRequest;
  uint32_t mState;
  uint32_t mImageStatus;
  bool mHadLastPart;
  bool mBlockingOnload;

  // List of proxies attached to the image. Each proxy represents a consumer
  // using the image.
  nsTObserverArray<imgRequestProxy*> mConsumers;

  mozilla::RefPtr<imgDecoderObserver> mTrackerObserver;
};

#endif
