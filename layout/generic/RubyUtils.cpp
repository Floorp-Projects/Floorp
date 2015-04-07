/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RubyUtils.h"
#include "nsRubyBaseContainerFrame.h"

using namespace mozilla;

NS_DECLARE_FRAME_PROPERTY(ReservedISize, nullptr);

union NSCoordValue
{
  nscoord mCoord;
  void* mPointer;
  static_assert(sizeof(nscoord) <= sizeof(void*),
                "Cannot store nscoord in pointer");
};

/* static */ void
RubyUtils::SetReservedISize(nsIFrame* aFrame, nscoord aISize)
{
  MOZ_ASSERT(IsExpandableRubyBox(aFrame));
  NSCoordValue value = { aISize };
  aFrame->Properties().Set(ReservedISize(), value.mPointer);
}

/* static */ void
RubyUtils::ClearReservedISize(nsIFrame* aFrame)
{
  MOZ_ASSERT(IsExpandableRubyBox(aFrame));
  aFrame->Properties().Remove(ReservedISize());
}

/* static */ nscoord
RubyUtils::GetReservedISize(nsIFrame* aFrame)
{
  MOZ_ASSERT(IsExpandableRubyBox(aFrame));
  NSCoordValue value;
  value.mPointer = aFrame->Properties().Get(ReservedISize());
  return value.mCoord;
}

RubyTextContainerIterator::RubyTextContainerIterator(
  nsRubyBaseContainerFrame* aBaseContainer)
{
  mFrame = aBaseContainer;
  Next();
}

void
RubyTextContainerIterator::Next()
{
  MOZ_ASSERT(mFrame, "Should have checked AtEnd()");
  mFrame = mFrame->GetNextSibling();
  if (mFrame && mFrame->GetType() != nsGkAtoms::rubyTextContainerFrame) {
    mFrame = nullptr;
  }
}
