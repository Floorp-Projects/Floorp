/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GeckoContentController_h
#define mozilla_layers_GeckoContentController_h

#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "Units.h"                      // for CSSPoint, CSSRect, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/EventForwards.h"      // for Modifiers
#include "nsISupportsImpl.h"

namespace mozilla {

class Runnable;

namespace layers {

class GeckoContentController
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GeckoContentController)

  /**
   * Requests a paint of the given FrameMetrics |aFrameMetrics| from Gecko.
   * Implementations per-platform are responsible for actually handling this.
   *
   * This method must always be called on the repaint thread, which depends
   * on the GeckoContentController. For ChromeProcessController it is the
   * Gecko main thread, while for RemoteContentController it is the compositor
   * thread where it can send IPDL messages.
   */
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) = 0;

  /**
   * Different types of tap-related events that can be sent in
   * the HandleTap function. The names should be relatively self-explanatory.
   * Note that the eLongTapUp will always be preceded by an eLongTap, but not
   * all eLongTap notifications will be followed by an eLongTapUp (for instance,
   * if the user moves their finger after triggering the long-tap but before
   * lifting it).
   */
  enum class TapType {
    eSingleTap,
    eDoubleTap,
    eLongTap,
    eLongTapUp,

    eSentinel,
  };

  /**
   * Requests handling of a tap event. |aPoint| is in LD pixels, relative to the
   * current scroll offset.
   */
  virtual void HandleTap(TapType aType,
                         const LayoutDevicePoint& aPoint,
                         Modifiers aModifiers,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId) = 0;

  /**
   * Schedules a runnable to run on the controller/UI thread at some time
   * in the future.
   * This method must always be called on the controller thread.
   */
  virtual void PostDelayedTask(already_AddRefed<Runnable> aRunnable, int aDelayMs) = 0;

  /**
   * Returns true if we are currently on the thread that can send repaint requests.
   */
  virtual bool IsRepaintThread() = 0;

  /**
   * Runs the given task on the "repaint" thread.
   */
  virtual void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) = 0;

  /**
   * APZ uses |FrameMetrics::mCompositionBounds| for hit testing. Sometimes,
   * widget code has knowledge of a touch-sensitive region that should
   * additionally constrain hit testing for all frames associated with the
   * controller. This method allows APZ to query the controller for such a
   * region. A return value of true indicates that the controller has such a
   * region, and it is returned in |aOutRegion|.
   * This method needs to be called on the main thread.
   * TODO: once bug 928833 is implemented, this should be removed, as
   * APZ can then get the correct touch-sensitive region for each frame
   * directly from the layer.
   */
  virtual bool GetTouchSensitiveRegion(CSSRect* aOutRegion)
  {
    return false;
  }

  enum class APZStateChange {
    /**
     * APZ started modifying the view (including panning, zooming, and fling).
     */
    eTransformBegin,
    /**
     * APZ finished modifying the view.
     */
    eTransformEnd,
    /**
     * APZ started a touch.
     * |aArg| is 1 if touch can be a pan, 0 otherwise.
     */
    eStartTouch,
    /**
     * APZ started a pan.
     */
    eStartPanning,
    /**
     * APZ finished processing a touch.
     * |aArg| is 1 if touch was a click, 0 otherwise.
     */
    eEndTouch,

    // Sentinel value for IPC, this must be the last item in the enum and
    // should not be used as an actual message value.
    eSentinel
  };
  /**
   * General notices of APZ state changes for consumers.
   * |aGuid| identifies the APZC originating the state change.
   * |aChange| identifies the type of state change
   * |aArg| is used by some state changes to pass extra information (see
   *        the documentation for each state change above)
   */
  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange,
                                    int aArg = 0) {}

  /**
   * Notify content of a MozMouseScrollFailed event.
   */
  virtual void NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId, const nsString& aEvent)
  {}

  /**
   * Notify content that the repaint requests have been flushed.
   */
  virtual void NotifyFlushComplete() = 0;

  virtual void UpdateOverscrollVelocity(float aX, float aY, bool aIsRootContent) {}
  virtual void UpdateOverscrollOffset(float aX, float aY, bool aIsRootContent) {}
  virtual void SetScrollingRootContent(bool isRootContent) {}

  GeckoContentController() {}

  /**
   * Needs to be called on the main thread.
   */
  virtual void Destroy() {}

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~GeckoContentController() {}
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_GeckoContentController_h
