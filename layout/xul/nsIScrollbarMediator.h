/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScrollbarMediator_h___
#define nsIScrollbarMediator_h___

#include "nsQueryFrame.h"
#include "nsCoord.h"

class nsScrollbarFrame;
class nsIDOMEventTarget;
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
   * One of the following three methods is called when the scrollbar's button is
   * clicked.
   * @note These methods might destroy the frame, pres shell, and other objects.
   */
  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection) = 0;
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection) = 0;
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection) = 0;
  /**
   * RepeatButtonScroll is called when the scrollbar's button is held down. When the
   * button is first clicked the increment is set; RepeatButtonScroll adds this
   * increment to the current position.
   * @note This method might destroy the frame, pres shell, and other objects.
   */
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) = 0;
  /**
   * aOldPos and aNewPos are scroll positions.
   * @note This method might destroy the frame, pres shell, and other objects.
   */
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar,
                          nscoord aOldPos,
                          nscoord aNewPos) = 0;
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
};

#endif

