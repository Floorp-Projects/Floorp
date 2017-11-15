/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlexLine.h"

#include "FlexItem.h"
#include "mozilla/dom/FlexBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FlexLine, mParent, mItems)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FlexLine)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FlexLine)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FlexLine)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FlexLine::FlexLine(Flex* aParent)
  : mParent(aParent)
{
}

JSObject*
FlexLine::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FlexLineBinding::Wrap(aCx, this, aGivenProto);
}

FlexLineGrowthState
FlexLine::GrowthState() const
{
  return FlexLineGrowthState::Unchanged;
}

double
FlexLine::CrossSize() const
{
  return 0;
}

double
FlexLine::FirstBaselineOffset() const
{
  return 0;
}

double
FlexLine::LastBaselineOffset() const
{
  return 0;
}

void
FlexLine::GetItems(nsTArray<RefPtr<FlexItem>>& aResult)
{
  aResult.AppendElements(mItems);
}

} // namespace dom
} // namespace mozilla
