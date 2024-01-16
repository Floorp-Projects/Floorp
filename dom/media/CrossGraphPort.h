/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_CROSS_GRAPH_TRACK_H_
#define MOZILLA_CROSS_GRAPH_TRACK_H_

#include "AudioDriftCorrection.h"
#include "AudioSegment.h"
#include "ForwardedInputTrack.h"
#include "mozilla/SPSCQueue.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
class CrossGraphReceiver;
}

namespace mozilla::dom {
class AudioStreamTrack;
}

namespace mozilla {

/**
 * CrossGraphTransmitter and CrossGraphPort are currently unused, but intended
 * for connecting MediaTracks of different MediaTrackGraphs with different
 * sample rates or clock sources for bug 1674892.
 *
 * Create with MediaTrackGraph::CreateCrossGraphTransmitter()
 */
class CrossGraphTransmitter : public ProcessedMediaTrack {
 public:
  CrossGraphTransmitter(TrackRate aSampleRate,
                        RefPtr<CrossGraphReceiver> aReceiver);
  CrossGraphTransmitter* AsCrossGraphTransmitter() override { return this; }

  uint32_t NumberOfChannels() const override {
    MOZ_CRASH("CrossGraphTransmitter has no segment. It cannot be played out.");
  }
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

 private:
  const RefPtr<CrossGraphReceiver> mReceiver;
};

/**
 * Create with MediaTrackGraph::CreateCrossGraphReceiver()
 */
class CrossGraphReceiver : public ProcessedMediaTrack {
 public:
  CrossGraphReceiver(TrackRate aSampleRate, TrackRate aTransmitterRate);
  CrossGraphReceiver* AsCrossGraphReceiver() override { return this; }

  uint32_t NumberOfChannels() const override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

  int EnqueueAudio(AudioChunk& aChunk);

 private:
  SPSCQueue<AudioChunk> mCrossThreadFIFO{30};
  // Indicates that tre CrossGraphTransmitter has started sending frames. It
  // is false untill the point, transmitter has sent the first valid frame.
  // Accessed in GraphThread only.
  bool mTransmitterHasStarted = false;
  // Correct the drift between transmitter and receiver. Reciever (this class)
  // is considered as the master clock.
  // Accessed in GraphThread only.
  AudioDriftCorrection mDriftCorrection;
};

class CrossGraphPort final {
 public:
  static UniquePtr<CrossGraphPort> Connect(
      const RefPtr<dom::AudioStreamTrack>& aStreamTrack,
      MediaTrackGraph* aPartnerGraph);
  ~CrossGraphPort();

  const RefPtr<CrossGraphTransmitter> mTransmitter;
  const RefPtr<CrossGraphReceiver> mReceiver;

 private:
  explicit CrossGraphPort(RefPtr<MediaInputPort> aTransmitterPort,
                          RefPtr<CrossGraphTransmitter> aTransmitter,
                          RefPtr<CrossGraphReceiver> aReceiver)
      : mTransmitter(std::move(aTransmitter)),
        mReceiver(std::move(aReceiver)),
        mTransmitterPort(std::move(aTransmitterPort)) {
    MOZ_ASSERT(mTransmitter);
    MOZ_ASSERT(mReceiver);
    MOZ_ASSERT(mTransmitterPort);
  }

  // The port that connects the input track to the transmitter.
  const RefPtr<MediaInputPort> mTransmitterPort;
};

}  // namespace mozilla

#endif /* MOZILLA_CROSS_GRAPH_TRACK_H_ */
