/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOCAPTURESTREAM_H_
#define MOZILLA_AUDIOCAPTURESTREAM_H_

#include "MediaStreamGraph.h"
#include "AudioMixer.h"
#include <algorithm>

namespace mozilla
{

class DOMMediaStream;

/**
 * See MediaStreamGraph::CreateAudioCaptureStream.
 */
class AudioCaptureStream : public ProcessedMediaStream,
                           public MixerCallbackReceiver
{
public:
  explicit AudioCaptureStream(DOMMediaStream* aWrapper);
  virtual ~AudioCaptureStream();

  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

protected:
  enum { AUDIO_TRACK = 1 };
  void MixerCallback(AudioDataValue* aMixedBuffer, AudioSampleFormat aFormat,
                     uint32_t aChannels, uint32_t aFrames,
                     uint32_t aSampleRate) override;
  AudioMixer mMixer;
  bool mTrackCreated;
};
}

#endif /* MOZILLA_AUDIOCAPTURESTREAM_H_ */
