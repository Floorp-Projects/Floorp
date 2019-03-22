/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VIDEOSTREAMTRACK_H_
#define VIDEOSTREAMTRACK_H_

#include "MediaStreamTrack.h"
#include "DOMMediaStream.h"

namespace mozilla {

class VideoFrameContainer;
class VideoOutput;

namespace dom {

class VideoStreamTrack : public MediaStreamTrack {
 public:
  VideoStreamTrack(
      DOMMediaStream* aStream, TrackID aTrackID, TrackID aInputTrackID,
      MediaStreamTrackSource* aSource,
      const MediaTrackConstraints& aConstraints = MediaTrackConstraints());

  void Destroy() override;

  VideoStreamTrack* AsVideoStreamTrack() override { return this; }
  const VideoStreamTrack* AsVideoStreamTrack() const override { return this; }

  void AddVideoOutput(VideoFrameContainer* aSink);
  void RemoveVideoOutput(VideoFrameContainer* aSink);

  // WebIDL
  void GetKind(nsAString& aKind) override { aKind.AssignLiteral("video"); }

  void GetLabel(nsAString& aLabel, CallerType aCallerType) override;

 protected:
  already_AddRefed<MediaStreamTrack> CloneInternal(
      DOMMediaStream* aOwningStream, TrackID aTrackID) override {
    return do_AddRef(new VideoStreamTrack(
        aOwningStream, aTrackID, mInputTrackID, mSource, mConstraints));
  }

 private:
  nsTArray<RefPtr<VideoOutput>> mVideoOutputs;
};

}  // namespace dom
}  // namespace mozilla

#endif /* VIDEOSTREAMTRACK_H_ */
