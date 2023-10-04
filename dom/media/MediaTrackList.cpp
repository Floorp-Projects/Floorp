/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrack.h"
#include "MediaTrackList.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/TrackEvent.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

MediaTrackList::MediaTrackList(nsIGlobalObject* aOwnerObject,
                               HTMLMediaElement* aMediaElement)
    : DOMEventTargetHelper(aOwnerObject), mMediaElement(aMediaElement) {}

MediaTrackList::~MediaTrackList() = default;

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaTrackList, DOMEventTargetHelper,
                                   mTracks, mMediaElement)

NS_IMPL_ADDREF_INHERITED(MediaTrackList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaTrackList, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaTrackList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

MediaTrack* MediaTrackList::operator[](uint32_t aIndex) {
  return mTracks.ElementAt(aIndex);
}

MediaTrack* MediaTrackList::IndexedGetter(uint32_t aIndex, bool& aFound) {
  aFound = aIndex < mTracks.Length();
  if (!aFound) {
    return nullptr;
  }
  return mTracks[aIndex];
}

MediaTrack* MediaTrackList::GetTrackById(const nsAString& aId) {
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    if (aId.Equals(mTracks[i]->GetId())) {
      return mTracks[i];
    }
  }
  return nullptr;
}

void MediaTrackList::AddTrack(MediaTrack* aTrack) {
  MOZ_ASSERT(aTrack->GetOwnerGlobal() == GetOwnerGlobal(),
             "Where is this track from?");
  mTracks.AppendElement(aTrack);
  aTrack->SetTrackList(this);
  CreateAndDispatchTrackEventRunner(aTrack, u"addtrack"_ns);

  if (HTMLMediaElement* element = GetMediaElement()) {
    element->NotifyMediaTrackAdded(aTrack);
  }

  if ((!aTrack->AsAudioTrack() || !aTrack->AsAudioTrack()->Enabled()) &&
      (!aTrack->AsVideoTrack() || !aTrack->AsVideoTrack()->Selected())) {
    // Track not enabled, no need to notify media element.
    return;
  }

  if (HTMLMediaElement* element = GetMediaElement()) {
    element->NotifyMediaTrackEnabled(aTrack);
  }
}

void MediaTrackList::RemoveTrack(const RefPtr<MediaTrack>& aTrack) {
  mTracks.RemoveElement(aTrack);
  aTrack->SetEnabledInternal(false, MediaTrack::FIRE_NO_EVENTS);
  aTrack->SetTrackList(nullptr);
  CreateAndDispatchTrackEventRunner(aTrack, u"removetrack"_ns);
  if (HTMLMediaElement* element = GetMediaElement()) {
    element->NotifyMediaTrackRemoved(aTrack);
  }
}

void MediaTrackList::RemoveTracks() {
  while (!mTracks.IsEmpty()) {
    RefPtr<MediaTrack> track = mTracks.LastElement();
    RemoveTrack(track);
  }
}

already_AddRefed<AudioTrack> MediaTrackList::CreateAudioTrack(
    nsIGlobalObject* aOwnerGlobal, const nsAString& aId, const nsAString& aKind,
    const nsAString& aLabel, const nsAString& aLanguage, bool aEnabled,
    AudioStreamTrack* aAudioTrack) {
  RefPtr<AudioTrack> track = new AudioTrack(aOwnerGlobal, aId, aKind, aLabel,
                                            aLanguage, aEnabled, aAudioTrack);
  return track.forget();
}

already_AddRefed<VideoTrack> MediaTrackList::CreateVideoTrack(
    nsIGlobalObject* aOwnerGlobal, const nsAString& aId, const nsAString& aKind,
    const nsAString& aLabel, const nsAString& aLanguage,
    VideoStreamTrack* aVideoTrack) {
  RefPtr<VideoTrack> track =
      new VideoTrack(aOwnerGlobal, aId, aKind, aLabel, aLanguage, aVideoTrack);
  return track.forget();
}

void MediaTrackList::EmptyTracks() {
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    mTracks[i]->SetEnabledInternal(false, MediaTrack::FIRE_NO_EVENTS);
    mTracks[i]->SetTrackList(nullptr);
  }
  mTracks.Clear();
}

void MediaTrackList::CreateAndDispatchChangeEvent() {
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, u"change"_ns, CanBubble::eNo);
  asyncDispatcher->PostDOMEvent();
}

void MediaTrackList::CreateAndDispatchTrackEventRunner(
    MediaTrack* aTrack, const nsAString& aEventName) {
  TrackEventInit eventInit;

  if (aTrack->AsAudioTrack()) {
    eventInit.mTrack.SetValue().SetAsAudioTrack() = aTrack->AsAudioTrack();
  } else if (aTrack->AsVideoTrack()) {
    eventInit.mTrack.SetValue().SetAsVideoTrack() = aTrack->AsVideoTrack();
  }

  RefPtr<TrackEvent> event =
      TrackEvent::Constructor(this, aEventName, eventInit);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, event.forget());
  asyncDispatcher->PostDOMEvent();
}

}  // namespace mozilla::dom
