/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioNode_h_
#define AudioNode_h_

#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "EnableWebAudioCheck.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "AudioContext.h"
#include "AudioParamTimeline.h"
#include "MediaStreamGraph.h"

struct JSContext;

namespace mozilla {

class ErrorResult;

namespace dom {

struct ThreeDPoint;

/**
 * The DOM object representing a Web Audio AudioNode.
 *
 * Each AudioNode has a MediaStream representing the actual
 * real-time processing and output of this AudioNode.
 *
 * We track the incoming and outgoing connections to other AudioNodes.
 * All connections are strong and thus rely on cycle collection to break them.
 * However, we also track whether an AudioNode is capable of producing output
 * in the future. If it isn't, then we break its connections to its inputs
 * and outputs, allowing nodes to be immediately disconnected. This
 * disconnection is done internally, invisible to DOM users.
 *
 * We say that a node cannot produce output in the future if it has no inputs
 * that can, and it is not producing output itself without any inputs, and
 * either it can never have any inputs or it has no JS wrapper. (If it has a
 * JS wrapper and can accept inputs, then a new input could be added in
 * the future.)
 */
class AudioNode : public nsISupports,
                  public EnableWebAudioCheck
{
public:
  explicit AudioNode(AudioContext* aContext);
  virtual ~AudioNode();

  // This should be idempotent (safe to call multiple times).
  // This should be called in the destructor of every class that overrides
  // this method.
  virtual void DestroyMediaStream()
  {
    if (mStream) {
      mStream->Destroy();
      mStream = nullptr;
    }
  }

  // This method should be overridden to return true in nodes
  // which support being hooked up to the Media Stream graph.
  virtual bool SupportsMediaStreams() const
  {
    return false;
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AudioNode)

  void JSBindingFinalized()
  {
    NS_ASSERTION(!mJSBindingFinalized, "JS binding already finalized");
    mJSBindingFinalized = true;
    UpdateOutputEnded();
  }

  virtual AudioBufferSourceNode* AsAudioBufferSourceNode() {
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

  void Connect(AudioNode& aDestination, uint32_t aOutput,
               uint32_t aInput, ErrorResult& aRv);

  void Disconnect(uint32_t aOutput, ErrorResult& aRv);

  // The following two virtual methods must be implemented by each node type
  // to provide their number of input and output ports. These numbers are
  // constant for the lifetime of the node. Both default to 1.
  virtual uint32_t NumberOfInputs() const { return 1; }
  virtual uint32_t NumberOfOutputs() const { return 1; }

  // This could possibly delete 'this'.
  void UpdateOutputEnded();
  bool IsOutputEnded() const { return mOutputEnded; }

  struct InputNode {
    ~InputNode()
    {
      if (mStreamPort) {
        mStreamPort->Destroy();
      }
    }

    // Strong reference.
    // May be null if the source node has gone away.
    nsRefPtr<AudioNode> mInputNode;
    nsRefPtr<MediaInputPort> mStreamPort;
    // The index of the input port this node feeds into.
    uint32_t mInputPort;
    // The index of the output port this node comes out of.
    uint32_t mOutputPort;
  };

  MediaStream* Stream() { return mStream; }

  // Set this to true when the node can produce its own output even if there
  // are no inputs.
  void SetProduceOwnOutput(bool aCanProduceOwnOutput)
  {
    mCanProduceOwnOutput = aCanProduceOwnOutput;
    if (!aCanProduceOwnOutput) {
      UpdateOutputEnded();
    }
  }

  const nsTArray<InputNode>& InputNodes() const
  {
    return mInputNodes;
  }

protected:
  static void Callback(AudioNode* aNode) { /* not implemented */ }

  // Helpers for sending different value types to streams
  void SendDoubleParameterToStream(uint32_t aIndex, double aValue);
  void SendInt32ParameterToStream(uint32_t aIndex, int32_t aValue);
  void SendThreeDPointParameterToStream(uint32_t aIndex, const ThreeDPoint& aValue);
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
  // True if the JS binding has been finalized (so script no longer has
  // a reference to this node).
  bool mJSBindingFinalized;
  // True if this node can produce its own output even when all inputs
  // have ended their output.
  bool mCanProduceOwnOutput;
  // True if this node can never produce anything except silence in the future.
  // Updated by UpdateOutputEnded().
  bool mOutputEnded;
};

}
}

#endif
