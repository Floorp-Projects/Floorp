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
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "AudioContext.h"
#include "MediaStreamGraph.h"
#include "WebAudioUtils.h"

namespace mozilla {

namespace dom {

class AudioContext;
class AudioBufferSourceNode;
class AudioParam;
class AudioParamTimeline;
struct ThreeDPoint;

template<class T>
class SelfReference {
public:
  SelfReference() : mHeld(false) {}
  ~SelfReference()
  {
    NS_ASSERTION(!mHeld, "Forgot to drop the self reference?");
  }

  void Take(T* t)
  {
    if (!mHeld) {
      mHeld = true;
      t->AddRef();
    }
  }
  void Drop(T* t)
  {
    if (mHeld) {
      mHeld = false;
      t->Release();
    }
  }

  operator bool() const { return mHeld; }

private:
  bool mHeld;
};

/**
 * The DOM object representing a Web Audio AudioNode.
 *
 * Each AudioNode has a MediaStream representing the actual
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
class AudioNode : public DOMEventTargetHelper
{
protected:
  // You can only use refcounting to delete this object
  virtual ~AudioNode();

public:
  AudioNode(AudioContext* aContext,
            uint32_t aChannelCount,
            ChannelCountMode aChannelCountMode,
            ChannelInterpretation aChannelInterpretation);

  // This should be idempotent (safe to call multiple times).
  virtual void DestroyMediaStream();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioNode,
                                           DOMEventTargetHelper)

  virtual AudioBufferSourceNode* AsAudioBufferSourceNode() {
    return nullptr;
  }

  virtual const DelayNode* AsDelayNode() const {
    return nullptr;
  }

  AudioContext* GetParentObject() const
  {
    return mContext;
  }

  AudioContext* Context() const
  {
    return mContext;
  }

  virtual void Connect(AudioNode& aDestination, uint32_t aOutput,
                       uint32_t aInput, ErrorResult& aRv);

  virtual void Connect(AudioParam& aDestination, uint32_t aOutput,
                       ErrorResult& aRv);

  virtual void Disconnect(uint32_t aOutput, ErrorResult& aRv);

  // The following two virtual methods must be implemented by each node type
  // to provide their number of input and output ports. These numbers are
  // constant for the lifetime of the node. Both default to 1.
  virtual uint16_t NumberOfInputs() const { return 1; }
  virtual uint16_t NumberOfOutputs() const { return 1; }

  uint32_t ChannelCount() const { return mChannelCount; }
  virtual void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv)
  {
    if (aChannelCount == 0 ||
        aChannelCount > WebAudioUtils::MaxChannelCount) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    mChannelCount = aChannelCount;
    SendChannelMixingParametersToStream();
  }
  ChannelCountMode ChannelCountModeValue() const
  {
    return mChannelCountMode;
  }
  virtual void SetChannelCountModeValue(ChannelCountMode aMode, ErrorResult& aRv)
  {
    mChannelCountMode = aMode;
    SendChannelMixingParametersToStream();
  }
  ChannelInterpretation ChannelInterpretationValue() const
  {
    return mChannelInterpretation;
  }
  void SetChannelInterpretationValue(ChannelInterpretation aMode)
  {
    mChannelInterpretation = aMode;
    SendChannelMixingParametersToStream();
  }

  struct InputNode {
    ~InputNode()
    {
      if (mStreamPort) {
        mStreamPort->Destroy();
      }
    }

    // Weak reference.
    AudioNode* mInputNode;
    nsRefPtr<MediaInputPort> mStreamPort;
    // The index of the input port this node feeds into.
    // This is not used for connections to AudioParams.
    uint32_t mInputPort;
    // The index of the output port this node comes out of.
    uint32_t mOutputPort;
  };

  MediaStream* Stream() { return mStream; }

  const nsTArray<InputNode>& InputNodes() const
  {
    return mInputNodes;
  }
  const nsTArray<nsRefPtr<AudioNode> >& OutputNodes() const
  {
    return mOutputNodes;
  }
  const nsTArray<nsRefPtr<AudioParam> >& OutputParams() const
  {
    return mOutputParams;
  }

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

private:
  friend class AudioBufferSourceNode;
  // This could possibly delete 'this'.
  void DisconnectFromGraph();

protected:
  static void Callback(AudioNode* aNode) { /* not implemented */ }

  // Helpers for sending different value types to streams
  void SendDoubleParameterToStream(uint32_t aIndex, double aValue);
  void SendInt32ParameterToStream(uint32_t aIndex, int32_t aValue);
  void SendThreeDPointParameterToStream(uint32_t aIndex, const ThreeDPoint& aValue);
  void SendChannelMixingParametersToStream();
  static void SendTimelineParameterToStream(AudioNode* aNode, uint32_t aIndex,
                                            const AudioParamTimeline& aValue);

private:
  nsRefPtr<AudioContext> mContext;

protected:
  // Must be set in the constructor. Must not be null.
  // If MaxNumberOfInputs() is > 0, then mStream must be a ProcessedMediaStream.
  nsRefPtr<MediaStream> mStream;

private:
  // For every InputNode, there is a corresponding entry in mOutputNodes of the
  // InputNode's mInputNode.
  nsTArray<InputNode> mInputNodes;
  // For every mOutputNode entry, there is a corresponding entry in mInputNodes
  // of the mOutputNode entry. We won't necessarily be able to identify the
  // exact matching entry, since mOutputNodes doesn't include the port
  // identifiers and the same node could be connected on multiple ports.
  nsTArray<nsRefPtr<AudioNode> > mOutputNodes;
  // For every mOutputParams entry, there is a corresponding entry in
  // AudioParam::mInputNodes of the mOutputParams entry. We won't necessarily be
  // able to identify the exact matching entry, since mOutputParams doesn't
  // include the port identifiers and the same node could be connected on
  // multiple ports.
  nsTArray<nsRefPtr<AudioParam> > mOutputParams;
  uint32_t mChannelCount;
  ChannelCountMode mChannelCountMode;
  ChannelInterpretation mChannelInterpretation;
};

}
}

#endif
