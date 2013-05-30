/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIASTREAMTRACK_H_
#define MEDIASTREAMTRACK_H_

#include "nsDOMEventTargetHelper.h"
#include "DOMMediaStream.h"
#include "nsID.h"
#include "StreamBuffer.h"

namespace mozilla {
namespace dom {

class AudioStreamTrack;
class VideoStreamTrack;

/**
 * Class representing a track in a DOMMediaStream.
 */
class MediaStreamTrack : public nsDOMEventTargetHelper {
public:
  /**
   * aTrackID is the MediaStreamGraph track ID for the track in the
   * MediaStream owned by aStream.
   */
  MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID);
  virtual ~MediaStreamTrack();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamTrack, nsDOMEventTargetHelper)

  DOMMediaStream* GetParentObject() const { return mStream; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE = 0;

  DOMMediaStream* GetStream() const { return mStream; }
  TrackID GetTrackID() const { return mTrackID; }
  virtual AudioStreamTrack* AsAudioStreamTrack() { return nullptr; }
  virtual VideoStreamTrack* AsVideoStreamTrack() { return nullptr; }

  // WebIDL
  virtual void GetKind(nsAString& aKind) = 0;
  void GetId(nsAString& aID);
  void GetLabel(nsAString& aLabel) { aLabel.Truncate(); }

  // Notifications from the MediaStreamGraph
  void NotifyEnded() { mEnded = true; }

protected:
  nsRefPtr<DOMMediaStream> mStream;
  TrackID mTrackID;
  nsID mID;
  bool mEnded;
};

}
}

#endif /* MEDIASTREAMTRACK_H_ */
