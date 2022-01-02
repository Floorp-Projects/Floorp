/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlexLineValues.h"

#include "Flex.h"
#include "FlexItemValues.h"
#include "mozilla/dom/FlexBinding.h"
#include "nsFlexContainerFrame.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FlexLineValues, mParent, mItems)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FlexLineValues)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FlexLineValues)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FlexLineValues)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FlexLineValues::FlexLineValues(Flex* aParent, const ComputedFlexLineInfo* aLine)
    : mParent(aParent) {
  MOZ_ASSERT(aLine,
             "Should never be instantiated with a null ComputedFlexLineInfo.");

  // Eagerly copy values from aLine, because we're not
  // going to keep it around.
  mGrowthState = aLine->mGrowthState;

  // Convert all the app unit values into css pixels.
  mCrossStart = nsPresContext::AppUnitsToDoubleCSSPixels(aLine->mCrossStart);
  mCrossSize = nsPresContext::AppUnitsToDoubleCSSPixels(aLine->mCrossSize);
  mFirstBaselineOffset =
      nsPresContext::AppUnitsToDoubleCSSPixels(aLine->mFirstBaselineOffset);
  mLastBaselineOffset =
      nsPresContext::AppUnitsToDoubleCSSPixels(aLine->mLastBaselineOffset);

  mItems.SetLength(aLine->mItems.Length());
  uint32_t index = 0;
  for (auto&& i : aLine->mItems) {
    FlexItemValues* item = new FlexItemValues(this, &i);
    mItems.ElementAt(index) = item;
    index++;
  }
}

JSObject* FlexLineValues::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return FlexLineValues_Binding::Wrap(aCx, this, aGivenProto);
}

FlexLineGrowthState FlexLineValues::GrowthState() const { return mGrowthState; }

double FlexLineValues::CrossStart() const { return mCrossStart; }

double FlexLineValues::CrossSize() const { return mCrossSize; }

double FlexLineValues::FirstBaselineOffset() const {
  return mFirstBaselineOffset;
}

double FlexLineValues::LastBaselineOffset() const {
  return mLastBaselineOffset;
}

void FlexLineValues::GetItems(nsTArray<RefPtr<FlexItemValues>>& aResult) {
  aResult.AppendElements(mItems);
}

}  // namespace mozilla::dom
