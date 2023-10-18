/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Unused.h"

#include "AudioSegment.h"
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "AudioCaptureTrack.h"
#include "ImageContainer.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "AudioNodeExternalInputTrack.h"
#include "webaudio/MediaStreamAudioDestinationNode.h"
#include <algorithm>
#include "DOMMediaStream.h"

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

// We are mixing to mono until PeerConnection can accept stereo
static const uint32_t MONO = 1;

AudioCaptureTrack::AudioCaptureTrack(TrackRate aRate)
    : ProcessedMediaTrack(aRate, MediaSegment::AUDIO, new AudioSegment()),
      mStarted(false) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(AudioCaptureTrack);
  mMixer.AddCallback(WrapNotNull(this));
}

AudioCaptureTrack::~AudioCaptureTrack() {
  MOZ_COUNT_DTOR(AudioCaptureTrack);
  mMixer.RemoveCallback(this);
}

void AudioCaptureTrack::Start() {
  QueueControlMessageWithNoShutdown(
      [self = RefPtr{this}, this] { mStarted = true; });
}

void AudioCaptureTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                     uint32_t aFlags) {
  if (!mStarted) {
    return;
  }

  uint32_t inputCount = mInputs.Length();

  if (mEnded) {
    return;
  }

  // If the captured track is connected back to a object on the page (be it an
  // HTMLMediaElement with a track as source, or an AudioContext), a cycle
  // situation occur. This can work if it's an AudioContext with at least one
  // DelayNode, but the MTG will mute the whole cycle otherwise.
  if (InMutedCycle() || inputCount == 0) {
    GetData<AudioSegment>()->AppendNullData(aTo - aFrom);
  } else {
    // We mix down all the tracks of all inputs, to a stereo track. Everything
    // is {up,down}-mixed to stereo.
    mMixer.StartMixing();
    AudioSegment output;
    for (uint32_t i = 0; i < inputCount; i++) {
      MediaTrack* s = mInputs[i]->GetSource();
      AudioSegment* inputSegment = s->GetData<AudioSegment>();
      TrackTime inputStart = s->GraphTimeToTrackTimeWithBlocking(aFrom);
      TrackTime inputEnd = s->GraphTimeToTrackTimeWithBlocking(aTo);
      AudioSegment toMix;
      if (s->Ended() && inputSegment->GetDuration() <= inputStart) {
        toMix.AppendNullData(aTo - aFrom);
      } else {
        toMix.AppendSlice(*inputSegment, inputStart, inputEnd);
        // Care for tracks blocked in the [aTo, aFrom] range.
        if (inputEnd - inputStart < aTo - aFrom) {
          toMix.AppendNullData((aTo - aFrom) - (inputEnd - inputStart));
        }
      }
      toMix.Mix(mMixer, MONO, Graph()->GraphRate());
    }
    // This calls MixerCallback below
    mMixer.FinishMixing();
  }
}

uint32_t AudioCaptureTrack::NumberOfChannels() const {
  return GetData<AudioSegment>()->MaxChannelCount();
}

void AudioCaptureTrack::MixerCallback(AudioDataValue* aMixedBuffer,
                                      AudioSampleFormat aFormat,
                                      uint32_t aChannels, uint32_t aFrames,
                                      uint32_t aSampleRate) {
  AutoTArray<nsTArray<AudioDataValue>, MONO> output;
  AutoTArray<const AudioDataValue*, MONO> bufferPtrs;
  output.SetLength(MONO);
  bufferPtrs.SetLength(MONO);

  uint32_t written = 0;
  // We need to copy here, because the mixer will reuse the storage, we should
  // not hold onto it. Buffers are in planar format.
  for (uint32_t channel = 0; channel < aChannels; channel++) {
    AudioDataValue* out = output[channel].AppendElements(aFrames);
    PodCopy(out, aMixedBuffer + written, aFrames);
    bufferPtrs[channel] = out;
    written += aFrames;
  }
  AudioChunk chunk;
  chunk.mBuffer =
      new mozilla::SharedChannelArrayBuffer<AudioDataValue>(std::move(output));
  chunk.mDuration = aFrames;
  chunk.mBufferFormat = aFormat;
  chunk.mChannelData.SetLength(MONO);
  for (uint32_t channel = 0; channel < aChannels; channel++) {
    chunk.mChannelData[channel] = bufferPtrs[channel];
  }

  // Now we have mixed data, simply append it.
  GetData<AudioSegment>()->AppendAndConsumeChunk(std::move(chunk));
}
}  // namespace mozilla
