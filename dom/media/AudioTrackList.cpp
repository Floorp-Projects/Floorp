/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/AudioTrackListBinding.h"

namespace mozilla {
namespace dom {

JSObject*
AudioTrackList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioTrackListBinding::Wrap(aCx, this, aGivenProto);
}

AudioTrack*
AudioTrackList::operator[](uint32_t aIndex)
{
  MediaTrack* track = MediaTrackList::operator[](aIndex);
  return track->AsAudioTrack();
}

AudioTrack*
AudioTrackList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  MediaTrack* track = MediaTrackList::IndexedGetter(aIndex, aFound);
  return track ? track->AsAudioTrack() : nullptr;
}

AudioTrack*
AudioTrackList::GetTrackById(const nsAString& aId)
{
  MediaTrack* track = MediaTrackList::GetTrackById(aId);
  return track ? track->AsAudioTrack() : nullptr;
}

} // namespace dom
} // namespace mozilla
