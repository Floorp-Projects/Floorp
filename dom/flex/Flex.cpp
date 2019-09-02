/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Flex.h"

#include "FlexLineValues.h"
#include "mozilla/dom/FlexBinding.h"
#include "nsFlexContainerFrame.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Flex, mParent, mLines)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Flex)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Flex)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Flex)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Flex::Flex(Element* aParent, nsFlexContainerFrame* aFrame) : mParent(aParent) {
  MOZ_ASSERT(aFrame,
             "Should never be instantiated with a null nsFlexContainerFrame");

  // Eagerly create property values from aFrame, because we're not
  // going to keep it around.
  const ComputedFlexContainerInfo* containerInfo =
      aFrame->GetFlexContainerInfo();
  if (!containerInfo) {
    // It's weird but possible to fail to get a ComputedFlexContainerInfo
    // structure. Assign sensible default values.
    mMainAxisDirection = FlexPhysicalDirection::Horizontal_lr;
    mCrossAxisDirection = FlexPhysicalDirection::Vertical_tb;
    return;
  }
  mLines.SetLength(containerInfo->mLines.Length());
  uint32_t index = 0;
  for (auto&& l : containerInfo->mLines) {
    FlexLineValues* line = new FlexLineValues(this, &l);
    mLines.ElementAt(index) = line;
    index++;
  }

  mMainAxisDirection = containerInfo->mMainAxisDirection;
  mCrossAxisDirection = containerInfo->mCrossAxisDirection;
}

JSObject* Flex::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return Flex_Binding::Wrap(aCx, this, aGivenProto);
}

void Flex::GetLines(nsTArray<RefPtr<FlexLineValues>>& aResult) {
  aResult.AppendElements(mLines);
}

FlexPhysicalDirection Flex::MainAxisDirection() const {
  return mMainAxisDirection;
}

FlexPhysicalDirection Flex::CrossAxisDirection() const {
  return mCrossAxisDirection;
}

}  // namespace dom
}  // namespace mozilla
