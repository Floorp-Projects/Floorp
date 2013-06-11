/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackListBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(TextTrackList, mGlobal, mTextTracks)

NS_IMPL_ADDREF_INHERITED(TextTrackList, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackList, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackList)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

TextTrackList::TextTrackList(nsISupports* aGlobal) : mGlobal(aGlobal)
{
  SetIsDOMBinding();
}

void
TextTrackList::Update(double aTime)
{
  uint32_t length = Length(), i;
  for (i = 0; i < length; i++) {
    mTextTracks[i]->Update(aTime);
  }
}

JSObject*
TextTrackList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TextTrackListBinding::Wrap(aCx, aScope, this);
}

TextTrack*
TextTrackList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mTextTracks.Length();
  return aFound ? mTextTracks[aIndex] : nullptr;
}

already_AddRefed<TextTrack>
TextTrackList::AddTextTrack(TextTrackKind aKind,
                            const nsAString& aLabel,
                            const nsAString& aLanguage)
{
  nsRefPtr<TextTrack> track = new TextTrack(mGlobal, aKind, aLabel, aLanguage);
  mTextTracks.AppendElement(track);
  // TODO: dispatch addtrack event
  return track.forget();
}

void
TextTrackList::RemoveTextTrack(const TextTrack& aTrack)
{
  mTextTracks.RemoveElement(&aTrack);
}

} // namespace dom
} // namespace mozilla
