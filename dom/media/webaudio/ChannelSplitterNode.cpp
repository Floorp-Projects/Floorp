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

class ChannelSplitterNodeEngine final : public AudioNodeEngine
{
public:
  explicit ChannelSplitterNodeEngine(ChannelSplitterNode* aNode)
    : AudioNodeEngine(aNode)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void ProcessBlocksOnPorts(AudioNodeStream* aStream,
                            const OutputChunks& aInput,
                            OutputChunks& aOutput,
                            bool* aFinished) override
  {
    MOZ_ASSERT(aInput.Length() == 1, "Should only have one input port");

    aOutput.SetLength(OutputCount());
    for (uint16_t i = 0; i < OutputCount(); ++i) {
      if (i < aInput[0].ChannelCount()) {
        // Split out existing channels
        aOutput[i].AllocateChannels(1);
        AudioBlockCopyChannelWithScale(
            static_cast<const float*>(aInput[0].mChannelData[i]),
            aInput[0].mVolume,
            aOutput[i].ChannelFloatsForWrite(0));
      } else {
        // Pad with silent channels if needed
        aOutput[i].SetNull(WEBAUDIO_BLOCK_SIZE);
      }
    }
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

ChannelSplitterNode::ChannelSplitterNode(AudioContext* aContext,
                                         uint16_t aOutputCount)
  : AudioNode(aContext,
              aOutputCount,
              ChannelCountMode::Explicit,
              ChannelInterpretation::Discrete)
  , mOutputCount(aOutputCount)
{
  mStream = AudioNodeStream::Create(aContext,
                                    new ChannelSplitterNodeEngine(this),
                                    AudioNodeStream::NO_STREAM_FLAGS,
                                    aContext->Graph());
}

/* static */ already_AddRefed<ChannelSplitterNode>
ChannelSplitterNode::Create(AudioContext& aAudioContext,
                            const ChannelSplitterOptions& aOptions,
                            ErrorResult& aRv)
{
  if (aAudioContext.CheckClosed(aRv)) {
    return nullptr;
  }

  if (aOptions.mNumberOfOutputs == 0 ||
      aOptions.mNumberOfOutputs > WebAudioUtils::MaxChannelCount) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<ChannelSplitterNode> audioNode =
    new ChannelSplitterNode(&aAudioContext, aOptions.mNumberOfOutputs);

  // Manually check that the other options are valid, this node has channelCount,
  // channelCountMode and channelInterpretation constraints: they cannot be
  // changed from the default.
  if (aOptions.mChannelCount.WasPassed() &&
      aOptions.mChannelCount.Value() != audioNode->ChannelCount()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  if (aOptions.mChannelInterpretation.WasPassed() &&
      aOptions.mChannelInterpretation.Value() != audioNode->ChannelInterpretationValue()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  if (aOptions.mChannelCountMode.WasPassed() &&
      aOptions.mChannelCountMode.Value() != audioNode->ChannelCountModeValue()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return audioNode.forget();
}

JSObject*
ChannelSplitterNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ChannelSplitterNodeBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
