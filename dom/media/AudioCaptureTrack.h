/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOCAPTURETRACK_H_
#define MOZILLA_AUDIOCAPTURETRACK_H_

#include "MediaTrackGraph.h"
#include "AudioMixer.h"
#include <algorithm>

namespace mozilla {

class AbstractThread;
class DOMMediaStream;

/**
 * See MediaTrackGraph::CreateAudioCaptureTrack.
 */
class AudioCaptureTrack : public ProcessedMediaTrack {
 public:
  explicit AudioCaptureTrack(TrackRate aRate);
  virtual ~AudioCaptureTrack();

  void Start();

  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

  uint32_t NumberOfChannels() const override;

 protected:
  AudioMixer mMixer;
  bool mStarted;
  bool mTrackCreated;
};
}  // namespace mozilla

#endif /* MOZILLA_AUDIOCAPTURETRACK_H_ */
