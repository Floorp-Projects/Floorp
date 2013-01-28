/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BiquadFilterNode.h"
#include "mozilla/dom/BiquadFilterNodeBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_3(BiquadFilterNode, AudioNode,
                                     mFrequency, mQ, mGain)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BiquadFilterNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(BiquadFilterNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(BiquadFilterNode, AudioNode)

static float
Nyquist(AudioContext* aContext)
{
  return 0.5f * aContext->SampleRate();
}

BiquadFilterNode::BiquadFilterNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mType(BiquadTypeEnum::LOWPASS)
  , mFrequency(new AudioParam(this, Callback, 350.f, 10.f, Nyquist(aContext)))
  , mQ(new AudioParam(this, Callback, 1.f, 0.0001f, 1000.f))
  , mGain(new AudioParam(this, Callback, 0.f, -40.f, 40.f))
{
}

JSObject*
BiquadFilterNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return BiquadFilterNodeBinding::Wrap(aCx, aScope, this);
}

}
}

