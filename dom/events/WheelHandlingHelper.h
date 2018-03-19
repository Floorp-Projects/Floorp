/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WheelHandlingHelper_h_
#define mozilla_WheelHandlingHelper_h_

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsCoord.h"
#include "nsIFrame.h"
#include "nsPoint.h"

class nsIScrollableFrame;
class nsITimer;

namespace mozilla {

class EventStateManager;

/**
 * DeltaValues stores two delta values which are along X and Y axis.  This is
 * useful for arguments and results of some methods.
 */

struct DeltaValues
{
  DeltaValues()
    : deltaX(0.0)
    , deltaY(0.0)
  {
  }

  DeltaValues(double aDeltaX, double aDeltaY)
    : deltaX(aDeltaX)
    , deltaY(aDeltaY)
  {
  }

  explicit DeltaValues(WidgetWheelEvent* aEvent);

  double deltaX;
  double deltaY;
};

/**
 * WheelHandlingUtils provides some static methods which are useful at handling
 * wheel events.
 */

class WheelHandlingUtils
{
public:
  /**
   * Returns true if aFrame is a scrollable frame and it can be scrolled to
   * either aDirectionX or aDirectionY along each axis.  Or if aFrame is a
   * plugin frame (in this case, aDirectionX and aDirectionY are ignored).
   * Otherwise, false.
   */
  static bool CanScrollOn(nsIFrame* aFrame,
                          double aDirectionX, double aDirectionY);
  /**
   * Returns true if the scrollable frame can be scrolled to either aDirectionX
   * or aDirectionY along each axis.  Otherwise, false.
   */
  static bool CanScrollOn(nsIScrollableFrame* aScrollFrame,
                          double aDirectionX, double aDirectionY);

  // For more details about the concept of a disregarded direction, refer to the
  // code in struct mozilla::layers::ScrollMetadata which defines
  // mDisregardedDirection.
  static Maybe<layers::ScrollDirection>
  GetDisregardedWheelScrollDirection(const nsIFrame* aFrame);

private:
  static bool CanScrollInRange(nscoord aMin, nscoord aValue, nscoord aMax,
                               double aDirection);
};

/**
 * ScrollbarsForWheel manages scrollbars state during wheel operation.
 * E.g., on some platforms, scrollbars should show only while user attempts to
 * scroll.  At that time, scrollbars which may be possible to scroll by
 * operation of wheel at the point should show temporarily.
 */

class ScrollbarsForWheel
{
public:
  static void PrepareToScrollText(EventStateManager* aESM,
                                  nsIFrame* aTargetFrame,
                                  WidgetWheelEvent* aEvent);
  static void SetActiveScrollTarget(nsIScrollableFrame* aScrollTarget);
  // Hide all scrollbars (both mActiveOwner's and mActivatedScrollTargets')
  static void MayInactivate();
  static void Inactivate();
  static bool IsActive();
  static void OwnWheelTransaction(bool aOwn);

protected:
  static const size_t kNumberOfTargets = 4;
  static const DeltaValues directions[kNumberOfTargets];
  static AutoWeakFrame sActiveOwner;
  static AutoWeakFrame sActivatedScrollTargets[kNumberOfTargets];
  static bool sHadWheelStart;
  static bool sOwnWheelTransaction;


  /**
   * These two methods are called upon eWheelOperationStart/eWheelOperationEnd
   * events to show/hide the right scrollbars.
   */
  static void TemporarilyActivateAllPossibleScrollTargets(
                EventStateManager* aESM,
                nsIFrame* aTargetFrame,
                WidgetWheelEvent* aEvent);
  static void DeactivateAllTemporarilyActivatedScrollTargets();
};

/**
 * WheelTransaction manages a series of wheel events as a transaction.
 * While in a transaction, every wheel event should scroll the same scrollable
 * element even if a different scrollable element is under the mouse cursor.
 *
 * Additionally, this class also manages wheel scroll speed acceleration.
 */

class WheelTransaction
{
public:
  static nsIFrame* GetTargetFrame() { return sTargetFrame; }
  static void EndTransaction();
  /**
   * WillHandleDefaultAction() is called before handling aWheelEvent on
   * aTargetFrame.
   *
   * @return    false if the caller cannot continue to handle the default
   *            action.  Otherwise, true.
   */
  static bool WillHandleDefaultAction(WidgetWheelEvent* aWheelEvent,
                                      AutoWeakFrame& aTargetWeakFrame);
  static bool WillHandleDefaultAction(WidgetWheelEvent* aWheelEvent,
                                      nsIFrame* aTargetFrame)
  {
    AutoWeakFrame targetWeakFrame(aTargetFrame);
    return WillHandleDefaultAction(aWheelEvent, targetWeakFrame);
  }
  static void OnEvent(WidgetEvent* aEvent);
  static void Shutdown();
  static uint32_t GetTimeoutTime()
  {
    return Prefs::sMouseWheelTransactionTimeout;
  }

  static void OwnScrollbars(bool aOwn);

  static DeltaValues AccelerateWheelDelta(WidgetWheelEvent* aEvent,
                                          bool aAllowScrollSpeedOverride);
  static void InitializeStatics()
  {
    Prefs::InitializeStatics();
  }

protected:
  static void BeginTransaction(nsIFrame* aTargetFrame,
                               const WidgetWheelEvent* aEvent);
  // Be careful, UpdateTransaction may fire a DOM event, therefore, the target
  // frame might be destroyed in the event handler.
  static bool UpdateTransaction(const WidgetWheelEvent* aEvent);
  static void MayEndTransaction();

  static LayoutDeviceIntPoint GetScreenPoint(WidgetGUIEvent* aEvent);
  static void OnFailToScrollTarget();
  static void OnTimeout(nsITimer* aTimer, void* aClosure);
  static void SetTimeout();
  static uint32_t GetIgnoreMoveDelayTime()
  {
    return Prefs::sMouseWheelTransactionIgnoreMoveDelay;
  }
  static int32_t GetAccelerationStart()
  {
    return Prefs::sMouseWheelAccelerationStart;
  }
  static int32_t GetAccelerationFactor()
  {
    return Prefs::sMouseWheelAccelerationFactor;
  }
  static DeltaValues OverrideSystemScrollSpeed(WidgetWheelEvent* aEvent);
  static double ComputeAcceleratedWheelDelta(double aDelta, int32_t aFactor);
  static bool OutOfTime(uint32_t aBaseTime, uint32_t aThreshold);

  static AutoWeakFrame sTargetFrame;
  static uint32_t sTime; // in milliseconds
  static uint32_t sMouseMoved; // in milliseconds
  static nsITimer* sTimer;
  static int32_t sScrollSeriesCounter;
  static bool sOwnScrollbars;

  class Prefs
  {
  public:
    static void InitializeStatics();
    static int32_t sMouseWheelAccelerationStart;
    static int32_t sMouseWheelAccelerationFactor;
    static uint32_t sMouseWheelTransactionTimeout;
    static uint32_t sMouseWheelTransactionIgnoreMoveDelay;
    static bool sTestMouseScroll;
  };
};

// For some kinds of scrollings, the delta values of WidgetWheelEvent are
// possbile to be adjusted. For example, the user has configured the pref to let
// [vertical wheel + Shift key] to perform horizontal scrolling instead of
// vertical scrolling.
// The values in this enumeration list all kinds of scrollings whose delta
// values are possible to be adjusted.
enum class WheelDeltaAdjustmentStrategy : uint8_t
{
  // There is no strategy, don't adjust delta values in any cases.
  eNone,
  // This strategy means we're receiving a horizontalized scroll, so we should
  // apply horizontalization strategy for its delta values.
  // Horizontalized scrolling means treating vertical wheel scrolling as
  // horizontal scrolling by adjusting delta values.
  // It's important to keep in mind with the percise concept of horizontalized
  // scrolling: Delta values are *ONLY* going to be adjusted during the process
  // of its default action handling; in views of any programmes other than the
  // default action handler, such as a DOM event listener or a plugin, delta
  // values are never going to be adjusted, they will still retrive original
  // delta values when horizontalization occured for default actions.
  eHorizontalize,
  // The following two strategies mean we're receving an auto-dir scroll, so we
  // should apply auto-dir adjustment to the delta of the wheel event if needed.
  // Auto-dir is a feature which treats any single-wheel scroll as a scroll in
  // the only one scrollable direction if the target has only one scrollable
  // direction. For example, if the user scrolls a vertical wheel inside a
  // target which is horizontally scrollable but vertical unscrollable, then the
  // vertical scroll is converted to a horizontal scroll for that target.
  // So why do we need two different strategies for auto-dir scrolling? That's
  // because when a wheel scroll is converted due to auto-dir, there is one
  // thing called "honoured target" which decides which side the converted
  // scroll goes towards. If the content of the honoured target horizontally
  // is RTL content, then an upward scroll maps to a rightward scroll and a
  // downward scroll maps to a leftward scroll; otherwise, an upward scroll maps
  // to a leftward scroll and a downward scroll maps to a rightward scroll.
  // |eAutoDir| considers the scrolling target as the honoured target.
  // |eAutoDirWithRootHonour| takes the root element of the document with the
  // scrolling element, and considers that as the honoured target. But note that
  // there's one exception: for targets in an HTML document, the real root
  // element(I.e. the <html> element) is typically not considered as a root
  // element, but the <body> element is typically considered as a root element.
  // If there is no <body> element, then consider the <html> element instead.
  // And also note that like |eHorizontalize|, delta values are *ONLY* going to
  // be adjusted during the process of its default action handling; in views of
  // any programmes other than the default action handler, such as a DOM event
  // listener or a plugin, delta values are never going to be adjusted.
  eAutoDir,
  eAutoDirWithRootHonour,
  // Not an actual strategy. This is just used as an upper bound for
  // ContiguousEnumSerializer.
  eSentinel,
};

/**
 * When a *pure* vertical wheel event should be treated as if it was a
 * horizontal scroll because the user wants to horizontalize the wheel scroll,
 * an instance of this class will adjust the delta values upon calling
 * Horizontalize(). And the horizontalized delta values will be restored
 * automatically when the instance of this class is being destructed. Or you can
 * restore them in advance by calling CancelHorizontalization().
 */
class MOZ_STACK_CLASS WheelDeltaHorizontalizer final
{
public:
  /**
   * @param aWheelEvent        A wheel event whose delta values will be adjusted
   *                           upon calling Horizontalize().
   */
  explicit WheelDeltaHorizontalizer(WidgetWheelEvent& aWheelEvent)
    : mWheelEvent(aWheelEvent)
    , mHorizontalized(false)
  {
  }
  /**
   * Converts vertical scrolling into horizontal scrolling by adjusting the
   * its delta values.
   */
  void Horizontalize();
  ~WheelDeltaHorizontalizer();
  void CancelHorizontalization();

private:
  WidgetWheelEvent& mWheelEvent;
  double mOldDeltaX;
  double mOldDeltaZ;
  double mOldOverflowDeltaX;
  int32_t mOldLineOrPageDeltaX;
  bool mHorizontalized;
};

/**
 * This class is used to adjust the delta values for wheel scrolling with the
 * auto-dir functionality.
 * A traditional wheel scroll only allows the user use the wheel in the same
 * scrollable direction as that of the scrolling target to scroll the target,
 * whereas an auto-dir scroll lets the user use any wheel(either a vertical
 * wheel or a horizontal tilt wheel) to scroll a frame which is scrollable in
 * only one direction. For detailed information on auto-dir scrolling,
 * @see mozilla::WheelDeltaAdjustmentStrategy.
 */
class MOZ_STACK_CLASS AutoDirWheelDeltaAdjuster
{
protected:
  /**
   * @param aDeltaX            DeltaX for a wheel event whose delta values will
   *                           be adjusted upon calling Adjust() when
   *                           ShouldBeAdjusted() returns true.
   * @param aDeltaY            DeltaY for a wheel event, like DeltaX.
   */
  AutoDirWheelDeltaAdjuster(double& aDeltaX,
                            double& aDeltaY)
    : mDeltaX(aDeltaX)
    , mDeltaY(aDeltaY)
    , mCheckedIfShouldBeAdjusted(false)
    , mShouldBeAdjusted(false)
  {
  }

public:
  /**
   * Gets whether the values of the delta should be adjusted for auto-dir
   * scrolling. Note that if Adjust() has been called, this function simply
   * returns false.
   *
   * @return true if the delta should be adjusted; otherwise false.
   */
  bool ShouldBeAdjusted();
  /**
   * Adjusts the values of the delta values for auto-dir scrolling when
   * ShouldBeAdjusted() returns true. If you call it when ShouldBeAdjusted()
   * returns false, this function will simply do nothing.
   */
  void Adjust();

private:
  /**
   * Called by Adjust() if Adjust() successfully adjusted the delta values.
   */
  virtual void OnAdjusted()
  {
  }

  virtual bool CanScrollAlongXAxis() const = 0;
  virtual bool CanScrollAlongYAxis() const = 0;
  virtual bool CanScrollUpwards() const = 0;
  virtual bool CanScrollDownwards() const = 0;
  virtual bool CanScrollLeftwards() const = 0;
  virtual bool CanScrollRightwards() const = 0;

  /**
   * Gets whether the horizontal content starts at rightside.
   *
   * @return If the content is in vertical-RTL writing mode(E.g. "writing-mode:
   *         vertical-rl" in CSS), or if it's in horizontal-RTL writing-mode
   *         (E.g. "writing-mode: horizontal-tb; direction: rtl;" in CSS), then
   *         this function returns true. From the representation perspective,
   *         frames whose horizontal contents start at rightside also cause
   *         their horizontal scrollbars, if any, initially start at rightside.
   *         So we can also learn about the initial side of the horizontal
   *         scrollbar for the frame by calling this function.
   */
  virtual bool IsHorizontalContentRightToLeft() const = 0;

protected:
  double& mDeltaX;
  double& mDeltaY;

private:
  bool mCheckedIfShouldBeAdjusted;
  bool mShouldBeAdjusted;
};

/**
 * This is the implementation of AutoDirWheelDeltaAdjuster for EventStateManager
 *
 * Detailed comments about some member functions are given in the base class
 * AutoDirWheelDeltaAdjuster.
 */
class MOZ_STACK_CLASS ESMAutoDirWheelDeltaAdjuster final
                        : public AutoDirWheelDeltaAdjuster
{
public:
  /**
   * @param aEvent             The auto-dir wheel scroll event.
   * @param aScrollFrame       The scroll target for the event.
   * @param aHonoursRoot       If set to true, the honoured frame is the root
   *                           frame in the same document where the target is;
   *                           If false, the honoured frame is the scroll
   *                           target. For the concept of an honoured target,
   *                           @see mozilla::WheelDeltaAdjustmentStrategy
   */
  ESMAutoDirWheelDeltaAdjuster(WidgetWheelEvent& aEvent,
                               nsIFrame& aScrollFrame,
                               bool aHonoursRoot);

private:
  virtual void OnAdjusted() override;
  virtual bool CanScrollAlongXAxis() const override;
  virtual bool CanScrollAlongYAxis() const override;
  virtual bool CanScrollUpwards() const override;
  virtual bool CanScrollDownwards() const override;
  virtual bool CanScrollLeftwards() const override;
  virtual bool CanScrollRightwards() const override;
  virtual bool IsHorizontalContentRightToLeft() const override;

  nsIScrollableFrame* mScrollTargetFrame;
  bool mIsHorizontalContentRightToLeft;

  int32_t& mLineOrPageDeltaX;
  int32_t& mLineOrPageDeltaY;
  double& mOverflowDeltaX;
  double& mOverflowDeltaY;
};

/**
 * This class is used for restoring the delta in an auto-dir wheel.
 *
 * An instance of this calss monitors auto-dir adjustment which may happen
 * during its lifetime. If the delta values is adjusted during its lifetime, the
 * instance will restore the adjusted delta when it's being destrcuted.
 */
class MOZ_STACK_CLASS ESMAutoDirWheelDeltaRestorer final
{
public:
  /**
   * @param aEvent             The wheel scroll event to be monitored.
   */
  explicit ESMAutoDirWheelDeltaRestorer(WidgetWheelEvent& aEvent);
  ~ESMAutoDirWheelDeltaRestorer();

private:
  WidgetWheelEvent& mEvent;
  double mOldDeltaX;
  double mOldDeltaY;
  int32_t mOldLineOrPageDeltaX;
  int32_t mOldLineOrPageDeltaY;
  double mOldOverflowDeltaX;
  double mOldOverflowDeltaY;
};

} // namespace mozilla

#endif // mozilla_WheelHandlingHelper_h_
