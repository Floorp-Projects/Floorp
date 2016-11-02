/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraphImpl.h"
#include "MediaStreamListener.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Unused.h"

#include "AudioSegment.h"
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "AudioCaptureStream.h"
#include "ImageContainer.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include "webaudio/MediaStreamAudioDestinationNode.h"
#include <algorithm>
#include "DOMMediaStream.h"

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla
{

// We are mixing to mono until PeerConnection can accept stereo
static const uint32_t MONO = 1;

AudioCaptureStream::AudioCaptureStream(TrackID aTrackId)
  : ProcessedMediaStream(), mTrackId(aTrackId), mStarted(false), mTrackCreated(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(AudioCaptureStream);
  mMixer.AddCallback(this);
}

AudioCaptureStream::~AudioCaptureStream()
{
  MOZ_COUNT_DTOR(AudioCaptureStream);
  mMixer.RemoveCallback(this);
}

void
AudioCaptureStream::Start()
{
  class Message : public ControlMessage {
  public:
    explicit Message(AudioCaptureStream* aStream)
      : ControlMessage(aStream), mStream(aStream) {}

    virtual void Run()
    {
      mStream->mStarted = true;
    }

  protected:
    AudioCaptureStream* mStream;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this));
}

void
AudioCaptureStream::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                 uint32_t aFlags)
{
  if (!mStarted) {
    return;
  }

  uint32_t inputCount = mInputs.Length();
  StreamTracks::Track* track = EnsureTrack(mTrackId);
  // Notify the DOM everything is in order.
  if (!mTrackCreated) {
    for (uint32_t i = 0; i < mListeners.Length(); i++) {
      MediaStreamListener* l = mListeners[i];
      AudioSegment tmp;
      l->NotifyQueuedTrackChanges(
        Graph(), mTrackId, 0, TrackEventCommand::TRACK_EVENT_CREATED, tmp);
      l->NotifyFinishedTrackCreation(Graph());
    }
    mTrackCreated = true;
  }

  if (IsFinishedOnGraphThread()) {
    return;
  }

  // If the captured stream is connected back to a object on the page (be it an
  // HTMLMediaElement with a stream as source, or an AudioContext), a cycle
  // situation occur. This can work if it's an AudioContext with at least one
  // DelayNode, but the MSG will mute the whole cycle otherwise.
  if (InMutedCycle() || inputCount == 0) {
    track->Get<AudioSegment>()->AppendNullData(aTo - aFrom);
  } else {
    // We mix down all the tracks of all inputs, to a stereo track. Everything
    // is {up,down}-mixed to stereo.
    mMixer.StartMixing();
    AudioSegment output;
    for (uint32_t i = 0; i < inputCount; i++) {
      MediaStream* s = mInputs[i]->GetSource();
      StreamTracks::TrackIter track(s->GetStreamTracks(), MediaSegment::AUDIO);
      if (track.IsEnded()) {
        // No tracks for this input. Still we append data to trigger the mixer.
        AudioSegment toMix;
        toMix.AppendNullData(aTo - aFrom);
        toMix.Mix(mMixer, MONO, Graph()->GraphRate());
      }
      for (; !track.IsEnded(); track.Next()) {
        AudioSegment* inputSegment = track->Get<AudioSegment>();
        StreamTime inputStart = s->GraphTimeToStreamTimeWithBlocking(aFrom);
        StreamTime inputEnd = s->GraphTimeToStreamTimeWithBlocking(aTo);
        AudioSegment toMix;
        if (track->IsEnded() && inputSegment->GetDuration() <= inputStart) {
          toMix.AppendNullData(aTo - aFrom);
        } else {
          toMix.AppendSlice(*inputSegment, inputStart, inputEnd);
          // Care for streams blocked in the [aTo, aFrom] range.
          if (inputEnd - inputStart < aTo - aFrom) {
            toMix.AppendNullData((aTo - aFrom) - (inputEnd - inputStart));
          }
        }
        toMix.Mix(mMixer, MONO, Graph()->GraphRate());
      }
    }
    // This calls MixerCallback below
    mMixer.FinishMixing();
  }

  // Regardless of the status of the input tracks, we go foward.
  mTracks.AdvanceKnownTracksTime(GraphTimeToStreamTimeWithBlocking((aTo)));
}

void
AudioCaptureStream::MixerCallback(AudioDataValue* aMixedBuffer,
                                  AudioSampleFormat aFormat, uint32_t aChannels,
                                  uint32_t aFrames, uint32_t aSampleRate)
{
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
  chunk.mBuffer = new mozilla::SharedChannelArrayBuffer<AudioDataValue>(&output);
  chunk.mDuration = aFrames;
  chunk.mBufferFormat = aFormat;
  chunk.mVolume = 1.0f;
  chunk.mChannelData.SetLength(MONO);
  for (uint32_t channel = 0; channel < aChannels; channel++) {
    chunk.mChannelData[channel] = bufferPtrs[channel];
  }

  // Now we have mixed data, simply append it to out track.
  EnsureTrack(mTrackId)->Get<AudioSegment>()->AppendAndConsumeChunk(&chunk);
}
}
