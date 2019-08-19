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
      nsPIDOMWindowInner* aWindow, MediaStream* aInputStream, TrackID aTrackID,
      MediaStreamTrackSource* aSource,
      MediaStreamTrackState aState = MediaStreamTrackState::Live,
      const MediaTrackConstraints& aConstraints = MediaTrackConstraints());

  void Destroy() override;

  VideoStreamTrack* AsVideoStreamTrack() override { return this; }
  const VideoStreamTrack* AsVideoStreamTrack() const override { return this; }

  void AddVideoOutput(VideoFrameContainer* aSink);
  void AddVideoOutput(VideoOutput* aOutput);
  void RemoveVideoOutput(VideoFrameContainer* aSink);
  void RemoveVideoOutput(VideoOutput* aOutput);

  // WebIDL
  void GetKind(nsAString& aKind) override { aKind.AssignLiteral("video"); }

  void GetLabel(nsAString& aLabel, CallerType aCallerType) override;

 protected:
  already_AddRefed<MediaStreamTrack> CloneInternal() override;

 private:
  nsTArray<RefPtr<VideoOutput>> mVideoOutputs;
};

}  // namespace dom
}  // namespace mozilla

#endif /* VIDEOSTREAMTRACK_H_ */
