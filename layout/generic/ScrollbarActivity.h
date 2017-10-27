/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScrollbarActivity_h___
#define ScrollbarActivity_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"

class nsIContent;
class nsIScrollbarMediator;
class nsITimer;

namespace mozilla {
namespace layout {

/**
 * ScrollbarActivity
 *
 * This class manages scrollbar behavior that imitates the native Mac OS X
 * Lion overlay scrollbar behavior: Scrollbars are only shown while "scrollbar
 * activity" occurs, and they're hidden with a fade animation after a short
 * delay.
 *
 * Scrollbar activity has these states:
 *  - inactive:
 *      Scrollbars are hidden.
 *  - ongoing activity:
 *      Scrollbars are visible and being operated on in some way, for example
 *      because they're hovered or pressed.
 *  - active, but waiting for fade out
 *      Scrollbars are still completely visible but are about to fade away.
 *  - fading out
 *      Scrollbars are subject to a fade-out animation.
 *
 * Initial scrollbar activity needs to be reported by the scrollbar holder that
 * owns the ScrollbarActivity instance. This needs to happen via a call to
 * ActivityOccurred(), for example when the current scroll position or the size
 * of the scroll area changes.
 *
 * As soon as scrollbars are visible, the ScrollbarActivity class manages the
 * rest of the activity behavior: It ensures that mouse motions inside the
 * scroll area keep the scrollbars visible, and that scrollbars don't fade away
 * while they're being hovered / dragged. It also sets a sticky hover attribute
 * on the most recently hovered scrollbar.
 *
 * ScrollbarActivity falls into hibernation after the scrollbars have faded
 * out. It only starts acting after the next call to ActivityOccurred() /
 * ActivityStarted().
 */

class ScrollbarActivity final : public nsIDOMEventListener,
                                public nsARefreshObserver
{
public:
  explicit ScrollbarActivity(nsIScrollbarMediator* aScrollableFrame)
   : mScrollableFrame(aScrollableFrame)
   , mNestedActivityCounter(0)
   , mIsActive(false)
   , mIsFading(false)
   , mListeningForScrollbarEvents(false)
   , mListeningForScrollAreaEvents(false)
   , mHScrollbarHovered(false)
   , mVScrollbarHovered(false)
   , mDisplayOnMouseMove(false)
   , mScrollbarFadeBeginDelay(0)
   , mScrollbarFadeDuration(0)
  {
    QueryLookAndFeelVals();
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void Destroy();

  void ActivityOccurred();
  void ActivityStarted();
  void ActivityStopped();

  virtual void WillRefresh(TimeStamp aTime) override;

  static void FadeBeginTimerFired(nsITimer* aTimer, void* aSelf) {
    RefPtr<ScrollbarActivity> scrollbarActivity(
      reinterpret_cast<ScrollbarActivity*>(aSelf));
    scrollbarActivity->BeginFade();
  }

protected:
  virtual ~ScrollbarActivity() {}

  bool IsActivityOngoing()
  { return mNestedActivityCounter > 0; }
  bool IsStillFading(TimeStamp aTime);
  void QueryLookAndFeelVals();

  void HandleEventForScrollbar(const nsAString& aType,
                               nsIContent* aTarget,
                               nsIContent* aScrollbar,
                               bool* aStoredHoverState);

  void SetIsActive(bool aNewActive);
  bool SetIsFading(bool aNewFading); // returns false if 'this' was destroyed

  void BeginFade();
  void EndFade();

  void StartFadeBeginTimer();
  void CancelFadeBeginTimer();

  void StartListeningForScrollbarEvents();
  void StartListeningForScrollAreaEvents();
  void StopListeningForScrollbarEvents();
  void StopListeningForScrollAreaEvents();
  void AddScrollbarEventListeners(nsIDOMEventTarget* aScrollbar);
  void RemoveScrollbarEventListeners(nsIDOMEventTarget* aScrollbar);

  void RegisterWithRefreshDriver();
  void UnregisterFromRefreshDriver();

  bool UpdateOpacity(TimeStamp aTime); // returns false if 'this' was destroyed
  void HoveredScrollbar(nsIContent* aScrollbar);

  nsRefreshDriver* GetRefreshDriver();
  nsIContent* GetScrollbarContent(bool aVertical);
  nsIContent* GetHorizontalScrollbar() { return GetScrollbarContent(false); }
  nsIContent* GetVerticalScrollbar() { return GetScrollbarContent(true); }

  const TimeDuration FadeDuration() {
    return TimeDuration::FromMilliseconds(mScrollbarFadeDuration);
  }

  nsIScrollbarMediator* mScrollableFrame;
  TimeStamp mFadeBeginTime;
  nsCOMPtr<nsITimer> mFadeBeginTimer;
  nsCOMPtr<nsIDOMEventTarget> mHorizontalScrollbar; // null while inactive
  nsCOMPtr<nsIDOMEventTarget> mVerticalScrollbar;   // null while inactive
  int mNestedActivityCounter;
  bool mIsActive;
  bool mIsFading;
  bool mListeningForScrollbarEvents;
  bool mListeningForScrollAreaEvents;
  bool mHScrollbarHovered;
  bool mVScrollbarHovered;

  // LookAndFeel values we load on creation
  bool mDisplayOnMouseMove;
  int mScrollbarFadeBeginDelay;
  int mScrollbarFadeDuration;
};

} // namespace layout
} // namespace mozilla

#endif /* ScrollbarActivity_h___ */
