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

using namespace std;

class PannerNodeEngine : public AudioNodeEngine
{
public:
  PannerNodeEngine()
    // Please keep these default values consistent with PannerNode::PannerNode below.
    : mPanningModel(PanningModelTypeValues::HRTF)
    , mPanningModelFunction(&PannerNodeEngine::HRTFPanningFunction)
    , mDistanceModel(DistanceModelTypeValues::Inverse)
    , mDistanceModelFunction(&PannerNodeEngine::InverseGainFunction)
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
      switch (mPanningModel) {
        case PanningModelTypeValues::Equalpower:
          mPanningModelFunction = &PannerNodeEngine::EqualPowerPanningFunction;
          break;
        case PanningModelTypeValues::HRTF:
          mPanningModelFunction = &PannerNodeEngine::HRTFPanningFunction;
          break;
        case PanningModelTypeValues::Soundfield:
          mPanningModelFunction = &PannerNodeEngine::SoundfieldPanningFunction;
          break;
      }
      break;
    case PannerNode::DISTANCE_MODEL:
      mDistanceModel = DistanceModelType(aParam);
      switch (mDistanceModel) {
        case DistanceModelTypeValues::Inverse:
          mDistanceModelFunction = &PannerNodeEngine::InverseGainFunction;
          break;
        case DistanceModelTypeValues::Linear:
          mDistanceModelFunction = &PannerNodeEngine::LinearGainFunction;
          break;
        case DistanceModelTypeValues::Exponential:
          mDistanceModelFunction = &PannerNodeEngine::ExponentialGainFunction;
          break;
      }
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
    if (aInput.IsNull()) {
      *aOutput = aInput;
      return;
    }
    (this->*mPanningModelFunction)(aInput, aOutput);
  }

  void ComputeAzimuthAndElevation(float& aAzimuth, float& aElevation);
  void DistanceGain(AudioChunk* aChunk, float aGain);

  void GainMonoToStereo(const AudioChunk& aInput, AudioChunk* aOutput,
                        float aGainL, float aGainR);
  void GainStereoToStereo(const AudioChunk& aInput, AudioChunk* aOutput,
                          float aGainL, float aGainR, double aAzimuth);

  void EqualPowerPanningFunction(const AudioChunk& aInput, AudioChunk* aOutput);
  void HRTFPanningFunction(const AudioChunk& aInput, AudioChunk* aOutput);
  void SoundfieldPanningFunction(const AudioChunk& aInput, AudioChunk* aOutput);

  float LinearGainFunction(float aDistance);
  float InverseGainFunction(float aDistance);
  float ExponentialGainFunction(float aDistance);

  PanningModelType mPanningModel;
  typedef void (PannerNodeEngine::*PanningModelFunction)(const AudioChunk& aInput, AudioChunk* aOutput);
  PanningModelFunction mPanningModelFunction;
  DistanceModelType mDistanceModel;
  typedef float (PannerNodeEngine::*DistanceModelFunction)(float aDistance);
  DistanceModelFunction mDistanceModelFunction;
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

// Those three functions are described in the spec.
float
PannerNodeEngine::LinearGainFunction(float aDistance)
{
  return 1 - mRolloffFactor * (aDistance - mRefDistance) / (mMaxDistance - mRefDistance);
}

float
PannerNodeEngine::InverseGainFunction(float aDistance)
{
  return mRefDistance / (mRefDistance + mRolloffFactor * (aDistance - mRefDistance));
}

float
PannerNodeEngine::ExponentialGainFunction(float aDistance)
{
  return pow(aDistance / mRefDistance, -mRolloffFactor);
}

void
PannerNodeEngine::SoundfieldPanningFunction(const AudioChunk& aInput,
                                            AudioChunk* aOutput)
{
  // not implemented: noop
  *aOutput = aInput;
}

void
PannerNodeEngine::HRTFPanningFunction(const AudioChunk& aInput,
                                      AudioChunk* aOutput)
{
  // not implemented: noop
  *aOutput = aInput;
}

void
PannerNodeEngine::EqualPowerPanningFunction(const AudioChunk& aInput,
                                            AudioChunk* aOutput)
{
  float azimuth, elevation, gainL, gainR, normalizedAzimuth, distance, distanceGain;
  int inputChannels = aInput.mChannelData.Length();
  ThreeDPoint distanceVec;

  // If both the listener are in the same spot, and no cone gain is specified,
  // this node is noop.
  if (mListenerPosition == mPosition &&
      mConeInnerAngle == 360 &&
      mConeOuterAngle == 360) {
    *aOutput = aInput;
  }

  // The output of this node is always stereo, no matter what the inputs are.
  AllocateAudioBlock(2, aOutput);

  ComputeAzimuthAndElevation(azimuth, elevation);

  // The following algorithm is described in the spec.
  // Clamp azimuth in the [-90, 90] range.
  azimuth = min(180.f, max(-180.f, azimuth));

  // Wrap around
  if (azimuth < -90.f) {
    azimuth = -180.f - azimuth;
  } else if (azimuth > 90) {
    azimuth = 180.f - azimuth;
  }

  // Normalize the value in the [0, 1] range.
  if (inputChannels == 1) {
    normalizedAzimuth = (azimuth + 90.f) / 180.f;
  } else {
    if (azimuth <= 0) {
      normalizedAzimuth = (azimuth + 90.f) / 90.f;
    } else {
      normalizedAzimuth = azimuth / 90.f;
    }
  }

  // Compute how much the distance contributes to the gain reduction.
  distanceVec = mPosition - mListenerPosition;
  distance = sqrt(distanceVec.DotProduct(distanceVec));
  distanceGain = (this->*mDistanceModelFunction)(distance);

  // Actually compute the left and right gain.
  gainL = cos(0.5 * M_PI * normalizedAzimuth);
  gainR = sin(0.5 * M_PI * normalizedAzimuth);

  // Compute the output.
  if (inputChannels == 1) {
    GainMonoToStereo(aInput, aOutput, gainL, gainR);
  } else {
    GainStereoToStereo(aInput, aOutput, gainL, gainR, azimuth);
  }

  DistanceGain(aOutput, distanceGain);
}

void
PannerNodeEngine::GainMonoToStereo(const AudioChunk& aInput, AudioChunk* aOutput,
                                   float aGainL, float aGainR)
{
  float* outputL = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[0]));
  float* outputR = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[1]));
  const float* input = static_cast<float*>(const_cast<void*>(aInput.mChannelData[0]));

  AudioBlockPanMonoToStereo(input, aGainL, aGainR, outputL, outputR);
}

void
PannerNodeEngine::GainStereoToStereo(const AudioChunk& aInput, AudioChunk* aOutput,
                                     float aGainL, float aGainR, double aAzimuth)
{
  float* outputL = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[0]));
  float* outputR = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[1]));
  const float* inputL = static_cast<float*>(const_cast<void*>(aInput.mChannelData[0]));
  const float* inputR = static_cast<float*>(const_cast<void*>(aInput.mChannelData[1]));

  AudioBlockPanStereoToStereo(inputL, inputR, aGainL, aGainR, aAzimuth <= 0, outputL, outputR);
}

void
PannerNodeEngine::DistanceGain(AudioChunk* aChunk, float aGain)
{
  float* samples = static_cast<float*>(const_cast<void*>(*aChunk->mChannelData.Elements()));
  uint32_t channelCount = aChunk->mChannelData.Length();

  AudioBlockInPlaceScale(samples, channelCount, aGain);
}

// This algorithm is specicied in the webaudio spec.
void
PannerNodeEngine::ComputeAzimuthAndElevation(float& aAzimuth, float& aElevation)
{
  ThreeDPoint sourceListener = mPosition - mListenerPosition;

  if (sourceListener.IsZero()) {
    aAzimuth = 0.0;
    aElevation = 0.0;
    return;
  }

  sourceListener.Normalize();

  // Project the source-listener vector on the x-z plane.
  ThreeDPoint& listenerFront = mListenerOrientation;
  ThreeDPoint listenerRightNorm = listenerFront.CrossProduct(mListenerUpVector);
  listenerRightNorm.Normalize();

  ThreeDPoint listenerFrontNorm(listenerFront);
  listenerFrontNorm.Normalize();

  ThreeDPoint up = listenerRightNorm.CrossProduct(listenerFrontNorm);

  double upProjection = sourceListener.DotProduct(up);

  ThreeDPoint projectedSource = sourceListener - up * upProjection;
  projectedSource.Normalize();

  // Actually compute the angle, and convert to degrees
  double projection = projectedSource.DotProduct(listenerRightNorm);
  aAzimuth = 180 * acos(projection) / M_PI;

  // Compute whether the source is in front or behind the listener.
  double frontBack = projectedSource.DotProduct(listenerFrontNorm);
  if (frontBack < 0) {
    aAzimuth = 360 - aAzimuth;
  }
  // Rotate the azimuth so it is relative to the listener front vector instead
  // of the right vector.
  if ((aAzimuth >= 0) && (aAzimuth <= 270)) {
    aAzimuth = 90 - aAzimuth;
  } else {
    aAzimuth = 450 - aAzimuth;
  }

  aElevation = 90 - 180 * acos(sourceListener.DotProduct(up)) / M_PI;

  if (aElevation > 90) {
    aElevation = 180 - aElevation;
  } else if (aElevation < -90) {
    aElevation = -180 - aElevation;
  }
}

}
}

