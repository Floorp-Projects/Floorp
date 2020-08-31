/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossGraphPort.h"

#include "AudioDeviceInfo.h"
#include "AudioStreamTrack.h"
#include "MediaTrackGraphImpl.h"
#include "mozilla/Logging.h"

namespace mozilla {

#ifdef LOG
#  undef LOG
#endif
#ifdef LOG_TEST
#  undef LOG_TEST
#endif

extern LazyLogModule gForwardedInputTrackLog;
#define LOG(type, msg) MOZ_LOG(gForwardedInputTrackLog, type, msg)
#define LOG_TEST(type) MOZ_LOG_TEST(gForwardedInputTrackLog, type)

UniquePtr<CrossGraphPort> CrossGraphPort::Connect(
    const RefPtr<dom::AudioStreamTrack>& aStreamTrack, AudioDeviceInfo* aSink,
    nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(aSink);
  MOZ_ASSERT(aStreamTrack);
  uint32_t defaultRate;
  aSink->GetDefaultRate(&defaultRate);
  LOG(LogLevel::Debug,
      ("CrossGraphPort::Connect: sink id: %p at rate %u, primary rate %d",
       aSink->DeviceID(), defaultRate, aStreamTrack->Graph()->GraphRate()));

  if (!aSink->DeviceID()) {
    return nullptr;
  }

  MediaTrackGraph* newGraph =
      MediaTrackGraph::GetInstance(MediaTrackGraph::AUDIO_THREAD_DRIVER,
                                   aWindow, defaultRate, aSink->DeviceID());

  return CrossGraphPort::Connect(aStreamTrack, newGraph);
}

UniquePtr<CrossGraphPort> CrossGraphPort::Connect(
    const RefPtr<dom::AudioStreamTrack>& aStreamTrack,
    MediaTrackGraph* aPartnerGraph) {
  MOZ_ASSERT(aStreamTrack);
  MOZ_ASSERT(aPartnerGraph);
  if (aStreamTrack->Graph() == aPartnerGraph) {
    // Primary graph the same as partner graph, just remove the existing cross
    // graph connection
    return nullptr;
  }

  RefPtr<CrossGraphReceiver> receiver = aPartnerGraph->CreateCrossGraphReceiver(
      aStreamTrack->Graph()->GraphRate());

  RefPtr<CrossGraphTransmitter> transmitter =
      aStreamTrack->Graph()->CreateCrossGraphTransmitter(receiver);

  RefPtr<MediaInputPort> port =
      aStreamTrack->ForwardTrackContentsTo(transmitter);

  return WrapUnique(new CrossGraphPort(std::move(port), std::move(transmitter),
                                       std::move(receiver)));
}

void CrossGraphPort::AddAudioOutput(void* aKey) {
  mReceiver->AddAudioOutput(aKey);
}

void CrossGraphPort::RemoveAudioOutput(void* aKey) {
  mReceiver->RemoveAudioOutput(aKey);
}

void CrossGraphPort::SetAudioOutputVolume(void* aKey, float aVolume) {
  mReceiver->SetAudioOutputVolume(aKey, aVolume);
}

void CrossGraphPort::Destroy() {
  mTransmitter->Destroy();
  mReceiver->Destroy();
  mTransmitterPort->Destroy();
}

RefPtr<GenericPromise> CrossGraphPort::EnsureConnected() {
  // The primary graph is already working check the partner (receiver's) graph.
  return mReceiver->Graph()->NotifyWhenDeviceStarted(mReceiver.get());
}

/** CrossGraphTransmitter **/

CrossGraphTransmitter::CrossGraphTransmitter(
    TrackRate aSampleRate, RefPtr<CrossGraphReceiver> aReceiver)
    : ForwardedInputTrack(aSampleRate, MediaSegment::AUDIO),
      mReceiver(std::move(aReceiver)) {}

void CrossGraphTransmitter::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                         uint32_t aFlags) {
  // Get previous end before mSegment is updated.
  TrackTime previousEnd = GetEnd();
  // Update mSegment from source track.
  ForwardedInputTrack::ProcessInput(aFrom, aTo, aFlags);

  MOZ_ASSERT(mInputPort);
  if (mInputPort->GetSource()->Ended() &&
      (mInputPort->GetSource()->GetEnd() <=
       mInputPort->GetSource()->GraphTimeToTrackTimeWithBlocking(aTo))) {
    return;
  }

  LOG(LogLevel::Verbose,
      ("Transmitter (%p) mSegment: duration: %" PRId64 ", from %" PRId64
       ", to %" PRId64 ", ticks %" PRId64 "",
       this, mSegment->GetDuration(), aFrom, aTo, aTo - aFrom));

  AudioSegment* audio = GetData<AudioSegment>();
  TrackTime ticks = aTo - aFrom;
  MOZ_ASSERT(ticks);
  MOZ_ASSERT(audio->GetDuration() >= ticks);

  AudioSegment transmittingAudio;
  transmittingAudio.AppendSlice(*audio, previousEnd, GetEnd());
  MOZ_ASSERT(ticks >= transmittingAudio.GetDuration());
  if (transmittingAudio.IsNull()) {
    AudioChunk chunk;
    chunk.SetNull(ticks);
    Unused << mReceiver->EnqueueAudio(chunk);
  } else {
    for (AudioSegment::ChunkIterator iter(transmittingAudio); !iter.IsEnded();
         iter.Next()) {
      Unused << mReceiver->EnqueueAudio(*iter);
    }
  }
}

/** CrossGraphReceiver **/

CrossGraphReceiver::CrossGraphReceiver(TrackRate aSampleRate,
                                       TrackRate aTransmitterRate)
    : ProcessedMediaTrack(aSampleRate, MediaSegment::AUDIO,
                          static_cast<MediaSegment*>(new AudioSegment())),
      mDriftCorrection(aTransmitterRate, aSampleRate) {}

void CrossGraphReceiver::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                      uint32_t aFlags) {
  LOG(LogLevel::Verbose,
      ("Receiver (%p) mSegment: duration: %" PRId64 ", from %" PRId64
       ", to %" PRId64 ", ticks %" PRId64 "",
       this, mSegment->GetDuration(), aFrom, aTo, aTo - aFrom));

  AudioSegment transmittedAudio;
  while (mCrossThreadFIFO.AvailableRead()) {
    AudioChunk chunk;
    Unused << mCrossThreadFIFO.Dequeue(&chunk, 1);
    transmittedAudio.AppendAndConsumeChunk(&chunk);
    mTransmitterHasStarted = true;
  }

  if (mTransmitterHasStarted) {
    // If it does not have enough frames the result will be silence.
    AudioSegment audioCorrected =
        mDriftCorrection.RequestFrames(transmittedAudio, aTo - aFrom);
    if (LOG_TEST(LogLevel::Verbose) && audioCorrected.IsNull()) {
      LOG(LogLevel::Verbose,
          ("Receiver(%p): Silence has been added, not enough input", this));
    }
    mSegment->AppendFrom(&audioCorrected);
  } else {
    mSegment->AppendNullData(aTo - aFrom);
  }
}

int CrossGraphReceiver::EnqueueAudio(AudioChunk& aChunk) {
  // This will take place on transmitter graph thread only.
  return mCrossThreadFIFO.Enqueue(aChunk);
}

}  // namespace mozilla
