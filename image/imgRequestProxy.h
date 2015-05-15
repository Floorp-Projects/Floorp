/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_imgRequestProxy_h
#define mozilla_image_imgRequestProxy_h

#include "imgIRequest.h"
#include "nsISecurityInfoProvider.h"

#include "nsILoadGroup.h"
#include "nsISupportsPriority.h"
#include "nsITimedChannel.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/gfx/Rect.h"

#include "imgRequest.h"
#include "IProgressObserver.h"

#define NS_IMGREQUESTPROXY_CID \
{ /* 20557898-1dd2-11b2-8f65-9c462ee2bc95 */         \
     0x20557898,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x8f, 0x65, 0x9c, 0x46, 0x2e, 0xe2, 0xbc, 0x95} \
}

class imgINotificationObserver;
class imgStatusNotifyRunnable;
class ProxyBehaviour;

namespace mozilla {
namespace image {
class Image;
class ImageURL;
class ProgressTracker;
} // namespace image
} // namespace mozilla

class imgRequestProxy : public imgIRequest,
                        public mozilla::image::IProgressObserver,
                        public nsISupportsPriority,
                        public nsISecurityInfoProvider,
                        public nsITimedChannel
{
protected:
  virtual ~imgRequestProxy();

public:
  typedef mozilla::image::Image Image;
  typedef mozilla::image::ImageURL ImageURL;
  typedef mozilla::image::ProgressTracker ProgressTracker;

  MOZ_DECLARE_REFCOUNTED_TYPENAME(imgRequestProxy)
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIREQUEST
  NS_DECL_NSIREQUEST
  NS_DECL_NSISUPPORTSPRIORITY
  NS_DECL_NSISECURITYINFOPROVIDER
  // nsITimedChannel declared below

  imgRequestProxy();

  // Callers to Init or ChangeOwner are required to call NotifyListener after
  // (although not immediately after) doing so.
  nsresult Init(imgRequest* aOwner,
                nsILoadGroup* aLoadGroup,
                ImageURL* aURI,
                imgINotificationObserver* aObserver);

  nsresult ChangeOwner(imgRequest* aNewOwner); // this will change mOwner.
                                               // Do not call this if the
                                               // previous owner has already
                                               // sent notifications out!

  void AddToLoadGroup();
  void RemoveFromLoadGroup(bool releaseLoadGroup);

  inline bool HasObserver() const {
    return mListener != nullptr;
  }

  // Asynchronously notify this proxy's listener of the current state of the
  // image, and, if we have an imgRequest mOwner, any status changes that
  // happen between the time this function is called and the time the
  // notification is scheduled.
  void NotifyListener();

  // Synchronously notify this proxy's listener of the current state of the
  // image. Only use this function if you are currently servicing an
  // asynchronously-called function.
  void SyncNotifyListener();

  // imgINotificationObserver methods:
  virtual void Notify(int32_t aType,
                      const mozilla::gfx::IntRect* aRect = nullptr) override;
  virtual void OnLoadComplete(bool aLastPart) override;

  // imgIOnloadBlocker methods:
  virtual void BlockOnload() override;
  virtual void UnblockOnload() override;

  // Other, internal-only methods:
  virtual void SetHasImage() override;
  virtual void OnStartDecode() override;

  // Whether we want notifications from ProgressTracker to be deferred until
  // an event it has scheduled has been fired.
  virtual bool NotificationsDeferred() const override
  {
    return mDeferNotifications;
  }
  virtual void SetNotificationsDeferred(bool aDeferNotifications) override
  {
    mDeferNotifications = aDeferNotifications;
  }

  // Removes all animation consumers that were created with
  // IncrementAnimationConsumers. This is necessary since we need
  // to do it before the proxy itself is destroyed. See
  // imgRequest::RemoveProxy
  void ClearAnimationConsumers();

  virtual nsresult Clone(imgINotificationObserver* aObserver,
                         imgRequestProxy** aClone);
  nsresult GetStaticRequest(imgRequestProxy** aReturn);

  nsresult GetURI(ImageURL** aURI);

protected:
  friend class mozilla::image::ProgressTracker;
  friend class imgStatusNotifyRunnable;

  class imgCancelRunnable;
  friend class imgCancelRunnable;

  class imgCancelRunnable : public nsRunnable
  {
    public:
      imgCancelRunnable(imgRequestProxy* owner, nsresult status)
        : mOwner(owner), mStatus(status)
      { }

      NS_IMETHOD Run() override {
        mOwner->DoCancel(mStatus);
        return NS_OK;
      }

    private:
      nsRefPtr<imgRequestProxy> mOwner;
      nsresult mStatus;
  };

  /* Finish up canceling ourselves */
  void DoCancel(nsresult status);

  /* Do the proper refcount management to null out mListener */
  void NullOutListener();

  void DoRemoveFromLoadGroup() {
    RemoveFromLoadGroup(true);
  }

  // Return the ProgressTracker associated with mOwner and/or mImage. It may
  // live either on mOwner or mImage, depending on whether
  //   (a) we have an mOwner at all
  //   (b) whether mOwner has instantiated its image yet
  already_AddRefed<ProgressTracker> GetProgressTracker() const;

  nsITimedChannel* TimedChannel()
  {
    if (!GetOwner()) {
      return nullptr;
    }
    return GetOwner()->GetTimedChannel();
  }

  already_AddRefed<Image> GetImage() const;
  bool HasImage() const;
  imgRequest* GetOwner() const;

  nsresult PerformClone(imgINotificationObserver* aObserver,
                        imgRequestProxy* (aAllocFn)(imgRequestProxy*),
                        imgRequestProxy** aClone);

public:
  NS_FORWARD_SAFE_NSITIMEDCHANNEL(TimedChannel())

protected:
  nsAutoPtr<ProxyBehaviour> mBehaviour;

private:
  friend class imgCacheValidator;
  friend imgRequestProxy* NewStaticProxy(imgRequestProxy* aThis);

  // The URI of our request.
  nsRefPtr<ImageURL> mURI;

  // mListener is only promised to be a weak ref (see imgILoader.idl),
  // but we actually keep a strong ref to it until we've seen our
  // first OnStopRequest.
  imgINotificationObserver* mListener;
  nsCOMPtr<nsILoadGroup> mLoadGroup;

  nsLoadFlags mLoadFlags;
  uint32_t    mLockCount;
  uint32_t    mAnimationConsumers;
  bool mCanceled;
  bool mIsInLoadGroup;
  bool mListenerIsStrongRef;
  bool mDecodeRequested;

  // Whether we want to defer our notifications by the non-virtual Observer
  // interfaces as image loads proceed.
  bool mDeferNotifications;
};

// Used for static image proxies for which no requests are available, so
// certain behaviours must be overridden to compensate.
class imgRequestProxyStatic : public imgRequestProxy
{

public:
  imgRequestProxyStatic(Image* aImage, nsIPrincipal* aPrincipal);

  NS_IMETHOD GetImagePrincipal(nsIPrincipal** aPrincipal) override;

  using imgRequestProxy::Clone;

  virtual nsresult Clone(imgINotificationObserver* aObserver,
                         imgRequestProxy** aClone) override;

protected:
  friend imgRequestProxy* NewStaticProxy(imgRequestProxy*);

  // Our principal. We have to cache it, rather than accessing the underlying
  // request on-demand, because static proxies don't have an underlying request.
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

#endif // mozilla_image_imgRequestProxy_h
