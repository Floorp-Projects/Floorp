/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PannerNode.h"
#include "mozilla/dom/PannerNodeBinding.h"

namespace mozilla {
namespace dom {

PannerNode::PannerNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mPanningModel(PanningModelEnum::HRTF)
  , mDistanceModel(DistanceModelEnum::INVERSE_DISTANCE)
  , mPosition()
  , mOrientation(1.f, 0.f, 0.f)
  , mVelocity()
  , mRefDistance(1.f)
  , mMaxDistance(10000.f)
  , mRolloffFactor(1.f)
  , mConeInnerAngle(360.f)
  , mConeOuterAngle(360.f)
  , mConeOuterGain(0.f)
{
}

JSObject*
PannerNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return PannerNodeBinding::Wrap(aCx, aScope, this);
}

}
}

