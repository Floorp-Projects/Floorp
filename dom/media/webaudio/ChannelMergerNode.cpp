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

class ChannelMergerNodeEngine final : public AudioNodeEngine {
 public:
  explicit ChannelMergerNodeEngine(ChannelMergerNode* aNode)
      : AudioNodeEngine(aNode) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void ProcessBlocksOnPorts(AudioNodeStream* aStream,
                            const OutputChunks& aInput, OutputChunks& aOutput,
                            bool* aFinished) override {
    MOZ_ASSERT(aInput.Length() >= 1, "Should have one or more input ports");

    // Get the number of output channels, and allocate it
    size_t channelCount = InputCount();
    bool allNull = true;
    for (size_t i = 0; i < channelCount; ++i) {
      allNull &= aInput[i].IsNull();
    }
    if (allNull) {
      aOutput[0].SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    aOutput[0].AllocateChannels(channelCount);

    for (size_t i = 0; i < channelCount; ++i) {
      float* output = aOutput[0].ChannelFloatsForWrite(i);
      if (aInput[i].IsNull()) {
        PodZero(output, WEBAUDIO_BLOCK_SIZE);
      } else {
        AudioBlockCopyChannelWithScale(
            static_cast<const float*>(aInput[i].mChannelData[0]),
            aInput[i].mVolume, output);
      }
    }
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

ChannelMergerNode::ChannelMergerNode(AudioContext* aContext,
                                     uint16_t aInputCount)
    : AudioNode(aContext, 1, ChannelCountMode::Explicit,
                ChannelInterpretation::Speakers),
      mInputCount(aInputCount) {
  mStream = AudioNodeStream::Create(aContext, new ChannelMergerNodeEngine(this),
                                    AudioNodeStream::NO_STREAM_FLAGS,
                                    aContext->Graph());
}

/* static */
already_AddRefed<ChannelMergerNode> ChannelMergerNode::Create(
    AudioContext& aAudioContext, const ChannelMergerOptions& aOptions,
    ErrorResult& aRv) {
  if (aOptions.mNumberOfInputs == 0 ||
      aOptions.mNumberOfInputs > WebAudioUtils::MaxChannelCount) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<ChannelMergerNode> audioNode =
      new ChannelMergerNode(&aAudioContext, aOptions.mNumberOfInputs);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return audioNode.forget();
}

JSObject* ChannelMergerNode::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return ChannelMergerNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
