/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_MULTIPARTIMAGE_H_
#define MOZILLA_IMAGELIB_MULTIPARTIMAGE_H_

#include "ImageWrapper.h"
#include "IProgressObserver.h"
#include "ProgressTracker.h"

namespace mozilla {
namespace image {

class NextPartObserver;

/**
 * An Image wrapper that implements support for multipart/x-mixed-replace
 * images.
 */
class MultipartImage
  : public ImageWrapper
  , public IProgressObserver
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(MultipartImage)
  NS_DECL_ISUPPORTS

  MultipartImage(Image* aImage, ProgressTracker* aTracker);

  void BeginTransitionToPart(Image* aNextPart);

  // Overridden ImageWrapper methods:
  virtual already_AddRefed<imgIContainer> Unwrap() MOZ_OVERRIDE;
  virtual already_AddRefed<ProgressTracker> GetProgressTracker() MOZ_OVERRIDE;
  virtual void SetProgressTracker(ProgressTracker* aTracker) MOZ_OVERRIDE;
  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) MOZ_OVERRIDE;
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext,
                                       nsresult aStatus,
                                       bool aLastPart) MOZ_OVERRIDE;

  // We don't support locking or track animation consumers for individual parts,
  // so we override these methods to do nothing.
  NS_IMETHOD LockImage() MOZ_OVERRIDE { return NS_OK; }
  NS_IMETHOD UnlockImage() MOZ_OVERRIDE { return NS_OK; }
  virtual void IncrementAnimationConsumers() MOZ_OVERRIDE { }
  virtual void DecrementAnimationConsumers() MOZ_OVERRIDE { }
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() MOZ_OVERRIDE { return 1; }
#endif

  // Overridden IProgressObserver methods:
  virtual void Notify(int32_t aType,
                      const nsIntRect* aRect = nullptr) MOZ_OVERRIDE;
  virtual void OnLoadComplete(bool aLastPart) MOZ_OVERRIDE;
  virtual void SetHasImage() MOZ_OVERRIDE;
  virtual void OnStartDecode() MOZ_OVERRIDE;
  virtual bool NotificationsDeferred() const MOZ_OVERRIDE;
  virtual void SetNotificationsDeferred(bool aDeferNotifications) MOZ_OVERRIDE;

  // We don't allow multipart images to block onload, so we override these
  // methods to do nothing.
  virtual void BlockOnload() MOZ_OVERRIDE { }
  virtual void UnblockOnload() MOZ_OVERRIDE { }

protected:
  virtual ~MultipartImage();

private:
  friend class NextPartObserver;

  void FinishTransition();

  nsRefPtr<ProgressTracker> mTracker;
  nsAutoPtr<ProgressTrackerInit> mProgressTrackerInit;
  nsRefPtr<NextPartObserver> mNextPartObserver;
  nsRefPtr<Image> mNextPart;
  bool mDeferNotifications : 1;
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_MULTIPARTIMAGE_H_
