/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChannelMergerNode.h"
#include "mozilla/dom/ChannelMergerNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "mozilla/PodOperations.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(ChannelMergerNode, AudioNode)

class ChannelMergerNodeEngine : public AudioNodeEngine
{
public:
  ChannelMergerNodeEngine(ChannelMergerNode* aNode)
    : AudioNodeEngine(aNode)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual void ProduceAudioBlocksOnPorts(AudioNodeStream* aStream,
                                         const OutputChunks& aInput,
                                         OutputChunks& aOutput,
                                         bool* aFinished) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aInput.Length() >= 1, "Should have one or more input ports");

    // Get the number of output channels, and allocate it
    uint32_t channelCount = 0;
    for (uint16_t i = 0; i < InputCount(); ++i) {
      channelCount += aInput[i].mChannelData.Length();
    }
    if (channelCount == 0) {
      aOutput[0].SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }
    AllocateAudioBlock(channelCount, &aOutput[0]);

    // Append each channel in each input to the output
    uint32_t channelIndex = 0;
    for (uint16_t i = 0; i < InputCount(); ++i) {
      for (uint32_t j = 0; j < aInput[i].mChannelData.Length(); ++j) {
        AudioBlockCopyChannelWithScale(
            static_cast<const float*>(aInput[i].mChannelData[j]),
            aInput[i].mVolume,
            static_cast<float*>(const_cast<void*>(aOutput[0].mChannelData[channelIndex])));
        ++channelIndex;
      }
    }
  }
};

ChannelMergerNode::ChannelMergerNode(AudioContext* aContext,
                                     uint16_t aInputCount)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mInputCount(aInputCount)
{
  mStream = aContext->Graph()->CreateAudioNodeStream(new ChannelMergerNodeEngine(this),
                                                     MediaStreamGraph::INTERNAL_STREAM);
}

JSObject*
ChannelMergerNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return ChannelMergerNodeBinding::Wrap(aCx, aScope, this);
}

}
}

