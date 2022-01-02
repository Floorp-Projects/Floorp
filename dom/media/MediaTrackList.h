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

class AudioStreamTrack;
class AudioTrack;
class AudioTrackList;
class HTMLMediaElement;
class MediaTrack;
class VideoStreamTrack;
class VideoTrack;
class VideoTrackList;

/**
 * Base class of AudioTrackList and VideoTrackList. The AudioTrackList and
 * VideoTrackList objects represent a dynamic list of zero or more audio and
 * video tracks respectively.
 *
 * When a media element is to forget its media-resource-specific tracks, its
 * audio track list and video track list will be emptied.
 */
class MediaTrackList : public DOMEventTargetHelper {
 public:
  MediaTrackList(nsIGlobalObject* aOwnerObject,
                 HTMLMediaElement* aMediaElement);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaTrackList, DOMEventTargetHelper)

  using DOMEventTargetHelper::DispatchTrustedEvent;

  // The return value is non-null, assert an error if aIndex is out of bound
  // for array mTracks.
  MediaTrack* operator[](uint32_t aIndex);

  // The track must be from the same Window as this MediaTrackList.
  void AddTrack(MediaTrack* aTrack);

  // In remove track case, the VideoTrackList::mSelectedIndex should be updated
  // due to mTracks changed. No need to take care this in add track case.
  virtual void RemoveTrack(const RefPtr<MediaTrack>& aTrack);

  void RemoveTracks();

  // For the case of src of HTMLMediaElement is non-MediaStream, leave the
  // aAudioTrack as default(nullptr).
  static already_AddRefed<AudioTrack> CreateAudioTrack(
      nsIGlobalObject* aOwnerGlobal, const nsAString& aId,
      const nsAString& aKind, const nsAString& aLabel,
      const nsAString& aLanguage, bool aEnabled,
      AudioStreamTrack* aAudioTrack = nullptr);

  // For the case of src of HTMLMediaElement is non-MediaStream, leave the
  // aVideoTrack as default(nullptr).
  static already_AddRefed<VideoTrack> CreateVideoTrack(
      nsIGlobalObject* aOwnerGlobal, const nsAString& aId,
      const nsAString& aKind, const nsAString& aLabel,
      const nsAString& aLanguage, VideoStreamTrack* aVideoTrack = nullptr);

  virtual void EmptyTracks();

  void CreateAndDispatchChangeEvent();

  // WebIDL
  MediaTrack* IndexedGetter(uint32_t aIndex, bool& aFound);

  MediaTrack* GetTrackById(const nsAString& aId);

  bool IsEmpty() const { return mTracks.IsEmpty(); }

  uint32_t Length() const { return mTracks.Length(); }

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

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MediaTrackList_h
