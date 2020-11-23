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

namespace mozilla {
namespace dom {

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

  void AddAudioOutput(void* aKey);
  void RemoveAudioOutput(void* aKey);
  void SetAudioOutputVolume(void* aKey, float aVolume);
  RefPtr<GenericPromise> SetAudioOutputDevice(void* key,
                                              AudioDeviceInfo* aSink);

  // WebIDL
  void GetKind(nsAString& aKind) override { aKind.AssignLiteral("audio"); }

  void GetLabel(nsAString& aLabel, CallerType aCallerType) override;

 protected:
  already_AddRefed<MediaStreamTrack> CloneInternal() override;
  void SetReadyState(MediaStreamTrackState aState) override;

 private:
  // Track CrossGraphPort per AudioOutput key. This is required in order to
  // redirect all AudioOutput requests (add, remove, set volume) to the
  // receiver track which, belonging to the remote graph. MainThread only.
  nsClassHashtable<nsPtrHashKey<void>, UniquePtr<CrossGraphPort>> mCrossGraphs;
};

}  // namespace dom
}  // namespace mozilla

#endif /* AUDIOSTREAMTRACK_H_ */
