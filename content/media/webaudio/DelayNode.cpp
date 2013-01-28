/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayNode.h"
#include "mozilla/dom/DelayNodeBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(DelayNode, AudioNode,
                                     mDelay)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DelayNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DelayNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DelayNode, AudioNode)

DelayNode::DelayNode(AudioContext* aContext, double aMaxDelay)
  : AudioNode(aContext)
  , mDelay(new AudioParam(this, Callback, 0.0f, 0.0f, float(aMaxDelay)))
{
}

JSObject*
DelayNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return DelayNodeBinding::Wrap(aCx, aScope, this);
}

}
}

