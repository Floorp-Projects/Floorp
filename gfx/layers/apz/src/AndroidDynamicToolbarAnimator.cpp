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
static const float   ANIMATION_DURATION  = 0.15f; // How many seconds the complete animation should span
static const int32_t MOVE_TOOLBAR_DOWN =  1;      // Multiplier to move the toolbar down
static const int32_t MOVE_TOOLBAR_UP   = -1;      // Multiplier to move the toolbar up
static const float   SHRINK_FACTOR     = 0.95f;   // Amount to shrink the either the full content for small pages or the amount left
                                                  // See: IsEnoughPageToHideToolbar()
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
  , mControllerStartTouch(0)
  , mControllerPreviousTouch(0)
  , mControllerTotalDistance(0)
  , mControllerMaxToolbarHeight(0)
  , mControllerToolbarHeight(0)
  , mControllerSurfaceHeight(0)
  , mControllerCompositionHeight(0)
  , mControllerLastDragDirection(0)
  , mControllerLastEventTimeStamp(0)
  , mControllerState(eNothingPending)
  // Compositor thread only
  , mCompositorShutdown(false)
  , mCompositorAnimationDeferred(false)
  , mCompositorLayersUpdateEnabled(false)
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
  mRootLayerTreeId = aRootLayerTreeId;
  RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeId);
  MOZ_ASSERT(uiController);
  uiController->RegisterAndroidDynamicToolbarAnimator(this);
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
AndroidDynamicToolbarAnimator::ReceiveInputEvent(InputData& aEvent)
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());

  // Only process and adjust touch events. Wheel events (aka scroll events) are adjusted in the NativePanZoomController
  if (aEvent.mInputType != MULTITOUCH_INPUT) {
    return nsEventStatus_eIgnore;
  }

  MultiTouchInput& multiTouch = aEvent.AsMultiTouchInput();
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
    if (currentToolbarState == eToolbarAnimating) {
      StopCompositorAnimation();
    }
    break;
  case MultiTouchInput::MULTITOUCH_MOVE: {
    if ((mControllerState != eAnimationStartPending) &&
        (mControllerState != eAnimationStopPending) &&
        (currentToolbarState != eToolbarAnimating) &&
        !mControllerCancelTouchTracking) {

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
      if (IsEnoughPageToHideToolbar(delta)) {
        // NOTE: gfxPrefs::ToolbarScrollThreshold() returns a percentage as an intt32_t. So multiply it by 0.01f to convert.
        const uint32_t dragThreshold = Abs(std::lround(0.01f * gfxPrefs::ToolbarScrollThreshold() * mControllerCompositionHeight));
        if ((Abs(mControllerTotalDistance.value) > dragThreshold) && (delta != 0)) {
          mControllerDragThresholdReached = true;
          status = ProcessTouchDelta(currentToolbarState, delta, multiTouch.mTime);
        }
      }
      mControllerLastEventTimeStamp = multiTouch.mTime;
    }
    break;
  }
  case MultiTouchInput::MULTITOUCH_END:
  case MultiTouchInput::MULTITOUCH_CANCEL:
    HandleTouchEnd(currentToolbarState, currentTouch);
    break;
  default:
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

bool
AndroidDynamicToolbarAnimator::SetCompositionSize(ScreenIntSize aSize)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mCompositorCompositionSize == aSize) {
    return false;
  }

  ScreenIntCoord prevHeight = mCompositorCompositionSize.height;
  mCompositorCompositionSize = aSize;

  if (prevHeight != aSize.height) {
    UpdateControllerCompositionHeight(aSize.height);
    UpdateFixedLayerMargins();
  }

  return true;
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
        StartCompositorAnimation(mCompositorAnimationDirection, mCompositorAnimationStyle, mCompositorToolbarHeight);
      }
    } else {
      // The compositor is already animating the toolbar so no need to defer.
      mCompositorAnimationDeferred = false;
    }
    break;
  case TOOLBAR_VISIBLE:
    mToolbarState = eToolbarVisible;
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
  default:
    break;
  }
}

bool
AndroidDynamicToolbarAnimator::UpdateAnimation(const TimeStamp& aCurrentFrame)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mToolbarState != eToolbarAnimating) {
    return false;
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
    continueAnimating = false;
    mToolbarState = eToolbarVisible;
    PostMessage(TOOLBAR_SHOW);
    mCompositorToolbarHeight = mCompositorMaxToolbarHeight;
  } else if ((mCompositorAnimationDirection == MOVE_TOOLBAR_UP) && (mCompositorToolbarHeight <= 0)) {
    continueAnimating = false;
    mToolbarState = eToolbarUnlocked;
    mCompositorToolbarHeight = 0;
  }

  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
  if (parent) {
    AsyncCompositionManager* manager = parent->GetCompositionManager(nullptr);
    if (manager) {
      manager->SetFixedLayerMarginsBottom(GetFixedLayerMarginsBottom());
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
    if (mCompositorToolbarTexture) {
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
      PostMessage(TOOLBAR_SHOW);
      mControllerState = eShowPending;
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
  int32_t direction = mControllerLastDragDirection;
  mControllerLastDragDirection = 0;
  bool isRoot = mControllerScrollingRootContent;
  mControllerScrollingRootContent = false;
  bool dragChangedDirection = mControllerDragChangedDirection;
  mControllerDragChangedDirection = false;

  // If the drag direction changed and the toolbar is partially visible, hide in the direction with the least distance to travel.
  if (dragChangedDirection &&
      (mControllerToolbarHeight != mControllerMaxToolbarHeight) &&
      (mControllerToolbarHeight != 0)) {
    direction = ((float)mControllerToolbarHeight / (float)mControllerMaxToolbarHeight) < 0.5f ? MOVE_TOOLBAR_UP : MOVE_TOOLBAR_DOWN;
  }

  // If the last touch didn't have a drag direction, use start of touch to find direction
  if (!direction) {
    direction = ((aCurrentTouch - mControllerStartTouch) > 0 ? MOVE_TOOLBAR_DOWN : MOVE_TOOLBAR_UP);
    // If there still isn't a direction, default to show just to be safe
    if (!direction) {
      direction = MOVE_TOOLBAR_DOWN;
    }
  }
  bool dragThresholdReached = mControllerDragThresholdReached;
  mControllerStartTouch = 0;
  mControllerPreviousTouch = 0;
  mControllerTotalDistance = 0;
  mControllerDragThresholdReached = false;
  mControllerLastEventTimeStamp = 0;

  // Received a UI thread request to show or hide the snapshot during a touch.
  // This overrides the touch event so just return
  if (mControllerCancelTouchTracking) {
    mControllerCancelTouchTracking = false;
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

  // The page is either too small or too close to the end to animate
  if (!IsEnoughPageToHideToolbar(direction)) {
    if (mControllerToolbarHeight == mControllerMaxToolbarHeight) {
      ShowToolbarIfNotVisible(aCurrentToolbarState);
      return;
    } else if (mControllerToolbarHeight != 0) {
      // The snapshot is partially visible but there is not enough page
      // to hide the snapshot so make it visible by moving it down
      direction = MOVE_TOOLBAR_DOWN;
    }
  }

  // This makes sure the snapshot is not left partially visible at the end of a touch.
  if ((aCurrentToolbarState != eToolbarAnimating) && dragThresholdReached) {
    if (((direction == MOVE_TOOLBAR_DOWN) && (mControllerToolbarHeight != mControllerMaxToolbarHeight)) ||
        ((direction == MOVE_TOOLBAR_UP) && (mControllerToolbarHeight != 0))) {
      StartCompositorAnimation(direction, eAnimate, mControllerToolbarHeight);
    }
  } else {
    ShowToolbarIfNotVisible(aCurrentToolbarState);
  }
}

void
AndroidDynamicToolbarAnimator::PostMessage(int32_t aMessage) {
  RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeId);
  MOZ_ASSERT(uiController);
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
  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
  if (parent) {
    ScreenIntCoord surfaceHeight = parent->GetEGLSurfaceSize().height;
    if (surfaceHeight != mCompositorSurfaceHeight) {
      mCompositorSurfaceHeight = surfaceHeight;
      UpdateControllerSurfaceHeight(mCompositorSurfaceHeight);
    }
    AsyncCompositionManager* manager = parent->GetCompositionManager(nullptr);
    if (manager) {
      manager->SetFixedLayerMarginsBottom(GetFixedLayerMarginsBottom());
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
  StartCompositorAnimation(aDirection, aAnimationStyle, mControllerToolbarHeight);
  MOZ_ASSERT(mControllerState == eAnimationStartPending);
}

void
AndroidDynamicToolbarAnimator::StartCompositorAnimation(int32_t aDirection, AnimationStyle aAnimationStyle, ScreenIntCoord aHeight)
{
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    mControllerState = eAnimationStartPending;
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<int32_t, AnimationStyle, ScreenIntCoord>(
      this, &AndroidDynamicToolbarAnimator::StartCompositorAnimation, aDirection, aAnimationStyle, aHeight));
    return;
  }

  MOZ_ASSERT(aDirection == MOVE_TOOLBAR_UP || aDirection == MOVE_TOOLBAR_DOWN);

  const StaticToolbarState currentToolbarState = mToolbarState;
  mCompositorAnimationDirection = aDirection;
  mCompositorAnimationStartHeight = mCompositorToolbarHeight = aHeight;
  mCompositorAnimationStyle = aAnimationStyle;
  // If the snapshot is not unlocked, request the UI thread update the snapshot
  // and defer animation until it has been unlocked
  if (currentToolbarState != eToolbarUnlocked) {
    mCompositorAnimationDeferred = true;
    PostMessage(STATIC_TOOLBAR_NEEDS_UPDATE);
  } else {
    // Toolbar is unlocked so animation may begin immediately
    mCompositorAnimationDeferred = false;
    mToolbarState = eToolbarAnimating;
    // Let the controller know we starting an animation so it may clear the AnimationStartPending flag.
    NotifyControllerAnimationStarted();
    // Kick the compositor to start the animation
    CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
    if (parent) {
      mCompositorAnimationStartTimeStamp = parent->GetAPZCTreeManager()->GetFrameTime();
    }
    RequestComposite();
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

  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(mRootLayerTreeId);
  if (parent) {
    AsyncCompositionManager* manager = parent->GetCompositionManager(nullptr);
    if (manager) {
      manager->SetFixedLayerMarginsBottom(GetFixedLayerMarginsBottom());
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

  if (mControllerFrameMetrics.Update(aScrollOffset, aScale, aCssPageRect)) {
    RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeId);
    MOZ_ASSERT(uiController);
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<ScreenPoint, CSSToScreenScale, CSSRect>(
                                               uiController, &UiCompositorControllerParent::SendRootFrameMetrics,
                                               aScrollOffset, aScale, aCssPageRect));
  }
}

bool
AndroidDynamicToolbarAnimator::IsEnoughPageToHideToolbar(ScreenIntCoord delta)
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  // The toolbar will only hide if dragging up so ignore positive deltas from dragging down.
  if (delta >= 0) {
    return true;
  }

  // if the page is 1) too small or 2) too close to the bottom, then the toolbar can not be hidden
  if (((float)mControllerSurfaceHeight >= (mControllerFrameMetrics.mPageRect.YMost() * SHRINK_FACTOR)) ||
      ((float)mControllerSurfaceHeight >= ((mControllerFrameMetrics.mPageRect.YMost() - mControllerFrameMetrics.mScrollOffset.y) * SHRINK_FACTOR))) {
    return false;
  }

  return true;
}

void
AndroidDynamicToolbarAnimator::ShowToolbarIfNotVisible(StaticToolbarState aCurrentToolbarState)
{
  MOZ_ASSERT(APZThreadUtils::IsControllerThread());
  if ((mControllerToolbarHeight == mControllerMaxToolbarHeight) &&
      (aCurrentToolbarState != eToolbarVisible) &&
      (mControllerState != eShowPending)) {
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

} // namespace layers
} // namespace mozilla
