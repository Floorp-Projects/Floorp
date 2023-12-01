/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScrollbarActivity_h___
#define ScrollbarActivity_h___

#include "mozilla/Assertions.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"

class nsIContent;
class nsIScrollbarMediator;
class nsITimer;

namespace mozilla {

namespace dom {
class Element;
class EventTarget;
}  // namespace dom

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

class ScrollbarActivity final : public nsIDOMEventListener {
 public:
  explicit ScrollbarActivity(nsIScrollbarMediator* aScrollableFrame)
      : mScrollableFrame(aScrollableFrame) {
    MOZ_ASSERT(mScrollableFrame);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void Destroy();

  void ActivityOccurred();
  void ActivityStarted();
  void ActivityStopped();

  bool IsActive() const { return mNestedActivityCounter; }

 protected:
  virtual ~ScrollbarActivity() = default;

  void StartFadeTimer();
  void CancelFadeTimer();
  void BeginFade();
  void HandleEventForScrollbar(const nsAString& aType, nsIContent* aTarget,
                               dom::Element* aScrollbar,
                               bool* aStoredHoverState);

  void StartListeningForScrollbarEvents();
  void StartListeningForScrollAreaEvents();
  void StopListeningForScrollbarEvents();
  void StopListeningForScrollAreaEvents();
  void AddScrollbarEventListeners(dom::EventTarget* aScrollbar);
  void RemoveScrollbarEventListeners(dom::EventTarget* aScrollbar);

  void HoveredScrollbar(dom::Element* aScrollbar);

  dom::Element* GetScrollbarContent(bool aVertical);
  dom::Element* GetHorizontalScrollbar() { return GetScrollbarContent(false); }
  dom::Element* GetVerticalScrollbar() { return GetScrollbarContent(true); }

  nsIScrollbarMediator* const mScrollableFrame;
  nsCOMPtr<dom::EventTarget> mHorizontalScrollbar;  // null while inactive
  nsCOMPtr<dom::EventTarget> mVerticalScrollbar;    // null while inactive
  nsCOMPtr<nsITimer> mFadeTimer;
  uint32_t mNestedActivityCounter = 0;
  // This boolean is true from the point activity starts to the point BeginFade
  // runs, and effectively reflects the "active" attribute of the scrollbar.
  bool mScrollbarEffectivelyVisible = false;
  bool mListeningForScrollbarEvents = false;
  bool mListeningForScrollAreaEvents = false;
  bool mHScrollbarHovered = false;
  bool mVScrollbarHovered = false;
};

}  // namespace layout
}  // namespace mozilla

#endif /* ScrollbarActivity_h___ */
