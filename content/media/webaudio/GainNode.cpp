/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GainNode.h"
#include "mozilla/dom/GainNodeBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(GainNode, AudioNode,
                                     mGain)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(GainNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(GainNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(GainNode, AudioNode)

GainNode::GainNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mGain(new AudioParam(this, Callback, 1.0f, 0.0f, 1.0f))
{
}

JSObject*
GainNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return GainNodeBinding::Wrap(aCx, aScope, this);
}

}
}

