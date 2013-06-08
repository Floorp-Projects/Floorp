/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
class nsIScrollbarOwner;
class nsITimer;
class nsIAtom;

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

class ScrollbarActivity : public nsIDOMEventListener,
                          public nsARefreshObserver {
public:
  ScrollbarActivity(nsIScrollbarOwner* aScrollableFrame)
   : mScrollableFrame(aScrollableFrame)
   , mNestedActivityCounter(0)
   , mIsActive(false)
   , mIsFading(false)
   , mListeningForEvents(false)
   , mHScrollbarHovered(false)
   , mVScrollbarHovered(false)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  virtual ~ScrollbarActivity() {}

  void Destroy();

  void ActivityOccurred();
  void ActivityStarted();
  void ActivityStopped();

  virtual void WillRefresh(TimeStamp aTime) MOZ_OVERRIDE;

  static void FadeBeginTimerFired(nsITimer* aTimer, void* aSelf) {
    reinterpret_cast<ScrollbarActivity*>(aSelf)->BeginFade();
  }

  static const uint32_t kScrollbarFadeBeginDelay = 450; // milliseconds
  static const uint32_t kScrollbarFadeDuration = 200; // milliseconds

protected:

  bool IsActivityOngoing()
  { return mNestedActivityCounter > 0; }
  bool IsStillFading(TimeStamp aTime);

  void HandleEventForScrollbar(const nsAString& aType,
                               nsIContent* aTarget,
                               nsIContent* aScrollbar,
                               bool* aStoredHoverState);

  void SetIsActive(bool aNewActive);
  void SetIsFading(bool aNewFading);

  void BeginFade();
  void EndFade();

  void StartFadeBeginTimer();
  void CancelFadeBeginTimer();
  void StartListeningForEvents();
  void StartListeningForEventsOnScrollbar(nsIDOMEventTarget* aScrollbar);
  void StopListeningForEvents();
  void StopListeningForEventsOnScrollbar(nsIDOMEventTarget* aScrollbar);
  void RegisterWithRefreshDriver();
  void UnregisterFromRefreshDriver();

  bool UpdateOpacity(TimeStamp aTime); // returns false if 'this' was destroyed
  void HoveredScrollbar(nsIContent* aScrollbar);

  nsRefreshDriver* GetRefreshDriver();
  nsIContent* GetScrollbarContent(bool aVertical);
  nsIContent* GetHorizontalScrollbar() { return GetScrollbarContent(false); }
  nsIContent* GetVerticalScrollbar() { return GetScrollbarContent(true); }

  static const TimeDuration FadeDuration() {
    return TimeDuration::FromMilliseconds(kScrollbarFadeDuration);
  }

  nsIScrollbarOwner* mScrollableFrame;
  TimeStamp mFadeBeginTime;
  nsCOMPtr<nsITimer> mFadeBeginTimer;
  nsCOMPtr<nsIDOMEventTarget> mHorizontalScrollbar; // null while inactive
  nsCOMPtr<nsIDOMEventTarget> mVerticalScrollbar;   // null while inactive
  int mNestedActivityCounter;
  bool mIsActive;
  bool mIsFading;
  bool mListeningForEvents;
  bool mHScrollbarHovered;
  bool mVScrollbarHovered;
};

} // namespace layout
} // namespace mozilla

#endif /* ScrollbarActivity_h___ */
