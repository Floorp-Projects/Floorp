/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackList.h"
#include "mozilla/dom/VideoTrackListBinding.h"

namespace mozilla {
namespace dom {

JSObject*
VideoTrackList::WrapObject(JSContext* aCx)
{
  return VideoTrackListBinding::Wrap(aCx, this);
}

VideoTrack*
VideoTrackList::operator[](uint32_t aIndex)
{
  MediaTrack* track = MediaTrackList::operator[](aIndex);
  return track->AsVideoTrack();
}

void
VideoTrackList::EmptyTracks()
{
  mSelectedIndex = -1;
  MediaTrackList::EmptyTracks();
}

VideoTrack*
VideoTrackList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  MediaTrack* track = MediaTrackList::IndexedGetter(aIndex, aFound);
  return track ? track->AsVideoTrack() : nullptr;
}

VideoTrack*
VideoTrackList::GetTrackById(const nsAString& aId)
{
  MediaTrack* track = MediaTrackList::GetTrackById(aId);
  return track ? track->AsVideoTrack() : nullptr;
}

} // namespace dom
} // namespace mozilla
