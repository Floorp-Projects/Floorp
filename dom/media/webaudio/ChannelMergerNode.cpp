/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChannelMergerNode.h"
#include "mozilla/dom/ChannelMergerNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(ChannelMergerNode, AudioNode)

class ChannelMergerNodeEngine final : public AudioNodeEngine
{
public:
  explicit ChannelMergerNodeEngine(ChannelMergerNode* aNode)
    : AudioNodeEngine(aNode)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual void ProcessBlocksOnPorts(AudioNodeStream* aStream,
                                    const OutputChunks& aInput,
                                    OutputChunks& aOutput,
                                    bool* aFinished) override
  {
    MOZ_ASSERT(aInput.Length() >= 1, "Should have one or more input ports");

    // Get the number of output channels, and allocate it
    size_t channelCount = 0;
    for (uint16_t i = 0; i < InputCount(); ++i) {
      channelCount += aInput[i].ChannelCount();
    }
    if (channelCount == 0) {
      aOutput[0].SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }
    channelCount = std::min(channelCount, WebAudioUtils::MaxChannelCount);
    aOutput[0].AllocateChannels(channelCount);

    // Append each channel in each input to the output
    size_t channelIndex = 0;
    for (uint16_t i = 0; true; ++i) {
      MOZ_ASSERT(i < InputCount());
      for (size_t j = 0; j < aInput[i].ChannelCount(); ++j) {
        AudioBlockCopyChannelWithScale(
            static_cast<const float*>(aInput[i].mChannelData[j]),
            aInput[i].mVolume,
            aOutput[0].ChannelFloatsForWrite(channelIndex));
        ++channelIndex;
        if (channelIndex >= channelCount) {
          return;
        }
      }
    }
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
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
  mStream = AudioNodeStream::Create(aContext->Graph(),
                                    new ChannelMergerNodeEngine(this),
                                    AudioNodeStream::NO_STREAM_FLAGS);
}

ChannelMergerNode::~ChannelMergerNode()
{
}

JSObject*
ChannelMergerNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ChannelMergerNodeBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

