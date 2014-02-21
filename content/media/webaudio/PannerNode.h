/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PannerNode_h_
#define PannerNode_h_

#include "AudioNode.h"
#include "mozilla/dom/PannerNodeBinding.h"
#include "ThreeDPoint.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/Preferences.h"
#include "WebAudioUtils.h"
#include <set>

namespace mozilla {
namespace dom {

class AudioContext;
class AudioBufferSourceNode;

class PannerNode : public AudioNode,
                   public SupportsWeakPtr<PannerNode>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(PannerNode)
  explicit PannerNode(AudioContext* aContext);
  virtual ~PannerNode();


  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  virtual void DestroyMediaStream() MOZ_OVERRIDE;

  virtual void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) MOZ_OVERRIDE
  {
    if (aChannelCount > 2) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCount(aChannelCount, aRv);
  }
  virtual void SetChannelCountModeValue(ChannelCountMode aMode, ErrorResult& aRv) MOZ_OVERRIDE
  {
    if (aMode == ChannelCountMode::Max) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCountModeValue(aMode, aRv);
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PannerNode, AudioNode)

  PanningModelType PanningModel() const
  {
    return mPanningModel;
  }
  void SetPanningModel(PanningModelType aPanningModel)
  {
    if (!Preferences::GetBool("media.webaudio.legacy.PannerNode")) {
      // Do not accept the alternate enum values unless the legacy pref
      // has been turned on.
      switch (aPanningModel) {
      case PanningModelType::_0:
      case PanningModelType::_1:
        // Do nothing in order to emulate setting an invalid enum value.
        return;
      default:
        // Shut up the compiler warning
        break;
      }
    }

    // Handle the alternate enum values
    switch (aPanningModel) {
    case PanningModelType::_0: aPanningModel = PanningModelType::Equalpower; break;
    case PanningModelType::_1: aPanningModel = PanningModelType::HRTF; break;
    default:
      // Shut up the compiler warning
      break;
    }

    mPanningModel = aPanningModel;
    SendInt32ParameterToStream(PANNING_MODEL, int32_t(mPanningModel));
  }

  DistanceModelType DistanceModel() const
  {
    return mDistanceModel;
  }
  void SetDistanceModel(DistanceModelType aDistanceModel)
  {
    if (!Preferences::GetBool("media.webaudio.legacy.PannerNode")) {
      // Do not accept the alternate enum values unless the legacy pref
      // has been turned on.
      switch (aDistanceModel) {
      case DistanceModelType::_0:
      case DistanceModelType::_1:
      case DistanceModelType::_2:
        // Do nothing in order to emulate setting an invalid enum value.
        return;
      default:
        // Shut up the compiler warning
        break;
      }
    }

    // Handle the alternate enum values
    switch (aDistanceModel) {
    case DistanceModelType::_0: aDistanceModel = DistanceModelType::Linear; break;
    case DistanceModelType::_1: aDistanceModel = DistanceModelType::Inverse; break;
    case DistanceModelType::_2: aDistanceModel = DistanceModelType::Exponential; break;
    default:
      // Shut up the compiler warning
      break;
    }

    mDistanceModel = aDistanceModel;
    SendInt32ParameterToStream(DISTANCE_MODEL, int32_t(mDistanceModel));
  }

  void SetPosition(double aX, double aY, double aZ)
  {
    if (WebAudioUtils::FuzzyEqual(mPosition.x, aX) &&
        WebAudioUtils::FuzzyEqual(mPosition.y, aY) &&
        WebAudioUtils::FuzzyEqual(mPosition.z, aZ)) {
      return;
    }
    mPosition.x = aX;
    mPosition.y = aY;
    mPosition.z = aZ;
    SendThreeDPointParameterToStream(POSITION, mPosition);
  }

  void SetOrientation(double aX, double aY, double aZ)
  {
    ThreeDPoint orientation(aX, aY, aZ);
    if (!orientation.IsZero()) {
      orientation.Normalize();
    }
    if (mOrientation.FuzzyEqual(orientation)) {
      return;
    }
    mOrientation = orientation;
    SendThreeDPointParameterToStream(ORIENTATION, mOrientation);
  }

  void SetVelocity(double aX, double aY, double aZ)
  {
    if (WebAudioUtils::FuzzyEqual(mVelocity.x, aX) &&
        WebAudioUtils::FuzzyEqual(mVelocity.y, aY) &&
        WebAudioUtils::FuzzyEqual(mVelocity.z, aZ)) {
      return;
    }
    mVelocity.x = aX;
    mVelocity.y = aY;
    mVelocity.z = aZ;
    SendThreeDPointParameterToStream(VELOCITY, mVelocity);
    SendDopplerToSourcesIfNeeded();
  }

  double RefDistance() const
  {
    return mRefDistance;
  }
  void SetRefDistance(double aRefDistance)
  {
    if (WebAudioUtils::FuzzyEqual(mRefDistance, aRefDistance)) {
      return;
    }
    mRefDistance = aRefDistance;
    SendDoubleParameterToStream(REF_DISTANCE, mRefDistance);
  }

  double MaxDistance() const
  {
    return mMaxDistance;
  }
  void SetMaxDistance(double aMaxDistance)
  {
    if (WebAudioUtils::FuzzyEqual(mMaxDistance, aMaxDistance)) {
      return;
    }
    mMaxDistance = aMaxDistance;
    SendDoubleParameterToStream(MAX_DISTANCE, mMaxDistance);
  }

  double RolloffFactor() const
  {
    return mRolloffFactor;
  }
  void SetRolloffFactor(double aRolloffFactor)
  {
    if (WebAudioUtils::FuzzyEqual(mRolloffFactor, aRolloffFactor)) {
      return;
    }
    mRolloffFactor = aRolloffFactor;
    SendDoubleParameterToStream(ROLLOFF_FACTOR, mRolloffFactor);
  }

  double ConeInnerAngle() const
  {
    return mConeInnerAngle;
  }
  void SetConeInnerAngle(double aConeInnerAngle)
  {
    if (WebAudioUtils::FuzzyEqual(mConeInnerAngle, aConeInnerAngle)) {
      return;
    }
    mConeInnerAngle = aConeInnerAngle;
    SendDoubleParameterToStream(CONE_INNER_ANGLE, mConeInnerAngle);
  }

  double ConeOuterAngle() const
  {
    return mConeOuterAngle;
  }
  void SetConeOuterAngle(double aConeOuterAngle)
  {
    if (WebAudioUtils::FuzzyEqual(mConeOuterAngle, aConeOuterAngle)) {
      return;
    }
    mConeOuterAngle = aConeOuterAngle;
    SendDoubleParameterToStream(CONE_OUTER_ANGLE, mConeOuterAngle);
  }

  double ConeOuterGain() const
  {
    return mConeOuterGain;
  }
  void SetConeOuterGain(double aConeOuterGain)
  {
    if (WebAudioUtils::FuzzyEqual(mConeOuterGain, aConeOuterGain)) {
      return;
    }
    mConeOuterGain = aConeOuterGain;
    SendDoubleParameterToStream(CONE_OUTER_GAIN, mConeOuterGain);
  }

  float ComputeDopplerShift();
  void SendDopplerToSourcesIfNeeded();
  void FindConnectedSources();
  void FindConnectedSources(AudioNode* aNode, nsTArray<AudioBufferSourceNode*>& aSources, std::set<AudioNode*>& aSeenNodes);

private:
  friend class AudioListener;
  friend class PannerNodeEngine;
  enum EngineParameters {
    LISTENER_POSITION,
    LISTENER_FRONT_VECTOR, // unit length
    LISTENER_RIGHT_VECTOR, // unit length, orthogonal to LISTENER_FRONT_VECTOR
    LISTENER_VELOCITY,
    LISTENER_DOPPLER_FACTOR,
    LISTENER_SPEED_OF_SOUND,
    PANNING_MODEL,
    DISTANCE_MODEL,
    POSITION,
    ORIENTATION, // unit length or zero
    VELOCITY,
    REF_DISTANCE,
    MAX_DISTANCE,
    ROLLOFF_FACTOR,
    CONE_INNER_ANGLE,
    CONE_OUTER_ANGLE,
    CONE_OUTER_GAIN
  };

private:
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

  // An array of all the AudioBufferSourceNode connected directly or indirectly
  // to this AudioPannerNode.
  nsTArray<AudioBufferSourceNode*> mSources;
};

}
}

#endif

