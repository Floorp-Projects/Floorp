/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PannerNode.h"
#include "AlignmentUtils.h"
#include "AudioDestinationNode.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "AudioListener.h"
#include "PanningUtils.h"
#include "AudioBufferSourceNode.h"
#include "PlayingRefChangeHandler.h"
#include "blink/HRTFPanner.h"
#include "blink/HRTFDatabaseLoader.h"
#include "Tracing.h"

using WebCore::HRTFDatabaseLoader;
using WebCore::HRTFPanner;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PannerNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PannerNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPositionX, mPositionY, mPositionZ,
                                  mOrientationX, mOrientationY, mOrientationZ)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PannerNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPositionX, mPositionY, mPositionZ,
                                    mOrientationX, mOrientationY, mOrientationZ)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PannerNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(PannerNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(PannerNode, AudioNode)

class PannerNodeEngine final : public AudioNodeEngine {
 public:
  explicit PannerNodeEngine(AudioNode* aNode,
                            AudioDestinationNode* aDestination,
                            AudioListenerEngine* aListenerEngine)
      : AudioNodeEngine(aNode),
        mDestination(aDestination->Track()),
        mListenerEngine(aListenerEngine)
        // Please keep these default values consistent with
        // PannerNode::PannerNode below.
        ,
        mPanningModelFunction(&PannerNodeEngine::EqualPowerPanningFunction),
        mDistanceModelFunction(&PannerNodeEngine::InverseGainFunction),
        mPositionX(0.),
        mPositionY(0.),
        mPositionZ(0.),
        mOrientationX(1.),
        mOrientationY(0.),
        mOrientationZ(0.),
        mRefDistance(1.),
        mMaxDistance(10000.),
        mRolloffFactor(1.),
        mConeInnerAngle(360.),
        mConeOuterAngle(360.),
        mConeOuterGain(0.),
        mLeftOverData(INT_MIN) {}

  void RecvTimelineEvent(uint32_t aIndex, AudioParamEvent& aEvent) override {
    MOZ_ASSERT(mDestination);
    aEvent.ConvertToTicks(mDestination);
    switch (aIndex) {
      case PannerNode::POSITIONX:
        mPositionX.InsertEvent<int64_t>(aEvent);
        break;
      case PannerNode::POSITIONY:
        mPositionY.InsertEvent<int64_t>(aEvent);
        break;
      case PannerNode::POSITIONZ:
        mPositionZ.InsertEvent<int64_t>(aEvent);
        break;
      case PannerNode::ORIENTATIONX:
        mOrientationX.InsertEvent<int64_t>(aEvent);
        break;
      case PannerNode::ORIENTATIONY:
        mOrientationY.InsertEvent<int64_t>(aEvent);
        break;
      case PannerNode::ORIENTATIONZ:
        mOrientationZ.InsertEvent<int64_t>(aEvent);
        break;
      default:
        NS_ERROR("Bad PannerNode TimelineParameter");
    }
  }

  void CreateHRTFPanner() {
    MOZ_ASSERT(NS_IsMainThread());
    if (mHRTFPanner) {
      return;
    }
    // HRTFDatabaseLoader needs to be fetched on the main thread.
    RefPtr<HRTFDatabaseLoader> loader =
        HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(
            NodeMainThread()->Context()->SampleRate());
    mHRTFPanner = MakeUnique<HRTFPanner>(
        NodeMainThread()->Context()->SampleRate(), loader.forget());
  }

  void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override {
    switch (aIndex) {
      case PannerNode::PANNING_MODEL:
        switch (PanningModelType(aParam)) {
          case PanningModelType::Equalpower:
            mPanningModelFunction =
                &PannerNodeEngine::EqualPowerPanningFunction;
            break;
          case PanningModelType::HRTF:
            mPanningModelFunction = &PannerNodeEngine::HRTFPanningFunction;
            break;
          default:
            MOZ_ASSERT_UNREACHABLE("We should never see alternate names here");
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
            MOZ_ASSERT_UNREACHABLE("We should never see alternate names here");
            break;
        }
        break;
      default:
        NS_ERROR("Bad PannerNodeEngine Int32Parameter");
    }
  }
  void SetDoubleParameter(uint32_t aIndex, double aParam) override {
    switch (aIndex) {
      case PannerNode::REF_DISTANCE:
        mRefDistance = aParam;
        break;
      case PannerNode::MAX_DISTANCE:
        mMaxDistance = aParam;
        break;
      case PannerNode::ROLLOFF_FACTOR:
        mRolloffFactor = aParam;
        break;
      case PannerNode::CONE_INNER_ANGLE:
        mConeInnerAngle = aParam;
        break;
      case PannerNode::CONE_OUTER_ANGLE:
        mConeOuterAngle = aParam;
        break;
      case PannerNode::CONE_OUTER_GAIN:
        mConeOuterGain = aParam;
        break;
      default:
        NS_ERROR("Bad PannerNodeEngine DoubleParameter");
    }
  }

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    TRACE("PannerNodeEngine::ProcessBlock");

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
          aTrack->ScheduleCheckForInactive();
          mHRTFPanner->reset();

          RefPtr<PlayingRefChangeHandler> refchanged =
              new PlayingRefChangeHandler(aTrack,
                                          PlayingRefChangeHandler::RELEASE);
          aTrack->Graph()->DispatchToMainThreadStableState(refchanged.forget());
        }
        aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
        return;
      }
    } else if (mPanningModelFunction ==
               &PannerNodeEngine::HRTFPanningFunction) {
      if (mLeftOverData == INT_MIN) {
        RefPtr<PlayingRefChangeHandler> refchanged =
            new PlayingRefChangeHandler(aTrack,
                                        PlayingRefChangeHandler::ADDREF);
        aTrack->Graph()->DispatchToMainThreadStableState(refchanged.forget());
      }
      mLeftOverData = mHRTFPanner->maxTailFrames();
    }

    TrackTime tick = mDestination->GraphTimeToTrackTime(aFrom);
    (this->*mPanningModelFunction)(aInput, aOutput, tick);
  }

  bool IsActive() const override { return mLeftOverData != INT_MIN; }

  void ComputeAzimuthAndElevation(const ThreeDPoint& position, float& aAzimuth,
                                  float& aElevation);
  float ComputeConeGain(const ThreeDPoint& position,
                        const ThreeDPoint& orientation);
  // Compute how much the distance contributes to the gain reduction.
  double ComputeDistanceGain(const ThreeDPoint& position);

  void EqualPowerPanningFunction(const AudioBlock& aInput, AudioBlock* aOutput,
                                 TrackTime tick);
  void HRTFPanningFunction(const AudioBlock& aInput, AudioBlock* aOutput,
                           TrackTime tick);

  float LinearGainFunction(double aDistance);
  float InverseGainFunction(double aDistance);
  float ExponentialGainFunction(double aDistance);

  ThreeDPoint ConvertAudioParamTimelineTo3DP(AudioParamTimeline& aX,
                                             AudioParamTimeline& aY,
                                             AudioParamTimeline& aZ,
                                             TrackTime& tick);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    if (mHRTFPanner) {
      amount += mHRTFPanner->sizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  RefPtr<AudioNodeTrack> mDestination;
  // This member is set on the main thread, but is not accessed on the rendering
  // thread untile mPanningModelFunction has changed, and this happens strictly
  // later, via a MediaTrackGraph ControlMessage.
  UniquePtr<HRTFPanner> mHRTFPanner;
  RefPtr<AudioListenerEngine> mListenerEngine;
  typedef void (PannerNodeEngine::*PanningModelFunction)(
      const AudioBlock& aInput, AudioBlock* aOutput, TrackTime tick);
  PanningModelFunction mPanningModelFunction;
  typedef float (PannerNodeEngine::*DistanceModelFunction)(double aDistance);
  DistanceModelFunction mDistanceModelFunction;
  AudioParamTimeline mPositionX;
  AudioParamTimeline mPositionY;
  AudioParamTimeline mPositionZ;
  AudioParamTimeline mOrientationX;
  AudioParamTimeline mOrientationY;
  AudioParamTimeline mOrientationZ;
  double mRefDistance;
  double mMaxDistance;
  double mRolloffFactor;
  double mConeInnerAngle;
  double mConeOuterAngle;
  double mConeOuterGain;
  int mLeftOverData;
};

PannerNode::PannerNode(AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Clamped_max,
                ChannelInterpretation::Speakers)
      // Please keep these default values consistent with
      // PannerNodeEngine::PannerNodeEngine above.
      ,
      mPanningModel(PanningModelType::Equalpower),
      mDistanceModel(DistanceModelType::Inverse),
      mRefDistance(1.),
      mMaxDistance(10000.),
      mRolloffFactor(1.),
      mConeInnerAngle(360.),
      mConeOuterAngle(360.),
      mConeOuterGain(0.) {
  mPositionX = CreateAudioParam(PannerNode::POSITIONX, u"PositionX"_ns, 0.f);
  mPositionY = CreateAudioParam(PannerNode::POSITIONY, u"PositionY"_ns, 0.f);
  mPositionZ = CreateAudioParam(PannerNode::POSITIONZ, u"PositionZ"_ns, 0.f);
  mOrientationX =
      CreateAudioParam(PannerNode::ORIENTATIONX, u"OrientationX"_ns, 1.0f);
  mOrientationY =
      CreateAudioParam(PannerNode::ORIENTATIONY, u"OrientationY"_ns, 0.f);
  mOrientationZ =
      CreateAudioParam(PannerNode::ORIENTATIONZ, u"OrientationZ"_ns, 0.f);
  mTrack = AudioNodeTrack::Create(
      aContext,
      new PannerNodeEngine(this, aContext->Destination(),
                           aContext->Listener()->Engine()),
      AudioNodeTrack::NO_TRACK_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<PannerNode> PannerNode::Create(AudioContext& aAudioContext,
                                                const PannerOptions& aOptions,
                                                ErrorResult& aRv) {
  RefPtr<PannerNode> audioNode = new PannerNode(&aAudioContext);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  audioNode->SetPanningModel(aOptions.mPanningModel);
  audioNode->SetDistanceModel(aOptions.mDistanceModel);
  audioNode->mPositionX->SetInitialValue(aOptions.mPositionX);
  audioNode->mPositionY->SetInitialValue(aOptions.mPositionY);
  audioNode->mPositionZ->SetInitialValue(aOptions.mPositionZ);
  audioNode->mOrientationX->SetInitialValue(aOptions.mOrientationX);
  audioNode->mOrientationY->SetInitialValue(aOptions.mOrientationY);
  audioNode->mOrientationZ->SetInitialValue(aOptions.mOrientationZ);
  audioNode->SetRefDistance(aOptions.mRefDistance, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  audioNode->SetMaxDistance(aOptions.mMaxDistance, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  audioNode->SetRolloffFactor(aOptions.mRolloffFactor, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  audioNode->SetConeInnerAngle(aOptions.mConeInnerAngle);
  audioNode->SetConeOuterAngle(aOptions.mConeOuterAngle);
  audioNode->SetConeOuterGain(aOptions.mConeOuterGain, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return audioNode.forget();
}

void PannerNode::SetPanningModel(PanningModelType aPanningModel) {
  mPanningModel = aPanningModel;
  if (mPanningModel == PanningModelType::HRTF) {
    // We can set the engine's `mHRTFPanner` member here from the main thread,
    // because the engine will not touch it from the MediaTrackGraph
    // thread until the PANNING_MODEL message sent below is received.
    static_cast<PannerNodeEngine*>(mTrack->Engine())->CreateHRTFPanner();
  }
  SendInt32ParameterToTrack(PANNING_MODEL, int32_t(mPanningModel));
}

static bool SetParamFromDouble(AudioParam* aParam, double aValue,
                               const char (&aParamName)[2], ErrorResult& aRv) {
  float value = static_cast<float>(aValue);
  if (!std::isfinite(value)) {
    aRv.ThrowTypeError<MSG_NOT_FINITE>(aParamName);
    return false;
  }
  aParam->SetValue(value, aRv);
  return !aRv.Failed();
}

void PannerNode::SetPosition(double aX, double aY, double aZ,
                             ErrorResult& aRv) {
  if (!SetParamFromDouble(mPositionX, aX, "x", aRv)) {
    return;
  }
  if (!SetParamFromDouble(mPositionY, aY, "y", aRv)) {
    return;
  }
  SetParamFromDouble(mPositionZ, aZ, "z", aRv);
}

void PannerNode::SetOrientation(double aX, double aY, double aZ,
                                ErrorResult& aRv) {
  if (!SetParamFromDouble(mOrientationX, aX, "x", aRv)) {
    return;
  }
  if (!SetParamFromDouble(mOrientationY, aY, "y", aRv)) {
    return;
  }
  SetParamFromDouble(mOrientationZ, aZ, "z", aRv);
}

size_t PannerNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  return AudioNode::SizeOfExcludingThis(aMallocSizeOf);
}

size_t PannerNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject* PannerNode::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return PannerNode_Binding::Wrap(aCx, this, aGivenProto);
}

// Those three functions are described in the spec.
float PannerNodeEngine::LinearGainFunction(double aDistance) {
  return 1 - mRolloffFactor *
                 (std::max(std::min(aDistance, mMaxDistance), mRefDistance) -
                  mRefDistance) /
                 (mMaxDistance - mRefDistance);
}

float PannerNodeEngine::InverseGainFunction(double aDistance) {
  return mRefDistance /
         (mRefDistance +
          mRolloffFactor * (std::max(aDistance, mRefDistance) - mRefDistance));
}

float PannerNodeEngine::ExponentialGainFunction(double aDistance) {
  return fdlibm_pow(std::max(aDistance, mRefDistance) / mRefDistance,
                    -mRolloffFactor);
}

void PannerNodeEngine::HRTFPanningFunction(const AudioBlock& aInput,
                                           AudioBlock* aOutput,
                                           TrackTime tick) {
  // The output of this node is always stereo, no matter what the inputs are.
  aOutput->AllocateChannels(2);

  float azimuth, elevation;

  ThreeDPoint position =
      ConvertAudioParamTimelineTo3DP(mPositionX, mPositionY, mPositionZ, tick);
  ThreeDPoint orientation = ConvertAudioParamTimelineTo3DP(
      mOrientationX, mOrientationY, mOrientationZ, tick);
  if (!orientation.IsZero()) {
    orientation.Normalize();
  }
  ComputeAzimuthAndElevation(position, azimuth, elevation);

  AudioBlock input = aInput;
  // Gain is applied before the delay and convolution of the HRTF.
  input.mVolume *=
      ComputeConeGain(position, orientation) * ComputeDistanceGain(position);

  mHRTFPanner->pan(azimuth, elevation, &input, aOutput);
}

ThreeDPoint PannerNodeEngine::ConvertAudioParamTimelineTo3DP(
    AudioParamTimeline& aX, AudioParamTimeline& aY, AudioParamTimeline& aZ,
    TrackTime& tick) {
  return ThreeDPoint(aX.GetValueAtTime(tick), aY.GetValueAtTime(tick),
                     aZ.GetValueAtTime(tick));
}

void PannerNodeEngine::EqualPowerPanningFunction(const AudioBlock& aInput,
                                                 AudioBlock* aOutput,
                                                 TrackTime tick) {
  float azimuth, elevation, gainL, gainR, normalizedAzimuth, distanceGain,
      coneGain;
  int inputChannels = aInput.ChannelCount();

  // Optimize the case where the position and orientation is constant for this
  // processing block: we can just apply a constant gain on the left and right
  // channel
  if (mPositionX.HasSimpleValue() && mPositionY.HasSimpleValue() &&
      mPositionZ.HasSimpleValue() && mOrientationX.HasSimpleValue() &&
      mOrientationY.HasSimpleValue() && mOrientationZ.HasSimpleValue()) {
    ThreeDPoint position(mPositionX.GetValue(), mPositionY.GetValue(),
                         mPositionZ.GetValue());
    ThreeDPoint orientation(mOrientationX.GetValue(), mOrientationY.GetValue(),
                            mOrientationZ.GetValue());
    if (!orientation.IsZero()) {
      orientation.Normalize();
    }

    // For a stereo source, when both the listener and the panner are in
    // the same spot, and no cone gain is specified, this node is noop.
    if (inputChannels == 2 && mListenerEngine->Position() == position &&
        mConeInnerAngle == 360 && mConeOuterAngle == 360) {
      *aOutput = aInput;
      return;
    }

    ComputeAzimuthAndElevation(position, azimuth, elevation);
    coneGain = ComputeConeGain(position, orientation);

    // The following algorithm is described in the spec.
    // Clamp azimuth in the [-90, 90] range.
    azimuth = std::min(180.f, std::max(-180.f, azimuth));

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

    distanceGain = ComputeDistanceGain(position);

    // Actually compute the left and right gain.
    gainL = fdlibm_cos(0.5 * M_PI * normalizedAzimuth);
    gainR = fdlibm_sin(0.5 * M_PI * normalizedAzimuth);

    // Compute the output.
    ApplyStereoPanning(aInput, aOutput, gainL, gainR, azimuth <= 0);

    aOutput->mVolume *= distanceGain * coneGain;
  } else {
    float positionX[WEBAUDIO_BLOCK_SIZE];
    float positionY[WEBAUDIO_BLOCK_SIZE];
    float positionZ[WEBAUDIO_BLOCK_SIZE];
    float orientationX[WEBAUDIO_BLOCK_SIZE];
    float orientationY[WEBAUDIO_BLOCK_SIZE];
    float orientationZ[WEBAUDIO_BLOCK_SIZE];

    if (!mPositionX.HasSimpleValue()) {
      mPositionX.GetValuesAtTime(tick, positionX, WEBAUDIO_BLOCK_SIZE);
    } else {
      positionX[0] = mPositionX.GetValue();
    }
    if (!mPositionY.HasSimpleValue()) {
      mPositionY.GetValuesAtTime(tick, positionY, WEBAUDIO_BLOCK_SIZE);
    } else {
      positionY[0] = mPositionY.GetValue();
    }
    if (!mPositionZ.HasSimpleValue()) {
      mPositionZ.GetValuesAtTime(tick, positionZ, WEBAUDIO_BLOCK_SIZE);
    } else {
      positionZ[0] = mPositionZ.GetValue();
    }
    if (!mOrientationX.HasSimpleValue()) {
      mOrientationX.GetValuesAtTime(tick, orientationX, WEBAUDIO_BLOCK_SIZE);
    } else {
      orientationX[0] = mOrientationX.GetValue();
    }
    if (!mOrientationY.HasSimpleValue()) {
      mOrientationY.GetValuesAtTime(tick, orientationY, WEBAUDIO_BLOCK_SIZE);
    } else {
      orientationY[0] = mOrientationY.GetValue();
    }
    if (!mOrientationZ.HasSimpleValue()) {
      mOrientationZ.GetValuesAtTime(tick, orientationZ, WEBAUDIO_BLOCK_SIZE);
    } else {
      orientationZ[0] = mOrientationZ.GetValue();
    }

    float buffer[3 * WEBAUDIO_BLOCK_SIZE + 4];
    bool onLeft[WEBAUDIO_BLOCK_SIZE];

    float* alignedPanningL = ALIGNED16(buffer);
    float* alignedPanningR = alignedPanningL + WEBAUDIO_BLOCK_SIZE;
    float* alignedGain = alignedPanningR + WEBAUDIO_BLOCK_SIZE;
    ASSERT_ALIGNED16(alignedPanningL);
    ASSERT_ALIGNED16(alignedPanningR);
    ASSERT_ALIGNED16(alignedGain);

    for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
      ThreeDPoint position(
          mPositionX.HasSimpleValue() ? positionX[0] : positionX[counter],
          mPositionY.HasSimpleValue() ? positionY[0] : positionY[counter],
          mPositionZ.HasSimpleValue() ? positionZ[0] : positionZ[counter]);
      ThreeDPoint orientation(
          mOrientationX.HasSimpleValue() ? orientationX[0]
                                         : orientationX[counter],
          mOrientationY.HasSimpleValue() ? orientationY[0]
                                         : orientationY[counter],
          mOrientationZ.HasSimpleValue() ? orientationZ[0]
                                         : orientationZ[counter]);
      if (!orientation.IsZero()) {
        orientation.Normalize();
      }

      ComputeAzimuthAndElevation(position, azimuth, elevation);
      coneGain = ComputeConeGain(position, orientation);

      // The following algorithm is described in the spec.
      // Clamp azimuth in the [-90, 90] range.
      azimuth = std::min(180.f, std::max(-180.f, azimuth));

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

      distanceGain = ComputeDistanceGain(position);

      // Actually compute the left and right gain.
      float gainL = fdlibm_cos(0.5 * M_PI * normalizedAzimuth);
      float gainR = fdlibm_sin(0.5 * M_PI * normalizedAzimuth);

      alignedPanningL[counter] = gainL;
      alignedPanningR[counter] = gainR;
      alignedGain[counter] = distanceGain * coneGain;
      onLeft[counter] = azimuth <= 0;
    }

    // Apply the panning to the output buffer
    ApplyStereoPanning(aInput, aOutput, alignedPanningL, alignedPanningR,
                       onLeft);

    // Apply the input volume, cone and distance gain to the output buffer.
    float* outputL = aOutput->ChannelFloatsForWrite(0);
    float* outputR = aOutput->ChannelFloatsForWrite(1);
    AudioBlockInPlaceScale(outputL, alignedGain);
    AudioBlockInPlaceScale(outputR, alignedGain);
  }
}

// This algorithm is specified in the webaudio spec.
void PannerNodeEngine::ComputeAzimuthAndElevation(const ThreeDPoint& position,
                                                  float& aAzimuth,
                                                  float& aElevation) {
  ThreeDPoint sourceListener = position - mListenerEngine->Position();
  if (sourceListener.IsZero()) {
    aAzimuth = 0.0;
    aElevation = 0.0;
    return;
  }

  sourceListener.Normalize();

  // Project the source-listener vector on the x-z plane.
  const ThreeDPoint& listenerFront = mListenerEngine->FrontVector();
  const ThreeDPoint& listenerRight = mListenerEngine->RightVector();
  ThreeDPoint up = listenerRight.CrossProduct(listenerFront);

  double upProjection = sourceListener.DotProduct(up);
  aElevation = 90 - 180 * fdlibm_acos(upProjection) / M_PI;

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
  aAzimuth = 180 * fdlibm_acos(projection) / M_PI;

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
float PannerNodeEngine::ComputeConeGain(const ThreeDPoint& position,
                                        const ThreeDPoint& orientation) {
  // Omnidirectional source
  if (orientation.IsZero() ||
      ((mConeInnerAngle == 360) && (mConeOuterAngle == 360))) {
    return 1;
  }

  // Normalized source-listener vector
  ThreeDPoint sourceToListener = mListenerEngine->Position() - position;
  sourceToListener.Normalize();

  // Angle between the source orientation vector and the source-listener vector
  double dotProduct = sourceToListener.DotProduct(orientation);
  double angle = 180 * fdlibm_acos(dotProduct) / M_PI;
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

double PannerNodeEngine::ComputeDistanceGain(const ThreeDPoint& position) {
  ThreeDPoint distanceVec = position - mListenerEngine->Position();
  float distance = sqrt(distanceVec.DotProduct(distanceVec));
  return std::max(0.0f, (this->*mDistanceModelFunction)(distance));
}

}  // namespace mozilla::dom
