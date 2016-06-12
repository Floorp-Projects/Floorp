/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PannerNode.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioListener.h"
#include "PanningUtils.h"
#include "AudioBufferSourceNode.h"
#include "PlayingRefChangeHandler.h"
#include "blink/HRTFPanner.h"
#include "blink/HRTFDatabaseLoader.h"
#include "nsAutoPtr.h"

using WebCore::HRTFDatabaseLoader;
using WebCore::HRTFPanner;

namespace mozilla {
namespace dom {

using namespace std;

NS_IMPL_CYCLE_COLLECTION_CLASS(PannerNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PannerNode)
  if (tmp->Context()) {
    tmp->Context()->UnregisterPannerNode(tmp);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(AudioNode)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PannerNode, AudioNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PannerNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(PannerNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(PannerNode, AudioNode)

class PannerNodeEngine final : public AudioNodeEngine
{
public:
  explicit PannerNodeEngine(AudioNode* aNode)
    : AudioNodeEngine(aNode)
    // Please keep these default values consistent with PannerNode::PannerNode below.
    , mPanningModelFunction(&PannerNodeEngine::EqualPowerPanningFunction)
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
    , mLeftOverData(INT_MIN)
  {
  }

  void CreateHRTFPanner()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mHRTFPanner) {
      return;
    }
    // HRTFDatabaseLoader needs to be fetched on the main thread.
    already_AddRefed<HRTFDatabaseLoader> loader =
      HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(NodeMainThread()->Context()->SampleRate());
    mHRTFPanner = new HRTFPanner(NodeMainThread()->Context()->SampleRate(), Move(loader));
  }

  void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override
  {
    switch (aIndex) {
    case PannerNode::PANNING_MODEL:
      switch (PanningModelType(aParam)) {
        case PanningModelType::Equalpower:
          mPanningModelFunction = &PannerNodeEngine::EqualPowerPanningFunction;
          break;
        case PanningModelType::HRTF:
          mPanningModelFunction = &PannerNodeEngine::HRTFPanningFunction;
          break;
        default:
          NS_NOTREACHED("We should never see the alternate names here");
          break;
      }
      break;
    case PannerNode::DISTANCE_MODEL:
      switch (DistanceModelType(aParam)) {
        case DistanceModelType::Inverse:
          mDistanceModelFunction = &PannerNodeEngine::InverseGainFunction;
          break;
        case DistanceModelType::Linear:
          mDistanceModelFunction = &PannerNodeEngine::LinearGainFunction;
          break;
        case DistanceModelType::Exponential:
          mDistanceModelFunction = &PannerNodeEngine::ExponentialGainFunction;
          break;
        default:
          NS_NOTREACHED("We should never see the alternate names here");
          break;
      }
      break;
    default:
      NS_ERROR("Bad PannerNodeEngine Int32Parameter");
    }
  }
  void SetThreeDPointParameter(uint32_t aIndex, const ThreeDPoint& aParam) override
  {
    switch (aIndex) {
    case PannerNode::LISTENER_POSITION: mListenerPosition = aParam; break;
    case PannerNode::LISTENER_FRONT_VECTOR: mListenerFrontVector = aParam; break;
    case PannerNode::LISTENER_RIGHT_VECTOR: mListenerRightVector = aParam; break;
    case PannerNode::LISTENER_VELOCITY: mListenerVelocity = aParam; break;
    case PannerNode::POSITION: mPosition = aParam; break;
    case PannerNode::ORIENTATION: mOrientation = aParam; break;
    case PannerNode::VELOCITY: mVelocity = aParam; break;
    default:
      NS_ERROR("Bad PannerNodeEngine ThreeDPointParameter");
    }
  }
  void SetDoubleParameter(uint32_t aIndex, double aParam) override
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

  void ProcessBlock(AudioNodeStream* aStream,
                    GraphTime aFrom,
                    const AudioBlock& aInput,
                    AudioBlock* aOutput,
                    bool *aFinished) override
  {
    if (aInput.IsNull()) {
      // mLeftOverData != INT_MIN means that the panning model was HRTF and a
      // tail-time reference was added.  Even if the model is now equalpower,
      // the reference will need to be removed.
      if (mLeftOverData > 0 &&
          mPanningModelFunction == &PannerNodeEngine::HRTFPanningFunction) {
        mLeftOverData -= WEBAUDIO_BLOCK_SIZE;
      } else {
        if (mLeftOverData != INT_MIN) {
          mLeftOverData = INT_MIN;
          aStream->ScheduleCheckForInactive();
          mHRTFPanner->reset();

          RefPtr<PlayingRefChangeHandler> refchanged =
            new PlayingRefChangeHandler(aStream, PlayingRefChangeHandler::RELEASE);
          aStream->Graph()->
            DispatchToMainThreadAfterStreamStateUpdate(refchanged.forget());
        }
        aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
        return;
      }
    } else if (mPanningModelFunction == &PannerNodeEngine::HRTFPanningFunction) {
      if (mLeftOverData == INT_MIN) {
        RefPtr<PlayingRefChangeHandler> refchanged =
          new PlayingRefChangeHandler(aStream, PlayingRefChangeHandler::ADDREF);
        aStream->Graph()->
          DispatchToMainThreadAfterStreamStateUpdate(refchanged.forget());
      }
      mLeftOverData = mHRTFPanner->maxTailFrames();
    }

    (this->*mPanningModelFunction)(aInput, aOutput);
  }

  bool IsActive() const override
  {
    return mLeftOverData != INT_MIN;
  }

  void ComputeAzimuthAndElevation(float& aAzimuth, float& aElevation);
  float ComputeConeGain();
  // Compute how much the distance contributes to the gain reduction.
  float ComputeDistanceGain();

  void EqualPowerPanningFunction(const AudioBlock& aInput, AudioBlock* aOutput);
  void HRTFPanningFunction(const AudioBlock& aInput, AudioBlock* aOutput);

  float LinearGainFunction(float aDistance);
  float InverseGainFunction(float aDistance);
  float ExponentialGainFunction(float aDistance);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    if (mHRTFPanner) {
      amount += mHRTFPanner->sizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  // This member is set on the main thread, but is not accessed on the rendering
  // thread untile mPanningModelFunction has changed, and this happens strictly
  // later, via a MediaStreamGraph ControlMessage.
  nsAutoPtr<HRTFPanner> mHRTFPanner;
  typedef void (PannerNodeEngine::*PanningModelFunction)(const AudioBlock& aInput, AudioBlock* aOutput);
  PanningModelFunction mPanningModelFunction;
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
  ThreeDPoint mListenerFrontVector;
  ThreeDPoint mListenerRightVector;
  ThreeDPoint mListenerVelocity;
  double mListenerDopplerFactor;
  double mListenerSpeedOfSound;
  int mLeftOverData;
};

PannerNode::PannerNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Clamped_max,
              ChannelInterpretation::Speakers)
  // Please keep these default values consistent with PannerNodeEngine::PannerNodeEngine above.
  , mPanningModel(PanningModelType::Equalpower)
  , mDistanceModel(DistanceModelType::Inverse)
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
  mStream = AudioNodeStream::Create(aContext,
                                    new PannerNodeEngine(this),
                                    AudioNodeStream::NO_STREAM_FLAGS);
  // We should register once we have set up our stream and engine.
  Context()->Listener()->RegisterPannerNode(this);
}

PannerNode::~PannerNode()
{
  if (Context()) {
    Context()->UnregisterPannerNode(this);
  }
}

void PannerNode::SetPanningModel(PanningModelType aPanningModel)
{
  mPanningModel = aPanningModel;
  if (mPanningModel == PanningModelType::HRTF) {
    // We can set the engine's `mHRTFPanner` member here from the main thread,
    // because the engine will not touch it from the MediaStreamGraph
    // thread until the PANNING_MODEL message sent below is received.
    static_cast<PannerNodeEngine*>(mStream->Engine())->CreateHRTFPanner();
  }
  SendInt32ParameterToStream(PANNING_MODEL, int32_t(mPanningModel));
}

size_t
PannerNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mSources.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t
PannerNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
PannerNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PannerNodeBinding::Wrap(aCx, this, aGivenProto);
}

void PannerNode::DestroyMediaStream()
{
  if (Context()) {
    Context()->UnregisterPannerNode(this);
  }
  AudioNode::DestroyMediaStream();
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
PannerNodeEngine::HRTFPanningFunction(const AudioBlock& aInput,
                                      AudioBlock* aOutput)
{
  // The output of this node is always stereo, no matter what the inputs are.
  aOutput->AllocateChannels(2);

  float azimuth, elevation;
  ComputeAzimuthAndElevation(azimuth, elevation);

  AudioBlock input = aInput;
  // Gain is applied before the delay and convolution of the HRTF.
  input.mVolume *= ComputeConeGain() * ComputeDistanceGain();

  mHRTFPanner->pan(azimuth, elevation, &input, aOutput);
}

void
PannerNodeEngine::EqualPowerPanningFunction(const AudioBlock& aInput,
                                            AudioBlock* aOutput)
{
  float azimuth, elevation, gainL, gainR, normalizedAzimuth, distanceGain, coneGain;
  int inputChannels = aInput.ChannelCount();

  // If both the listener are in the same spot, and no cone gain is specified,
  // this node is noop.
  if (mListenerPosition == mPosition &&
      mConeInnerAngle == 360 &&
      mConeOuterAngle == 360) {
    *aOutput = aInput;
    return;
  }

  // The output of this node is always stereo, no matter what the inputs are.
  aOutput->AllocateChannels(2);

  ComputeAzimuthAndElevation(azimuth, elevation);
  coneGain = ComputeConeGain();

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

  distanceGain = ComputeDistanceGain();

  // Actually compute the left and right gain.
  gainL = cos(0.5 * M_PI * normalizedAzimuth);
  gainR = sin(0.5 * M_PI * normalizedAzimuth);

  // Compute the output.
  ApplyStereoPanning(aInput, aOutput, gainL, gainR, azimuth <= 0);

  aOutput->mVolume = aInput.mVolume * distanceGain * coneGain;
}

// This algorithm is specified in the webaudio spec.
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
  const ThreeDPoint& listenerFront = mListenerFrontVector;
  const ThreeDPoint& listenerRight = mListenerRightVector;
  ThreeDPoint up = listenerRight.CrossProduct(listenerFront);

  double upProjection = sourceListener.DotProduct(up);
  aElevation = 90 - 180 * acos(upProjection) / M_PI;

  if (aElevation > 90) {
    aElevation = 180 - aElevation;
  } else if (aElevation < -90) {
    aElevation = -180 - aElevation;
  }

  ThreeDPoint projectedSource = sourceListener - up * upProjection;
  if (projectedSource.IsZero()) {
    // source - listener direction is up or down.
    aAzimuth = 0.0;
    return;
  }
  projectedSource.Normalize();

  // Actually compute the angle, and convert to degrees
  double projection = projectedSource.DotProduct(listenerRight);
  aAzimuth = 180 * acos(projection) / M_PI;

  // Compute whether the source is in front or behind the listener.
  double frontBack = projectedSource.DotProduct(listenerFront);
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
}

// This algorithm is described in the WebAudio spec.
float
PannerNodeEngine::ComputeConeGain()
{
  // Omnidirectional source
  if (mOrientation.IsZero() || ((mConeInnerAngle == 360) && (mConeOuterAngle == 360))) {
    return 1;
  }

  // Normalized source-listener vector
  ThreeDPoint sourceToListener = mListenerPosition - mPosition;
  sourceToListener.Normalize();

  // Angle between the source orientation vector and the source-listener vector
  double dotProduct = sourceToListener.DotProduct(mOrientation);
  double angle = 180 * acos(dotProduct) / M_PI;
  double absAngle = fabs(angle);

  // Divide by 2 here since API is entire angle (not half-angle)
  double absInnerAngle = fabs(mConeInnerAngle) / 2;
  double absOuterAngle = fabs(mConeOuterAngle) / 2;
  double gain = 1;

  if (absAngle <= absInnerAngle) {
    // No attenuation
    gain = 1;
  } else if (absAngle >= absOuterAngle) {
    // Max attenuation
    gain = mConeOuterGain;
  } else {
    // Between inner and outer cones
    // inner -> outer, x goes from 0 -> 1
    double x = (absAngle - absInnerAngle) / (absOuterAngle - absInnerAngle);
    gain = (1 - x) + mConeOuterGain * x;
  }

  return gain;
}

float
PannerNodeEngine::ComputeDistanceGain()
{
  ThreeDPoint distanceVec = mPosition - mListenerPosition;
  float distance = sqrt(distanceVec.DotProduct(distanceVec));
  return std::max(0.0f, (this->*mDistanceModelFunction)(distance));
}

float
PannerNode::ComputeDopplerShift()
{
  double dopplerShift = 1.0; // Initialize to default value

  AudioListener* listener = Context()->Listener();

  if (listener->DopplerFactor() > 0) {
    // Don't bother if both source and listener have no velocity.
    if (!mVelocity.IsZero() || !listener->Velocity().IsZero()) {
      // Calculate the source to listener vector.
      ThreeDPoint sourceToListener = mPosition - listener->Velocity();

      double sourceListenerMagnitude = sourceToListener.Magnitude();

      double listenerProjection = sourceToListener.DotProduct(listener->Velocity()) / sourceListenerMagnitude;
      double sourceProjection = sourceToListener.DotProduct(mVelocity) / sourceListenerMagnitude;

      listenerProjection = -listenerProjection;
      sourceProjection = -sourceProjection;

      double scaledSpeedOfSound = listener->SpeedOfSound() / listener->DopplerFactor();
      listenerProjection = min(listenerProjection, scaledSpeedOfSound);
      sourceProjection = min(sourceProjection, scaledSpeedOfSound);

      dopplerShift = ((listener->SpeedOfSound() - listener->DopplerFactor() * listenerProjection) / (listener->SpeedOfSound() - listener->DopplerFactor() * sourceProjection));

      WebAudioUtils::FixNaN(dopplerShift); // Avoid illegal values

      // Limit the pitch shifting to 4 octaves up and 3 octaves down.
      dopplerShift = min(dopplerShift, 16.);
      dopplerShift = max(dopplerShift, 0.125);
    }
  }

  return dopplerShift;
}

void
PannerNode::FindConnectedSources()
{
  mSources.Clear();
  std::set<AudioNode*> cycleSet;
  FindConnectedSources(this, mSources, cycleSet);
}

void
PannerNode::FindConnectedSources(AudioNode* aNode,
                                 nsTArray<AudioBufferSourceNode*>& aSources,
                                 std::set<AudioNode*>& aNodesSeen)
{
  if (!aNode) {
    return;
  }

  const nsTArray<InputNode>& inputNodes = aNode->InputNodes();

  for(unsigned i = 0; i < inputNodes.Length(); i++) {
    // Return if we find a node that we have seen already.
    if (aNodesSeen.find(inputNodes[i].mInputNode) != aNodesSeen.end()) {
      return;
    }
    aNodesSeen.insert(inputNodes[i].mInputNode);
    // Recurse
    FindConnectedSources(inputNodes[i].mInputNode, aSources, aNodesSeen);

    // Check if this node is an AudioBufferSourceNode that still have a stream,
    // which means it has not finished playing.
    AudioBufferSourceNode* node = inputNodes[i].mInputNode->AsAudioBufferSourceNode();
    if (node && node->GetStream()) {
      aSources.AppendElement(node);
    }
  }
}

void
PannerNode::SendDopplerToSourcesIfNeeded()
{
  // Don't bother sending the doppler shift if both the source and the listener
  // are not moving, because the doppler shift is going to be 1.0.
  if (!(Context()->Listener()->Velocity().IsZero() && mVelocity.IsZero())) {
    for(uint32_t i = 0; i < mSources.Length(); i++) {
      mSources[i]->SendDopplerShiftToStream(ComputeDopplerShift());
    }
  }
}


} // namespace dom
} // namespace mozilla

