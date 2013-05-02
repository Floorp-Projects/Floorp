/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScrollbarActivity_h___
#define ScrollbarActivity_h___

#include "nsCOMPtr.h"
#include "mozilla/TimeStamp.h"

class nsIContent;
class nsITimer;
class nsIAtom;
class nsIScrollableFrame;

namespace mozilla {

/**
 * ScrollbarActivity
 *
 * This class manages scrollbar active state. When some activity occured
 * the 'active' attribute of the both the horizontal scrollbar and vertical
 * scrollbar is set.
 * After a small amount of time of inactivity this attribute is unset from
 * both scrollbars.
 * Some css specific rules can affect the scrollbar, like showing/hiding it
 * with a fade transition.
 *
 * Initial scrollbar activity needs to be reported by the scrollbar frame that
 * owns the ScrollbarActivity instance. This needs to happen via a call to
 * ActivityOccurred(), for example when the current scroll position or the size
 * of the scroll area changes.
 *
 * ScrollbarActivity then wait until a timeout has expired or a new call to
 * ActivityOccured() has been made. When the timeout expires ActivityFinished()
 * is call and reset the active state.
 */

class ScrollbarActivity {
public:
  ScrollbarActivity(nsIScrollableFrame* aScrollableFrame)
   : mIsActive(false)
   , mScrollableFrame(aScrollableFrame)
  {}

  void ActivityOccurred();
  void ActivityFinished();
  ~ScrollbarActivity();

protected:
  /*
   * mIsActive is true once any type of activity occurent on the scrollable
   * frame and until kScrollbarActivityFinishedDelay has expired.
   * This does not reflect the value of the 'active' attributes on scrollbars.
   */
  bool mIsActive;

  /*
   * Hold a reference to the scrollable frame in order to retrieve the
   * horizontal and vertical scrollbar boxes where to set the 'active'
   * attribute.
   */
  nsIScrollableFrame* mScrollableFrame;

  nsCOMPtr<nsITimer> mActivityFinishedTimer;

  void SetIsActive(bool aNewActive);

  enum { kScrollbarActivityFinishedDelay = 450 }; // milliseconds
  static void ActivityFinishedTimerFired(nsITimer* aTimer, void* aSelf) {
    reinterpret_cast<ScrollbarActivity*>(aSelf)->ActivityFinished();
  }
  void StartActivityFinishedTimer();
  void CancelActivityFinishedTimer();

  nsIContent* GetScrollbarContent(bool aVertical);
  nsIContent* GetHorizontalScrollbar() {
    return GetScrollbarContent(false);
  }
  nsIContent* GetVerticalScrollbar() {
    return GetScrollbarContent(true);
  }
};

} // namespace mozilla

#endif /* ScrollbarActivity_h___ */
