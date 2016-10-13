/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConstantSourceNode.h"

#include "AudioDestinationNode.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ConstantSourceNode, AudioNode,
                                   mOffset)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ConstantSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(ConstantSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(ConstantSourceNode, AudioNode)

ConstantSourceNode::ConstantSourceNode(AudioContext* aContext)
  : AudioNode(aContext,
              1,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
{
}

ConstantSourceNode::~ConstantSourceNode()
{
}

size_t
ConstantSourceNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);

  amount += mOffset->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
ConstantSourceNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
ConstantSourceNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ConstantSourceNodeBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<ConstantSourceNode>
ConstantSourceNode::Constructor(const GlobalObject& aGlobal,
                                const AudioContext& aContext,
                                const ConstantSourceOptions& aOptions,
                                ErrorResult& aRv)
{
}

void
ConstantSourceNode::DestroyMediaStream()
{
}

void
ConstantSourceNode::Start(double aWhen, ErrorResult& rv)
{
}

void
ConstantSourceNode::Stop(double aWhen, ErrorResult& rv)
{
}

void
ConstantSourceNode::NotifyMainThreadStreamFinished()
{
}

} // namespace dom
} // namespace mozilla
