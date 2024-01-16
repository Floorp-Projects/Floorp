/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossGraphPort.h"

#include "AudioDeviceInfo.h"
#include "AudioStreamTrack.h"
#include "MediaTrackGraph.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"

namespace mozilla {

#ifdef LOG
#  undef LOG
#endif
#ifdef LOG_TEST
#  undef LOG_TEST
#endif

extern LazyLogModule gMediaTrackGraphLog;
#define LOG(type, msg) MOZ_LOG(gMediaTrackGraphLog, type, msg)
#define LOG_TEST(type) MOZ_LOG_TEST(gMediaTrackGraphLog, type)

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

CrossGraphPort::~CrossGraphPort() {
  mTransmitter->Destroy();
  mReceiver->Destroy();
  mTransmitterPort->Destroy();
}

/** CrossGraphTransmitter **/

CrossGraphTransmitter::CrossGraphTransmitter(
    TrackRate aSampleRate, RefPtr<CrossGraphReceiver> aReceiver)
    : ProcessedMediaTrack(aSampleRate, MediaSegment::AUDIO,
                          nullptr /* aSegment */),
      mReceiver(std::move(aReceiver)) {}

void CrossGraphTransmitter::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                         uint32_t aFlags) {
  MOZ_ASSERT(!mInputs.IsEmpty());
  MOZ_ASSERT(mDisabledMode == DisabledTrackMode::ENABLED);

  MediaTrack* input = mInputs[0]->GetSource();

  if (input->Ended() &&
      (input->GetEnd() <= input->GraphTimeToTrackTimeWithBlocking(aFrom))) {
    mEnded = true;
    return;
  }

  LOG(LogLevel::Verbose,
      ("Transmitter (%p) from %" PRId64 ", to %" PRId64 ", ticks %" PRId64 "",
       this, aFrom, aTo, aTo - aFrom));

  AudioSegment audio;
  GraphTime next;
  for (GraphTime t = aFrom; t < aTo; t = next) {
    MediaInputPort::InputInterval interval =
        MediaInputPort::GetNextInputInterval(mInputs[0], t);
    interval.mEnd = std::min(interval.mEnd, aTo);

    TrackTime ticks = interval.mEnd - interval.mStart;
    next = interval.mEnd;

    if (interval.mStart >= interval.mEnd) {
      break;
    }

    if (interval.mInputIsBlocked) {
      audio.AppendNullData(ticks);
    } else if (input->IsSuspended()) {
      audio.AppendNullData(ticks);
    } else {
      MOZ_ASSERT(GetEnd() == GraphTimeToTrackTimeWithBlocking(interval.mStart),
                 "Samples missing");
      TrackTime inputStart =
          input->GraphTimeToTrackTimeWithBlocking(interval.mStart);
      TrackTime inputEnd =
          input->GraphTimeToTrackTimeWithBlocking(interval.mEnd);
      audio.AppendSlice(*input->GetData(), inputStart, inputEnd);
    }
  }

  mStartTime = aTo;

  for (AudioSegment::ChunkIterator iter(audio); !iter.IsEnded(); iter.Next()) {
    Unused << mReceiver->EnqueueAudio(*iter);
  }
}

/** CrossGraphReceiver **/

CrossGraphReceiver::CrossGraphReceiver(TrackRate aSampleRate,
                                       TrackRate aTransmitterRate)
    : ProcessedMediaTrack(aSampleRate, MediaSegment::AUDIO,
                          static_cast<MediaSegment*>(new AudioSegment())),
      mDriftCorrection(aTransmitterRate, aSampleRate, PRINCIPAL_HANDLE_NONE) {}

uint32_t CrossGraphReceiver::NumberOfChannels() const {
  return GetData<AudioSegment>()->MaxChannelCount();
}

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
    transmittedAudio.AppendAndConsumeChunk(std::move(chunk));
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
