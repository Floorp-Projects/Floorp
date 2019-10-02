/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONODEEXTERNALINPUTTRACK_H_
#define MOZILLA_AUDIONODEEXTERNALINPUTTRACK_H_

#include "MediaTrackGraph.h"
#include "AudioNodeTrack.h"
#include "mozilla/Atomics.h"

namespace mozilla {

class AbstractThread;

/**
 * This is a MediaTrack implementation that acts for a Web Audio node but
 * unlike other AudioNodeTracks, supports any kind of MediaTrack as an
 * input --- handling any number of audio tracks and handling blocking of
 * the input MediaTrack.
 */
class AudioNodeExternalInputTrack final : public AudioNodeTrack {
 public:
  static already_AddRefed<AudioNodeExternalInputTrack> Create(
      MediaTrackGraph* aGraph, AudioNodeEngine* aEngine);

 protected:
  AudioNodeExternalInputTrack(AudioNodeEngine* aEngine, TrackRate aSampleRate);
  ~AudioNodeExternalInputTrack();

 public:
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

 private:
  /**
   * Determines if this is enabled or not.  Disabled nodes produce silence.
   * This node becomes disabled if the document principal does not subsume the
   * DOMMediaStream principal.
   */
  bool IsEnabled();
};

}  // namespace mozilla

#endif /* MOZILLA_AUDIONODEEXTERNALINPUTTRACK_H_ */
