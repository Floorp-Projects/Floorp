/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlexItemValues.h"

#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/FlexBinding.h"
#include "mozilla/dom/FlexLineValues.h"
#include "nsFlexContainerFrame.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FlexItemValues, mParent, mNode,
                                      mFrameRect)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FlexItemValues)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FlexItemValues)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FlexItemValues)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/**
 * Utility function to convert a nscoord size to CSS pixel values, with
 * graceful treatment of NS_UNCONSTRAINEDSIZE (which this function converts to
 * the double "positive infinity" value).
 *
 * This function is suitable for converting quantities that are expected to
 * sometimes legitimately (rather than arbitrarily/accidentally) contain the
 * sentinel value NS_UNCONSTRAINEDSIZE -- e.g. to handle "max-width: none".
 */
static double ToPossiblyUnconstrainedPixels(nscoord aSize) {
  if (aSize == NS_UNCONSTRAINEDSIZE) {
    return std::numeric_limits<double>::infinity();
  }
  return nsPresContext::AppUnitsToDoubleCSSPixels(aSize);
}

FlexItemValues::FlexItemValues(FlexLineValues* aParent,
                               const ComputedFlexItemInfo* aItem)
    : mParent(aParent) {
  MOZ_ASSERT(aItem,
             "Should never be instantiated with a null ComputedFlexLineInfo.");

  // Eagerly copy values from aItem, because we're not
  // going to keep it around.
  mNode = aItem->mNode;

  // Since mNode might be null, we associate the mFrameRect with
  // our parent.
  mFrameRect = new DOMRectReadOnly(
      mParent, nsPresContext::AppUnitsToDoubleCSSPixels(aItem->mFrameRect.X()),
      nsPresContext::AppUnitsToDoubleCSSPixels(aItem->mFrameRect.Y()),
      nsPresContext::AppUnitsToDoubleCSSPixels(aItem->mFrameRect.Width()),
      nsPresContext::AppUnitsToDoubleCSSPixels(aItem->mFrameRect.Height()));

  // Convert app unit sizes to css pixel sizes.
  mMainBaseSize =
      nsPresContext::AppUnitsToDoubleCSSPixels(aItem->mMainBaseSize);
  mMainDeltaSize =
      nsPresContext::AppUnitsToDoubleCSSPixels(aItem->mMainDeltaSize);
  mMainMinSize = nsPresContext::AppUnitsToDoubleCSSPixels(aItem->mMainMinSize);
  mMainMaxSize = ToPossiblyUnconstrainedPixels(aItem->mMainMaxSize);
  mCrossMinSize =
      nsPresContext::AppUnitsToDoubleCSSPixels(aItem->mCrossMinSize);
  mCrossMaxSize = ToPossiblyUnconstrainedPixels(aItem->mCrossMaxSize);

  mClampState = aItem->mClampState;
}

JSObject* FlexItemValues::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return FlexItemValues_Binding::Wrap(aCx, this, aGivenProto);
}

nsINode* FlexItemValues::GetNode() const { return mNode; }

DOMRectReadOnly* FlexItemValues::FrameRect() const { return mFrameRect; }

double FlexItemValues::MainBaseSize() const { return mMainBaseSize; }

double FlexItemValues::MainDeltaSize() const { return mMainDeltaSize; }

double FlexItemValues::MainMinSize() const { return mMainMinSize; }

double FlexItemValues::MainMaxSize() const { return mMainMaxSize; }

double FlexItemValues::CrossMinSize() const { return mCrossMinSize; }

double FlexItemValues::CrossMaxSize() const { return mCrossMaxSize; }

FlexItemClampState FlexItemValues::ClampState() const { return mClampState; }

}  // namespace mozilla::dom
