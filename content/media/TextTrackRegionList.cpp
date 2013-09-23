/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/dom/TextTrackRegionList.h"
#include "mozilla/dom/VTTRegionListBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(TextTrackRegionList,
                                        mParent,
                                        mTextTrackRegions)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TextTrackRegionList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextTrackRegionList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrackRegionList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
TextTrackRegionList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return VTTRegionListBinding::Wrap(aCx, aScope, this);
}

TextTrackRegionList::TextTrackRegionList(nsISupports* aGlobal)
  : mParent(aGlobal)
{
  SetIsDOMBinding();
}

TextTrackRegion*
TextTrackRegionList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mTextTrackRegions.Length();
  return aFound ? mTextTrackRegions[aIndex] : nullptr;
}

TextTrackRegion*
TextTrackRegionList::GetRegionById(const nsAString& aId)
{
  if (aId.IsEmpty()) {
    return nullptr;
  }

  for (uint32_t i = 0; i < Length(); ++i) {
    if (aId.Equals(mTextTrackRegions[i]->Id())) {
      return mTextTrackRegions[i];
    }
  }

  return nullptr;
}

void
TextTrackRegionList::AddTextTrackRegion(TextTrackRegion* aRegion)
{
  mTextTrackRegions.AppendElement(aRegion);
}

void
TextTrackRegionList::RemoveTextTrackRegion(const TextTrackRegion& aRegion)
{
  mTextTrackRegions.RemoveElement(&aRegion);
}

} //namespace dom
} //namespace mozilla
