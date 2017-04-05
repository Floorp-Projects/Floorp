/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AndroidDynamicToolbarAnimator_h_
#define mozilla_layers_AndroidDynamicToolbarAnimator_h_

#include "InputData.h"
#include "mozilla/Atomics.h"
#include "mozilla/EventForwards.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsISupports.h"

namespace mozilla {
namespace layers {

struct FrameMetrics;
class CompositorOGL;

/*
 * The AndroidDynamicToolbarAnimator is responsible for calculating the position
 * and drawing the static snapshot of the toolbar. The animator lives in both
 * compositor thread and controller thread. It intercepts input events in the
 * controller thread and determines if the intercepted touch events will cause
 * the toolbar to move or be animated. Once the proper conditions have been met,
 * the animator requests that the UI thread send a static snapshot of the current
 * state of the toolbar. Once the animator has received the snapshot and
 * converted it into an OGL texture, the animator notifies the UI thread it is
 * ready. The UI thread will then hide the real toolbar and notify the animator
 * that it is unlocked and may begin translating the snapshot. The
 * animator is responsible for rendering the snapshot until it receives a message
 * to show the toolbar or touch events cause the snapshot to be completely visible.
 * When the snapshot is made completely visible the animator locks the static
 * toolbar and sends a message to the UI thread to show the real toolbar and the
 * whole process may start again. The toolbar height is in screen pixels. The
 * toolbar height will be at max height when completely visible and at 0 when
 * completely hidden. The toolbar is only locked when it is completely visible.
 * The animator must ask for an update of the toolbar snapshot and that the real
 * toolbar be hidden in order to unlock the static snapshot and begin translating it.
 *
 * See Bug 1335895 for more details.
 */

class AndroidDynamicToolbarAnimator {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AndroidDynamicToolbarAnimator);
  AndroidDynamicToolbarAnimator();
  void Initialize(uint64_t aRootLayerTreeId);
  // Used to intercept events to determine if the event affects the toolbar.
  // May apply translation to touch events if the toolbar is visible.
  // Returns nsEventStatus_eIgnore when the event is not consumed and
  // nsEventStatus_eConsumeNoDefault when the event was used to translate the
  // toolbar.
  nsEventStatus ReceiveInputEvent(InputData& aEvent);
  void SetMaxToolbarHeight(ScreenIntCoord aHeight);
  // When a pinned reason is set to true, the animator will prevent
  // touch events from altering the height of the toolbar. All pinned
  // reasons must be cleared before touch events will affect the toolbar.
  // Animation requests from the UI thread are still honored even if any
  // pin reason is set. This allows the UI thread to pin the toolbar for
  // full screen and then request the animator hide the toolbar.
  void SetPinned(bool aPinned, int32_t aReason);
  // returns maximum number of Y device pixels the toolbar will cover when fully visible.
  ScreenIntCoord GetMaxToolbarHeight() const;
  // returns the current number of Y device pixels the toolbar is currently showing.
  ScreenIntCoord GetCurrentToolbarHeight() const;
  // returns the height in device pixels of the current Android surface used to display content and the toolbar.
  // This will only change when the surface provided by the system actually changes size such as when
  // the device is rotated or the virtual keyboard is made visible.
  ScreenIntCoord GetCurrentSurfaceHeight() const;
  // This is the height in device pixels of the root document's content. While the toolbar is being hidden or
  // shown, the content may extend beyond the bottom of the surface until the toolbar is completely
  // visible or hidden.
  ScreenIntCoord GetCompositionHeight() const;
  // Returns true if the composition size has changed from the last time it was set.
  bool SetCompositionSize(ScreenIntSize aSize);
  // Called to signal that root content is being scrolled. This prevents sub scroll frames from
  // affecting the toolbar when being scrolled up. The idea is a scrolling down will always
  // show the toolbar while scrolling up will only hide the toolbar if it is the root content
  // being scrolled.
  void SetScrollingRootContent();
  void ToolbarAnimatorMessageFromUI(int32_t aMessage);
  // Returns true if the animation will continue and false if it has completed.
  bool UpdateAnimation(const TimeStamp& aCurrentFrame);
  // Called to signify the first paint has occurred.
  void FirstPaint();
  // Called whenever the root document's FrameMetrics have reached a steady state.
  void UpdateRootFrameMetrics(const FrameMetrics& aMetrics);
  // When aEnable is set to true, it informs the animator that the UI thread expects to
  // be notified when the layer tree  has been updated. Enabled currently by robocop tests.
  void EnableLayersUpdateNotifications(bool aEnable);
  // Called when a layer has been updated so the UI thread may be notified if necessary.
  void NotifyLayersUpdated();
  // Adopts the Shmem containing the toolbar snapshot sent from the UI thread.
  // The AndroidDynamicToolbarAnimator is responsible for deallocating the Shmem when
  // it is done being used.
  void AdoptToolbarPixels(mozilla::ipc::Shmem&& aMem, const ScreenIntSize& aSize);
  // Returns the Effect object used by the compositor to render the toolbar snapshot.
  Effect* GetToolbarEffect(CompositorOGL* gl);
  void Shutdown();

protected:
  enum StaticToolbarState {
    eToolbarVisible,
    eToolbarUpdated,
    eToolbarUnlocked,
    eToolbarAnimating
  };
  enum ControllerThreadState {
    eNothingPending,
    eShowPending,
    eUnlockPending,
    eAnimationStartPending,
    eAnimationStopPending
  };
  enum AnimationStyle {
    eImmediate,
    eAnimate
  };

  ~AndroidDynamicToolbarAnimator(){}
  nsEventStatus ProcessTouchDelta(StaticToolbarState aCurrentToolbarState, ScreenIntCoord aDelta, uint32_t aTimeStamp);
  // Called when a touch ends
  void HandleTouchEnd(StaticToolbarState aCurrentToolbarState, ScreenIntCoord aCurrentTouch);
  // Sends a message to the UI thread. May be called from any thread
  void PostMessage(int32_t aMessage);
  void UpdateCompositorToolbarHeight(ScreenIntCoord aHeight);
  void UpdateControllerToolbarHeight(ScreenIntCoord aHeight, ScreenIntCoord aMaxHeight = -1);
  void UpdateControllerSurfaceHeight(ScreenIntCoord aHeight);
  void UpdateControllerCompositionHeight(ScreenIntCoord aHeight);
  void UpdateFixedLayerMargins();
  void NotifyControllerPendingAnimation(int32_t aDirection, AnimationStyle aStyle);
  void StartCompositorAnimation(int32_t aDirection, AnimationStyle aStyle, ScreenIntCoord aHeight);
  void NotifyControllerAnimationStarted();
  void StopCompositorAnimation();
  void NotifyControllerAnimationStopped(ScreenIntCoord aHeight);
  void RequestComposite();
  void PostToolbarReady();
  void UpdateFrameMetrics(ScreenPoint aScrollOffset,
                          CSSToScreenScale aScale,
                          CSSRect aCssPageRect);
  bool IsEnoughPageToHideToolbar(ScreenIntCoord delta);
  void ShowToolbarIfNotVisible(StaticToolbarState aCurrentToolbarState);
  void TranslateTouchEvent(MultiTouchInput& aTouchEvent);
  ScreenIntCoord GetFixedLayerMarginsBottom();

  // Read only Compositor and Controller threads after Initialize()
  uint64_t mRootLayerTreeId;

  // Read/Write Compositor Thread, Read only Controller thread
  Atomic<StaticToolbarState> mToolbarState; // Current toolbar state.
  Atomic<uint32_t> mPinnedFlags;            // The toolbar should not be moved or animated unless no flags are set

  // Controller thread only
  bool    mControllerScrollingRootContent;     // Set to true when the root content is being scrolled
  bool    mControllerDragThresholdReached;     // Set to true when the drag threshold has been passed in a single drag
  bool    mControllerCancelTouchTracking;      // Set to true when the UI thread requests the toolbar be made visible
  bool    mControllerDragChangedDirection;     // Set to true if the drag ever goes in more than one direction
  ScreenIntCoord mControllerStartTouch;        // The Y position where the touch started
  ScreenIntCoord mControllerPreviousTouch;     // The previous Y position of the touch
  ScreenIntCoord mControllerTotalDistance;     // Total distance travel during the current touch
  ScreenIntCoord mControllerMaxToolbarHeight;  // Max height of the toolbar
  ScreenIntCoord mControllerToolbarHeight;     // Current height of the toolbar
  ScreenIntCoord mControllerSurfaceHeight;     // Current height of the render surface
  ScreenIntCoord mControllerCompositionHeight; // Current height of the visible page
  int32_t mControllerLastDragDirection;        // Direction of movement of the previous touch move event
  uint32_t mControllerLastEventTimeStamp;      // Time stamp for the previous touch event received
  ControllerThreadState mControllerState;      // Contains the expected pending state of the mToolbarState

  // Contains the values from the last steady state root content FrameMetrics
  struct FrameMetricsState {
    ScreenPoint mScrollOffset;
    CSSToScreenScale mScale;
    CSSRect mCssPageRect;
    ScreenRect mPageRect;

    // Returns true if any of the values have changed.
    bool Update(const ScreenPoint& aScrollOffset,
                const CSSToScreenScale& aScale,
                const CSSRect& aCssPageRect);
  };

  // Controller thread only
  FrameMetricsState mControllerFrameMetrics;

  // Compositor thread only
  bool    mCompositorShutdown;
  bool    mCompositorAnimationDeferred;           // An animation has been deferred until the toolbar is unlocked
  bool    mCompositorLayersUpdateEnabled;         // Flag set to true when the UI thread is expecting to be notified when a layer has been updated
  AnimationStyle mCompositorAnimationStyle;       // Set to true when the snap should be immediately hidden or shown in the animation update
  ScreenIntCoord mCompositorMaxToolbarHeight;     // Should contain the same value as mControllerMaxToolbarHeight
  ScreenIntCoord mCompositorToolbarHeight;        // This value is only updated by the compositor thread when the mToolbarState == ToolbarAnimating
  ScreenIntCoord mCompositorSurfaceHeight;        // Current height of the render surface
  ScreenIntSize  mCompositorCompositionSize;      // Current size of the visible page
  int32_t mCompositorAnimationDirection;          // Direction the snapshot should be animated
  ScreenIntCoord mCompositorAnimationStartHeight; // The height of the snapshot at the start of an animation
  ScreenIntSize mCompositorToolbarPixelsSize;     // Size of the received toolbar pixels
  Maybe<mozilla::ipc::Shmem> mCompositorToolbarPixels; // Shared memory contain the updated snapshot pixels used to create the OGL texture
  RefPtr<DataTextureSource> mCompositorToolbarTexture; // The OGL texture used to render the snapshot in the compositor
  RefPtr<EffectRGB> mCompositorToolbarEffect;          // Effect used to render the snapshot in the compositor
  TimeStamp mCompositorAnimationStartTimeStamp;        // Time stamp when the current animation started
};

} // namespace layers
} // namespace mozilla
#endif // mozilla_layers_AndroidDynamicToolbarAnimator_h_
