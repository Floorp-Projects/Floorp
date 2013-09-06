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
  AudioStreamTrack(DOMMediaStream* aStream, TrackID aTrackID)
    : MediaStreamTrack(aStream, aTrackID) {}

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  virtual AudioStreamTrack* AsAudioStreamTrack() { return this; }

  // WebIDL
  virtual void GetKind(nsAString& aKind) { aKind.AssignLiteral("audio"); }
};

}
}

#endif /* AUDIOSTREAMTRACK_H_ */
