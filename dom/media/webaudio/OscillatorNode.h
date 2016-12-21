/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OscillatorNode_h_
#define OscillatorNode_h_

#include "AudioScheduledSourceNode.h"
#include "AudioParam.h"
#include "PeriodicWave.h"
#include "mozilla/dom/OscillatorNodeBinding.h"

namespace mozilla {
namespace dom {

class AudioContext;
struct OscillatorOptions;

class OscillatorNode final : public AudioScheduledSourceNode
                           , public MainThreadMediaStreamListener
{
public:
  static already_AddRefed<OscillatorNode>
  Create(AudioContext& aAudioContext, const OscillatorOptions& aOptions,
         ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(OscillatorNode, AudioScheduledSourceNode)

  static already_AddRefed<OscillatorNode>
  Constructor(const GlobalObject& aGlobal, AudioContext& aAudioContext,
              const OscillatorOptions& aOptions, ErrorResult& aRv)
  {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void DestroyMediaStream() override;

  uint16_t NumberOfInputs() const final override
  {
    return 0;
  }

  OscillatorType Type() const
  {
    return mType;
  }
  void SetType(OscillatorType aType, ErrorResult& aRv)
  {
    if (aType == OscillatorType::Custom) {
      // ::Custom can only be set by setPeriodicWave().
      // https://github.com/WebAudio/web-audio-api/issues/105 for exception.
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
    mType = aType;
    SendTypeToStream();
  }

  AudioParam* Frequency() const
  {
    return mFrequency;
  }
  AudioParam* Detune() const
  {
    return mDetune;
  }

  void Start(double aWhen, ErrorResult& aRv) override;
  void Stop(double aWhen, ErrorResult& aRv) override;

  void SetPeriodicWave(PeriodicWave& aPeriodicWave)
  {
    mPeriodicWave = &aPeriodicWave;
    // SendTypeToStream will call SendPeriodicWaveToStream for us.
    mType = OscillatorType::Custom;
    SendTypeToStream();
  }

  void NotifyMainThreadStreamFinished() override;

  const char* NodeType() const override
  {
    return "OscillatorNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  explicit OscillatorNode(AudioContext* aContext);
  ~OscillatorNode() = default;

  void SendTypeToStream();
  void SendPeriodicWaveToStream();

  OscillatorType mType;
  RefPtr<PeriodicWave> mPeriodicWave;
  RefPtr<AudioParam> mFrequency;
  RefPtr<AudioParam> mDetune;
  bool mStartCalled;
};

} // namespace dom
} // namespace mozilla

#endif
