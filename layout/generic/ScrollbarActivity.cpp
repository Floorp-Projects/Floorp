/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarActivity.h"
#include "nsIScrollbarOwner.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIFrame.h"
#include "nsContentUtils.h"
#include "nsAString.h"
#include "nsQueryFrame.h"
#include "nsComponentManagerUtils.h"

namespace mozilla {
namespace layout {

NS_IMPL_ISUPPORTS1(ScrollbarActivity, nsIDOMEventListener)

void
ScrollbarActivity::Destroy()
{
  StopListeningForEvents();
  UnregisterFromRefreshDriver();
  CancelFadeBeginTimer();
}

void
ScrollbarActivity::ActivityOccurred()
{
  ActivityStarted();
  ActivityStopped();
}

void
ScrollbarActivity::ActivityStarted()
{
  mNestedActivityCounter++;
  CancelFadeBeginTimer();
  SetIsFading(false);
  UnregisterFromRefreshDriver();
  StartListeningForEvents();
  SetIsActive(true);

  NS_ASSERTION(mIsActive, "need to be active during activity");
  NS_ASSERTION(!mIsFading, "must not be fading during activity");
  NS_ASSERTION(!mFadeBeginTimer, "fade begin timer shouldn't be running");
}

void
ScrollbarActivity::ActivityStopped()
{
  NS_ASSERTION(IsActivityOngoing(), "activity stopped while none was going on");
  NS_ASSERTION(mIsActive, "need to be active during activity");
  NS_ASSERTION(!mIsFading, "must not be fading during ongoing activity");
  NS_ASSERTION(!mFadeBeginTimer, "must not be waiting for fade during ongoing activity");

  mNestedActivityCounter--;

  if (!IsActivityOngoing()) {
    StartFadeBeginTimer();

    NS_ASSERTION(mIsActive, "need to be active right after activity");
    NS_ASSERTION(!mIsFading, "must not be fading right after activity");
    NS_ASSERTION(mFadeBeginTimer, "fade begin timer should be running");
  }
}

NS_IMETHODIMP
ScrollbarActivity::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!mIsActive)
    return NS_OK;

  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("mousemove")) {
    // Mouse motions anywhere in the scrollable frame should keep the
    // scrollbars visible.
    ActivityOccurred();
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetOriginalTarget(getter_AddRefs(target));
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);

  HandleEventForScrollbar(type, targetContent, GetHorizontalScrollbar(),
                          &mHScrollbarHovered);
  HandleEventForScrollbar(type, targetContent, GetVerticalScrollbar(),
                          &mVScrollbarHovered);

  return NS_OK;
}

void
ScrollbarActivity::WillRefresh(TimeStamp aTime)
{
  NS_ASSERTION(mIsActive, "should only fade while scrollbars are visible");
  NS_ASSERTION(!IsActivityOngoing(), "why weren't we unregistered from the refresh driver when scrollbar activity started?");
  NS_ASSERTION(mIsFading, "should only animate fading during fade");

  if (!UpdateOpacity(aTime)) {
    return;
  }

  if (!IsStillFading(aTime)) {
    EndFade();
  }
}

bool
ScrollbarActivity::IsStillFading(TimeStamp aTime)
{
  return !mFadeBeginTime.IsNull() && (aTime - mFadeBeginTime < FadeDuration());
}

void
ScrollbarActivity::HandleEventForScrollbar(const nsAString& aType,
                                           nsIContent* aTarget,
                                           nsIContent* aScrollbar,
                                           bool* aStoredHoverState)
{
  if (!aTarget || !aScrollbar ||
      !nsContentUtils::ContentIsDescendantOf(aTarget, aScrollbar))
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

void
ScrollbarActivity::StartListeningForEvents()
{
  if (mListeningForEvents)
    return;

  nsIFrame* scrollArea = do_QueryFrame(mScrollableFrame);
  nsCOMPtr<nsIDOMEventTarget> scrollAreaTarget = do_QueryInterface(
                                                   scrollArea->GetContent());
  mHorizontalScrollbar = do_QueryInterface(GetHorizontalScrollbar());
  mVerticalScrollbar = do_QueryInterface(GetVerticalScrollbar());

  if (scrollAreaTarget) {
    scrollAreaTarget->AddEventListener(NS_LITERAL_STRING("mousemove"), this,
                                       true);
  }
  StartListeningForEventsOnScrollbar(mHorizontalScrollbar);
  StartListeningForEventsOnScrollbar(mVerticalScrollbar);
  mListeningForEvents = true;
}

void
ScrollbarActivity::StartListeningForEventsOnScrollbar(nsIDOMEventTarget* aScrollbar)
{
  if (aScrollbar) {
    aScrollbar->AddEventListener(NS_LITERAL_STRING("mousedown"), this, true);
    aScrollbar->AddEventListener(NS_LITERAL_STRING("mouseup"), this, true);
    aScrollbar->AddEventListener(NS_LITERAL_STRING("mouseover"), this, true);
    aScrollbar->AddEventListener(NS_LITERAL_STRING("mouseout"), this, true);
  }
}

void
ScrollbarActivity::StopListeningForEvents()
{
  if (!mListeningForEvents)
    return;

  nsIFrame* scrollArea = do_QueryFrame(mScrollableFrame);
  nsCOMPtr<nsIDOMEventTarget> scrollAreaTarget = do_QueryInterface(scrollArea->GetContent());

  if (scrollAreaTarget) {
    scrollAreaTarget->RemoveEventListener(NS_LITERAL_STRING("mousemove"), this, true);
  }
  StopListeningForEventsOnScrollbar(mHorizontalScrollbar);
  StopListeningForEventsOnScrollbar(mVerticalScrollbar);

  mHorizontalScrollbar = nullptr;
  mVerticalScrollbar = nullptr;
  mListeningForEvents = false;
}

void
ScrollbarActivity::StopListeningForEventsOnScrollbar(nsIDOMEventTarget* aScrollbar)
{
  if (aScrollbar) {
    aScrollbar->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this, true);
    aScrollbar->RemoveEventListener(NS_LITERAL_STRING("mouseup"), this, true);
    aScrollbar->RemoveEventListener(NS_LITERAL_STRING("mouseover"), this, true);
    aScrollbar->RemoveEventListener(NS_LITERAL_STRING("mouseout"), this, true);
  }
}

void
ScrollbarActivity::BeginFade()
{
  NS_ASSERTION(mIsActive, "can't begin fade when we're already inactive");
  NS_ASSERTION(!IsActivityOngoing(), "why wasn't the fade begin timer cancelled when scrollbar activity started?");
  NS_ASSERTION(!mIsFading, "shouldn't be fading just yet");

  CancelFadeBeginTimer();
  mFadeBeginTime = TimeStamp::Now();
  SetIsFading(true);
  RegisterWithRefreshDriver();

  NS_ASSERTION(mIsActive, "only fade while scrollbars are visible");
  NS_ASSERTION(mIsFading, "should be fading now");
}

void
ScrollbarActivity::EndFade()
{
  NS_ASSERTION(mIsActive, "still need to be active at this point");
  NS_ASSERTION(!IsActivityOngoing(), "why wasn't the fade end timer cancelled when scrollbar activity started?");

  SetIsFading(false);
  SetIsActive(false);
  UnregisterFromRefreshDriver();
  StopListeningForEvents();

  NS_ASSERTION(!mIsActive, "should have gone inactive after fade end");
  NS_ASSERTION(!mIsFading, "shouldn't be fading anymore");
  NS_ASSERTION(!mFadeBeginTimer, "fade begin timer shouldn't be running");
}

void
ScrollbarActivity::RegisterWithRefreshDriver()
{
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (refreshDriver) {
    refreshDriver->AddRefreshObserver(this, Flush_Style);
  }
}

void
ScrollbarActivity::UnregisterFromRefreshDriver()
{
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (refreshDriver) {
    refreshDriver->RemoveRefreshObserver(this, Flush_Style);
  }
}

static void
SetBooleanAttribute(nsIContent* aContent, nsIAtom* aAttribute, bool aValue)
{
  if (aContent) {
    if (aValue) {
      aContent->SetAttr(kNameSpaceID_None, aAttribute,
                        NS_LITERAL_STRING("true"), true);
    } else {
      aContent->UnsetAttr(kNameSpaceID_None, aAttribute, true);
    }
  }
}

void
ScrollbarActivity::SetIsActive(bool aNewActive)
{
  if (mIsActive == aNewActive)
    return;

  mIsActive = aNewActive;
  if (!mIsActive) {
    // Clear sticky scrollbar hover status.
    HoveredScrollbar(nullptr);
  }

  SetBooleanAttribute(GetHorizontalScrollbar(), nsGkAtoms::active, mIsActive);
  SetBooleanAttribute(GetVerticalScrollbar(), nsGkAtoms::active, mIsActive);
}

static void
SetOpacityOnElement(nsIContent* aContent, double aOpacity)
{
  nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyleContent =
    do_QueryInterface(aContent);
  if (inlineStyleContent) {
    nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
    inlineStyleContent->GetStyle(getter_AddRefs(decl));
    if (decl) {
      nsAutoString str;
      str.AppendFloat(aOpacity);
      decl->SetProperty(NS_LITERAL_STRING("opacity"), str, EmptyString());
    }
  }
}

bool
ScrollbarActivity::UpdateOpacity(TimeStamp aTime)
{
  double progress = (aTime - mFadeBeginTime) / FadeDuration();
  double opacity = 1.0 - std::max(0.0, std::min(1.0, progress));

  // 'this' may be getting destroyed during SetOpacityOnElement calls.
  nsWeakFrame weakFrame((do_QueryFrame(mScrollableFrame)));
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

static void
UnsetOpacityOnElement(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyleContent =
    do_QueryInterface(aContent);
  if (inlineStyleContent) {
    nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
    inlineStyleContent->GetStyle(getter_AddRefs(decl));
    if (decl) {
      nsAutoString dummy;
      decl->RemoveProperty(NS_LITERAL_STRING("opacity"), dummy);
    }
  }
}

void
ScrollbarActivity::SetIsFading(bool aNewFading)
{
  if (mIsFading == aNewFading)
    return;

  mIsFading = aNewFading;
  if (!mIsFading) {
    mFadeBeginTime = TimeStamp();
    UnsetOpacityOnElement(GetHorizontalScrollbar());
    UnsetOpacityOnElement(GetVerticalScrollbar());
  }
}

void
ScrollbarActivity::StartFadeBeginTimer()
{
  NS_ASSERTION(!mFadeBeginTimer, "timer already alive!");
  mFadeBeginTimer = do_CreateInstance("@mozilla.org/timer;1");
  mFadeBeginTimer->InitWithFuncCallback(FadeBeginTimerFired, this,
                                        kScrollbarFadeBeginDelay,
                                        nsITimer::TYPE_ONE_SHOT);
}

void
ScrollbarActivity::CancelFadeBeginTimer()
{
  if (mFadeBeginTimer) {
    mFadeBeginTimer->Cancel();
    mFadeBeginTimer = nullptr;
  }
}

void
ScrollbarActivity::HoveredScrollbar(nsIContent* aScrollbar)
{
  SetBooleanAttribute(GetHorizontalScrollbar(), nsGkAtoms::hover, false);
  SetBooleanAttribute(GetVerticalScrollbar(), nsGkAtoms::hover, false);
  SetBooleanAttribute(aScrollbar, nsGkAtoms::hover, true);
}

nsRefreshDriver*
ScrollbarActivity::GetRefreshDriver()
{
  nsIFrame* box = mScrollableFrame->GetScrollbarBox(false);
  if (!box) {
    box = mScrollableFrame->GetScrollbarBox(true);
  }
  return box ? box->PresContext()->RefreshDriver() : nullptr;
}

nsIContent*
ScrollbarActivity::GetScrollbarContent(bool aVertical)
{
  nsIFrame* box = mScrollableFrame->GetScrollbarBox(aVertical);
  return box ? box->GetContent() : nullptr;
}

} // namespace layout
} // namespace mozilla
