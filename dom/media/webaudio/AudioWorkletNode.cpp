/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioWorkletNode.h"

#include "AudioParamMap.h"
#include "mozilla/dom/AudioWorkletNodeBinding.h"
#include "mozilla/dom/MessagePort.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(AudioWorkletNode, AudioNode)

class WorkletNodeEngine final : public AudioNodeEngine {
 public:
  explicit WorkletNodeEngine(AudioWorkletNode* aNode)
      : AudioNodeEngine(aNode) {}

  MOZ_CAN_RUN_SCRIPT
  void ConstructProcessor(
      AudioWorkletImpl* aWorkletImpl, const nsAString& aName,
      NotNull<StructuredCloneHolder*> aOptionsSerialization);

  void ProcessBlock(AudioNodeStream* aStream, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    ProcessBlocksOnPorts(aStream, MakeSpan(&aInput, 1), MakeSpan(aOutput, 1),
                         aFinished);
  }

  void NotifyForcedShutdown() override { ReleaseJSResources(); }

 private:
  void SendProcessorError();

  void ReleaseJSResources() {
    mGlobal = nullptr;
    mProcessor.reset();
  }

  // The AudioWorkletGlobalScope-associated objects referenced from
  // WorkletNodeEngine are typically kept alive as long as the
  // AudioWorkletNode in the main-thread global.  The objects must be released
  // on the rendering thread, which usually happens simply because
  // AudioWorkletNode is such that the last AudioNodeStream reference is
  // released by the MSG.  That occurs on the rendering thread except during
  // process shutdown, in which case NotifyForcedShutdown() is called on the
  // rendering thread.
  RefPtr<AudioWorkletGlobalScope> mGlobal;
  JS::PersistentRooted<JSObject*> mProcessor;
};

void WorkletNodeEngine::SendProcessorError() {
  /**
   * https://webaudio.github.io/web-audio-api/#dom-audioworkletnode-onprocessorerror
   * TODO: bug 1558124
   * queue a task on the control thread to fire onprocessorerror event
   * to the node.
   */

  /**
   * Note that once an exception is thrown, the processor will output silence
   * throughout its lifetime.
   */
  ReleaseJSResources();
}

void WorkletNodeEngine::ConstructProcessor(
    AudioWorkletImpl* aWorkletImpl, const nsAString& aName,
    NotNull<StructuredCloneHolder*> aOptionsSerialization) {
  RefPtr<AudioWorkletGlobalScope> global = aWorkletImpl->GetGlobalScope();
  MOZ_ASSERT(global);  // global has already been used to register processor
  JS::RootingContext* cx = RootingCx();
  mProcessor.init(cx);
  if (!global->ConstructProcessor(aName, aOptionsSerialization, &mProcessor)) {
    SendProcessorError();
    return;
  }
  mGlobal = std::move(global);
}

AudioWorkletNode::AudioWorkletNode(AudioContext* aAudioContext,
                                   const nsAString& aName,
                                   const AudioWorkletNodeOptions& aOptions)
    : AudioNode(aAudioContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers),
      mNodeName(aName),
      mInputCount(aOptions.mNumberOfInputs),
      mOutputCount(aOptions.mNumberOfOutputs) {}

/* static */
already_AddRefed<AudioWorkletNode> AudioWorkletNode::Constructor(
    const GlobalObject& aGlobal, AudioContext& aAudioContext,
    const nsAString& aName, const AudioWorkletNodeOptions& aOptions,
    ErrorResult& aRv) {
  if (aOptions.mNumberOfInputs == 0 && aOptions.mNumberOfOutputs == 0) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (aOptions.mOutputChannelCount.WasPassed()) {
    if (aOptions.mOutputChannelCount.Value().Length() !=
        aOptions.mNumberOfOutputs) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return nullptr;
    }

    for (uint32_t channelCount : aOptions.mOutputChannelCount.Value()) {
      if (channelCount == 0 || channelCount > WebAudioUtils::MaxChannelCount) {
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
      }
    }
  }
  /**
   * 2. If nodeName does not exists as a key in the BaseAudioContext’s node
   *    name to parameter descriptor map, throw a NotSupportedError exception
   *    and abort these steps.
   */
  const AudioParamDescriptorMap* parameterDescriptors =
      aAudioContext.GetParamMapForWorkletName(aName);
  if (!parameterDescriptors) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  // MSG does not support more than UINT16_MAX inputs or outputs.
  if (aOptions.mNumberOfInputs > UINT16_MAX) {
    aRv.ThrowRangeError<MSG_VALUE_OUT_OF_RANGE>(
        NS_LITERAL_STRING("numberOfInputs"));
    return nullptr;
  }
  if (aOptions.mNumberOfOutputs > UINT16_MAX) {
    aRv.ThrowRangeError<MSG_VALUE_OUT_OF_RANGE>(
        NS_LITERAL_STRING("numberOfOutputs"));
    return nullptr;
  }

  RefPtr<AudioWorkletNode> audioWorkletNode =
      new AudioWorkletNode(&aAudioContext, aName, aOptions);

  audioWorkletNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  /**
   * 7. Let optionsSerialization be the result of StructuredSerialize(options).
   */
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JS::Value> optionsVal(cx);
  if (NS_WARN_IF(!ToJSValue(cx, aOptions, &optionsVal))) {
    aRv.NoteJSContextException(cx);
    return nullptr;
  }
  // StructuredCloneHolder does not have a move constructor.  Instead allocate
  // memory so that the pointer can be passed to the rendering thread.
  UniquePtr<StructuredCloneHolder> optionsSerialization =
      MakeUnique<StructuredCloneHolder>(
          StructuredCloneHolder::CloningSupported,
          StructuredCloneHolder::TransferringNotSupported,
          JS::StructuredCloneScope::SameProcessDifferentThread);
  optionsSerialization->Write(cx, optionsVal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  audioWorkletNode->mStream = AudioNodeStream::Create(
      &aAudioContext, new WorkletNodeEngine(audioWorkletNode),
      AudioNodeStream::NO_STREAM_FLAGS, aAudioContext.Graph());
  /**
   * 10. Queue a control message to create an AudioWorkletProcessor, given
   *     nodeName, processorPortSerialization, optionsSerialization, and node.
   */
  Worklet* worklet = aAudioContext.GetAudioWorklet(aRv);
  MOZ_ASSERT(worklet, "Worklet already existed and so getter shouldn't fail.");
  auto workletImpl = static_cast<AudioWorkletImpl*>(worklet->Impl());
  audioWorkletNode->mStream->SendRunnable(NS_NewRunnableFunction(
      "WorkletNodeEngine::ConstructProcessor",
      // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.
      // See bug 1535398.
      [stream = audioWorkletNode->mStream,
       workletImpl = RefPtr<AudioWorkletImpl>(workletImpl),
       name = nsString(aName), options = std::move(optionsSerialization)]()
          MOZ_CAN_RUN_SCRIPT_BOUNDARY {
            auto engine = static_cast<WorkletNodeEngine*>(stream->Engine());
            engine->ConstructProcessor(workletImpl, name,
                                       WrapNotNull(options.get()));
          }));

  return audioWorkletNode.forget();
}

AudioParamMap* AudioWorkletNode::GetParameters(ErrorResult& aRv) const {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

MessagePort* AudioWorkletNode::GetPort(ErrorResult& aRv) const {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

JSObject* AudioWorkletNode::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return AudioWorkletNode_Binding::Wrap(aCx, this, aGivenProto);
}

size_t AudioWorkletNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t AudioWorkletNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

}  // namespace dom
}  // namespace mozilla
