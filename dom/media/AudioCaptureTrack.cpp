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
}

AudioCaptureTrack::~AudioCaptureTrack() { MOZ_COUNT_DTOR(AudioCaptureTrack); }

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
    AudioChunk* mixed = mMixer.MixedChunk();
    MOZ_ASSERT(mixed->ChannelCount() == MONO);
    // Now we have mixed data, simply append it.
    GetData<AudioSegment>()->AppendAndConsumeChunk(std::move(*mixed));
  }
}

uint32_t AudioCaptureTrack::NumberOfChannels() const {
  return GetData<AudioSegment>()->MaxChannelCount();
}

}  // namespace mozilla
