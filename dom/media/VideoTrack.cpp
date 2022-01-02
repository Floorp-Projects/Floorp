/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackBinding.h"
#include "mozilla/dom/VideoTrackList.h"

namespace mozilla::dom {

VideoTrack::VideoTrack(nsIGlobalObject* aOwnerGlobal, const nsAString& aId,
                       const nsAString& aKind, const nsAString& aLabel,
                       const nsAString& aLanguage,
                       VideoStreamTrack* aStreamTrack)
    : MediaTrack(aOwnerGlobal, aId, aKind, aLabel, aLanguage),
      mSelected(false),
      mVideoStreamTrack(aStreamTrack) {}

VideoTrack::~VideoTrack() = default;

NS_IMPL_CYCLE_COLLECTION_INHERITED(VideoTrack, MediaTrack, mVideoStreamTrack)

NS_IMPL_ADDREF_INHERITED(VideoTrack, MediaTrack)
NS_IMPL_RELEASE_INHERITED(VideoTrack, MediaTrack)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VideoTrack)
NS_INTERFACE_MAP_END_INHERITING(MediaTrack)

JSObject* VideoTrack::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return VideoTrack_Binding::Wrap(aCx, this, aGivenProto);
}

void VideoTrack::SetSelected(bool aSelected) {
  SetEnabledInternal(aSelected, MediaTrack::DEFAULT);
}

void VideoTrack::SetEnabledInternal(bool aEnabled, int aFlags) {
  if (aEnabled == mSelected) {
    return;
  }

  mSelected = aEnabled;

  // If this VideoTrack is no longer in its original VideoTrackList, then
  // whether it is selected or not has no effect on its original list.
  if (!mList) {
    return;
  }

  VideoTrackList& list = static_cast<VideoTrackList&>(*mList);
  if (mSelected) {
    uint32_t curIndex = 0;

    // Unselect all video tracks except the current one.
    for (uint32_t i = 0; i < list.Length(); ++i) {
      if (list[i] == this) {
        curIndex = i;
        continue;
      }

      VideoTrack* track = list[i];
      track->SetSelected(false);
    }

    // Set the index of selected video track to the current's index.
    list.mSelectedIndex = curIndex;

    HTMLMediaElement* element = mList->GetMediaElement();
    if (element) {
      element->NotifyMediaTrackEnabled(this);
    }
  } else {
    list.mSelectedIndex = -1;

    HTMLMediaElement* element = mList->GetMediaElement();
    if (element) {
      element->NotifyMediaTrackDisabled(this);
    }
  }

  // Fire the change event at selection changes on this video track, shall
  // propose a spec change later.
  if (!(aFlags & MediaTrack::FIRE_NO_EVENTS)) {
    list.CreateAndDispatchChangeEvent();
  }
}

}  // namespace mozilla::dom
