/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarActivity.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsComponentManagerUtils.h"

namespace mozilla {

ScrollbarActivity::~ScrollbarActivity()
{
  CancelActivityFinishedTimer();
}

void
ScrollbarActivity::ActivityOccurred()
{
  CancelActivityFinishedTimer();
  StartActivityFinishedTimer();

  SetIsActive(true);
  NS_ASSERTION(mIsActive, "need to be active during activity");
}

void
ScrollbarActivity::ActivityFinished()
{
  SetIsActive(false);
  NS_ASSERTION(!mIsActive, "need to be unactive once activity is finished");
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

  SetBooleanAttribute(GetHorizontalScrollbar(), nsGkAtoms::active, mIsActive);
  SetBooleanAttribute(GetVerticalScrollbar(), nsGkAtoms::active, mIsActive);
}

void
ScrollbarActivity::StartActivityFinishedTimer()
{
  NS_ASSERTION(!mActivityFinishedTimer, "timer already alive!");
  mActivityFinishedTimer = do_CreateInstance("@mozilla.org/timer;1");
  mActivityFinishedTimer->InitWithFuncCallback(ActivityFinishedTimerFired, this,
                                            kScrollbarActivityFinishedDelay,
                                            nsITimer::TYPE_ONE_SHOT);
}

void
ScrollbarActivity::CancelActivityFinishedTimer()
{
  if (mActivityFinishedTimer) {
    mActivityFinishedTimer->Cancel();
    mActivityFinishedTimer = nullptr;
  }
}

nsIContent*
ScrollbarActivity::GetScrollbarContent(bool aVertical)
{
  nsIFrame* box = mScrollableFrame->GetScrollbarBox(aVertical);
  return box ? box->GetContent() : nullptr;

  return nullptr;
}

} // namespace mozilla
