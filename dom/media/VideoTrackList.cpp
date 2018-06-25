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
VideoTrackList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VideoTrackList_Binding::Wrap(aCx, this, aGivenProto);
}

VideoTrack*
VideoTrackList::operator[](uint32_t aIndex)
{
  MediaTrack* track = MediaTrackList::operator[](aIndex);
  return track->AsVideoTrack();
}

void
VideoTrackList::RemoveTrack(const RefPtr<MediaTrack>& aTrack)
{
  // we need to find the video track before |MediaTrackList::RemoveTrack|. Or
  // mSelectedIndex will not be valid. The check of mSelectedIndex == -1
  // need to be done after RemoveTrack. Also the call of
  // |MediaTrackList::RemoveTrack| is necessary even when mSelectedIndex = -1.
  bool found;
  VideoTrack* selectedVideoTrack = IndexedGetter(mSelectedIndex, found);
  MediaTrackList::RemoveTrack(aTrack);
  if (mSelectedIndex == -1) {
    // There was no selected track and we don't select another track on removal.
    return;
  }
  MOZ_ASSERT(found, "When mSelectedIndex is set it should point to a track");
  MOZ_ASSERT(selectedVideoTrack, "The mSelectedIndex should be set to video track only");

  // Let the caller of RemoveTrack deal with choosing the new selected track if
  // it removes the currently-selected track.
  if (aTrack == selectedVideoTrack) {
    mSelectedIndex = -1;
    return;
  }

  // The removed track was not the selected track and there is a
  // currently-selected video track. We need to find the new location of the
  // selected track.
  for (size_t ix = 0; ix < mTracks.Length(); ix++) {
    if (mTracks[ix] == selectedVideoTrack) {
      mSelectedIndex = ix;
      return;
    }
  }
}

void
VideoTrackList::EmptyTracks()
{
  mSelectedIndex = -1;
  MediaTrackList::EmptyTracks();
}

VideoTrack* VideoTrackList::GetSelectedTrack()
{
  if (mSelectedIndex < 0 || static_cast<size_t>(mSelectedIndex) >= mTracks.Length()) {
    return nullptr;
  }

  return operator[](mSelectedIndex);
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
