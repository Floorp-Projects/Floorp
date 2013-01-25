/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DynamicsCompressorNode.h"
#include "mozilla/dom/DynamicsCompressorNodeBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_6(DynamicsCompressorNode, AudioNode,
                                     mThreshold,
                                     mKnee,
                                     mRatio,
                                     mReduction,
                                     mAttack,
                                     mRelease)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DynamicsCompressorNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DynamicsCompressorNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DynamicsCompressorNode, AudioNode)

DynamicsCompressorNode::DynamicsCompressorNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mThreshold(new AudioParam(aContext, -24.f, -100.f, 0.f))
  , mKnee(new AudioParam(aContext, 30.f, 0.f, 40.f))
  , mRatio(new AudioParam(aContext, 12.f, 1.f, 20.f))
  , mReduction(new AudioParam(aContext, 0.f, -20.f, 0.f))
  , mAttack(new AudioParam(aContext, 0.003f, 0.f, 1.f))
  , mRelease(new AudioParam(aContext, 0.25f, 0.f, 1.f))
{
}

JSObject*
DynamicsCompressorNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return DynamicsCompressorNodeBinding::Wrap(aCx, aScope, this);
}

}
}

