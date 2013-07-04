/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/dom/TextTrackCueListBinding.h"
#include "mozilla/dom/TextTrackCue.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(TextTrackCueList, mParent, mList)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextTrackCueList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextTrackCueList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrackCueList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TextTrackCueList::TextTrackCueList(nsISupports* aParent) : mParent(aParent)
{
  SetIsDOMBinding();
}

void
TextTrackCueList::Update(double aTime)
{
  const uint32_t length = mList.Length();
  for (uint32_t i = 0; i < length; i++) {
    if (aTime > mList[i]->StartTime() && aTime < mList[i]->EndTime()) {
      mList[i]->RenderCue();
    }
  }
}

JSObject*
TextTrackCueList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TextTrackCueListBinding::Wrap(aCx, aScope, this);
}

TextTrackCue*
TextTrackCueList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mList.Length();
  return aFound ? mList[aIndex] : nullptr;
}

TextTrackCue*
TextTrackCueList::GetCueById(const nsAString& aId)
{
  if (aId.IsEmpty()) {
    return nullptr;
  }

  for (uint32_t i = 0; i < mList.Length(); i++) {
    if (aId.Equals(mList[i]->Id())) {
      return mList[i];
    }
  }
  return nullptr;
}

void
TextTrackCueList::AddCue(TextTrackCue& cue)
{
  mList.AppendElement(&cue);
}

void
TextTrackCueList::RemoveCue(TextTrackCue& cue)
{
  mList.RemoveElement(&cue);
}

} // namespace dom
} // namespace mozilla
