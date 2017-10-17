/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlexItem.h"

#include "mozilla/dom/FlexBinding.h"
#include "nsFlexContainerFrame.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FlexItem, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FlexItem)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FlexItem)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FlexItem)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FlexItem::FlexItem(FlexLine* aParent,
                   const ComputedFlexItemInfo* aItem)
  : mParent(aParent)
{
  MOZ_ASSERT(aItem,
    "Should never be instantiated with a null ComputedFlexLineInfo.");

  // Eagerly copy values from aItem, because we're not
  // going to keep it around.
  mNode = aItem->mNode;

  // Convert app unit sizes to css pixel sizes.
  mMainBaseSize = nsPresContext::AppUnitsToDoubleCSSPixels(
    aItem->mMainBaseSize);
  mMainDeltaSize = nsPresContext::AppUnitsToDoubleCSSPixels(
    aItem->mMainDeltaSize);
  mMainMinSize = nsPresContext::AppUnitsToDoubleCSSPixels(
    aItem->mMainMinSize);
  mMainMaxSize = nsPresContext::AppUnitsToDoubleCSSPixels(
    aItem->mMainMaxSize);
  mCrossMinSize = nsPresContext::AppUnitsToDoubleCSSPixels(
    aItem->mCrossMinSize);
  mCrossMaxSize = nsPresContext::AppUnitsToDoubleCSSPixels(
    aItem->mCrossMaxSize);
}

JSObject*
FlexItem::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FlexItemBinding::Wrap(aCx, this, aGivenProto);
}

nsINode*
FlexItem::GetNode() const
{
  return mNode;
}

double
FlexItem::MainBaseSize() const
{
  return mMainBaseSize;
}

double
FlexItem::MainDeltaSize() const
{
  return mMainDeltaSize;
}

double
FlexItem::MainMinSize() const
{
  return mMainMinSize;
}

double
FlexItem::MainMaxSize() const
{
  return mMainMaxSize;
}

double
FlexItem::CrossMinSize() const
{
  return mCrossMinSize;
}

double
FlexItem::CrossMaxSize() const
{
  return mCrossMaxSize;
}

} // namespace dom
} // namespace mozilla
