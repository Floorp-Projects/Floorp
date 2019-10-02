/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIOSTREAMTRACK_H_
#define AUDIOSTREAMTRACK_H_

#include "MediaStreamTrack.h"
#include "DOMMediaStream.h"

namespace mozilla {
namespace dom {

class AudioStreamTrack : public MediaStreamTrack {
 public:
  AudioStreamTrack(
      nsPIDOMWindowInner* aWindow, MediaStream* aInputStream, TrackID aTrackID,
      MediaStreamTrackSource* aSource,
      MediaStreamTrackState aReadyState = MediaStreamTrackState::Live,
      const MediaTrackConstraints& aConstraints = MediaTrackConstraints())
      : MediaStreamTrack(aWindow, aInputStream, aTrackID, aSource, aReadyState,
                         aConstraints) {}

  AudioStreamTrack* AsAudioStreamTrack() override { return this; }
  const AudioStreamTrack* AsAudioStreamTrack() const override { return this; }

  void AddAudioOutput(void* aKey);
  void RemoveAudioOutput(void* aKey);
  void SetAudioOutputVolume(void* aKey, float aVolume);

  // WebIDL
  void GetKind(nsAString& aKind) override { aKind.AssignLiteral("audio"); }

  void GetLabel(nsAString& aLabel, CallerType aCallerType) override;

 protected:
  already_AddRefed<MediaStreamTrack> CloneInternal() override;
};

}  // namespace dom
}  // namespace mozilla

#endif /* AUDIOSTREAMTRACK_H_ */
