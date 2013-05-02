/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNode.h"
#include "AudioContext.h"
#include "nsContentUtils.h"
#include "mozilla/ErrorResult.h"
#include "AudioNodeStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AudioNode, nsDOMEventTargetHelper)
  tmp->DisconnectFromGraph();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOutputNodes)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AudioNode, nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOutputNodes)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(AudioNode, nsDOMEventTargetHelper)

NS_IMETHODIMP_(nsrefcnt)
AudioNode::Release()
{
  if (mRefCnt.get() == 1) {
    // We are about to be deleted, disconnect the object from the graph before
    // the derived type is destroyed.
    DisconnectFromGraph();
  }
  nsrefcnt r = nsDOMEventTargetHelper::Release();
  NS_LOG_RELEASE(this, r, "AudioNode");
  return r;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioNode)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

AudioNode::AudioNode(AudioContext* aContext,
                     uint32_t aChannelCount,
                     ChannelCountMode aChannelCountMode,
                     ChannelInterpretation aChannelInterpretation)
  : mContext(aContext)
  , mChannelCount(aChannelCount)
  , mChannelCountMode(aChannelCountMode)
  , mChannelInterpretation(aChannelInterpretation)
{
  MOZ_ASSERT(aContext);
  nsDOMEventTargetHelper::BindToOwner(aContext->GetParentObject());
  SetIsDOMBinding();
}

AudioNode::~AudioNode()
{
  MOZ_ASSERT(mInputNodes.IsEmpty());
  MOZ_ASSERT(mOutputNodes.IsEmpty());
}

static uint32_t
FindIndexOfNode(const nsTArray<AudioNode::InputNode>& aInputNodes, const AudioNode* aNode)
{
  for (uint32_t i = 0; i < aInputNodes.Length(); ++i) {
    if (aInputNodes[i].mInputNode == aNode) {
      return i;
    }
  }
  return nsTArray<AudioNode::InputNode>::NoIndex;
}

static uint32_t
FindIndexOfNodeWithPorts(const nsTArray<AudioNode::InputNode>& aInputNodes, const AudioNode* aNode,
                         uint32_t aInputPort, uint32_t aOutputPort)
{
  for (uint32_t i = 0; i < aInputNodes.Length(); ++i) {
    if (aInputNodes[i].mInputNode == aNode &&
        aInputNodes[i].mInputPort == aInputPort &&
        aInputNodes[i].mOutputPort == aOutputPort) {
      return i;
    }
  }
  return nsTArray<AudioNode::InputNode>::NoIndex;
}

void
AudioNode::DisconnectFromGraph()
{
  // Addref this temporarily so the refcount bumping below doesn't destroy us
  // prematurely
  nsRefPtr<AudioNode> kungFuDeathGrip = this;

  // The idea here is that we remove connections one by one, and at each step
  // the graph is in a valid state.

  // Disconnect inputs. We don't need them anymore.
  while (!mInputNodes.IsEmpty()) {
    uint32_t i = mInputNodes.Length() - 1;
    nsRefPtr<AudioNode> input = mInputNodes[i].mInputNode;
    mInputNodes.RemoveElementAt(i);
    input->mOutputNodes.RemoveElement(this);
  }

  while (!mOutputNodes.IsEmpty()) {
    uint32_t i = mOutputNodes.Length() - 1;
    nsRefPtr<AudioNode> output = mOutputNodes[i].forget();
    mOutputNodes.RemoveElementAt(i);
    uint32_t inputIndex = FindIndexOfNode(output->mInputNodes, this);
    // It doesn't matter which one we remove, since we're going to remove all
    // entries for this node anyway.
    output->mInputNodes.RemoveElementAt(inputIndex);
  }

  DestroyMediaStream();
}

void
AudioNode::Connect(AudioNode& aDestination, uint32_t aOutput,
                   uint32_t aInput, ErrorResult& aRv)
{
  if (aOutput >= NumberOfOutputs() ||
      aInput >= aDestination.NumberOfInputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (Context() != aDestination.Context()) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  if (FindIndexOfNodeWithPorts(aDestination.mInputNodes, this, aInput, aOutput) !=
      nsTArray<AudioNode::InputNode>::NoIndex) {
    // connection already exists.
    return;
  }

  // The MediaStreamGraph will handle cycle detection. We don't need to do it
  // here.

  mOutputNodes.AppendElement(&aDestination);
  InputNode* input = aDestination.mInputNodes.AppendElement();
  input->mInputNode = this;
  input->mInputPort = aInput;
  input->mOutputPort = aOutput;
  if (aDestination.mStream) {
    // Connect streams in the MediaStreamGraph
    MOZ_ASSERT(aDestination.mStream->AsProcessedStream());
    ProcessedMediaStream* ps =
      static_cast<ProcessedMediaStream*>(aDestination.mStream.get());
    input->mStreamPort =
      ps->AllocateInputPort(mStream, MediaInputPort::FLAG_BLOCK_INPUT);
  }

  // This connection may have connected a panner and a source.
  Context()->UpdatePannerSource();
}

void
AudioNode::SendDoubleParameterToStream(uint32_t aIndex, double aValue)
{
  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  MOZ_ASSERT(ns, "How come we don't have a stream here?");
  ns->SetDoubleParameter(aIndex, aValue);
}

void
AudioNode::SendInt32ParameterToStream(uint32_t aIndex, int32_t aValue)
{
  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  MOZ_ASSERT(ns, "How come we don't have a stream here?");
  ns->SetInt32Parameter(aIndex, aValue);
}

void
AudioNode::SendThreeDPointParameterToStream(uint32_t aIndex, const ThreeDPoint& aValue)
{
  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  MOZ_ASSERT(ns, "How come we don't have a stream here?");
  ns->SetThreeDPointParameter(aIndex, aValue);
}

void
AudioNode::SendChannelMixingParametersToStream()
{
  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  MOZ_ASSERT(ns, "How come we don't have a stream here?");
  ns->SetChannelMixingParameters(mChannelCount, mChannelCountMode,
                                 mChannelInterpretation);
}

void
AudioNode::SendTimelineParameterToStream(AudioNode* aNode, uint32_t aIndex,
                                         const AudioParamTimeline& aValue)
{
  AudioNodeStream* ns = static_cast<AudioNodeStream*>(aNode->mStream.get());
  MOZ_ASSERT(ns, "How come we don't have a stream here?");
  ns->SetTimelineParameter(aIndex, aValue);
}

void
AudioNode::Disconnect(uint32_t aOutput, ErrorResult& aRv)
{
  if (aOutput >= NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  for (int32_t i = mOutputNodes.Length() - 1; i >= 0; --i) {
    AudioNode* dest = mOutputNodes[i];
    for (int32_t j = dest->mInputNodes.Length() - 1; j >= 0; --j) {
      InputNode& input = dest->mInputNodes[j];
      if (input.mInputNode == this && input.mOutputPort == aOutput) {
        dest->mInputNodes.RemoveElementAt(j);
        // Remove one instance of 'dest' from mOutputNodes. There could be
        // others, and it's not correct to remove them all since some of them
        // could be for different output ports.
        mOutputNodes.RemoveElementAt(i);
        break;
      }
    }
  }

  // This disconnection may have disconnected a panner and a source.
  Context()->UpdatePannerSource();
}

void
AudioNode::DestroyMediaStream()
{
  if (mStream) {
    {
      // Remove the node reference on the engine, and take care to not
      // hold the lock when the stream gets destroyed, because that will
      // cause the engine to be destroyed as well, and we don't want to
      // be holding the lock as we're trying to destroy it!
      AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
      MutexAutoLock lock(ns->Engine()->NodeMutex());
      MOZ_ASSERT(ns, "How come we don't have a stream here?");
      MOZ_ASSERT(ns->Engine()->Node() == this, "Invalid node reference");
      ns->Engine()->ClearNode();
    }

    mStream->Destroy();
    mStream = nullptr;
  }
}

}
}
