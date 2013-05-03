/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioNode_h_
#define AudioNode_h_

#include "nsDOMEventTargetHelper.h"
#include "mozilla/dom/AudioNodeBinding.h"
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
 * All connections are strong and thus rely on cycle collection to break them.
 * However, we also track whether an AudioNode is capable of producing output
 * in the future. If it isn't, then we break its connections to its inputs
 * and outputs, allowing nodes to be immediately disconnected. This
 * disconnection is done internally, invisible to DOM users.
 */
class AudioNode : public nsDOMEventTargetHelper,
                  public EnableWebAudioCheck
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
                                           nsDOMEventTargetHelper)

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

  virtual void Connect(AudioNode& aDestination, uint32_t aOutput,
                       uint32_t aInput, ErrorResult& aRv);

  virtual void Disconnect(uint32_t aOutput, ErrorResult& aRv);

  // The following two virtual methods must be implemented by each node type
  // to provide their number of input and output ports. These numbers are
  // constant for the lifetime of the node. Both default to 1.
  virtual uint32_t NumberOfInputs() const { return 1; }
  virtual uint32_t NumberOfOutputs() const { return 1; }

  uint32_t ChannelCount() const { return mChannelCount; }
  void SetChannelCount(uint32_t aChannelCount)
  {
    mChannelCount = aChannelCount;
    SendChannelMixingParametersToStream();
  }
  ChannelCountMode ChannelCountModeValue() const
  {
    return mChannelCountMode;
  }
  void SetChannelCountModeValue(ChannelCountMode aMode)
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
    uint32_t mInputPort;
    // The index of the output port this node comes out of.
    uint32_t mOutputPort;
  };

  MediaStream* Stream() { return mStream; }

  const nsTArray<InputNode>& InputNodes() const
  {
    return mInputNodes;
  }

private:
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
  uint32_t mChannelCount;
  ChannelCountMode mChannelCountMode;
  ChannelInterpretation mChannelInterpretation;
};

}
}

#endif
