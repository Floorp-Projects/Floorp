/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioNode_h_
#define AudioNode_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/AudioNodeBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "AudioContext.h"
#include "MediaTrackGraph.h"
#include "WebAudioUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MemoryReporting.h"
#include "nsPrintfCString.h"
#include "nsWeakReference.h"
#include "SelfRef.h"

namespace mozilla {

class AbstractThread;

namespace dom {

class AudioContext;
class AudioBufferSourceNode;
class AudioParam;
class AudioParamTimeline;
struct ThreeDPoint;

/**
 * The DOM object representing a Web Audio AudioNode.
 *
 * Each AudioNode has a MediaTrack representing the actual
 * real-time processing and output of this AudioNode.
 *
 * We track the incoming and outgoing connections to other AudioNodes.
 * Outgoing connections have strong ownership.  Also, AudioNodes that will
 * produce sound on their output even when they have silent or no input ask
 * the AudioContext to keep playing or tail-time references to keep them alive
 * until the context is finished.
 *
 * Explicit disconnections will only remove references from output nodes after
 * the graph is notified and the main thread receives a reply.  Similarly,
 * nodes with playing or tail-time references release these references only
 * after receiving notification from their engine on the graph thread that
 * playing has stopped.  Engines notifying the main thread that they have
 * finished do so strictly *after* producing and returning their last block.
 * In this way, an engine that receives non-null input knows that the input
 * comes from nodes that are still alive and will keep their output nodes
 * alive for at least as long as it takes to process messages from the graph
 * thread.  i.e. the engine receiving non-null input knows that its node is
 * still alive, and will still be alive when it receives a message from the
 * engine.
 */
class AudioNode : public DOMEventTargetHelper, public nsSupportsWeakReference {
 protected:
  // You can only use refcounting to delete this object
  virtual ~AudioNode();

 public:
  AudioNode(AudioContext* aContext, uint32_t aChannelCount,
            ChannelCountMode aChannelCountMode,
            ChannelInterpretation aChannelInterpretation);

  // This should be idempotent (safe to call multiple times).
  virtual void DestroyMediaTrack();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioNode, DOMEventTargetHelper)

  virtual AudioBufferSourceNode* AsAudioBufferSourceNode() { return nullptr; }

  AudioContext* GetParentObject() const { return mContext; }

  AudioContext* Context() const { return mContext; }

  virtual AudioNode* Connect(AudioNode& aDestination, uint32_t aOutput,
                             uint32_t aInput, ErrorResult& aRv);

  virtual void Connect(AudioParam& aDestination, uint32_t aOutput,
                       ErrorResult& aRv);

  virtual void Disconnect(ErrorResult& aRv);
  virtual void Disconnect(uint32_t aOutput, ErrorResult& aRv);
  virtual void Disconnect(AudioNode& aDestination, ErrorResult& aRv);
  virtual void Disconnect(AudioNode& aDestination, uint32_t aOutput,
                          ErrorResult& aRv);
  virtual void Disconnect(AudioNode& aDestination, uint32_t aOutput,
                          uint32_t aInput, ErrorResult& aRv);
  virtual void Disconnect(AudioParam& aDestination, ErrorResult& aRv);
  virtual void Disconnect(AudioParam& aDestination, uint32_t aOutput,
                          ErrorResult& aRv);

  // Called after input nodes have been explicitly added or removed through
  // the Connect() or Disconnect() methods.
  virtual void NotifyInputsChanged() {}
  // Indicate that the node should continue indefinitely to behave as if an
  // input is connected, even though there is no longer a corresponding entry
  // in mInputNodes.  Called after an input node has been removed because it
  // is being garbage collected.
  virtual void NotifyHasPhantomInput() {}

  // The following two virtual methods must be implemented by each node type
  // to provide their number of input and output ports. These numbers are
  // constant for the lifetime of the node. Both default to 1.
  virtual uint16_t NumberOfInputs() const { return 1; }
  virtual uint16_t NumberOfOutputs() const { return 1; }

  uint32_t Id() const { return mId; }

  bool PassThrough() const;
  void SetPassThrough(bool aPassThrough);

  uint32_t ChannelCount() const { return mChannelCount; }
  virtual void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) {
    if (aChannelCount == 0 || aChannelCount > WebAudioUtils::MaxChannelCount) {
      aRv.ThrowNotSupportedError(
          nsPrintfCString("Channel count (%u) must be in the range [1, max "
                          "supported channel count]",
                          aChannelCount));
      return;
    }
    mChannelCount = aChannelCount;
    SendChannelMixingParametersToTrack();
  }
  ChannelCountMode ChannelCountModeValue() const { return mChannelCountMode; }
  virtual void SetChannelCountModeValue(ChannelCountMode aMode,
                                        ErrorResult& aRv) {
    mChannelCountMode = aMode;
    SendChannelMixingParametersToTrack();
  }
  ChannelInterpretation ChannelInterpretationValue() const {
    return mChannelInterpretation;
  }
  virtual void SetChannelInterpretationValue(ChannelInterpretation aMode,
                                             ErrorResult& aRv) {
    mChannelInterpretation = aMode;
    SendChannelMixingParametersToTrack();
  }

  struct InputNode final {
    InputNode() = default;
    InputNode(const InputNode&) = delete;
    InputNode(InputNode&&) = default;

    ~InputNode() {
      if (mTrackPort) {
        mTrackPort->Destroy();
      }
    }

    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
      size_t amount = 0;
      if (mTrackPort) {
        amount += mTrackPort->SizeOfIncludingThis(aMallocSizeOf);
      }

      return amount;
    }

    // The InputNode is destroyed when mInputNode is disconnected.
    AudioNode* MOZ_NON_OWNING_REF mInputNode;
    RefPtr<MediaInputPort> mTrackPort;
    // The index of the input port this node feeds into.
    // This is not used for connections to AudioParams.
    uint32_t mInputPort;
    // The index of the output port this node comes out of.
    uint32_t mOutputPort;
  };

  // Returns the track, if any.
  AudioNodeTrack* GetTrack() const { return mTrack; }

  const nsTArray<InputNode>& InputNodes() const { return mInputNodes; }
  const nsTArray<RefPtr<AudioNode>>& OutputNodes() const {
    return mOutputNodes;
  }
  const nsTArray<RefPtr<AudioParam>>& OutputParams() const {
    return mOutputParams;
  }

  template <typename T>
  const nsTArray<InputNode>& InputsForDestination(uint32_t aOutputIndex) const;

  void RemoveOutputParam(AudioParam* aParam);

  // MarkActive() asks the context to keep the AudioNode alive until the
  // context is finished.  This takes care of "playing" references and
  // "tail-time" references.
  void MarkActive() { Context()->RegisterActiveNode(this); }
  // Active nodes call MarkInactive() when they have finished producing sound
  // for the foreseeable future.
  // Do not call MarkInactive from a node destructor.  If the destructor is
  // called, then the node is already inactive.
  // MarkInactive() may delete |this|.
  void MarkInactive() { Context()->UnregisterActiveNode(this); }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // Returns a string from constant static storage identifying the dom node
  // type.
  virtual const char* NodeType() const = 0;

  // This can return nullptr, but only when the AudioNode has been created
  // during document shutdown.
  AbstractThread* GetAbstractMainThread() const { return mAbstractMainThread; }

  const nsTArray<RefPtr<AudioParam>>& GetAudioParams() const { return mParams; }

 private:
  // Given:
  //
  // - a DestinationType, that can be an AudioNode or an AudioParam ;
  // - a Predicate, a function that takes an InputNode& and returns a bool ;
  //
  // This method iterates on the InputNodes() of the node at the index
  // aDestinationIndex, and calls `DisconnectFromOutputIfConnected` with this
  // input node, if aPredicate returns true.
  template <typename DestinationType, typename Predicate>
  bool DisconnectMatchingDestinationInputs(uint32_t aDestinationIndex,
                                           Predicate aPredicate);

  virtual void LastRelease() override {
    // We are about to be deleted, disconnect the object from the graph before
    // the derived type is destroyed.
    DisconnectFromGraph();
  }
  // Callers must hold a reference to 'this'.
  void DisconnectFromGraph();

  template <typename DestinationType>
  bool DisconnectFromOutputIfConnected(uint32_t aOutputIndex,
                                       uint32_t aInputIndex);

 protected:
  // Helper for the Constructors for nodes.
  void Initialize(const AudioNodeOptions& aOptions, ErrorResult& aRv);

  // Helpers for sending different value types to tracks
  void SendDoubleParameterToTrack(uint32_t aIndex, double aValue);
  void SendInt32ParameterToTrack(uint32_t aIndex, int32_t aValue);
  void SendChannelMixingParametersToTrack();

 private:
  RefPtr<AudioContext> mContext;

 protected:
  // Set in the constructor of all nodes except offline AudioDestinationNode.
  // Must not become null until finished.
  RefPtr<AudioNodeTrack> mTrack;

  // The reference pointing out all audio params which belong to this node.
  nsTArray<RefPtr<AudioParam>> mParams;
  // Use this function to create an AudioParam, so as to automatically add
  // the new AudioParam to `mParams`.
  AudioParam* CreateAudioParam(
      uint32_t aIndex, const nsAString& aName, float aDefaultValue,
      float aMinValue = std::numeric_limits<float>::lowest(),
      float aMaxValue = std::numeric_limits<float>::max());

 private:
  // For every InputNode, there is a corresponding entry in mOutputNodes of the
  // InputNode's mInputNode.
  nsTArray<InputNode> mInputNodes;
  // For every mOutputNode entry, there is a corresponding entry in mInputNodes
  // of the mOutputNode entry. We won't necessarily be able to identify the
  // exact matching entry, since mOutputNodes doesn't include the port
  // identifiers and the same node could be connected on multiple ports.
  nsTArray<RefPtr<AudioNode>> mOutputNodes;
  // For every mOutputParams entry, there is a corresponding entry in
  // AudioParam::mInputNodes of the mOutputParams entry. We won't necessarily be
  // able to identify the exact matching entry, since mOutputParams doesn't
  // include the port identifiers and the same node could be connected on
  // multiple ports.
  nsTArray<RefPtr<AudioParam>> mOutputParams;
  uint32_t mChannelCount;
  ChannelCountMode mChannelCountMode;
  ChannelInterpretation mChannelInterpretation;
  const uint32_t mId;
  // Whether the node just passes through its input.  This is a devtools API
  // that only works for some node types.
  bool mPassThrough;
  // DocGroup-specifc AbstractThread::MainThread() for MediaTrackGraph
  // operations.
  const RefPtr<AbstractThread> mAbstractMainThread;
};

}  // namespace dom
}  // namespace mozilla

#endif
