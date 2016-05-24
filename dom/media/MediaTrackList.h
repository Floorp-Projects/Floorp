/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaTrackList_h
#define mozilla_dom_MediaTrackList_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
class DOMMediaStream;

namespace dom {

class HTMLMediaElement;
class MediaTrack;
class AudioTrackList;
class VideoTrackList;
class AudioTrack;
class VideoTrack;

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
  MediaTrackList(nsPIDOMWindowInner* aOwnerWindow, HTMLMediaElement* aMediaElement);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaTrackList, DOMEventTargetHelper)

  using DOMEventTargetHelper::DispatchTrustedEvent;

  // The return value is non-null, assert an error if aIndex is out of bound
  // for array mTracks.
  MediaTrack* operator[](uint32_t aIndex);

  void AddTrack(MediaTrack* aTrack);

  // In remove track case, the VideoTrackList::mSelectedIndex should be updated
  // due to mTracks changed. No need to take care this in add track case.
  virtual void RemoveTrack(const RefPtr<MediaTrack>& aTrack);

  void RemoveTracks();

  static already_AddRefed<AudioTrack>
  CreateAudioTrack(const nsAString& aId,
                   const nsAString& aKind,
                   const nsAString& aLabel,
                   const nsAString& aLanguage,
                   bool aEnabled);

  static already_AddRefed<VideoTrack>
  CreateVideoTrack(const nsAString& aId,
                   const nsAString& aKind,
                   const nsAString& aLabel,
                   const nsAString& aLanguage);

  virtual void EmptyTracks();

  void CreateAndDispatchChangeEvent();

  // WebIDL
  MediaTrack* IndexedGetter(uint32_t aIndex, bool& aFound);

  MediaTrack* GetTrackById(const nsAString& aId);

  bool IsEmpty() const
  {
    return mTracks.IsEmpty();
  }

  uint32_t Length() const
  {
    return mTracks.Length();
  }

  IMPL_EVENT_HANDLER(change)
  IMPL_EVENT_HANDLER(addtrack)
  IMPL_EVENT_HANDLER(removetrack)

  friend class AudioTrack;
  friend class VideoTrack;

protected:
  virtual ~MediaTrackList();

  void CreateAndDispatchTrackEventRunner(MediaTrack* aTrack,
                                         const nsAString& aEventName);

  virtual AudioTrackList* AsAudioTrackList() { return nullptr; }

  virtual VideoTrackList* AsVideoTrackList() { return nullptr; }

  HTMLMediaElement* GetMediaElement() { return mMediaElement; }

  nsTArray<RefPtr<MediaTrack>> mTracks;
  RefPtr<HTMLMediaElement> mMediaElement;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaTrackList_h
