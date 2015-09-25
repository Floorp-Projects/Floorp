/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioBufferSourceNode_h_
#define AudioBufferSourceNode_h_

#include "AudioNode.h"
#include "AudioBuffer.h"

namespace mozilla {
namespace dom {

class AudioParam;

class AudioBufferSourceNode final : public AudioNode,
                                    public MainThreadMediaStreamListener
{
public:
  explicit AudioBufferSourceNode(AudioContext* aContext);

  virtual void DestroyMediaStream() override;

  virtual uint16_t NumberOfInputs() const final override
  {
    return 0;
  }
  virtual AudioBufferSourceNode* AsAudioBufferSourceNode() override
  {
    return this;
  }
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioBufferSourceNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void Start(double aWhen, double aOffset,
             const Optional<double>& aDuration, ErrorResult& aRv);
  void Stop(double aWhen, ErrorResult& aRv);

  AudioBuffer* GetBuffer(JSContext* aCx) const
  {
    return mBuffer;
  }
  void SetBuffer(JSContext* aCx, AudioBuffer* aBuffer)
  {
    mBuffer = aBuffer;
    SendBufferParameterToStream(aCx);
    SendLoopParametersToStream();
  }
  AudioParam* PlaybackRate() const
  {
    return mPlaybackRate;
  }
  AudioParam* Detune() const
  {
    return mDetune;
  }
  bool Loop() const
  {
    return mLoop;
  }
  void SetLoop(bool aLoop)
  {
    mLoop = aLoop;
    SendLoopParametersToStream();
  }
  double LoopStart() const
  {
    return mLoopStart;
  }
  void SetLoopStart(double aStart)
  {
    mLoopStart = aStart;
    SendLoopParametersToStream();
  }
  double LoopEnd() const
  {
    return mLoopEnd;
  }
  void SetLoopEnd(double aEnd)
  {
    mLoopEnd = aEnd;
    SendLoopParametersToStream();
  }
  void SendDopplerShiftToStream(double aDopplerShift);

  IMPL_EVENT_HANDLER(ended)

  virtual void NotifyMainThreadStreamFinished() override;

  virtual const char* NodeType() const override
  {
    return "AudioBufferSourceNode";
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~AudioBufferSourceNode();

private:
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
  static void SendPlaybackRateToStream(AudioNode* aNode,
                                       const AudioTimelineEvent& aEvent);
  static void SendDetuneToStream(AudioNode* aNode,
                                 const AudioTimelineEvent& aEvent);

private:
  double mLoopStart;
  double mLoopEnd;
  double mOffset;
  double mDuration;
  nsRefPtr<AudioBuffer> mBuffer;
  nsRefPtr<AudioParam> mPlaybackRate;
  nsRefPtr<AudioParam> mDetune;
  bool mLoop;
  bool mStartCalled;
};

} // namespace dom
} // namespace mozilla

#endif

