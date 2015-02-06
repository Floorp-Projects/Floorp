/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultipartImage.h"

#include "imgINotificationObserver.h"

namespace mozilla {
namespace image {

///////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////

class NextPartObserver : public IProgressObserver
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(NextPartObserver)
  NS_INLINE_DECL_REFCOUNTING(NextPartObserver, override)

  explicit NextPartObserver(MultipartImage* aOwner)
    : mOwner(aOwner)
  {
    MOZ_ASSERT(mOwner);
  }

  void BeginObserving(Image* aImage)
  {
    MOZ_ASSERT(aImage);
    mImage = aImage;

    nsRefPtr<ProgressTracker> tracker = mImage->GetProgressTracker();
    tracker->AddObserver(this);
  }

  void FinishObservingWithoutNotifying()
  {
    FinishObserving(/* aNotify = */ false);
  }

  virtual void Notify(int32_t aType,
                      const nsIntRect* aRect = nullptr) override
  {
    if (!mImage) {
      // We've already finished observing the last image we were given.
      return;
    }

    if (aType == imgINotificationObserver::FRAME_COMPLETE) {
      FinishObserving(/* aNotify = */ true);
    }
  }

  virtual void OnLoadComplete(bool aLastPart) override
  {
    if (!mImage) {
      // We've already finished observing the last image we were given.
      return;
    }

    // If there's already an error, we may never get a FRAME_COMPLETE
    // notification, so go ahead and notify our owner right away.
    nsRefPtr<ProgressTracker> tracker = mImage->GetProgressTracker();
    if (tracker->GetProgress() & FLAG_HAS_ERROR) {
      FinishObserving(/* aNotify = */ true);
    }
  }

  // Other notifications are ignored.
  virtual void BlockOnload() override { }
  virtual void UnblockOnload() override { }
  virtual void SetHasImage() override { }
  virtual void OnStartDecode() override { }
  virtual bool NotificationsDeferred() const override { return false; }
  virtual void SetNotificationsDeferred(bool) override { }

private:
  virtual ~NextPartObserver() { }

  void FinishObserving(bool aNotify)
  {
    MOZ_ASSERT(mImage);

    nsRefPtr<ProgressTracker> tracker = mImage->GetProgressTracker();
    tracker->RemoveObserver(this);
    mImage = nullptr;

    if (aNotify) {
      mOwner->FinishTransition();
    }
  }

  MultipartImage* mOwner;
  nsRefPtr<Image> mImage;
};


///////////////////////////////////////////////////////////////////////////////
// Implementation
///////////////////////////////////////////////////////////////////////////////

MultipartImage::MultipartImage(Image* aImage, ProgressTracker* aTracker)
  : ImageWrapper(aImage)
  , mDeferNotifications(false)
{
  MOZ_ASSERT(aTracker);
  mProgressTrackerInit = new ProgressTrackerInit(this, aTracker);
  mNextPartObserver = new NextPartObserver(this);

  // Start observing the first part.
  nsRefPtr<ProgressTracker> firstPartTracker =
    InnerImage()->GetProgressTracker();
  firstPartTracker->AddObserver(this);
  InnerImage()->RequestDecode();
  InnerImage()->IncrementAnimationConsumers();
}

MultipartImage::~MultipartImage() { }

NS_IMPL_QUERY_INTERFACE_INHERITED0(MultipartImage, ImageWrapper)
NS_IMPL_ADDREF(MultipartImage)
NS_IMPL_RELEASE(MultipartImage)

void
MultipartImage::BeginTransitionToPart(Image* aNextPart)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aNextPart);

  if (mNextPart) {
    NS_WARNING("Decoder not keeping up with multipart image");
    mNextPartObserver->FinishObservingWithoutNotifying();
  }

  mNextPart = aNextPart;

  // Start observing the next part; we'll complete the transition when
  // NextPartObserver calls FinishTransition.
  mNextPartObserver->BeginObserving(mNextPart);
  mNextPart->RequestDecode();
  mNextPart->IncrementAnimationConsumers();
}

void MultipartImage::FinishTransition()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mNextPart, "Should have a next part here");

  // Stop observing the current part.
  {
    nsRefPtr<ProgressTracker> currentPartTracker =
      InnerImage()->GetProgressTracker();
    currentPartTracker->RemoveObserver(this);
  }

  // Make the next part become the current part.
  mTracker->ResetForNewRequest();
  SetInnerImage(mNextPart);
  mNextPart = nullptr;
  nsRefPtr<ProgressTracker> newCurrentPartTracker =
    InnerImage()->GetProgressTracker();
  newCurrentPartTracker->AddObserver(this);

  // Finally, send all the notifications for the new current part and send a
  // FRAME_UPDATE notification so that observers know to redraw.
  mTracker->SyncNotifyProgress(newCurrentPartTracker->GetProgress(),
                               nsIntRect::GetMaxSizedIntRect());
}

already_AddRefed<imgIContainer>
MultipartImage::Unwrap()
{
  // Although we wrap another image, we don't allow callers to unwrap as. As far
  // as external code is concerned, MultipartImage is atomic.
  nsCOMPtr<imgIContainer> image = this;
  return image.forget();
}

already_AddRefed<ProgressTracker>
MultipartImage::GetProgressTracker()
{
  MOZ_ASSERT(mTracker);
  nsRefPtr<ProgressTracker> tracker = mTracker;
  return tracker.forget();
}

void
MultipartImage::SetProgressTracker(ProgressTracker* aTracker)
{
  MOZ_ASSERT(aTracker);
  MOZ_ASSERT(!mTracker);
  mTracker = aTracker;
}

nsresult
MultipartImage::OnImageDataAvailable(nsIRequest* aRequest,
                                     nsISupports* aContext,
                                     nsIInputStream* aInStr,
                                     uint64_t aSourceOffset,
                                     uint32_t aCount)
{
  // Note that this method is special in that we forward it to the next part if
  // one exists, and *not* the current part.

  // We may trigger notifications that will free mNextPart, so keep it alive.
  nsRefPtr<Image> nextPart = mNextPart;
  if (nextPart) {
    return nextPart->OnImageDataAvailable(aRequest, aContext, aInStr,
                                          aSourceOffset, aCount);
  }

  return InnerImage()->OnImageDataAvailable(aRequest, aContext, aInStr,
                                            aSourceOffset, aCount);
}

nsresult
MultipartImage::OnImageDataComplete(nsIRequest* aRequest,
                                    nsISupports* aContext,
                                    nsresult aStatus,
                                    bool aLastPart)
{
  // Note that this method is special in that we forward it to the next part if
  // one exists, and *not* the current part.

  // We may trigger notifications that will free mNextPart, so keep it alive.
  nsRefPtr<Image> nextPart = mNextPart;
  if (nextPart) {
    return nextPart->OnImageDataComplete(aRequest, aContext, aStatus,
                                         aLastPart);
  }

  return InnerImage()->OnImageDataComplete(aRequest, aContext, aStatus,
                                           aLastPart);
}

void
MultipartImage::Notify(int32_t aType, const nsIntRect* aRect /* = nullptr*/)
{
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    mTracker->SyncNotifyProgress(FLAG_SIZE_AVAILABLE);
  } else if (aType == imgINotificationObserver::FRAME_UPDATE) {
    mTracker->SyncNotifyProgress(NoProgress, *aRect);
  } else if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    mTracker->SyncNotifyProgress(FLAG_FRAME_COMPLETE);
  } else if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    mTracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);
  } else if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    mTracker->SyncNotifyProgress(FLAG_DECODE_COMPLETE);
  } else if (aType == imgINotificationObserver::DISCARD) {
    mTracker->OnDiscard();
  } else if (aType == imgINotificationObserver::UNLOCKED_DRAW) {
    mTracker->OnUnlockedDraw();
  } else if (aType == imgINotificationObserver::IS_ANIMATED) {
    mTracker->SyncNotifyProgress(FLAG_IS_ANIMATED);
  } else if (aType == imgINotificationObserver::HAS_TRANSPARENCY) {
    mTracker->SyncNotifyProgress(FLAG_HAS_TRANSPARENCY);
  } else {
    NS_NOTREACHED("Notification list should be exhaustive");
  }
}

void
MultipartImage::OnLoadComplete(bool aLastPart)
{
  Progress progress = FLAG_LOAD_COMPLETE;
  if (aLastPart) {
    progress |= FLAG_LAST_PART_COMPLETE;
  }
  mTracker->SyncNotifyProgress(progress);
}

void
MultipartImage::SetHasImage()
{
  mTracker->OnImageAvailable();
}

void
MultipartImage::OnStartDecode()
{
  mTracker->SyncNotifyProgress(FLAG_DECODE_STARTED);
}

bool
MultipartImage::NotificationsDeferred() const
{
  return mDeferNotifications;
}

void
MultipartImage::SetNotificationsDeferred(bool aDeferNotifications)
{
  mDeferNotifications = aDeferNotifications;
}

} // namespace image
} // namespace mozilla
