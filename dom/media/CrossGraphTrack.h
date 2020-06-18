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

namespace mozilla {
class CrossGraphReceiver;
}

namespace mozilla::dom {
class AudioStreamTrack;
}

namespace mozilla {

/**
 * See MediaTrackGraph::CreateCrossGraphTransmitter()
 */
class CrossGraphTransmitter : public ForwardedInputTrack {
  friend class CrossGraphManager;

 public:
  CrossGraphTransmitter(TrackRate aSampleRate, CrossGraphReceiver* aReceiver);
  virtual CrossGraphTransmitter* AsCrossGraphTransmitter() override {
    return this;
  }

  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;
  void Destroy() override;

 private:
  RefPtr<CrossGraphReceiver> mReceiver;
};

/**
 * See MediaTrackGraph::CreateCrossGraphReceiver()
 */
class CrossGraphReceiver : public ProcessedMediaTrack {
 public:
  explicit CrossGraphReceiver(TrackRate aSampleRate,
                              TrackRate aTransmitterRate);
  virtual CrossGraphReceiver* AsCrossGraphReceiver() override { return this; }

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

class CrossGraphManager final {
 public:
  static CrossGraphManager* Connect(
      const RefPtr<dom::AudioStreamTrack>& aStreamTrack,
      MediaTrackGraph* aPartnerGraph);
  static CrossGraphManager* Connect(
      const RefPtr<dom::AudioStreamTrack>& aStreamTrack, AudioDeviceInfo* aSink,
      nsPIDOMWindowInner* aWindow);
  ~CrossGraphManager() = default;

  void AddAudioOutput(void* aKey);
  void RemoveAudioOutput(void* aKey);
  void SetAudioOutputVolume(void* aKey, float aVolume);
  void Destroy();

  RefPtr<GenericPromise> EnsureConnected();

 private:
  explicit CrossGraphManager(const RefPtr<MediaInputPort>& aPort)
      : mSourcePort(aPort) {}
  RefPtr<CrossGraphTransmitter> GetTransmitter();
  RefPtr<CrossGraphReceiver> GetReceiver();

  // The port that connect the transmitter with the source track.
  RefPtr<MediaInputPort> mSourcePort;
};

}  // namespace mozilla

#endif /* MOZILLA_CROSS_GRAPH_TRACK_H_ */
