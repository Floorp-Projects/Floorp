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
  AudioStreamTrack(DOMMediaStream* aStream, TrackID aTrackID,
                   TrackID aInputTrackID,
                   MediaStreamTrackSource* aSource,
                   const MediaTrackConstraints& aConstraints = MediaTrackConstraints())
    : MediaStreamTrack(aStream, aTrackID, aInputTrackID, aSource, aConstraints) {}

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  AudioStreamTrack* AsAudioStreamTrack() override { return this; }

  const AudioStreamTrack* AsAudioStreamTrack() const override { return this; }

  // WebIDL
  void GetKind(nsAString& aKind) override { aKind.AssignLiteral("audio"); }

protected:
  already_AddRefed<MediaStreamTrack> CloneInternal(DOMMediaStream* aOwningStream,
                                                   TrackID aTrackID) override
  {
    return do_AddRef(new AudioStreamTrack(aOwningStream,
                                          aTrackID,
                                          mInputTrackID,
                                          mSource,
                                          mConstraints));
  }
};

} // namespace dom
} // namespace mozilla

#endif /* AUDIOSTREAMTRACK_H_ */
