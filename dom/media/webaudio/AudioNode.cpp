/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNode.h"
#include "mozilla/ErrorResult.h"
#include "AudioNodeStream.h"
#include "AudioNodeEngine.h"
#include "mozilla/dom/AudioParam.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"

namespace mozilla {
namespace dom {

static const uint32_t INVALID_PORT = 0xffffffff;
static uint32_t gId = 0;

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AudioNode, DOMEventTargetHelper)
  tmp->DisconnectFromGraph();
  if (tmp->mContext) {
    tmp->mContext->UnregisterNode(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParams)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOutputNodes)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOutputParams)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AudioNode,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParams)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOutputNodes)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOutputParams)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(AudioNode, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioNode, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioNode)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

AudioNode::AudioNode(AudioContext* aContext, uint32_t aChannelCount,
                     ChannelCountMode aChannelCountMode,
                     ChannelInterpretation aChannelInterpretation)
    : DOMEventTargetHelper(aContext->GetParentObject()),
      mContext(aContext),
      mChannelCount(aChannelCount),
      mChannelCountMode(aChannelCountMode),
      mChannelInterpretation(aChannelInterpretation),
      mId(gId++),
      mPassThrough(false),
      mAbstractMainThread(
          aContext->GetOwnerGlobal()
              ? aContext->GetOwnerGlobal()->AbstractMainThreadFor(
                    TaskCategory::Other)
              : nullptr) {
  MOZ_ASSERT(aContext);
  aContext->RegisterNode(this);
}

AudioNode::~AudioNode() {
  MOZ_ASSERT(mInputNodes.IsEmpty());
  MOZ_ASSERT(mOutputNodes.IsEmpty());
  MOZ_ASSERT(mOutputParams.IsEmpty());
  MOZ_ASSERT(!mStream,
             "The webaudio-node-demise notification must have been sent");
  if (mContext) {
    mContext->UnregisterNode(this);
  }
}

void AudioNode::Initialize(const AudioNodeOptions& aOptions, ErrorResult& aRv) {
  if (aOptions.mChannelCount.WasPassed()) {
    SetChannelCount(aOptions.mChannelCount.Value(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  if (aOptions.mChannelCountMode.WasPassed()) {
    SetChannelCountModeValue(aOptions.mChannelCountMode.Value(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  if (aOptions.mChannelInterpretation.WasPassed()) {
    SetChannelInterpretationValue(aOptions.mChannelInterpretation.Value(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }
}

size_t AudioNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  // Not owned:
  // - mContext
  // - mStream
  size_t amount = 0;

  amount += mInputNodes.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < mInputNodes.Length(); i++) {
    amount += mInputNodes[i].SizeOfExcludingThis(aMallocSizeOf);
  }

  // Just measure the array. The entire audio node graph is measured via the
  // MediaStreamGraph's streams, so we don't want to double-count the elements.
  amount += mOutputNodes.ShallowSizeOfExcludingThis(aMallocSizeOf);

  amount += mOutputParams.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < mOutputParams.Length(); i++) {
    amount += mOutputParams[i]->SizeOfIncludingThis(aMallocSizeOf);
  }

  return amount;
}

size_t AudioNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

template <class InputNode>
static size_t FindIndexOfNode(const nsTArray<InputNode>& aInputNodes,
                              const AudioNode* aNode) {
  for (size_t i = 0; i < aInputNodes.Length(); ++i) {
    if (aInputNodes[i].mInputNode == aNode) {
      return i;
    }
  }
  return nsTArray<InputNode>::NoIndex;
}

template <class InputNode>
static size_t FindIndexOfNodeWithPorts(const nsTArray<InputNode>& aInputNodes,
                                       const AudioNode* aNode,
                                       uint32_t aInputPort,
                                       uint32_t aOutputPort) {
  for (size_t i = 0; i < aInputNodes.Length(); ++i) {
    if (aInputNodes[i].mInputNode == aNode &&
        aInputNodes[i].mInputPort == aInputPort &&
        aInputNodes[i].mOutputPort == aOutputPort) {
      return i;
    }
  }
  return nsTArray<InputNode>::NoIndex;
}

void AudioNode::DisconnectFromGraph() {
  MOZ_ASSERT(mRefCnt.get() > mInputNodes.Length(),
             "Caller should be holding a reference");

  // The idea here is that we remove connections one by one, and at each step
  // the graph is in a valid state.

  // Disconnect inputs. We don't need them anymore.
  while (!mInputNodes.IsEmpty()) {
    size_t i = mInputNodes.Length() - 1;
    RefPtr<AudioNode> input = mInputNodes[i].mInputNode;
    mInputNodes.RemoveElementAt(i);
    input->mOutputNodes.RemoveElement(this);
  }

  while (!mOutputNodes.IsEmpty()) {
    size_t i = mOutputNodes.Length() - 1;
    RefPtr<AudioNode> output = mOutputNodes[i].forget();
    mOutputNodes.RemoveElementAt(i);
    size_t inputIndex = FindIndexOfNode(output->mInputNodes, this);
    // It doesn't matter which one we remove, since we're going to remove all
    // entries for this node anyway.
    output->mInputNodes.RemoveElementAt(inputIndex);
    // This effects of this connection will remain.
    output->NotifyHasPhantomInput();
  }

  while (!mOutputParams.IsEmpty()) {
    size_t i = mOutputParams.Length() - 1;
    RefPtr<AudioParam> output = mOutputParams[i].forget();
    mOutputParams.RemoveElementAt(i);
    size_t inputIndex = FindIndexOfNode(output->InputNodes(), this);
    // It doesn't matter which one we remove, since we're going to remove all
    // entries for this node anyway.
    output->RemoveInputNode(inputIndex);
  }

  DestroyMediaStream();
}

AudioNode* AudioNode::Connect(AudioNode& aDestination, uint32_t aOutput,
                              uint32_t aInput, ErrorResult& aRv) {
  if (aOutput >= NumberOfOutputs() || aInput >= aDestination.NumberOfInputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  if (Context() != aDestination.Context()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  if (FindIndexOfNodeWithPorts(aDestination.mInputNodes, this, aInput,
                               aOutput) !=
      nsTArray<AudioNode::InputNode>::NoIndex) {
    // connection already exists.
    return &aDestination;
  }

  WEB_AUDIO_API_LOG("%f: %s %u Connect() to %s %u", Context()->CurrentTime(),
                    NodeType(), Id(), aDestination.NodeType(),
                    aDestination.Id());

  // The MediaStreamGraph will handle cycle detection. We don't need to do it
  // here.

  mOutputNodes.AppendElement(&aDestination);
  InputNode* input = aDestination.mInputNodes.AppendElement();
  input->mInputNode = this;
  input->mInputPort = aInput;
  input->mOutputPort = aOutput;
  AudioNodeStream* destinationStream = aDestination.mStream;
  if (mStream && destinationStream) {
    // Connect streams in the MediaStreamGraph
    MOZ_ASSERT(aInput <= UINT16_MAX, "Unexpected large input port number");
    MOZ_ASSERT(aOutput <= UINT16_MAX, "Unexpected large output port number");
    input->mStreamPort = destinationStream->AllocateInputPort(
        mStream, AudioNodeStream::AUDIO_TRACK, TRACK_ANY,
        static_cast<uint16_t>(aInput), static_cast<uint16_t>(aOutput));
  }
  aDestination.NotifyInputsChanged();

  return &aDestination;
}

void AudioNode::Connect(AudioParam& aDestination, uint32_t aOutput,
                        ErrorResult& aRv) {
  if (aOutput >= NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (Context() != aDestination.GetParentObject()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  if (FindIndexOfNodeWithPorts(aDestination.InputNodes(), this, INVALID_PORT,
                               aOutput) !=
      nsTArray<AudioNode::InputNode>::NoIndex) {
    // connection already exists.
    return;
  }

  mOutputParams.AppendElement(&aDestination);
  InputNode* input = aDestination.AppendInputNode();
  input->mInputNode = this;
  input->mInputPort = INVALID_PORT;
  input->mOutputPort = aOutput;

  MediaStream* stream = aDestination.Stream();
  MOZ_ASSERT(stream->AsProcessedStream());
  ProcessedMediaStream* ps = static_cast<ProcessedMediaStream*>(stream);
  if (mStream) {
    // Setup our stream as an input to the AudioParam's stream
    MOZ_ASSERT(aOutput <= UINT16_MAX, "Unexpected large output port number");
    input->mStreamPort =
        ps->AllocateInputPort(mStream, AudioNodeStream::AUDIO_TRACK, TRACK_ANY,
                              0, static_cast<uint16_t>(aOutput));
  }
}

void AudioNode::SendDoubleParameterToStream(uint32_t aIndex, double aValue) {
  MOZ_ASSERT(mStream, "How come we don't have a stream here?");
  mStream->SetDoubleParameter(aIndex, aValue);
}

void AudioNode::SendInt32ParameterToStream(uint32_t aIndex, int32_t aValue) {
  MOZ_ASSERT(mStream, "How come we don't have a stream here?");
  mStream->SetInt32Parameter(aIndex, aValue);
}

void AudioNode::SendChannelMixingParametersToStream() {
  if (mStream) {
    mStream->SetChannelMixingParameters(mChannelCount, mChannelCountMode,
                                        mChannelInterpretation);
  }
}

template <>
bool AudioNode::DisconnectFromOutputIfConnected<AudioNode>(
    uint32_t aOutputNodeIndex, uint32_t aInputIndex) {
  WEB_AUDIO_API_LOG("%f: %s %u Disconnect()", Context()->CurrentTime(),
                    NodeType(), Id());

  AudioNode* destination = mOutputNodes[aOutputNodeIndex];

  MOZ_ASSERT(aOutputNodeIndex < mOutputNodes.Length());
  MOZ_ASSERT(aInputIndex < destination->InputNodes().Length());

  // An upstream node may be starting to play on the graph thread, and the
  // engine for a downstream node may be sending a PlayingRefChangeHandler
  // ADDREF message to this (main) thread.  Wait for a round trip before
  // releasing nodes, to give engines receiving sound now time to keep their
  // nodes alive.
  class RunnableRelease final : public Runnable {
   public:
    explicit RunnableRelease(already_AddRefed<AudioNode> aNode)
        : mozilla::Runnable("RunnableRelease"), mNode(aNode) {}

    NS_IMETHOD Run() override {
      mNode = nullptr;
      return NS_OK;
    }

   private:
    RefPtr<AudioNode> mNode;
  };

  InputNode& input = destination->mInputNodes[aInputIndex];
  if (input.mInputNode != this) {
    return false;
  }

  // Remove one instance of 'dest' from mOutputNodes. There could be
  // others, and it's not correct to remove them all since some of them
  // could be for different output ports.
  RefPtr<AudioNode> output = mOutputNodes[aOutputNodeIndex].forget();
  mOutputNodes.RemoveElementAt(aOutputNodeIndex);
  // Destroying the InputNode here sends a message to the graph thread
  // to disconnect the streams, which should be sent before the
  // RunAfterPendingUpdates() call below.
  destination->mInputNodes.RemoveElementAt(aInputIndex);
  output->NotifyInputsChanged();
  if (mStream) {
    nsCOMPtr<nsIRunnable> runnable = new RunnableRelease(output.forget());
    mStream->RunAfterPendingUpdates(runnable.forget());
  }
  return true;
}

template <>
bool AudioNode::DisconnectFromOutputIfConnected<AudioParam>(
    uint32_t aOutputParamIndex, uint32_t aInputIndex) {
  MOZ_ASSERT(aOutputParamIndex < mOutputParams.Length());

  AudioParam* destination = mOutputParams[aOutputParamIndex];

  MOZ_ASSERT(aInputIndex < destination->InputNodes().Length());

  const InputNode& input = destination->InputNodes()[aInputIndex];
  if (input.mInputNode != this) {
    return false;
  }
  destination->RemoveInputNode(aInputIndex);
  // Remove one instance of 'dest' from mOutputParams. There could be
  // others, and it's not correct to remove them all since some of them
  // could be for different output ports.
  mOutputParams.RemoveElementAt(aOutputParamIndex);
  return true;
}

template <>
const nsTArray<AudioNode::InputNode>&
AudioNode::InputsForDestination<AudioNode>(uint32_t aOutputNodeIndex) const {
  return mOutputNodes[aOutputNodeIndex]->InputNodes();
}

template <>
const nsTArray<AudioNode::InputNode>&
AudioNode::InputsForDestination<AudioParam>(uint32_t aOutputNodeIndex) const {
  return mOutputParams[aOutputNodeIndex]->InputNodes();
}

template <typename DestinationType, typename Predicate>
bool AudioNode::DisconnectMatchingDestinationInputs(uint32_t aDestinationIndex,
                                                    Predicate aPredicate) {
  bool wasConnected = false;
  uint32_t inputCount =
      InputsForDestination<DestinationType>(aDestinationIndex).Length();

  for (int32_t inputIndex = inputCount - 1; inputIndex >= 0; --inputIndex) {
    const InputNode& input =
        InputsForDestination<DestinationType>(aDestinationIndex)[inputIndex];
    if (aPredicate(input)) {
      if (DisconnectFromOutputIfConnected<DestinationType>(aDestinationIndex,
                                                           inputIndex)) {
        wasConnected = true;
        break;
      }
    }
  }
  return wasConnected;
}

void AudioNode::Disconnect(ErrorResult& aRv) {
  for (int32_t outputIndex = mOutputNodes.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    DisconnectMatchingDestinationInputs<AudioNode>(
        outputIndex, [](const InputNode&) { return true; });
  }

  for (int32_t outputIndex = mOutputParams.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    DisconnectMatchingDestinationInputs<AudioParam>(
        outputIndex, [](const InputNode&) { return true; });
  }
}

void AudioNode::Disconnect(uint32_t aOutput, ErrorResult& aRv) {
  if (aOutput >= NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  for (int32_t outputIndex = mOutputNodes.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    DisconnectMatchingDestinationInputs<AudioNode>(
        outputIndex, [aOutput](const InputNode& aInputNode) {
          return aInputNode.mOutputPort == aOutput;
        });
  }

  for (int32_t outputIndex = mOutputParams.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    DisconnectMatchingDestinationInputs<AudioParam>(
        outputIndex, [aOutput](const InputNode& aInputNode) {
          return aInputNode.mOutputPort == aOutput;
        });
  }
}

void AudioNode::Disconnect(AudioNode& aDestination, ErrorResult& aRv) {
  bool wasConnected = false;

  for (int32_t outputIndex = mOutputNodes.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    if (mOutputNodes[outputIndex] != &aDestination) {
      continue;
    }
    wasConnected |= DisconnectMatchingDestinationInputs<AudioNode>(
        outputIndex, [](const InputNode&) { return true; });
  }

  if (!wasConnected) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
}

void AudioNode::Disconnect(AudioNode& aDestination, uint32_t aOutput,
                           ErrorResult& aRv) {
  if (aOutput >= NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  bool wasConnected = false;

  for (int32_t outputIndex = mOutputNodes.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    if (mOutputNodes[outputIndex] != &aDestination) {
      continue;
    }
    wasConnected |= DisconnectMatchingDestinationInputs<AudioNode>(
        outputIndex, [aOutput](const InputNode& aInputNode) {
          return aInputNode.mOutputPort == aOutput;
        });
  }

  if (!wasConnected) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
}

void AudioNode::Disconnect(AudioNode& aDestination, uint32_t aOutput,
                           uint32_t aInput, ErrorResult& aRv) {
  if (aOutput >= NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (aInput >= aDestination.NumberOfInputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  bool wasConnected = false;

  for (int32_t outputIndex = mOutputNodes.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    if (mOutputNodes[outputIndex] != &aDestination) {
      continue;
    }
    wasConnected |= DisconnectMatchingDestinationInputs<AudioNode>(
        outputIndex, [aOutput, aInput](const InputNode& aInputNode) {
          return aInputNode.mOutputPort == aOutput &&
                 aInputNode.mInputPort == aInput;
        });
  }

  if (!wasConnected) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
}

void AudioNode::Disconnect(AudioParam& aDestination, ErrorResult& aRv) {
  bool wasConnected = false;

  for (int32_t outputIndex = mOutputParams.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    if (mOutputParams[outputIndex] != &aDestination) {
      continue;
    }
    wasConnected |= DisconnectMatchingDestinationInputs<AudioParam>(
        outputIndex, [](const InputNode&) { return true; });
  }

  if (!wasConnected) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
}

void AudioNode::Disconnect(AudioParam& aDestination, uint32_t aOutput,
                           ErrorResult& aRv) {
  if (aOutput >= NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  bool wasConnected = false;

  for (int32_t outputIndex = mOutputParams.Length() - 1; outputIndex >= 0;
       --outputIndex) {
    if (mOutputParams[outputIndex] != &aDestination) {
      continue;
    }
    wasConnected |= DisconnectMatchingDestinationInputs<AudioParam>(
        outputIndex, [aOutput](const InputNode& aInputNode) {
          return aInputNode.mOutputPort == aOutput;
        });
  }

  if (!wasConnected) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
}

void AudioNode::DestroyMediaStream() {
  if (mStream) {
    // Remove the node pointer on the engine.
    AudioNodeStream* ns = mStream;
    MOZ_ASSERT(ns, "How come we don't have a stream here?");
    MOZ_ASSERT(ns->Engine()->NodeMainThread() == this,
               "Invalid node reference");
    ns->Engine()->ClearNode();

    mStream->Destroy();
    mStream = nullptr;

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      nsAutoString id;
      id.AppendPrintf("%u", mId);
      obs->NotifyObservers(nullptr, "webaudio-node-demise", id.get());
    }
  }
}

void AudioNode::RemoveOutputParam(AudioParam* aParam) {
  mOutputParams.RemoveElement(aParam);
}

bool AudioNode::PassThrough() const {
  MOZ_ASSERT(NumberOfInputs() <= 1 && NumberOfOutputs() == 1);
  return mPassThrough;
}

void AudioNode::SetPassThrough(bool aPassThrough) {
  MOZ_ASSERT(NumberOfInputs() <= 1 && NumberOfOutputs() == 1);
  mPassThrough = aPassThrough;
  if (mStream) {
    mStream->SetPassThrough(mPassThrough);
  }
}

void AudioNode::CreateAudioParam(RefPtr<AudioParam>& aParam, uint32_t aIndex,
                                 const char* aName, float aDefaultValue,
                                 float aMinValue, float aMaxValue) {
  aParam =
      new AudioParam(this, aIndex, aName, aDefaultValue, aMinValue, aMaxValue);
  mParams.AppendElement(aParam);
}

}  // namespace dom
}  // namespace mozilla
