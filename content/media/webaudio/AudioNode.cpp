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

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            AudioNode::InputNode& aField,
                            const char* aName,
                            unsigned aFlags)
{
  CycleCollectionNoteChild(aCallback, aField.mInputNode.get(), aName, aFlags);
}

inline void
ImplCycleCollectionUnlink(nsCycleCollectionTraversalCallback& aCallback,
                          AudioNode::InputNode& aField,
                          const char* aName,
                          unsigned aFlags)
{
  aField.mInputNode = nullptr;
}

NS_IMPL_CYCLE_COLLECTION_3(AudioNode, mContext, mInputNodes, mOutputNodes)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AudioNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AudioNode)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioNode)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

AudioNode::AudioNode(AudioContext* aContext)
  : mContext(aContext)
  , mJSBindingFinalized(false)
  , mCanProduceOwnOutput(false)
  , mOutputEnded(false)
{
  MOZ_ASSERT(aContext);
}

AudioNode::~AudioNode()
{
  DestroyMediaStream();
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
AudioNode::UpdateOutputEnded()
{
  if (mOutputEnded) {
    // Already ended, so nothing to do.
    return;
  }
  if (mCanProduceOwnOutput ||
      !mInputNodes.IsEmpty() ||
      (!mJSBindingFinalized && NumberOfInputs() > 0)) {
    // This node could still produce output in the future.
    return;
  }

  mOutputEnded = true;

  // Addref this temporarily so the refcount bumping below doesn't destroy us
  // prematurely
  nsRefPtr<AudioNode> kungFuDeathGrip = this;

  // The idea here is that we remove connections one by one, and at each step
  // the graph is in a valid state.

  // Disconnect inputs. We don't need them anymore.
  while (!mInputNodes.IsEmpty()) {
    uint32_t i = mInputNodes.Length() - 1;
    nsRefPtr<AudioNode> input = mInputNodes[i].mInputNode.forget();
    mInputNodes.RemoveElementAt(i);
    NS_ASSERTION(mOutputNodes.Contains(this), "input/output inconsistency");
    input->mOutputNodes.RemoveElement(this);
  }

  while (!mOutputNodes.IsEmpty()) {
    uint32_t i = mOutputNodes.Length() - 1;
    nsRefPtr<AudioNode> output = mOutputNodes[i].forget();
    mOutputNodes.RemoveElementAt(i);
    uint32_t inputIndex = FindIndexOfNode(output->mInputNodes, this);
    NS_ASSERTION(inputIndex != nsTArray<AudioNode::InputNode>::NoIndex, "input/output inconsistency");
    // It doesn't matter which one we remove, since we're going to remove all
    // entries for this node anyway.
    output->mInputNodes.RemoveElementAt(inputIndex);

    output->UpdateOutputEnded();
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

  if (IsOutputEnded() || aDestination.IsOutputEnded()) {
    // No need to connect since we're not going to produce anything other
    // than silence.
    return;
  }
  if (FindIndexOfNodeWithPorts(aDestination.mInputNodes, this, aInput, aOutput) !=
      nsTArray<AudioNode::InputNode>::NoIndex) {
    // connection already exists.
    return;
  }

  // The MediaStreamGraph will handle cycle detection. We don't need to do it
  // here.

  // Addref this temporarily so the refcount bumping below doesn't destroy us
  nsRefPtr<AudioNode> kungFuDeathGrip = this;

  mOutputNodes.AppendElement(&aDestination);
  InputNode* input = aDestination.mInputNodes.AppendElement();
  input->mInputNode = this;
  input->mInputPort = aInput;
  input->mOutputPort = aOutput;
  if (SupportsMediaStreams() && aDestination.mStream) {
    // Connect streams in the MediaStreamGraph
    MOZ_ASSERT(aDestination.mStream->AsProcessedStream());
    ProcessedMediaStream* ps =
      static_cast<ProcessedMediaStream*>(aDestination.mStream.get());
    input->mStreamPort =
      ps->AllocateInputPort(mStream, MediaInputPort::FLAG_BLOCK_INPUT);
  }
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
AudioNode::Disconnect(uint32_t aOutput, ErrorResult& aRv)
{
  if (aOutput >= NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  // Disconnect everything connected to this output. First find the
  // corresponding inputs and remove them.
  nsAutoTArray<nsRefPtr<AudioNode>,4> outputsToUpdate;

  for (int32_t i = mOutputNodes.Length() - 1; i >= 0; --i) {
    AudioNode* dest = mOutputNodes[i];
    for (int32_t j = dest->mInputNodes.Length() - 1; j >= 0; --j) {
      InputNode& input = dest->mInputNodes[j];
      if (input.mInputNode == this && input.mOutputPort == aOutput) {
        dest->mInputNodes.RemoveElementAt(j);
        // Remove one instance of 'dest' from mOutputNodes. There could be
        // others, and it's not correct to remove them all since some of them
        // could be for different output ports.
        *outputsToUpdate.AppendElement() = mOutputNodes[i].forget();
        mOutputNodes.RemoveElementAt(i);
        break;
      }
    }
  }

  for (uint32_t i = 0; i < outputsToUpdate.Length(); ++i) {
    outputsToUpdate[i]->UpdateOutputEnded();
  }
}

}
}
