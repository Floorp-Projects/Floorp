/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChannelSplitterNode.h"
#include "mozilla/dom/ChannelSplitterNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(ChannelSplitterNode, AudioNode)

class ChannelSplitterNodeEngine : public AudioNodeEngine
{
public:
  ChannelSplitterNodeEngine(ChannelSplitterNode* aNode)
    : AudioNodeEngine(aNode)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual void ProcessBlocksOnPorts(AudioNodeStream* aStream,
                                    const OutputChunks& aInput,
                                    OutputChunks& aOutput,
                                    bool* aFinished) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aInput.Length() == 1, "Should only have one input port");

    aOutput.SetLength(OutputCount());
    for (uint16_t i = 0; i < OutputCount(); ++i) {
      if (i < aInput[0].mChannelData.Length()) {
        // Split out existing channels
        AllocateAudioBlock(1, &aOutput[i]);
        AudioBlockCopyChannelWithScale(
            static_cast<const float*>(aInput[0].mChannelData[i]),
            aInput[0].mVolume,
            static_cast<float*>(const_cast<void*>(aOutput[i].mChannelData[0])));
      } else {
        // Pad with silent channels if needed
        aOutput[i].SetNull(WEBAUDIO_BLOCK_SIZE);
      }
    }
  }
};

ChannelSplitterNode::ChannelSplitterNode(AudioContext* aContext,
                                         uint16_t aOutputCount)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mOutputCount(aOutputCount)
{
  mStream = aContext->Graph()->CreateAudioNodeStream(new ChannelSplitterNodeEngine(this),
                                                     MediaStreamGraph::INTERNAL_STREAM);
}

JSObject*
ChannelSplitterNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return ChannelSplitterNodeBinding::Wrap(aCx, aScope, this);
}

}
}

