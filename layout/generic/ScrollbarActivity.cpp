/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarActivity.h"
#include "nsIScrollbarMediator.h"
#include "nsIContent.h"
#include "nsICSSDeclaration.h"
#include "nsIFrame.h"
#include "nsContentUtils.h"
#include "nsAString.h"
#include "nsQueryFrame.h"
#include "nsRefreshDriver.h"
#include "nsComponentManagerUtils.h"
#include "nsStyledElement.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/StaticPrefs_layout.h"

namespace mozilla {
namespace layout {

using mozilla::dom::Element;

NS_IMPL_ISUPPORTS(ScrollbarActivity, nsIDOMEventListener)

void ScrollbarActivity::QueryLookAndFeelVals() {
  // Fade animation constants
  mScrollbarFadeBeginDelay =
      LookAndFeel::GetInt(LookAndFeel::IntID::ScrollbarFadeBeginDelay);
  mScrollbarFadeDuration =
      LookAndFeel::GetInt(LookAndFeel::IntID::ScrollbarFadeDuration);
  // Controls whether we keep the mouse move listener so we can display the
  // scrollbars whenever the user moves the mouse within the scroll area.
  mDisplayOnMouseMove =
      LookAndFeel::GetInt(LookAndFeel::IntID::ScrollbarDisplayOnMouseMove);
}

void ScrollbarActivity::Destroy() {
  StopListeningForScrollbarEvents();
  StopListeningForScrollAreaEvents();
  UnregisterFromRefreshDriver();
  CancelFadeBeginTimer();
}

void ScrollbarActivity::ActivityOccurred() {
  ActivityStarted();
  ActivityStopped();
}

void ScrollbarActivity::ActivityStarted() {
  mNestedActivityCounter++;
  CancelFadeBeginTimer();
  if (!SetIsFading(false)) {
    return;
  }
  UnregisterFromRefreshDriver();
  StartListeningForScrollbarEvents();
  StartListeningForScrollAreaEvents();
  SetIsActive(true);

  NS_ASSERTION(mIsActive, "need to be active during activity");
  NS_ASSERTION(!mIsFading, "must not be fading during activity");
}

void ScrollbarActivity::ActivityStopped() {
  if (!IsActivityOngoing()) {
    // This can happen if there was a frame reconstruction while the activity
    // was ongoing. In this case we just do nothing. We should probably handle
    // this case better.
    return;
  }
  NS_ASSERTION(mIsActive, "need to be active during activity");
  NS_ASSERTION(!mIsFading, "must not be fading during ongoing activity");

  mNestedActivityCounter--;

  if (!IsActivityOngoing()) {
    StartFadeBeginTimer();

    NS_ASSERTION(mIsActive, "need to be active right after activity");
    NS_ASSERTION(!mIsFading, "must not be fading right after activity");
  }
}

NS_IMETHODIMP
ScrollbarActivity::HandleEvent(dom::Event* aEvent) {
  if (!mDisplayOnMouseMove && !mIsActive) return NS_OK;

  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("mousemove")) {
    // Mouse motions anywhere in the scrollable frame should keep the
    // scrollbars visible.
    ActivityOccurred();
    return NS_OK;
  }

  nsCOMPtr<nsIContent> targetContent =
      do_QueryInterface(aEvent->GetOriginalTarget());

  HandleEventForScrollbar(type, targetContent, GetHorizontalScrollbar(),
                          &mHScrollbarHovered);
  HandleEventForScrollbar(type, targetContent, GetVerticalScrollbar(),
                          &mVScrollbarHovered);

  return NS_OK;
}

void ScrollbarActivity::WillRefresh(TimeStamp aTime) {
  NS_ASSERTION(mIsActive, "should only fade while scrollbars are visible");
  NS_ASSERTION(!IsActivityOngoing(),
               "why weren't we unregistered from the refresh driver when "
               "scrollbar activity started?");
  NS_ASSERTION(mIsFading, "should only animate fading during fade");

  if (!UpdateOpacity(aTime)) {
    return;
  }

  if (!IsStillFading(aTime)) {
    EndFade();
  }
}

bool ScrollbarActivity::IsStillFading(TimeStamp aTime) {
  return !mFadeBeginTime.IsNull() && (aTime - mFadeBeginTime < FadeDuration());
}

void ScrollbarActivity::HandleEventForScrollbar(const nsAString& aType,
                                                nsIContent* aTarget,
                                                Element* aScrollbar,
                                                bool* aStoredHoverState) {
  if (!aTarget || !aScrollbar || !aTarget->IsInclusiveDescendantOf(aScrollbar))
    return;

  if (aType.EqualsLiteral("mousedown")) {
    ActivityStarted();
  } else if (aType.EqualsLiteral("mouseup")) {
    ActivityStopped();
  } else if (aType.EqualsLiteral("mouseover") ||
             aType.EqualsLiteral("mouseout")) {
    bool newHoveredState = aType.EqualsLiteral("mouseover");
    if (newHoveredState && !*aStoredHoverState) {
      ActivityStarted();
      HoveredScrollbar(aScrollbar);
    } else if (*aStoredHoverState && !newHoveredState) {
      ActivityStopped();
      // Don't call HoveredScrollbar(nullptr) here because we want the hover
      // attribute to stick until the scrollbars are hidden.
    }
    *aStoredHoverState = newHoveredState;
  }
}

void ScrollbarActivity::StartListeningForScrollbarEvents() {
  if (mListeningForScrollbarEvents) return;

  mHorizontalScrollbar = GetHorizontalScrollbar();
  mVerticalScrollbar = GetVerticalScrollbar();

  AddScrollbarEventListeners(mHorizontalScrollbar);
  AddScrollbarEventListeners(mVerticalScrollbar);

  mListeningForScrollbarEvents = true;
}

void ScrollbarActivity::StopListeningForScrollbarEvents() {
  if (!mListeningForScrollbarEvents) return;

  RemoveScrollbarEventListeners(mHorizontalScrollbar);
  RemoveScrollbarEventListeners(mVerticalScrollbar);

  mHorizontalScrollbar = nullptr;
  mVerticalScrollbar = nullptr;
  mListeningForScrollbarEvents = false;
}

void ScrollbarActivity::StartListeningForScrollAreaEvents() {
  if (mListeningForScrollAreaEvents) return;

  nsIFrame* scrollArea = do_QueryFrame(mScrollableFrame);
  scrollArea->GetContent()->AddEventListener(u"mousemove"_ns, this, true);
  mListeningForScrollAreaEvents = true;
}

void ScrollbarActivity::StopListeningForScrollAreaEvents() {
  if (!mListeningForScrollAreaEvents) return;

  nsIFrame* scrollArea = do_QueryFrame(mScrollableFrame);
  scrollArea->GetContent()->RemoveEventListener(u"mousemove"_ns, this, true);
  mListeningForScrollAreaEvents = false;
}

void ScrollbarActivity::AddScrollbarEventListeners(
    dom::EventTarget* aScrollbar) {
  if (aScrollbar) {
    aScrollbar->AddEventListener(u"mousedown"_ns, this, true);
    aScrollbar->AddEventListener(u"mouseup"_ns, this, true);
    aScrollbar->AddEventListener(u"mouseover"_ns, this, true);
    aScrollbar->AddEventListener(u"mouseout"_ns, this, true);
  }
}

void ScrollbarActivity::RemoveScrollbarEventListeners(
    dom::EventTarget* aScrollbar) {
  if (aScrollbar) {
    aScrollbar->RemoveEventListener(u"mousedown"_ns, this, true);
    aScrollbar->RemoveEventListener(u"mouseup"_ns, this, true);
    aScrollbar->RemoveEventListener(u"mouseover"_ns, this, true);
    aScrollbar->RemoveEventListener(u"mouseout"_ns, this, true);
  }
}

void ScrollbarActivity::BeginFade() {
  NS_ASSERTION(mIsActive, "can't begin fade when we're already inactive");
  NS_ASSERTION(!IsActivityOngoing(),
               "why wasn't the fade begin timer cancelled when scrollbar "
               "activity started?");
  NS_ASSERTION(!mIsFading, "shouldn't be fading just yet");

  CancelFadeBeginTimer();
  mFadeBeginTime = TimeStamp::Now();
  if (!SetIsFading(true)) {
    return;
  }
  RegisterWithRefreshDriver();

  NS_ASSERTION(mIsActive, "only fade while scrollbars are visible");
  NS_ASSERTION(mIsFading, "should be fading now");
}

void ScrollbarActivity::EndFade() {
  NS_ASSERTION(mIsActive, "still need to be active at this point");
  NS_ASSERTION(!IsActivityOngoing(),
               "why wasn't the fade end timer cancelled when scrollbar "
               "activity started?");

  if (!SetIsFading(false)) {
    return;
  }
  SetIsActive(false);
  UnregisterFromRefreshDriver();
  StopListeningForScrollbarEvents();
  if (!mDisplayOnMouseMove) {
    StopListeningForScrollAreaEvents();
  }

  NS_ASSERTION(!mIsActive, "should have gone inactive after fade end");
  NS_ASSERTION(!mIsFading, "shouldn't be fading anymore");
}

void ScrollbarActivity::RegisterWithRefreshDriver() {
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (refreshDriver) {
    refreshDriver->AddRefreshObserver(this, FlushType::Style,
                                      "Scrollbar fade animation");
  }
}

void ScrollbarActivity::UnregisterFromRefreshDriver() {
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (refreshDriver) {
    refreshDriver->RemoveRefreshObserver(this, FlushType::Style);
  }
}

static void SetBooleanAttribute(Element* aElement, nsAtom* aAttribute,
                                bool aValue) {
  if (aElement) {
    if (aValue) {
      aElement->SetAttr(kNameSpaceID_None, aAttribute, u"true"_ns, true);
    } else {
      aElement->UnsetAttr(kNameSpaceID_None, aAttribute, true);
    }
  }
}

void ScrollbarActivity::SetIsActive(bool aNewActive) {
  if (mIsActive == aNewActive) return;

  mIsActive = aNewActive;
  if (!mIsActive) {
    // Clear sticky scrollbar hover status.
    HoveredScrollbar(nullptr);
  }

  SetBooleanAttribute(GetHorizontalScrollbar(), nsGkAtoms::active, mIsActive);
  SetBooleanAttribute(GetVerticalScrollbar(), nsGkAtoms::active, mIsActive);
}

static void SetOpacityOnElement(nsIContent* aContent, double aOpacity) {
  if (RefPtr<nsStyledElement> inlineStyleContent =
          nsStyledElement::FromNodeOrNull(aContent)) {
    nsICSSDeclaration* decl = inlineStyleContent->Style();
    nsAutoCString str;
    str.AppendFloat(aOpacity);
    decl->SetProperty("opacity"_ns, str, EmptyCString(), IgnoreErrors());
  }
}

bool ScrollbarActivity::UpdateOpacity(TimeStamp aTime) {
  // Avoid division by zero if mScrollbarFadeDuration is zero, just jump
  // to the end of the fade animation
  double progress = mScrollbarFadeDuration
                        ? ((aTime - mFadeBeginTime) / FadeDuration())
                        : 1.0;
  double opacity = 1.0 - std::max(0.0, std::min(1.0, progress));

  // 'this' may be getting destroyed during SetOpacityOnElement calls.
  AutoWeakFrame weakFrame((do_QueryFrame(mScrollableFrame)));
  SetOpacityOnElement(GetHorizontalScrollbar(), opacity);
  if (!weakFrame.IsAlive()) {
    return false;
  }
  SetOpacityOnElement(GetVerticalScrollbar(), opacity);
  if (!weakFrame.IsAlive()) {
    return false;
  }
  return true;
}

static void UnsetOpacityOnElement(nsIContent* aContent) {
  if (RefPtr<nsStyledElement> inlineStyleContent =
          nsStyledElement::FromNodeOrNull(aContent)) {
    nsICSSDeclaration* decl = inlineStyleContent->Style();
    nsAutoCString dummy;
    decl->RemoveProperty("opacity"_ns, dummy, IgnoreErrors());
  }
}

bool ScrollbarActivity::SetIsFading(bool aNewFading) {
  if (mIsFading == aNewFading) return true;

  mIsFading = aNewFading;
  if (!mIsFading) {
    mFadeBeginTime = TimeStamp();
    // 'this' may be getting destroyed during UnsetOpacityOnElement calls.
    AutoWeakFrame weakFrame((do_QueryFrame(mScrollableFrame)));
    UnsetOpacityOnElement(GetHorizontalScrollbar());
    if (!weakFrame.IsAlive()) {
      return false;
    }
    UnsetOpacityOnElement(GetVerticalScrollbar());
    if (!weakFrame.IsAlive()) {
      return false;
    }
  }
  return true;
}

void ScrollbarActivity::StartFadeBeginTimer() {
  if (StaticPrefs::layout_testing_overlay_scrollbars_always_visible()) {
    return;
  }
  if (!mFadeBeginTimer) {
    mFadeBeginTimer = NS_NewTimer();
  }
  mFadeBeginTimer->InitWithNamedFuncCallback(
      FadeBeginTimerFired, this, mScrollbarFadeBeginDelay,
      nsITimer::TYPE_ONE_SHOT, "ScrollbarActivity::FadeBeginTimerFired");
}

void ScrollbarActivity::CancelFadeBeginTimer() {
  if (mFadeBeginTimer) {
    mFadeBeginTimer->Cancel();
  }
}

static void MaybeInvalidateScrollbarForHover(
    Element* aScrollbarToInvalidate, Element* aScrollbarAboutToGetHover) {
  if (aScrollbarToInvalidate) {
    bool hasHover =
        aScrollbarToInvalidate->HasAttr(kNameSpaceID_None, nsGkAtoms::hover);
    bool willHaveHover = (aScrollbarAboutToGetHover == aScrollbarToInvalidate);

    if (hasHover != willHaveHover) {
      if (nsIFrame* f = aScrollbarToInvalidate->GetPrimaryFrame()) {
        f->SchedulePaint();
      }
    }
  }
}

void ScrollbarActivity::HoveredScrollbar(Element* aScrollbar) {
  Element* vertScrollbar = GetVerticalScrollbar();
  Element* horzScrollbar = GetHorizontalScrollbar();
  MaybeInvalidateScrollbarForHover(vertScrollbar, aScrollbar);
  MaybeInvalidateScrollbarForHover(horzScrollbar, aScrollbar);

  SetBooleanAttribute(horzScrollbar, nsGkAtoms::hover, false);
  SetBooleanAttribute(vertScrollbar, nsGkAtoms::hover, false);
  SetBooleanAttribute(aScrollbar, nsGkAtoms::hover, true);
}

nsRefreshDriver* ScrollbarActivity::GetRefreshDriver() {
  nsIFrame* scrollableFrame = do_QueryFrame(mScrollableFrame);
  return scrollableFrame->PresContext()->RefreshDriver();
}

Element* ScrollbarActivity::GetScrollbarContent(bool aVertical) {
  nsIFrame* box = mScrollableFrame->GetScrollbarBox(aVertical);
  return box ? box->GetContent()->AsElement() : nullptr;
}

}  // namespace layout
}  // namespace mozilla
