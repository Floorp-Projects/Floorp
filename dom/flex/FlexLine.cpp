/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlexLine.h"

#include "FlexItem.h"
#include "mozilla/dom/FlexBinding.h"
#include "nsFlexContainerFrame.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FlexLine, mParent, mItems)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FlexLine)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FlexLine)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FlexLine)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FlexLine::FlexLine(Flex* aParent,
                   const ComputedFlexLineInfo* aLine)
  : mParent(aParent)
{
  MOZ_ASSERT(aLine,
    "Should never be instantiated with a null ComputedFlexLineInfo.");

  // Eagerly copy values from aLine, because we're not
  // going to keep it around.
  switch (aLine->mGrowthState) {
    case ComputedFlexLineInfo::GrowthState::SHRINKING:
      mGrowthState = FlexLineGrowthState::Shrinking;
      break;

    case ComputedFlexLineInfo::GrowthState::GROWING:
      mGrowthState = FlexLineGrowthState::Growing;
      break;

    default:
      mGrowthState = FlexLineGrowthState::Unchanged;
  };

  // Convert all the app unit values into css pixels.
  mCrossStart = nsPresContext::AppUnitsToDoubleCSSPixels(
    aLine->mCrossStart);
  mCrossSize = nsPresContext::AppUnitsToDoubleCSSPixels(
    aLine->mCrossSize);
  mFirstBaselineOffset = nsPresContext::AppUnitsToDoubleCSSPixels(
    aLine->mFirstBaselineOffset);
  mLastBaselineOffset = nsPresContext::AppUnitsToDoubleCSSPixels(
    aLine->mLastBaselineOffset);

  mItems.SetLength(aLine->mItems.Length());
  uint32_t index = 0;
  for (auto&& i : aLine->mItems) {
    FlexItem* item = new FlexItem(this, &i);
    mItems.ElementAt(index) = item;
    index++;
  }
}

JSObject*
FlexLine::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FlexLineBinding::Wrap(aCx, this, aGivenProto);
}

FlexLineGrowthState
FlexLine::GrowthState() const
{
  return mGrowthState;
}

double
FlexLine::CrossStart() const
{
  return mCrossStart;
}

double
FlexLine::CrossSize() const
{
  return mCrossSize;
}

double
FlexLine::FirstBaselineOffset() const
{
  return mFirstBaselineOffset;
}

double
FlexLine::LastBaselineOffset() const
{
  return mLastBaselineOffset;
}

void
FlexLine::GetItems(nsTArray<RefPtr<FlexItem>>& aResult)
{
  aResult.AppendElements(mItems);
}

} // namespace dom
} // namespace mozilla
