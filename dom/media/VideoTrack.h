/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoTrack_h
#define mozilla_dom_VideoTrack_h

#include "MediaTrack.h"

namespace mozilla {
namespace dom {

class VideoTrackList;
class VideoStreamTrack;

class VideoTrack : public MediaTrack {
 public:
  VideoTrack(nsIGlobalObject* aOwnerGlobal, const nsAString& aId,
             const nsAString& aKind, const nsAString& aLabel,
             const nsAString& aLanguage,
             VideoStreamTrack* aStreamTrack = nullptr);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VideoTrack, MediaTrack)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  VideoTrack* AsVideoTrack() override { return this; }

  // When fetching media resource, if no video track is selected by the media
  // resource, then the first VideoTrack object in the list is set selected as
  // default. If multiple video tracks are selected by its media resource at
  // fetching phase, then the first enabled video track is set selected.
  // aFlags contains FIRE_NO_EVENTS because no events are fired in such cases.
  void SetEnabledInternal(bool aEnabled, int aFlags) override;

  // Get associated video stream track when the video track comes from
  // MediaStream. This might be nullptr when the src of owning HTMLMediaElement
  // is not MediaStream.
  VideoStreamTrack* GetVideoStreamTrack() { return mVideoStreamTrack; }

  // WebIDL
  bool Selected() const { return mSelected; }

  // Either zero or one video track is selected in a list; If the selected track
  // is in a VideoTrackList, then all the other VideoTrack objects in that list
  // must be unselected.
  void SetSelected(bool aSelected);

 private:
  virtual ~VideoTrack();

  bool mSelected;
  RefPtr<VideoStreamTrack> mVideoStreamTrack;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_VideoTrack_h
