/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PannerNode.h"
#include "mozilla/dom/PannerNodeBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PannerNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PannerNode, AudioNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PannerNode, AudioNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PannerNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(PannerNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(PannerNode, AudioNode)

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
PannerNode::WrapObject(JSContext* aCx, JSObject* aScope,
                       bool* aTriedToWrap)
{
  return PannerNodeBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

}
}

