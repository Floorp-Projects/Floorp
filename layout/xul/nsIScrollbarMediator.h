/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScrollbarMediator_h___
#define nsIScrollbarMediator_h___

#include "nsQueryFrame.h"
#include "nsCoord.h"

class nsScrollbarFrame;
class nsIFrame;

class nsIScrollbarMediator : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIScrollbarMediator)

  /**
   * The aScrollbar argument denotes the scrollbar that's firing the notification.
   * aScrollbar is never null.
   * aDirection is either -1, 0, or 1.
   */

  /**
   * When set to ENABLE_SNAP, additional scrolling will be performed after the
   * scroll operation to maintain the constraints set by CSS Scroll snapping.
   * The additional scrolling may include asynchronous smooth scrolls that
   * continue to animate after the initial scroll position has been set.
   */
  enum ScrollSnapMode { DISABLE_SNAP, ENABLE_SNAP };

  /**
   * One of the following three methods is called when the scrollbar's button is
   * clicked.
   * @note These methods might destroy the frame, pres shell, and other objects.
   */
  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            ScrollSnapMode aSnap = DISABLE_SNAP) = 0;
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            ScrollSnapMode aSnap = DISABLE_SNAP) = 0;
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            ScrollSnapMode aSnap = DISABLE_SNAP) = 0;
  /**
   * RepeatButtonScroll is called when the scrollbar's button is held down. When the
   * button is first clicked the increment is set; RepeatButtonScroll adds this
   * increment to the current position.
   * @note This method might destroy the frame, pres shell, and other objects.
   */
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) = 0;
  /**
   * aOldPos and aNewPos are scroll positions.
   * The scroll positions start with zero at the left edge; implementors that want
   * zero at the right edge for RTL content will need to adjust accordingly.
   * (See ScrollFrameHelper::ThumbMoved in nsGfxScrollFrame.cpp.)
   * @note This method might destroy the frame, pres shell, and other objects.
   */
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar,
                          nscoord aOldPos,
                          nscoord aNewPos) = 0;
  /**
   * Called when the scroll bar thumb, slider, or any other component is
   * released.
   */
  virtual void ScrollbarReleased(nsScrollbarFrame* aScrollbar) = 0;
  virtual void VisibilityChanged(bool aVisible) = 0;

  /**
   * Obtain the frame for the horizontal or vertical scrollbar, or null
   * if there is no such box.
   */
  virtual nsIFrame* GetScrollbarBox(bool aVertical) = 0;
  /**
   * Show or hide scrollbars on 2 fingers touch.
   * Subclasses should call their ScrollbarActivity's corresponding methods.
   */
  virtual void ScrollbarActivityStarted() const = 0;
  virtual void ScrollbarActivityStopped() const = 0;

  virtual bool IsScrollbarOnRight() const = 0;

  /**
   * Returns true if the mediator is asking the scrollbar to suppress
   * repainting itself on changes.
   */
  virtual bool ShouldSuppressScrollbarRepaints() const = 0;
};

#endif

