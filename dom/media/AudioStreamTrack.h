/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIOSTREAMTRACK_H_
#define AUDIOSTREAMTRACK_H_

#include "MediaStreamTrack.h"
#include "DOMMediaStream.h"
#include "CrossGraphPort.h"
#include "nsClassHashtable.h"

namespace mozilla::dom {

class AudioStreamTrack : public MediaStreamTrack {
 public:
  AudioStreamTrack(
      nsPIDOMWindowInner* aWindow, mozilla::MediaTrack* aInputTrack,
      MediaStreamTrackSource* aSource,
      MediaStreamTrackState aReadyState = MediaStreamTrackState::Live,
      bool aMuted = false,
      const MediaTrackConstraints& aConstraints = MediaTrackConstraints())
      : MediaStreamTrack(aWindow, aInputTrack, aSource, aReadyState, aMuted,
                         aConstraints) {}

  AudioStreamTrack* AsAudioStreamTrack() override { return this; }
  const AudioStreamTrack* AsAudioStreamTrack() const override { return this; }

  // Direct output to aSink, or the default output device if aSink is null.
  // No more than one output may exist for a single aKey at any one time.
  // Returns a promise that resolves when the device is processing audio.
  RefPtr<GenericPromise> AddAudioOutput(void* aKey, AudioDeviceInfo* aSink);
  void RemoveAudioOutput(void* aKey);
  void SetAudioOutputVolume(void* aKey, float aVolume);

  // WebIDL
  void GetKind(nsAString& aKind) override { aKind.AssignLiteral("audio"); }

  void GetLabel(nsAString& aLabel, CallerType aCallerType) override;

 protected:
  already_AddRefed<MediaStreamTrack> CloneInternal() override;
};

}  // namespace mozilla::dom

#endif /* AUDIOSTREAMTRACK_H_ */
