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
#include "mozilla/dom/PannerNodeBinding.h"
#include "ThreeDPoint.h"

namespace mozilla {
namespace dom {

class AudioContext;

class PannerNode : public AudioNode
{
public:
  explicit PannerNode(AudioContext* aContext);

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope);

  PanningModelType PanningModel() const
  {
    return mPanningModel;
  }
  void SetPanningModel(PanningModelType aPanningModel)
  {
    mPanningModel = aPanningModel;
  }

  DistanceModelType DistanceModel() const
  {
    return mDistanceModel;
  }
  void SetDistanceModel(DistanceModelType aDistanceModel)
  {
    mDistanceModel = aDistanceModel;
  }

  void SetPosition(double aX, double aY, double aZ)
  {
    mPosition.x = aX;
    mPosition.y = aY;
    mPosition.z = aZ;
  }

  void SetOrientation(double aX, double aY, double aZ)
  {
    mOrientation.x = aX;
    mOrientation.y = aY;
    mOrientation.z = aZ;
  }

  void SetVelocity(double aX, double aY, double aZ)
  {
    mVelocity.x = aX;
    mVelocity.y = aY;
    mVelocity.z = aZ;
  }

  double RefDistance() const
  {
    return mRefDistance;
  }
  void SetRefDistance(double aRefDistance)
  {
    mRefDistance = aRefDistance;
  }

  double MaxDistance() const
  {
    return mMaxDistance;
  }
  void SetMaxDistance(double aMaxDistance)
  {
    mMaxDistance = aMaxDistance;
  }

  double RolloffFactor() const
  {
    return mRolloffFactor;
  }
  void SetRolloffFactor(double aRolloffFactor)
  {
    mRolloffFactor = aRolloffFactor;
  }

  double ConeInnerAngle() const
  {
    return mConeInnerAngle;
  }
  void SetConeInnerAngle(double aConeInnerAngle)
  {
    mConeInnerAngle = aConeInnerAngle;
  }

  double ConeOuterAngle() const
  {
    return mConeOuterAngle;
  }
  void SetConeOuterAngle(double aConeOuterAngle)
  {
    mConeOuterAngle = aConeOuterAngle;
  }

  double ConeOuterGain() const
  {
    return mConeOuterGain;
  }
  void SetConeOuterGain(double aConeOuterGain)
  {
    mConeOuterGain = aConeOuterGain;
  }

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
};

}
}

#endif

