/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PannerNode_h_
#define PannerNode_h_

#include "AudioNode.h"
#include "AudioParam.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/TypedEnum.h"
#include "ThreeDPoint.h"

namespace mozilla {
namespace dom {

class AudioContext;

MOZ_BEGIN_ENUM_CLASS(PanningModelEnum, uint16_t)
  EQUALPOWER = 0,
  HRTF = 1,
  SOUNDFIELD = 2,
  Max = 2
MOZ_END_ENUM_CLASS(PanningModelEnum)
MOZ_BEGIN_ENUM_CLASS(DistanceModelEnum, uint16_t)
  LINEAR_DISTANCE = 0,
  INVERSE_DISTANCE = 1,
  EXPONENTIAL_DISTANCE = 2,
  Max = 2
MOZ_END_ENUM_CLASS(DistanceModelEnum)

class PannerNode : public AudioNode
{
public:
  explicit PannerNode(AudioContext* aContext);

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope);

  uint16_t PanningModel() const
  {
    return static_cast<uint16_t> (mPanningModel);
  }
  void SetPanningModel(uint16_t aPanningModel, ErrorResult& aRv)
  {
    PanningModelEnum panningModel =
      static_cast<PanningModelEnum> (aPanningModel);
    if (panningModel > PanningModelEnum::Max) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    } else {
      mPanningModel = panningModel;
    }
  }

  uint16_t DistanceModel() const
  {
    return static_cast<uint16_t> (mDistanceModel);
  }
  void SetDistanceModel(uint16_t aDistanceModel, ErrorResult& aRv)
  {
    DistanceModelEnum distanceModel =
      static_cast<DistanceModelEnum> (aDistanceModel);
    if (distanceModel > DistanceModelEnum::Max) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    } else {
      mDistanceModel = distanceModel;
    }
  }

  void SetPosition(float aX, float aY, float aZ)
  {
    mPosition.x = aX;
    mPosition.y = aY;
    mPosition.z = aZ;
  }

  void SetOrientation(float aX, float aY, float aZ)
  {
    mOrientation.x = aX;
    mOrientation.y = aY;
    mOrientation.z = aZ;
  }

  void SetVelocity(float aX, float aY, float aZ)
  {
    mVelocity.x = aX;
    mVelocity.y = aY;
    mVelocity.z = aZ;
  }

  float RefDistance() const
  {
    return mRefDistance;
  }
  void SetRefDistance(float aRefDistance)
  {
    mRefDistance = aRefDistance;
  }

  float MaxDistance() const
  {
    return mMaxDistance;
  }
  void SetMaxDistance(float aMaxDistance)
  {
    mMaxDistance = aMaxDistance;
  }

  float RolloffFactor() const
  {
    return mRolloffFactor;
  }
  void SetRolloffFactor(float aRolloffFactor)
  {
    mRolloffFactor = aRolloffFactor;
  }

  float ConeInnerAngle() const
  {
    return mConeInnerAngle;
  }
  void SetConeInnerAngle(float aConeInnerAngle)
  {
    mConeInnerAngle = aConeInnerAngle;
  }

  float ConeOuterAngle() const
  {
    return mConeOuterAngle;
  }
  void SetConeOuterAngle(float aConeOuterAngle)
  {
    mConeOuterAngle = aConeOuterAngle;
  }

  float ConeOuterGain() const
  {
    return mConeOuterGain;
  }
  void SetConeOuterGain(float aConeOuterGain)
  {
    mConeOuterGain = aConeOuterGain;
  }

private:
  PanningModelEnum mPanningModel;
  DistanceModelEnum mDistanceModel;
  ThreeDPoint mPosition;
  ThreeDPoint mOrientation;
  ThreeDPoint mVelocity;
  float mRefDistance;
  float mMaxDistance;
  float mRolloffFactor;
  float mConeInnerAngle;
  float mConeOuterAngle;
  float mConeOuterGain;
};

}
}

#endif

