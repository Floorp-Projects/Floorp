/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/AndroidDynamicToolbarAnimator.h"

#include <cmath>
#include "FrameMetrics.h"
#include "gfxPrefs.h"
#include "mozilla/EventForwards.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/UiCompositorControllerMessageTypes.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Move.h"
#include "mozilla/Unused.h"

namespace {

// Internal flags and constants
static const float   ANIMATION_DURATION = 0.15f; // How many seconds the complete animation should span
static const int32_t MOVE_TOOLBAR_DOWN  =  1;    // Multiplier to move the toolbar down
static const int32_t MOVE_TOOLBAR_UP    = -1;    // Multiplier to move the toolbar up
static const float   SHRINK_FACTOR      = 0.95f; // Amount to shrink the either the full content for small pages or the amount left
                                                 // See: PageTooSmallEnsureToolbarVisible()
} // namespace

namespace mozilla {
namespace layers {

AndroidDynamicToolbarAnimator::AndroidDynamicToolbarAnimator()
  : mRootLayerTreeId(0)
  // Read/Write Compositor Thread, Read only Controller thread
  , mToolbarState(eToolbarVisible)
  , mPinnedFlags(0)
  // Controller thread only
  , mControllerScrollingRootContent(false)
  , mControllerDragThresholdReached(false)
  , mControllerCancelTouchTracking(false)
  , mControllerDragChangedDirection(false)
  , mControllerResetOnNextMove(false)
  , mControllerStartTouch(0)
  , mControllerPreviousTouch(0)
  , mControllerTotalDistance(0)
  , mControllerMaxToolbarHeight(0)
  , mControllerToolbarHeight(0)
  , mControllerSurfaceHeight(0)
  , mControllerCompositionHeight(0)
  , mControllerRootScrollY(0.0f)
  , mControllerLastDragDirection(0)
  , mControllerTouchCount(0)
  , mControllerLastEventTimeStamp(0)
  , mControllerState(eNothingPending)
  // Compositor thread only
  , mCompositorShutdown(false)
  , mCompositorAnimationDeferred(false)
  , mCompositorLayersUpdateEnabled(false)
  , mCompositorAnimationStarted(false)
  , mCompositorReceivedFirstPaint(false)
  , mCompositorWaitForPageResize(false)
  , mCompositorToolbarShowRequested(false)
  , mCompositorSendResponseForSnapshotUpdate(false)
  , mCompositorAnimationStyle(eAnimate)
  , mCompositorMaxToolbarHeight(0)
  , mCompositorToolbarHeight(0)
  , mCompositorSurfaceHeight(0)
  , mCompositorAnimationDirection(0)
  , mCompositorAnimationStartHeight(0)
{}

void
AndroidDynamicToolbarAnimator::Initialize(uint64_t aRootLayerTreeId)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mRootLayerTreeId = aRootLayerTreeId;
  RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeId);
  MOZ_ASSERT(uiController);
  uiController->RegisterAndroidDynamicToolbarAnimator(this);

  // Send queued messages that were posted before Initialize() was called.
  for (QueuedMessage* message = mCompositorQueuedMessages.getFirst(); message != nullptr; message = message->getNext()) {
    uiController->ToolbarAnimatorMessageFromCompositor(message->mMessage);
  }
  mCompositorQueuedMessages.clear();
}

static bool
GetTouchY(MultiTouchInput& multiTouch, ScreenIntCoord* value)
{
  MOZ_ASSERT(value);
  if (multiTouch.mTouches.Length() == 1) {
    *value = multiTouch.mTouches[0].mScreenPoint.y;
    return true;
  }

  return false;
}

nsEventStatus
AndroidDynamicToolbarAnimator::ReceiveInputEvent(InputData& aEvent, const ScreenPoint& aScrollOffset)
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());

  mControllerRootScrollY = aScrollOffset.y;

  // Only process and adjust touch events. Wheel events (aka scroll events) are adjusted in the NativePanZoomController
  if (aEvent.mInputType != MULTITOUCH_INPUT) {
    return nsEventStatus_eIgnore;
  }

  MultiTouchInput& multiTouch = aEvent.AsMultiTouchInput();

  if (PageTooSmallEnsureToolbarVisible()) {
    TranslateTouchEvent(multiTouch);
    return nsEventStatus_eIgnore;
  }

  switch (multiTouch.mType) {
  case MultiTouchInput::MULTITOUCH_START:
    mControllerTouchCount = multiTouch.mTouches.Length();
    break;
  case MultiTouchInput::MULTITOUCH_END:
  case MultiTouchInput::MULTITOUCH_CANCEL:
    mControllerTouchCount -= multiTouch.mTouches.Length();
    break;
  case MultiTouchInput::MULTITOUCH_SENTINEL:
    MOZ_FALLTHROUGH_ASSERT("Invalid value");
  case MultiTouchInput::MULTITOUCH_MOVE:
    break;
  }

  if (mControllerTouchCount > 1) {
    mControllerResetOnNextMove = true;
  }

  ScreenIntCoord currentTouch = 0;

  if (mPinnedFlags || !GetTouchY(multiTouch, &currentTouch)) {
    TranslateTouchEvent(multiTouch);
    return nsEventStatus_eIgnore;
  }

  // Only the return value from ProcessTouchDelta should
  // change status to nsEventStatus_eConsumeNoDefault
  nsEventStatus status = nsEventStatus_eIgnore;

  const StaticToolbarState currentToolbarState = mToolbarState;
  switch (multiTouch.mType) {
  case MultiTouchInput::MULTITOUCH_START:
    mControllerCancelTouchTracking = false;
    mControllerStartTouch = mControllerPreviousTouch = currentTouch;
    // We don't want to stop the animation if we are near the bottom of the page.
    if (!ScrollOffsetNearBottom() && (currentToolbarState == eToolbarAnimating)) {
      StopCompositorAnimation();
    }
    break;
  case MultiTouchInput::MULTITOUCH_MOVE: {
    CheckForResetOnNextMove(currentTouch);
    if ((mControllerState != eAnimationStartPending) &&
        (mControllerState != eAnimationStopPending) &&
        (currentToolbarState != eToolbarAnimating) &&
        !mControllerCancelTouchTracking) {

      // Don't move the toolbar if we are near the page bottom
      // and the toolbar is not in transition
      if (ScrollOffsetNearBottom() && !ToolbarInTransition()) {
        ShowToolbarIfNotVisible(currentToolbarState);
        break;
      }

      ScreenIntCoord delta = currentTouch - mControllerPreviousTouch;
      mControllerPreviousTouch = currentTouch;
      mControllerTotalDistance += delta;
      if (delta != 0) {
        ScreenIntCoord direction = (delta > 0 ? MOVE_TOOLBAR_DOWN : MOVE_TOOLBAR_UP);
        if (mControllerLastDragDirection && (direction != mControllerLastDragDirection)) {
          mControllerDragChangedDirection = true;
        }
        mControllerLastDragDirection = direction;
      }
      // NOTE: gfxPrefs::ToolbarScrollThreshold() returns a percentage as an int32_t. So multiply it by 0.01f to convert.
      const uint32_t dragThreshold = Abs(std::lround(0.01f * gfxPrefs::ToolbarScrollThreshold() * mControllerCompositionHeight));
      if ((Abs(mControllerTotalDistance.value) > dragThreshold) && (delta != 0)) {
        mControllerDragThresholdReached = true;
        status = ProcessTouchDelta(currentToolbarState, delta, multiTouch.mTime);
      }
      mControllerLastEventTimeStamp = multiTouch.mTime;
    }
    break;
  }
  case MultiTouchInput::MULTITOUCH_END:
  case MultiTouchInput::MULTITOUCH_CANCEL:
    // last finger was lifted
    if (mControllerTouchCount == 0) {
      HandleTouchEnd(currentToolbarState, currentTouch);
    }
    break;
  case MultiTouchInput::MULTITOUCH_SENTINEL:
    MOZ_ASSERT_UNREACHABLE("Invalid value");
    break;
  }

  TranslateTouchEvent(multiTouch);

  return status;
}

void
AndroidDynamicToolbarAnimator::SetMaxToolbarHeight(ScreenIntCoord aHeight)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  UpdateControllerToolbarHeight(aHeight, aHeight);
  mCompositorMaxToolbarHeight = aHeight;
  UpdateCompositorToolbarHeight(aHeight);
}

void
AndroidDynamicToolbarAnimator::SetPinned(bool aPinned, int32_t aReason)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aReason < 32);
  uint32_t bit = 0x01 << aReason;
  uint32_t current = mPinnedFlags;
  if (aPinned) {
    mPinnedFlags = current | bit;
  } else {
    mPinnedFlags = current & (~bit);
  }
}

ScreenIntCoord
AndroidDynamicToolbarAnimator::GetMaxToolbarHeight() const
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mCompositorMaxToolbarHeight;
}

ScreenIntCoord
AndroidDynamicToolbarAnimator::GetCurrentToolbarHeight() const
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mCompositorToolbarHeight;
}

ScreenIntCoord
AndroidDynamicToolbarAnimator::GetCurrentContentOffset() const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mCompositorAnimationStarted && (mToolbarState == eToolbarAnimating)) {
    return 0;
  }

  return mCompositorToolbarHeight;
}

ScreenIntCoord
AndroidDynamicToolbarAnimator::GetCurrentSurfaceHeight() const
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mCompositorSurfaceHeight;
}

ScreenIntCoord
AndroidDynamicToolbarAnimator::GetCompositionHeight() const
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mCompositorCompositionSize.height;
}

void
AndroidDynamicToolbarAnimator::SetScrollingRootContent()
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  mControllerScrollingRootContent = true;
}

void
AndroidDynamicToolbarAnimator::ToolbarAnimatorMessageFromUI(int32_t aMessage)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  switch(aMessage) {
  case STATIC_TOOLBAR_NEEDS_UPDATE:
    break;
  case STATIC_TOOLBAR_READY:
    break;
  case TOOLBAR_HIDDEN:
    // If the toolbar is animating, then it is already unlocked.
    if (mToolbarState != eToolbarAnimating) {
      mToolbarState = eToolbarUnlocked;
      if (mCompositorAnimationDeferred) {
        StartCompositorAnimation(mCompositorAnimationDirection, mCompositorAnimationStyle, mCompositorToolbarHeight, mCompositorWaitForPageResize);
      }
    } else {
      // Animation already running so just make sure it is going the right direction.
      StartCompositorAnimation(MOVE_TOOLBAR_UP, mCompositorAnimationStyle, mCompositorToolbarHeight, mCompositorWaitForPageResize);
    }
    break;
  case TOOLBAR_VISIBLE:
    // If we are currently animating, let the animation finish.
    if (mToolbarState != eToolbarAnimating) {
      mToolbarState = eToolbarVisible;
    }
    break;
  case TOOLBAR_SHOW:
    break;
  case FIRST_PAINT:
    break;
  case REQUEST_SHOW_TOOLBAR_IMMEDIATELY:
    NotifyControllerPendingAnimation(MOVE_TOOLBAR_DOWN, eImmediate);
    break;
  case REQUEST_SHOW_TOOLBAR_ANIMATED:
    NotifyControllerPendingAnimation(MOVE_TOOLBAR_DOWN, eAnimate);
    break;
  case REQUEST_HIDE_TOOLBAR_IMMEDIATELY:
    NotifyControllerPendingAnimation(MOVE_TOOLBAR_UP, eImmediate);
    break;
  case REQUEST_HIDE_TOOLBAR_ANIMATED:
    NotifyControllerPendingAnimation(MOVE_TOOLBAR_UP, eAnimate);
    break;
  case TOOLBAR_SNAPSHOT_FAILED:
    mToolbarState = eToolbarVisible;
    NotifyControllerSnapshotFailed();
    break;
  default:
    break;
  }
}

bool
AndroidDynamicToolbarAnimator::UpdateAnimation(const TimeStamp& aCurrentFrame)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if ((mToolbarState != eToolbarAnimating) || mCompositorShutdown) {
    return false;
  }

  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
  if (!parent) {
    return false;
  }

  AsyncCompositionManager* manager = parent->GetCompositionManager(nullptr);
  if (!manager) {
    return false;
  }

  if (mCompositorSurfaceHeight != mCompositorCompositionSize.height) {
    // Waiting for the composition to resize
    if (mCompositorWaitForPageResize && mCompositorAnimationStarted) {
      mCompositorWaitForPageResize = false;
    } else {
      return true;
    }
  } else if (!mCompositorAnimationStarted) {
    parent->GetAPZCTreeManager()->AdjustScrollForSurfaceShift(ScreenPoint(0.0f, (float)(-mCompositorToolbarHeight)));
    manager->SetFixedLayerMargins(mCompositorToolbarHeight, 0);
    mCompositorAnimationStarted = true;
    mCompositorReceivedFirstPaint = false;
    mCompositorToolbarShowRequested = false;
    // Reset the start time so the toolbar does not jump on the first animation frame
    mCompositorAnimationStartTimeStamp = aCurrentFrame;
    // Since the delta time for this frame will be zero. Just return, the animation will start on the next frame.
    return true;
  }

  bool continueAnimating = true;

  if (mCompositorAnimationStyle == eImmediate) {
    if (mCompositorAnimationDirection == MOVE_TOOLBAR_DOWN) {
      mCompositorToolbarHeight = mCompositorMaxToolbarHeight;
    } else if (mCompositorAnimationDirection == MOVE_TOOLBAR_UP) {
      mCompositorToolbarHeight = 0;
    }
  } else if (mCompositorAnimationStyle == eAnimate) {
    const float rate = ((float)mCompositorMaxToolbarHeight) / ANIMATION_DURATION;
    float deltaTime = (aCurrentFrame - mCompositorAnimationStartTimeStamp).ToSeconds();
    // This animation was started in the future!
    if (deltaTime < 0.0f) {
      deltaTime = 0.0f;
    }
    mCompositorToolbarHeight = mCompositorAnimationStartHeight + ((int32_t)(rate * deltaTime) * mCompositorAnimationDirection);
  }

  if ((mCompositorAnimationDirection == MOVE_TOOLBAR_DOWN) && (mCompositorToolbarHeight >= mCompositorMaxToolbarHeight)) {
    // if the toolbar is being animated and the page is at the end, the animation needs to wait for
    // the page to resize before ending the animation so that the page may be scrolled
    if (!mCompositorReceivedFirstPaint && mCompositorWaitForPageResize) {
      continueAnimating = true;
    } else {
      continueAnimating = false;
      mToolbarState = eToolbarVisible;
    }
    // Make sure we only send one show request per animation
    if (!mCompositorToolbarShowRequested) {
      PostMessage(TOOLBAR_SHOW);
      mCompositorToolbarShowRequested = true;
    }
    mCompositorToolbarHeight = mCompositorMaxToolbarHeight;
  } else if ((mCompositorAnimationDirection == MOVE_TOOLBAR_UP) && (mCompositorToolbarHeight <= 0)) {
    continueAnimating = false;
    mToolbarState = eToolbarUnlocked;
    mCompositorToolbarHeight = 0;
  }

  if (continueAnimating) {
    manager->SetFixedLayerMargins(mCompositorToolbarHeight, 0);
  } else {
    if (mCompositorAnimationDirection == MOVE_TOOLBAR_DOWN) {
      if (!mCompositorReceivedFirstPaint) {
        parent->GetAPZCTreeManager()->AdjustScrollForSurfaceShift(ScreenPoint(0.0f, (float)mCompositorMaxToolbarHeight));
      }
      manager->SetFixedLayerMargins(0, GetFixedLayerMarginsBottom());
    } else {
      manager->SetFixedLayerMargins(0, 0);
    }
  }

  if (!continueAnimating) {
    NotifyControllerAnimationStopped(mCompositorToolbarHeight);
  } else {
    UpdateControllerToolbarHeight(mCompositorToolbarHeight);
  }

  return continueAnimating;
}

void
AndroidDynamicToolbarAnimator::FirstPaint()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mCompositorReceivedFirstPaint = true;
  PostMessage(FIRST_PAINT);
}

void
AndroidDynamicToolbarAnimator::UpdateRootFrameMetrics(const FrameMetrics& aMetrics)
{
  CSSToScreenScale scale = ViewTargetAs<ScreenPixel>(aMetrics.GetZoom().ToScaleFactor(),
                                                     PixelCastJustification::ScreenIsParentLayerForRoot);
  ScreenPoint scrollOffset = aMetrics.GetScrollOffset() * scale;
  CSSRect cssPageRect = aMetrics.GetScrollableRect();

  UpdateFrameMetrics(scrollOffset, scale, cssPageRect);
}

void
AndroidDynamicToolbarAnimator::MaybeUpdateCompositionSizeAndRootFrameMetrics(const FrameMetrics& aMetrics)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  CSSToScreenScale scale = ViewTargetAs<ScreenPixel>(aMetrics.GetZoom().ToScaleFactor(),
                                                     PixelCastJustification::ScreenIsParentLayerForRoot);
  ScreenIntSize size = ScreenIntSize::Round(aMetrics.GetRootCompositionSize() * scale);

  if (mCompositorCompositionSize == size) {
    return;
  }

  ScreenIntSize prevSize = mCompositorCompositionSize;
  mCompositorCompositionSize = size;

  // The width has changed so the static snapshot needs to be updated
  if ((prevSize.width != size.width) && (mToolbarState == eToolbarUnlocked)) {
    // No need to set mCompositorSendResponseForSnapshotUpdate. If it is already true we don't want to change it.
    PostMessage(STATIC_TOOLBAR_NEEDS_UPDATE);
  }

  if (prevSize.height != size.height) {
    UpdateControllerCompositionHeight(size.height);
    UpdateFixedLayerMargins();
  }

  UpdateRootFrameMetrics(aMetrics);
}

// Layers updates are need by Robocop test which enables them
void
AndroidDynamicToolbarAnimator::EnableLayersUpdateNotifications(bool aEnable)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mCompositorLayersUpdateEnabled = aEnable;
}

void
AndroidDynamicToolbarAnimator::NotifyLayersUpdated()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mCompositorLayersUpdateEnabled) {
    PostMessage(LAYERS_UPDATED);
  }
}

void
AndroidDynamicToolbarAnimator::AdoptToolbarPixels(mozilla::ipc::Shmem&& aMem, const ScreenIntSize& aSize)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mCompositorToolbarPixels = Some(Move(aMem));
  mCompositorToolbarPixelsSize = aSize;
}

Effect*
AndroidDynamicToolbarAnimator::GetToolbarEffect(CompositorOGL* gl)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  // if the compositor has shutdown, do not create any new rendering objects.
  if (mCompositorShutdown) {
    return nullptr;
  }

  if (mCompositorToolbarPixels) {
    RefPtr<DataSourceSurface> surface = Factory::CreateWrappingDataSourceSurface(
        mCompositorToolbarPixels.ref().get<uint8_t>(),
        mCompositorToolbarPixelsSize.width * 4,
        IntSize(mCompositorToolbarPixelsSize.width, mCompositorToolbarPixelsSize.height),
        gfx::SurfaceFormat::B8G8R8A8);

    if (!mCompositorToolbarTexture) {
      mCompositorToolbarTexture = gl->CreateDataTextureSource();
      mCompositorToolbarEffect = nullptr;
    }

    if (!mCompositorToolbarTexture->Update(surface)) {
      // Upload failed!
      mCompositorToolbarTexture = nullptr;
    }

    RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeId);
    uiController->DeallocShmem(mCompositorToolbarPixels.ref());
    mCompositorToolbarPixels.reset();
    // Send notification that texture is ready after the current composition has completed.
    if (mCompositorToolbarTexture && mCompositorSendResponseForSnapshotUpdate) {
      mCompositorSendResponseForSnapshotUpdate = false;
      CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod(this, &AndroidDynamicToolbarAnimator::PostToolbarReady));
    }
  }

  if (mCompositorToolbarTexture) {
    if (!mCompositorToolbarEffect) {
      mCompositorToolbarEffect = new EffectRGB(mCompositorToolbarTexture, true, SamplingFilter::LINEAR);
    }

    float ratioVisible = (float)mCompositorToolbarHeight / (float)mCompositorMaxToolbarHeight;
    mCompositorToolbarEffect->mTextureCoords.y = 1.0f - ratioVisible;
    mCompositorToolbarEffect->mTextureCoords.height = ratioVisible;
  }

  return mCompositorToolbarEffect.get();
}

void
AndroidDynamicToolbarAnimator::Shutdown()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mCompositorShutdown = true;
  mCompositorToolbarEffect = nullptr;
  mCompositorToolbarTexture = nullptr;
  mCompositorQueuedMessages.clear();
  if (mCompositorToolbarPixels) {
    RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeId);
    uiController->DeallocShmem(mCompositorToolbarPixels.ref());
    mCompositorToolbarPixels.reset();
  }
}

nsEventStatus
AndroidDynamicToolbarAnimator::ProcessTouchDelta(StaticToolbarState aCurrentToolbarState, ScreenIntCoord aDelta, uint32_t aTimeStamp)
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  nsEventStatus status = nsEventStatus_eIgnore;

  const bool tryingToHideToolbar = aDelta < 0;

  if (tryingToHideToolbar && !mControllerScrollingRootContent) {
    // This prevent the toolbar from hiding if a subframe is being scrolled up.
    // The toolbar will always become visible regardless what is being scrolled down.
    return status;
  }

  if (aCurrentToolbarState == eToolbarVisible) {
    if (tryingToHideToolbar && (mControllerState != eUnlockPending)) {
      mCompositorSendResponseForSnapshotUpdate = true;
      PostMessage(STATIC_TOOLBAR_NEEDS_UPDATE);
      mControllerState = eUnlockPending;
    }
    return status;
  }

  if (aCurrentToolbarState != eToolbarUnlocked) {
    return status;
  }

  if ((mControllerState != eUnlockPending) && (mControllerState != eNothingPending)) {
    return status;
  }

  mControllerState = eNothingPending;
  if ((tryingToHideToolbar && (mControllerToolbarHeight > 0)) ||
      (!tryingToHideToolbar && (mControllerToolbarHeight < mControllerMaxToolbarHeight))) {
    ScreenIntCoord deltaRemainder = 0;
    mControllerToolbarHeight += aDelta;
    if (tryingToHideToolbar && (mControllerToolbarHeight <= 0 )) {
      deltaRemainder = mControllerToolbarHeight;
      mControllerToolbarHeight = 0;
    } else if (!tryingToHideToolbar && (mControllerToolbarHeight >= mControllerMaxToolbarHeight)) {
      deltaRemainder = mControllerToolbarHeight - mControllerMaxToolbarHeight;
      mControllerToolbarHeight = mControllerMaxToolbarHeight;
      mControllerState = eShowPending;
      PostMessage(TOOLBAR_SHOW);
    }

    UpdateCompositorToolbarHeight(mControllerToolbarHeight);
    RequestComposite();
    // If there was no delta left over, the event was completely consumed.
    if (deltaRemainder == 0) {
      status = nsEventStatus_eConsumeNoDefault;
    }

    uint32_t timeDelta = aTimeStamp - mControllerLastEventTimeStamp;
    if (mControllerLastEventTimeStamp && timeDelta && aDelta) {
      float speed = -(float)aDelta / (float)timeDelta;
      CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
      if (parent) {
        parent->GetAPZCTreeManager()->ProcessTouchVelocity(aTimeStamp, speed);
      }
    }
  }

  return status;
}

void
AndroidDynamicToolbarAnimator::HandleTouchEnd(StaticToolbarState aCurrentToolbarState, ScreenIntCoord aCurrentTouch)
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  // If there was no move before the reset flag was set and the touch ended, check for it here.
  // if mControllerResetOnNextMove is true, it will be set to false here
  CheckForResetOnNextMove(aCurrentTouch);
  int32_t direction = mControllerLastDragDirection;
  mControllerLastDragDirection = 0;
  bool isRoot = mControllerScrollingRootContent;
  mControllerScrollingRootContent = false;
  bool dragChangedDirection = mControllerDragChangedDirection;
  mControllerDragChangedDirection = false;

  // If the drag direction changed and the toolbar is partially visible, hide in the direction with the least distance to travel.
  if (dragChangedDirection && ToolbarInTransition()) {
    direction = ((float)mControllerToolbarHeight / (float)mControllerMaxToolbarHeight) < 0.5f ? MOVE_TOOLBAR_UP : MOVE_TOOLBAR_DOWN;
  }

  // If the last touch didn't have a drag direction, use start of touch to find direction
  if (!direction) {
    if (mControllerToolbarHeight == mControllerMaxToolbarHeight) {
      direction = MOVE_TOOLBAR_DOWN;
    } else if (mControllerToolbarHeight == 0) {
      direction = MOVE_TOOLBAR_UP;
    } else {
      direction = ((aCurrentTouch - mControllerStartTouch) > 0 ? MOVE_TOOLBAR_DOWN : MOVE_TOOLBAR_UP);
    }
    // If there still isn't a direction, default to show just to be safe
    if (!direction) {
      direction = MOVE_TOOLBAR_DOWN;
    }
  }
  mControllerStartTouch = 0;
  mControllerPreviousTouch = 0;
  mControllerTotalDistance = 0;
  bool dragThresholdReached = mControllerDragThresholdReached;
  mControllerDragThresholdReached = false;
  mControllerLastEventTimeStamp = 0;
  bool cancelTouchTracking = mControllerCancelTouchTracking;
  mControllerCancelTouchTracking = false;

  // Animation is in progress, bail out.
  if (aCurrentToolbarState == eToolbarAnimating) {
    return;
  }

  // Received a UI thread request to show or hide the snapshot during a touch.
  // This overrides the touch event so just return.
  if (cancelTouchTracking) {
    return;
  }

  // The drag threshold has not been reach and the toolbar is either completely visible or completely hidden.
  if (!dragThresholdReached && !ToolbarInTransition()) {
    ShowToolbarIfNotVisible(aCurrentToolbarState);
    return;
  }

  // The toolbar is already where it needs to be so just return.
  if (((direction == MOVE_TOOLBAR_DOWN) && (mControllerToolbarHeight == mControllerMaxToolbarHeight)) ||
      ((direction == MOVE_TOOLBAR_UP) && (mControllerToolbarHeight == 0))) {
    ShowToolbarIfNotVisible(aCurrentToolbarState);
    return;
  }

  // Don't animate up if not scrolling root content. Even though ShowToolbarIfNotVisible checks if
  // snapshot toolbar is completely visible before showing, we don't want to enter this if block
  // if the snapshot toolbar isn't completely visible to avoid early return.
  if (!isRoot &&
      ((direction == MOVE_TOOLBAR_UP) && (mControllerToolbarHeight == mControllerMaxToolbarHeight))) {
    ShowToolbarIfNotVisible(aCurrentToolbarState);
    return;
  }

  if (ScrollOffsetNearBottom()) {
    if (ToolbarInTransition()) {
      // Toolbar is partially visible so make it visible since we are near the end of the page
      direction = MOVE_TOOLBAR_DOWN;
    } else {
      // Don't start an animation if near the bottom of page and toolbar is completely visible or hidden
      ShowToolbarIfNotVisible(aCurrentToolbarState);
      return;
    }
  }

  StartCompositorAnimation(direction, eAnimate, mControllerToolbarHeight, ScrollOffsetNearBottom());
}

void
AndroidDynamicToolbarAnimator::PostMessage(int32_t aMessage) {
  // if the root layer tree id is zero then Initialize() has not been called yet
  // so queue the message until Initialize() is called.
  if (mRootLayerTreeId == 0) {
    QueueMessage(aMessage);
    return;
  }

  RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeId);
  if (!uiController) {
    // Looks like IPC may be shutdown.
    return;
  }

  // ToolbarAnimatorMessageFromCompositor may be called from any thread.
  uiController->ToolbarAnimatorMessageFromCompositor(aMessage);
}

void
AndroidDynamicToolbarAnimator::UpdateCompositorToolbarHeight(ScreenIntCoord aHeight)
{
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<ScreenIntCoord>(this, &AndroidDynamicToolbarAnimator::UpdateCompositorToolbarHeight, aHeight));
    return;
  }

  mCompositorToolbarHeight = aHeight;
  UpdateFixedLayerMargins();
}

void
AndroidDynamicToolbarAnimator::UpdateControllerToolbarHeight(ScreenIntCoord aHeight, ScreenIntCoord aMaxHeight)
{
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<ScreenIntCoord, ScreenIntCoord>(this, &AndroidDynamicToolbarAnimator::UpdateControllerToolbarHeight, aHeight, aMaxHeight));
    return;
  }

  mControllerToolbarHeight = aHeight;
  if (aMaxHeight >= 0) {
    mControllerMaxToolbarHeight = aMaxHeight;
  }
}

void
AndroidDynamicToolbarAnimator::UpdateControllerSurfaceHeight(ScreenIntCoord aHeight)
{
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<ScreenIntCoord>(this, &AndroidDynamicToolbarAnimator::UpdateControllerSurfaceHeight, aHeight));
    return;
  }

  mControllerSurfaceHeight = aHeight;
}

void
AndroidDynamicToolbarAnimator::UpdateControllerCompositionHeight(ScreenIntCoord aHeight)
{
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<ScreenIntCoord>(this, &AndroidDynamicToolbarAnimator::UpdateControllerCompositionHeight, aHeight));
    return;
  }

  mControllerCompositionHeight = aHeight;
}

// Ensures the margin for the fixed layers match the position of the toolbar
void
AndroidDynamicToolbarAnimator::UpdateFixedLayerMargins()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mCompositorShutdown) {
    return;
  }
  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
  if (parent) {
    ScreenIntCoord surfaceHeight = parent->GetEGLSurfaceSize().height;
    if (surfaceHeight != mCompositorSurfaceHeight) {
      mCompositorSurfaceHeight = surfaceHeight;
      UpdateControllerSurfaceHeight(mCompositorSurfaceHeight);
    }
    AsyncCompositionManager* manager = parent->GetCompositionManager(nullptr);
    if (manager) {
      if ((mToolbarState == eToolbarAnimating) && mCompositorAnimationStarted) {
        manager->SetFixedLayerMargins(mCompositorToolbarHeight, 0);
      } else {
        manager->SetFixedLayerMargins(0, GetFixedLayerMarginsBottom());
      }
    }
  }
}

void
AndroidDynamicToolbarAnimator::NotifyControllerPendingAnimation(int32_t aDirection, AnimationStyle aAnimationStyle)
{
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<int32_t, AnimationStyle>(this, &AndroidDynamicToolbarAnimator::NotifyControllerPendingAnimation, aDirection, aAnimationStyle));
    return;
  }

  mControllerCancelTouchTracking = true;

  // If the toolbar is already where it needs to be, just abort the request.
  if (((mControllerToolbarHeight == mControllerMaxToolbarHeight) && (aDirection == MOVE_TOOLBAR_DOWN)) ||
      ((mControllerToolbarHeight == 0) && (aDirection == MOVE_TOOLBAR_UP))) {
    // We received a show request but the real toolbar is hidden, so tell it to show now.
    if ((aDirection == MOVE_TOOLBAR_DOWN) && (mToolbarState == eToolbarUnlocked)) {
      PostMessage(TOOLBAR_SHOW);
    }
    return;
  }

  // NOTE: StartCompositorAnimation will set mControllerState to eAnimationStartPending
  StartCompositorAnimation(aDirection, aAnimationStyle, mControllerToolbarHeight, ScrollOffsetNearBottom());
  MOZ_ASSERT(mControllerState == eAnimationStartPending);
}

void
AndroidDynamicToolbarAnimator::StartCompositorAnimation(int32_t aDirection, AnimationStyle aAnimationStyle, ScreenIntCoord aHeight, bool aWaitForPageResize)
{
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    mControllerState = eAnimationStartPending;
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<int32_t, AnimationStyle, ScreenIntCoord, bool>(
      this, &AndroidDynamicToolbarAnimator::StartCompositorAnimation, aDirection, aAnimationStyle, aHeight, aWaitForPageResize));
    return;
  }

  MOZ_ASSERT(aDirection == MOVE_TOOLBAR_UP || aDirection == MOVE_TOOLBAR_DOWN);

  const StaticToolbarState initialToolbarState = mToolbarState;
  mCompositorAnimationDirection = aDirection;
  mCompositorAnimationStartHeight = mCompositorToolbarHeight = aHeight;
  mCompositorAnimationStyle = aAnimationStyle;
  mCompositorWaitForPageResize = aWaitForPageResize;
  // If the snapshot is not unlocked, request the UI thread update the snapshot
  // and defer animation until it has been unlocked
  if ((initialToolbarState != eToolbarUnlocked) && (initialToolbarState != eToolbarAnimating)) {
    mCompositorAnimationDeferred = true;
    mCompositorSendResponseForSnapshotUpdate = true;
    PostMessage(STATIC_TOOLBAR_NEEDS_UPDATE);
  } else {
    // Toolbar is either unlocked or already animating so animation may begin immediately
    mCompositorAnimationDeferred = false;
    mToolbarState = eToolbarAnimating;
    if (initialToolbarState != eToolbarAnimating) {
      mCompositorAnimationStarted = false;
    }
    // Let the controller know we are starting an animation so it may clear the AnimationStartPending flag.
    NotifyControllerAnimationStarted();
    // Only reset the time stamp and start compositor animation if not already animating.
    if (initialToolbarState != eToolbarAnimating) {
      CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
      if (parent) {
        mCompositorAnimationStartTimeStamp = parent->GetAPZCTreeManager()->GetFrameTime();
      }
      // Kick the compositor to start the animation if we aren't already animating.
      RequestComposite();
    }
  }
}

void
AndroidDynamicToolbarAnimator::NotifyControllerAnimationStarted()
{
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod(this, &AndroidDynamicToolbarAnimator::NotifyControllerAnimationStarted));
    return;
  }

  // It is possible there was a stop request after the start request so only set to NothingPending
  // if start is what were are still waiting for.
  if (mControllerState == eAnimationStartPending) {
    mControllerState = eNothingPending;
  }
}

void
AndroidDynamicToolbarAnimator::StopCompositorAnimation()
{
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    mControllerState = eAnimationStopPending;
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod(this, &AndroidDynamicToolbarAnimator::StopCompositorAnimation));
    return;
  }

  if (mToolbarState == eToolbarAnimating) {
    if (mCompositorAnimationStarted) {
      mCompositorAnimationStarted = false;
      CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
      if (parent) {
        AsyncCompositionManager* manager = parent->GetCompositionManager(nullptr);
        if (manager) {
            parent->GetAPZCTreeManager()->AdjustScrollForSurfaceShift(ScreenPoint(0.0f, (float)(mCompositorToolbarHeight)));
            RequestComposite();
        }
      }
    }
    mToolbarState = eToolbarUnlocked;
  }

  NotifyControllerAnimationStopped(mCompositorToolbarHeight);
}

void
AndroidDynamicToolbarAnimator::NotifyControllerAnimationStopped(ScreenIntCoord aHeight)
{
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<ScreenIntCoord>(this, &AndroidDynamicToolbarAnimator::NotifyControllerAnimationStopped, aHeight));
    return;
  }

  if (mControllerState == eAnimationStopPending) {
    mControllerState = eNothingPending;
  }

  mControllerToolbarHeight = aHeight;
}

void
AndroidDynamicToolbarAnimator::RequestComposite()
{
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod(this, &AndroidDynamicToolbarAnimator::RequestComposite));
    return;
  }

  if (mCompositorShutdown) {
    return;
  }

  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
  if (parent) {
    AsyncCompositionManager* manager = parent->GetCompositionManager(nullptr);
    if (manager) {
      if ((mToolbarState == eToolbarAnimating) && mCompositorAnimationStarted) {
        manager->SetFixedLayerMargins(mCompositorToolbarHeight, 0);
      } else {
        manager->SetFixedLayerMargins(0, GetFixedLayerMarginsBottom());
      }
      parent->Invalidate();
      parent->ScheduleComposition();
    }
  }
}

void
AndroidDynamicToolbarAnimator::PostToolbarReady()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RequestComposite();
  // Notify the UI thread the static toolbar is being rendered so the real
  // toolbar needs to be hidden. Once the TOOLBAR_HIDDEN message is
  // received, a pending animation may start or the toolbar snapshot may be
  // translated.
  PostMessage(STATIC_TOOLBAR_READY);
  if (mToolbarState != eToolbarAnimating) {
    mToolbarState = eToolbarUpdated;
  } else {
    // The compositor is already animating the toolbar so no need to defer.
    mCompositorAnimationDeferred = false;
  }
}

void
AndroidDynamicToolbarAnimator::UpdateFrameMetrics(ScreenPoint aScrollOffset,
                                                  CSSToScreenScale aScale,
                                                  CSSRect aCssPageRect)
{
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<ScreenPoint, CSSToScreenScale, CSSRect>(this, &AndroidDynamicToolbarAnimator::UpdateFrameMetrics, aScrollOffset, aScale, aCssPageRect));
    return;
  }

  mControllerRootScrollY = aScrollOffset.y;

  if (mControllerFrameMetrics.Update(aScrollOffset, aScale, aCssPageRect)) {
    if (FuzzyEqualsMultiplicative(mControllerFrameMetrics.mPageRect.YMost(), mControllerCompositionHeight + mControllerFrameMetrics.mScrollOffset.y) &&
        (mControllerFrameMetrics.mPageRect.YMost() > (mControllerSurfaceHeight * 2)) &&
        (mControllerToolbarHeight != mControllerMaxToolbarHeight) &&
        !mPinnedFlags) {
      // The end of the page has been reached, the page is twice the height of the visible height,
      // and the toolbar is not completely visible so animate it into view.
      StartCompositorAnimation(MOVE_TOOLBAR_DOWN, eAnimate, mControllerToolbarHeight, /* wait for page resize */ true);
    }
    RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeId);
    MOZ_ASSERT(uiController);
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<ScreenPoint, CSSToScreenScale, CSSRect>(
                                               uiController, &UiCompositorControllerParent::SendRootFrameMetrics,
                                               aScrollOffset, aScale, aCssPageRect));
  }
}

bool
AndroidDynamicToolbarAnimator::PageTooSmallEnsureToolbarVisible()
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  // if the page is too small then the toolbar can not be hidden
  if ((float)mControllerSurfaceHeight >= (mControllerFrameMetrics.mPageRect.YMost() * SHRINK_FACTOR)) {
    // If the toolbar is partial hidden, show it.
    if ((mControllerToolbarHeight != mControllerMaxToolbarHeight) && !mPinnedFlags) {
      StartCompositorAnimation(MOVE_TOOLBAR_DOWN, eImmediate, mControllerToolbarHeight, /* wait for page resize */ true);
    }
    return true;
  }

  return false;
}

void
AndroidDynamicToolbarAnimator::ShowToolbarIfNotVisible(StaticToolbarState aCurrentToolbarState)
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  if ((mControllerToolbarHeight == mControllerMaxToolbarHeight) &&
      (aCurrentToolbarState != eToolbarVisible) &&
      (mControllerState != eShowPending)) {
    mControllerState = eShowPending;
    PostMessage(TOOLBAR_SHOW);
  }
}

bool
AndroidDynamicToolbarAnimator::FrameMetricsState::Update(const ScreenPoint& aScrollOffset,
                                                         const CSSToScreenScale& aScale,
                                                         const CSSRect& aCssPageRect)
{
  if (!FuzzyEqualsMultiplicative(aScrollOffset.x, mScrollOffset.x) ||
      !FuzzyEqualsMultiplicative(aScrollOffset.y, mScrollOffset.y) ||
      !FuzzyEqualsMultiplicative(aScale.scale, mScale.scale) ||
      !FuzzyEqualsMultiplicative(aCssPageRect.width, mCssPageRect.width) ||
      !FuzzyEqualsMultiplicative(aCssPageRect.height, mCssPageRect.height) ||
      !FuzzyEqualsMultiplicative(aCssPageRect.x, mCssPageRect.x) ||
      !FuzzyEqualsMultiplicative(aCssPageRect.y, mCssPageRect.y)) {
    mScrollOffset = aScrollOffset;
    mScale = aScale;
    mCssPageRect = aCssPageRect;
    mPageRect = mCssPageRect * mScale;
    return true;
  }

  return false;
}

void
AndroidDynamicToolbarAnimator::TranslateTouchEvent(MultiTouchInput& aTouchEvent)
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  if (mControllerToolbarHeight > 0) {
    aTouchEvent.Translate(ScreenPoint(0.0f, -(float)mControllerToolbarHeight));
  }
}

ScreenIntCoord
AndroidDynamicToolbarAnimator::GetFixedLayerMarginsBottom()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mCompositorToolbarHeight - (mCompositorSurfaceHeight - mCompositorCompositionSize.height);
}

void
AndroidDynamicToolbarAnimator::NotifyControllerSnapshotFailed()
{
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod(this, &AndroidDynamicToolbarAnimator::NotifyControllerSnapshotFailed));
    return;
  }

  mControllerToolbarHeight = 0;
  mControllerState = eNothingPending;
  UpdateCompositorToolbarHeight(mControllerToolbarHeight);
}

void
AndroidDynamicToolbarAnimator::CheckForResetOnNextMove(ScreenIntCoord aCurrentTouch) {
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  if (mControllerResetOnNextMove) {
    mControllerTotalDistance = 0;
    mControllerLastDragDirection = 0;
    mControllerStartTouch = mControllerPreviousTouch = aCurrentTouch;
    mControllerDragThresholdReached = false;
    mControllerResetOnNextMove = false;
  }
}

bool
AndroidDynamicToolbarAnimator::ScrollOffsetNearBottom() const
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  // Twice the toolbar's height is considered near the bottom of the page.
  if ((mControllerToolbarHeight * 2) >= (mControllerFrameMetrics.mPageRect.YMost() - (mControllerRootScrollY + ScreenCoord(mControllerCompositionHeight)))) {
    return true;
  }
  return false;
}

bool
AndroidDynamicToolbarAnimator::ToolbarInTransition()
{
  if (APZThreadUtils::IsControllerThread()) {
    return (mControllerToolbarHeight != mControllerMaxToolbarHeight) && (mControllerToolbarHeight != 0);
  }

  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return (mCompositorToolbarHeight != mCompositorMaxToolbarHeight) && (mCompositorToolbarHeight != 0);
}

void
AndroidDynamicToolbarAnimator::QueueMessage(int32_t aMessage)
{
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<int32_t>(this, &AndroidDynamicToolbarAnimator::QueueMessage, aMessage));
    return;
  }

  // If the root layer tree id is no longer zero, Initialize() was called before QueueMessage was processed
  // so just post the message now.
  if (mRootLayerTreeId != 0) {
    PostMessage(aMessage);
    return;
  }

  mCompositorQueuedMessages.insertBack(new QueuedMessage(aMessage));
}

} // namespace layers
} // namespace mozilla
