/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayNode.h"
#include "mozilla/dom/DelayNodeBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(DelayNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DelayNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDelay)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DelayNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_PTR(tmp->mDelay, AudioParam, "delay value")
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DelayNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DelayNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DelayNode, AudioNode)

DelayNode::DelayNode(AudioContext* aContext, float aMaxDelay)
  : AudioNode(aContext)
  , mDelay(new AudioParam(aContext, 0.0f, 0.0f, aMaxDelay))
{
}

JSObject*
DelayNode::WrapObject(JSContext* aCx, JSObject* aScope,
                      bool* aTriedToWrap)
{
  return DelayNodeBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

}
}

