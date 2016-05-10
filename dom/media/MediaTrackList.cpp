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
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/TrackEvent.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

MediaTrackList::MediaTrackList(nsPIDOMWindowInner* aOwnerWindow,
                               HTMLMediaElement* aMediaElement)
  : DOMEventTargetHelper(aOwnerWindow)
  , mMediaElement(aMediaElement)
{
}

MediaTrackList::~MediaTrackList()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaTrackList,
                                   DOMEventTargetHelper,
                                   mTracks,
                                   mMediaElement)

NS_IMPL_ADDREF_INHERITED(MediaTrackList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaTrackList, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaTrackList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

MediaTrack*
MediaTrackList::operator[](uint32_t aIndex)
{
  return mTracks.ElementAt(aIndex);
}

MediaTrack*
MediaTrackList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mTracks.Length();
  if (!aFound) {
    return nullptr;
  }
  return mTracks[aIndex];
}

MediaTrack*
MediaTrackList::GetTrackById(const nsAString& aId)
{
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    if (aId.Equals(mTracks[i]->GetId())) {
      return mTracks[i];
    }
  }
  return nullptr;
}

void
MediaTrackList::AddTrack(MediaTrack* aTrack)
{
  mTracks.AppendElement(aTrack);
  aTrack->Init(GetOwner());
  aTrack->SetTrackList(this);
  CreateAndDispatchTrackEventRunner(aTrack, NS_LITERAL_STRING("addtrack"));
}

void
MediaTrackList::RemoveTrack(const RefPtr<MediaTrack>& aTrack)
{
  mTracks.RemoveElement(aTrack);
  aTrack->SetTrackList(nullptr);
  CreateAndDispatchTrackEventRunner(aTrack, NS_LITERAL_STRING("removetrack"));
}

void
MediaTrackList::RemoveTracks()
{
  while (!mTracks.IsEmpty()) {
    RefPtr<MediaTrack> track = mTracks.LastElement();
    RemoveTrack(track);
  }
}

already_AddRefed<AudioTrack>
MediaTrackList::CreateAudioTrack(const nsAString& aId,
                                 const nsAString& aKind,
                                 const nsAString& aLabel,
                                 const nsAString& aLanguage,
                                 bool aEnabled)
{
  RefPtr<AudioTrack> track = new AudioTrack(aId, aKind, aLabel, aLanguage,
                                              aEnabled);
  return track.forget();
}

already_AddRefed<VideoTrack>
MediaTrackList::CreateVideoTrack(const nsAString& aId,
                                 const nsAString& aKind,
                                 const nsAString& aLabel,
                                 const nsAString& aLanguage,
                                 VideoStreamTrack* aVideoTrack)
{
  RefPtr<VideoTrack> track = new VideoTrack(aId, aKind, aLabel, aLanguage, aVideoTrack);
  return track.forget();
}

void
MediaTrackList::EmptyTracks()
{
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    mTracks[i]->SetTrackList(nullptr);
  }
  mTracks.Clear();
}

void
MediaTrackList::CreateAndDispatchChangeEvent()
{
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, NS_LITERAL_STRING("change"), false);
  asyncDispatcher->PostDOMEvent();
}

void
MediaTrackList::CreateAndDispatchTrackEventRunner(MediaTrack* aTrack,
                                                  const nsAString& aEventName)
{
  TrackEventInit eventInit;

  if (aTrack->AsAudioTrack()) {
    eventInit.mTrack.SetValue().SetAsAudioTrack() = aTrack->AsAudioTrack();
  } else if (aTrack->AsVideoTrack()) {
    eventInit.mTrack.SetValue().SetAsVideoTrack() = aTrack->AsVideoTrack();
  }

  RefPtr<TrackEvent> event =
    TrackEvent::Constructor(this, aEventName, eventInit);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  asyncDispatcher->PostDOMEvent();
}

} // namespace dom
} // namespace mozilla
