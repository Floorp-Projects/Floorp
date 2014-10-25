/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaTrack_h
#define mozilla_dom_MediaTrack_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class MediaTrackList;
class VideoTrack;
class AudioTrack;

/**
 * Base class of AudioTrack and VideoTrack. The AudioTrack and VideoTrack
 * objects represent specific tracks of a media resource. Each track has aspects
 * of an identifier, category, label, and language, even if a track is removed
 * from its corresponding track list, those aspects do not change.
 *
 * When fetching the media resource, an audio/video track is created if the
 * media resource is found to have an audio/video track. When the UA has learned
 * that an audio/video track has ended, this audio/video track will be removed
 * from its corresponding track list.
 *
 * Although AudioTrack and VideoTrack are not EventTargets, TextTrack is, and
 * TextTrack inherits from MediaTrack as well (or is going to).
 */
class MediaTrack : public DOMEventTargetHelper
{
public:
  MediaTrack(const nsAString& aId,
             const nsAString& aKind,
             const nsAString& aLabel,
             const nsAString& aLanguage);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaTrack, DOMEventTargetHelper)

  enum {
    DEFAULT = 0,
    FIRE_NO_EVENTS = 1 << 0,
  };
  // The default behavior of enabling an audio track or selecting a video track
  // fires a change event and notifies its media resource about the changes.
  // It should not fire any events when fetching media resource.
  virtual void SetEnabledInternal(bool aEnabled, int aFlags) = 0;

  virtual AudioTrack* AsAudioTrack()
  {
    return nullptr;
  }

  virtual VideoTrack* AsVideoTrack()
  {
    return nullptr;
  }

  const nsString& GetId() const
  {
    return mId;
  }

  // WebIDL
  void GetId(nsAString& aId) const
  {
    aId = mId;
  }
  void GetKind(nsAString& aKind) const
  {
    aKind = mKind;
  }
  void GetLabel(nsAString& aLabel) const
  {
    aLabel = mLabel;
  }
  void GetLanguage(nsAString& aLanguage) const
  {
    aLanguage = mLanguage;
  }

  friend class MediaTrackList;

protected:
  virtual ~MediaTrack();

  void SetTrackList(MediaTrackList* aList);
  void Init(nsPIDOMWindow* aOwnerWindow);

  nsRefPtr<MediaTrackList> mList;
  nsString mId;
  nsString mKind;
  nsString mLabel;
  nsString mLanguage;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaTrack_h
