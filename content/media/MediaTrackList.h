/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaTrackList_h
#define mozilla_dom_MediaTrackList_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class HTMLMediaElement;
class MediaTrack;
class AudioTrackList;
class VideoTrackList;

/**
 * Base class of AudioTrackList and VideoTrackList. The AudioTrackList and
 * VideoTrackList objects represent a dynamic list of zero or more audio and
 * video tracks respectively.
 *
 * When a media element is to forget its media-resource-specific tracks, its
 * audio track list and video track list will be emptied.
 */
class MediaTrackList : public DOMEventTargetHelper
{
public:
  MediaTrackList(nsPIDOMWindow* aOwnerWindow, HTMLMediaElement* aMediaElement);
  virtual ~MediaTrackList();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaTrackList, DOMEventTargetHelper)

  using DOMEventTargetHelper::DispatchTrustedEvent;

  // The return value is non-null, assert an error if aIndex is out of bound
  // for array mTracks.
  MediaTrack* operator[](uint32_t aIndex);

  void AddTrack(MediaTrack* aTrack);

  void RemoveTrack(const nsRefPtr<MediaTrack>& aTrack);

  virtual void EmptyTracks();

  void CreateAndDispatchChangeEvent();

  // WebIDL
  MediaTrack* IndexedGetter(uint32_t aIndex, bool& aFound);

  MediaTrack* GetTrackById(const nsAString& aId);

  uint32_t Length() const
  {
    return mTracks.Length();
  }

  IMPL_EVENT_HANDLER(change)
  IMPL_EVENT_HANDLER(addtrack)
  IMPL_EVENT_HANDLER(removetrack)

protected:
  void CreateAndDispatchTrackEventRunner(MediaTrack* aTrack,
                                         const nsAString& aEventName);

  nsTArray<nsRefPtr<MediaTrack>> mTracks;
  nsRefPtr<HTMLMediaElement> mMediaElement;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaTrackList_h
