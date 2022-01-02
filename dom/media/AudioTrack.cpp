/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackBinding.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/HTMLMediaElement.h"

namespace mozilla::dom {

AudioTrack::AudioTrack(nsIGlobalObject* aOwnerGlobal, const nsAString& aId,
                       const nsAString& aKind, const nsAString& aLabel,
                       const nsAString& aLanguage, bool aEnabled,
                       AudioStreamTrack* aStreamTrack)
    : MediaTrack(aOwnerGlobal, aId, aKind, aLabel, aLanguage),
      mEnabled(aEnabled),
      mAudioStreamTrack(aStreamTrack) {}

AudioTrack::~AudioTrack() = default;

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioTrack, MediaTrack, mAudioStreamTrack)

NS_IMPL_ADDREF_INHERITED(AudioTrack, MediaTrack)
NS_IMPL_RELEASE_INHERITED(AudioTrack, MediaTrack)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioTrack)
NS_INTERFACE_MAP_END_INHERITING(MediaTrack)

JSObject* AudioTrack::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return AudioTrack_Binding::Wrap(aCx, this, aGivenProto);
}

void AudioTrack::SetEnabled(bool aEnabled) {
  SetEnabledInternal(aEnabled, MediaTrack::DEFAULT);
}

void AudioTrack::SetEnabledInternal(bool aEnabled, int aFlags) {
  if (aEnabled == mEnabled) {
    return;
  }
  mEnabled = aEnabled;

  // If this AudioTrack is no longer in its original AudioTrackList, then
  // whether it is enabled or not has no effect on its original list.
  if (!mList) {
    return;
  }

  if (mEnabled) {
    HTMLMediaElement* element = mList->GetMediaElement();
    if (element) {
      element->NotifyMediaTrackEnabled(this);
    }
  } else {
    HTMLMediaElement* element = mList->GetMediaElement();
    if (element) {
      element->NotifyMediaTrackDisabled(this);
    }
  }

  if (!(aFlags & MediaTrack::FIRE_NO_EVENTS)) {
    mList->CreateAndDispatchChangeEvent();
  }
}

}  // namespace mozilla::dom
