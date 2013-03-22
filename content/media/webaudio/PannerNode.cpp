/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PannerNode.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioListener.h"

namespace mozilla {
namespace dom {

class PannerNodeEngine : public AudioNodeEngine
{
public:
  PannerNodeEngine()
    // Please keep these default values consistent with PannerNode::PannerNode below.
    : mPanningModel(PanningModelTypeValues::HRTF)
    , mDistanceModel(DistanceModelTypeValues::Inverse)
    , mPosition()
    , mOrientation(1., 0., 0.)
    , mVelocity()
    , mRefDistance(1.)
    , mMaxDistance(10000.)
    , mRolloffFactor(1.)
    , mConeInnerAngle(360.)
    , mConeOuterAngle(360.)
    , mConeOuterGain(0.)
    // These will be initialized when a PannerNode is created, so just initialize them
    // to some dummy values here.
    , mListenerDopplerFactor(0.)
    , mListenerSpeedOfSound(0.)
  {
  }

  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case PannerNode::PANNING_MODEL:
      mPanningModel = PanningModelType(aParam);
      break;
    case PannerNode::DISTANCE_MODEL:
      mDistanceModel = DistanceModelType(aParam);
      break;
    default:
      NS_ERROR("Bad PannerNodeEngine Int32Parameter");
    }
  }
  virtual void SetThreeDPointParameter(uint32_t aIndex, const ThreeDPoint& aParam) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case PannerNode::LISTENER_POSITION: mListenerPosition = aParam; break;
    case PannerNode::LISTENER_ORIENTATION: mListenerOrientation = aParam; break;
    case PannerNode::LISTENER_UPVECTOR: mListenerUpVector = aParam; break;
    case PannerNode::LISTENER_VELOCITY: mListenerVelocity = aParam; break;
    case PannerNode::POSITION: mPosition = aParam; break;
    case PannerNode::ORIENTATION: mOrientation = aParam; break;
    case PannerNode::VELOCITY: mVelocity = aParam; break;
    default:
      NS_ERROR("Bad PannerNodeEngine ThreeDPointParameter");
    }
  }
  virtual void SetDoubleParameter(uint32_t aIndex, double aParam) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case PannerNode::LISTENER_DOPPLER_FACTOR: mListenerDopplerFactor = aParam; break;
    case PannerNode::LISTENER_SPEED_OF_SOUND: mListenerSpeedOfSound = aParam; break;
    case PannerNode::REF_DISTANCE: mRefDistance = aParam; break;
    case PannerNode::MAX_DISTANCE: mMaxDistance = aParam; break;
    case PannerNode::ROLLOFF_FACTOR: mRolloffFactor = aParam; break;
    case PannerNode::CONE_INNER_ANGLE: mConeInnerAngle = aParam; break;
    case PannerNode::CONE_OUTER_ANGLE: mConeOuterAngle = aParam; break;
    case PannerNode::CONE_OUTER_GAIN: mConeOuterGain = aParam; break;
    default:
      NS_ERROR("Bad PannerNodeEngine DoubleParameter");
    }
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool *aFinished) MOZ_OVERRIDE
  {
    // TODO: actually do 3D positioning computations here
    *aOutput = aInput;
  }

  PanningModelType mPanningModel;
  DistanceModelType mDistanceModel;
  ThreeDPoint mPosition;
  ThreeDPoint mOrientation;
  ThreeDPoint mVelocity;
  double mRefDistance;
  double mMaxDistance;
  double mRolloffFactor;
  double mConeInnerAngle;
  double mConeOuterAngle;
  double mConeOuterGain;
  ThreeDPoint mListenerPosition;
  ThreeDPoint mListenerOrientation;
  ThreeDPoint mListenerUpVector;
  ThreeDPoint mListenerVelocity;
  double mListenerDopplerFactor;
  double mListenerSpeedOfSound;
};

PannerNode::PannerNode(AudioContext* aContext)
  : AudioNode(aContext)
  // Please keep these default values consistent with PannerNodeEngine::PannerNodeEngine above.
  , mPanningModel(PanningModelTypeValues::HRTF)
  , mDistanceModel(DistanceModelTypeValues::Inverse)
  , mPosition()
  , mOrientation(1., 0., 0.)
  , mVelocity()
  , mRefDistance(1.)
  , mMaxDistance(10000.)
  , mRolloffFactor(1.)
  , mConeInnerAngle(360.)
  , mConeOuterAngle(360.)
  , mConeOuterGain(0.)
{
  mStream = aContext->Graph()->CreateAudioNodeStream(new PannerNodeEngine(),
                                                     MediaStreamGraph::INTERNAL_STREAM);
  // We should register once we have set up our stream and engine.
  Context()->Listener()->RegisterPannerNode(this);
}

PannerNode::~PannerNode()
{
  DestroyMediaStream();
}

JSObject*
PannerNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return PannerNodeBinding::Wrap(aCx, aScope, this);
}

}
}

