/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BiquadFilterNode_h_
#define BiquadFilterNode_h_

#include "AudioNode.h"
#include "AudioParam.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/TypedEnum.h"

namespace mozilla {
namespace dom {

class AudioContext;

MOZ_BEGIN_ENUM_CLASS(BiquadTypeEnum, uint16_t)
  LOWPASS = 0,
  HIGHPASS = 1,
  BANDPASS = 2,
  LOWSHELF = 3,
  HIGHSHELF = 4,
  PEAKING = 5,
  NOTCH = 6,
  ALLPASS = 7,
  Max = 7
MOZ_END_ENUM_CLASS(BiquadTypeEnum)

class BiquadFilterNode : public AudioNode
{
public:
  explicit BiquadFilterNode(AudioContext* aContext);
  ~BiquadFilterNode();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BiquadFilterNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope);

  virtual bool SupportsMediaStreams() const MOZ_OVERRIDE
  {
    return true;
  }

  uint16_t Type() const
  {
    return static_cast<uint16_t> (mType);
  }
  void SetType(uint16_t aType, ErrorResult& aRv);

  AudioParam* Frequency() const
  {
    return mFrequency;
  }

  AudioParam* Q() const
  {
    return mQ;
  }

  AudioParam* Gain() const
  {
    return mGain;
  }

private:
  static void SendFrequencyToStream(AudioNode* aNode);
  static void SendQToStream(AudioNode* aNode);
  static void SendGainToStream(AudioNode* aNode);

private:
  BiquadTypeEnum mType;
  nsRefPtr<AudioParam> mFrequency;
  nsRefPtr<AudioParam> mQ;
  nsRefPtr<AudioParam> mGain;
};

}
}

#endif

