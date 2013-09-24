/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OscillatorNode_h_
#define OscillatorNode_h_

#include "AudioNode.h"
#include "AudioParam.h"
#include "PeriodicWave.h"
#include "mozilla/dom/OscillatorNodeBinding.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {

class AudioContext;

class OscillatorNode : public AudioNode,
                       public MainThreadMediaStreamListener
{
public:
  explicit OscillatorNode(AudioContext* aContext);
  virtual ~OscillatorNode();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(OscillatorNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  virtual void DestroyMediaStream() MOZ_OVERRIDE
  {
    if (mStream) {
      mStream->RemoveMainThreadListener(this);
    }
    AudioNode::DestroyMediaStream();
  }
  virtual uint16_t NumberOfInputs() const MOZ_FINAL MOZ_OVERRIDE
  {
    return 0;
  }

  OscillatorType Type() const
  {
    return mType;
  }
  void SetType(OscillatorType aType, ErrorResult& aRv)
  {
    if (!Preferences::GetBool("media.webaudio.legacy.OscillatorNode")) {
      // Do not accept the alternate enum values unless the legacy pref
      // has been turned on.
      switch (aType) {
      case OscillatorType::_0:
      case OscillatorType::_1:
      case OscillatorType::_2:
      case OscillatorType::_3:
      case OscillatorType::_4:
        // Do nothing in order to emulate setting an invalid enum value.
        return;
      default:
        // Shut up the compiler warning
        break;
      }
    }

    // Handle the alternate enum values
    switch (aType) {
    case OscillatorType::_0: aType = OscillatorType::Sine; break;
    case OscillatorType::_1: aType = OscillatorType::Square; break;
    case OscillatorType::_2: aType = OscillatorType::Sawtooth; break;
    case OscillatorType::_3: aType = OscillatorType::Triangle; break;
    case OscillatorType::_4: aType = OscillatorType::Custom; break;
    default:
      // Shut up the compiler warning
      break;
    }
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

  void Start(double aWhen, ErrorResult& aRv);
  void NoteOn(double aWhen, ErrorResult& aRv)
  {
    Start(aWhen, aRv);
  }
  void Stop(double aWhen, ErrorResult& aRv);
  void NoteOff(double aWhen, ErrorResult& aRv)
  {
    Stop(aWhen, aRv);
  }
  void SetPeriodicWave(PeriodicWave& aPeriodicWave)
  {
    mPeriodicWave = &aPeriodicWave;
    // SendTypeToStream will call SendPeriodicWaveToStream for us.
    mType = OscillatorType::Custom;
    SendTypeToStream();
  }

  IMPL_EVENT_HANDLER(ended)

  virtual void NotifyMainThreadStateChanged() MOZ_OVERRIDE;

private:
  static void SendFrequencyToStream(AudioNode* aNode);
  static void SendDetuneToStream(AudioNode* aNode);
  void SendTypeToStream();
  void SendPeriodicWaveToStream();

private:
  OscillatorType mType;
  nsRefPtr<PeriodicWave> mPeriodicWave;
  nsRefPtr<AudioParam> mFrequency;
  nsRefPtr<AudioParam> mDetune;
  bool mStartCalled;
  bool mStopped;
};

}
}

#endif

