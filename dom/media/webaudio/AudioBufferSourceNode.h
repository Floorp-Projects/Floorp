/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioBufferSourceNode_h_
#define AudioBufferSourceNode_h_

#include "AudioScheduledSourceNode.h"
#include "AudioBuffer.h"

namespace mozilla {
namespace dom {

struct AudioBufferSourceOptions;
class AudioParam;

class AudioBufferSourceNode final : public AudioScheduledSourceNode,
                                    public MainThreadMediaStreamListener {
 public:
  static already_AddRefed<AudioBufferSourceNode> Create(
      JSContext* aCx, AudioContext& aAudioContext,
      const AudioBufferSourceOptions& aOptions);

  void DestroyMediaStream() override;

  uint16_t NumberOfInputs() const final { return 0; }
  AudioBufferSourceNode* AsAudioBufferSourceNode() override { return this; }
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioBufferSourceNode,
                                           AudioScheduledSourceNode)

  static already_AddRefed<AudioBufferSourceNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const AudioBufferSourceOptions& aOptions) {
    return Create(aGlobal.Context(), aAudioContext, aOptions);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void Start(double aWhen, double aOffset, const Optional<double>& aDuration,
             ErrorResult& aRv);

  void Start(double aWhen, ErrorResult& aRv) override;
  void Stop(double aWhen, ErrorResult& aRv) override;

  AudioBuffer* GetBuffer(JSContext* aCx) const { return mBuffer; }
  void SetBuffer(JSContext* aCx, AudioBuffer* aBuffer) {
    mBuffer = aBuffer;
    SendBufferParameterToStream(aCx);
    SendLoopParametersToStream();
  }
  AudioParam* PlaybackRate() const { return mPlaybackRate; }
  AudioParam* Detune() const { return mDetune; }
  bool Loop() const { return mLoop; }
  void SetLoop(bool aLoop) {
    mLoop = aLoop;
    SendLoopParametersToStream();
  }
  double LoopStart() const { return mLoopStart; }
  void SetLoopStart(double aStart) {
    mLoopStart = aStart;
    SendLoopParametersToStream();
  }
  double LoopEnd() const { return mLoopEnd; }
  void SetLoopEnd(double aEnd) {
    mLoopEnd = aEnd;
    SendLoopParametersToStream();
  }
  void SendDopplerShiftToStream(double aDopplerShift);

  void NotifyMainThreadStreamFinished() override;

  const char* NodeType() const override { return "AudioBufferSourceNode"; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

 private:
  explicit AudioBufferSourceNode(AudioContext* aContext);
  ~AudioBufferSourceNode() = default;

  friend class AudioBufferSourceNodeEngine;
  // START is sent during Start().
  // STOP is sent during Stop().
  // BUFFERSTART and BUFFEREND are sent when SetBuffer() and Start() have
  // been called (along with sending the buffer).
  enum EngineParameters {
    SAMPLE_RATE,
    START,
    STOP,
    // BUFFERSTART is the "offset" passed to start(), multiplied by
    // buffer.sampleRate.
    BUFFERSTART,
    // BUFFEREND is the sum of "offset" and "duration" passed to start(),
    // multiplied by buffer.sampleRate, or the size of the buffer, if smaller.
    BUFFEREND,
    LOOP,
    LOOPSTART,
    LOOPEND,
    PLAYBACKRATE,
    DETUNE,
    DOPPLERSHIFT
  };

  void SendLoopParametersToStream();
  void SendBufferParameterToStream(JSContext* aCx);
  void SendOffsetAndDurationParametersToStream(AudioNodeStream* aStream);

  double mLoopStart;
  double mLoopEnd;
  double mOffset;
  double mDuration;
  RefPtr<AudioBuffer> mBuffer;
  RefPtr<AudioParam> mPlaybackRate;
  RefPtr<AudioParam> mDetune;
  bool mLoop;
  bool mStartCalled;
};

}  // namespace dom
}  // namespace mozilla

#endif
