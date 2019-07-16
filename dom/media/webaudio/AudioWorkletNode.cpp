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
  WorkletNodeEngine(AudioWorkletNode* aNode,
                    const Optional<Sequence<uint32_t>>& aOutputChannelCount)
      : AudioNodeEngine(aNode) {
    if (aOutputChannelCount.WasPassed()) {
      mOutputChannelCount = aOutputChannelCount.Value();
    }
  }

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

  void ProcessBlocksOnPorts(AudioNodeStream* aStream,
                            Span<const AudioBlock> aInput,
                            Span<AudioBlock> aOutput, bool* aFinished) override;

  void NotifyForcedShutdown() override { ReleaseJSResources(); }

  // Vector<T> supports non-memmovable types such as PersistentRooted
  // (without any need to jump through hoops like
  // DECLARE_USE_COPY_CONSTRUCTORS_FOR_TEMPLATE for nsTArray).
  // PersistentRooted is used because these AudioWorkletGlobalScope scope
  // objects may be kept alive as long as the AudioWorkletNode in the
  // main-thread global.
  struct Channels {
    Vector<JS::PersistentRooted<JSObject*>, GUESS_AUDIO_CHANNELS>
        mFloat32Arrays;
    JS::PersistentRooted<JSObject*> mJSArray;
    // For SetArrayElements():
    operator JS::Handle<JSObject*>() const { return mJSArray; }
  };
  struct Ports {
    Vector<Channels, 1> mPorts;
    JS::PersistentRooted<JSObject*> mJSArray;
  };

 private:
  void SendProcessorError();

  void ReleaseJSResources() {
    mInputs.mPorts.clearAndFree();
    mOutputs.mPorts.clearAndFree();
    mInputs.mJSArray.reset();
    mOutputs.mJSArray.reset();
    mGlobal = nullptr;
    mProcessor.reset();
  }

  nsTArray<uint32_t> mOutputChannelCount;
  // The AudioWorkletGlobalScope-associated objects referenced from
  // WorkletNodeEngine are typically kept alive as long as the
  // AudioWorkletNode in the main-thread global.  The objects must be released
  // on the rendering thread, which usually happens simply because
  // AudioWorkletNode is such that the last AudioNodeStream reference is
  // released by the MSG.  That occurs on the rendering thread except during
  // process shutdown, in which case NotifyForcedShutdown() is called on the
  // rendering thread.
  //
  // mInputs and mOutputs keep references to all objects passed to process(),
  // for reuse of the same objects.  The JS objects are all in the compartment
  // of the realm of mGlobal.  Properties on the objects may be replaced by
  // script, so don't assume that getting indexed properties on the JS arrays
  // will return the same objects.  Only objects and buffers created by the
  // implementation are modified or read by the implementation.
  Ports mInputs;
  Ports mOutputs;

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
  MOZ_ASSERT(mInputs.mPorts.empty() && mOutputs.mPorts.empty());
  RefPtr<AudioWorkletGlobalScope> global = aWorkletImpl->GetGlobalScope();
  MOZ_ASSERT(global);  // global has already been used to register processor
  JS::RootingContext* cx = RootingCx();
  mProcessor.init(cx);
  if (!global->ConstructProcessor(aName, aOptionsSerialization, &mProcessor) ||
      // mInputs and mOutputs outer arrays are fixed length and so much of the
      // initialization need only be performed once (i.e. here).
      NS_WARN_IF(!mInputs.mPorts.growBy(InputCount())) ||
      NS_WARN_IF(!mOutputs.mPorts.growBy(OutputCount()))) {
    SendProcessorError();
    return;
  }
  mGlobal = std::move(global);
  mInputs.mJSArray.init(cx);
  mOutputs.mJSArray.init(cx);
  for (auto& port : mInputs.mPorts) {
    port.mJSArray.init(cx);
  }
  for (auto& port : mOutputs.mPorts) {
    port.mJSArray.init(cx);
  }
}

// Type T should support the length() and operator[]() methods and the return
// type of |operator[]() const| should support conversion to Handle<JSObject*>.
template <typename T>
static bool SetArrayElements(JSContext* aCx, const T& aElements,
                             JS::Handle<JSObject*> aArray) {
  for (size_t i = 0; i < aElements.length(); ++i) {
    if (!JS_DefineElement(aCx, aArray, i, aElements[i], JSPROP_ENUMERATE)) {
      return false;
    }
  }

  return true;
}

template <typename T>
static bool PrepareArray(JSContext* aCx, const T& aElements,
                         JS::MutableHandle<JSObject*> aArray) {
  size_t length = aElements.length();
  if (aArray) {
    // Attempt to reuse.
    uint32_t oldLength;
    if (JS_GetArrayLength(aCx, aArray, &oldLength) &&
        (oldLength == length || JS_SetArrayLength(aCx, aArray, length)) &&
        SetArrayElements(aCx, aElements, aArray)) {
      return true;
    }
    // Script may have frozen the array.  Try again with a new Array.
    JS_ClearPendingException(aCx);
  }
  JSObject* array = JS_NewArrayObject(aCx, length);
  if (NS_WARN_IF(!array)) {
    return false;
  }
  aArray.set(array);
  return SetArrayElements(aCx, aElements, aArray);
}

enum class ArrayElementInit { None, Zero };

// Exactly when to create new Float32Array and Array objects is not specified.
// This approach aims to minimize garbage creation, while continuing to
// function after objects are modified by content.
// See https://github.com/WebAudio/web-audio-api/issues/1934 and
// https://github.com/WebAudio/web-audio-api/issues/1933
static bool PrepareBufferArrays(JSContext* aCx, Span<const AudioBlock> aBlocks,
                                WorkletNodeEngine::Ports* aPorts,
                                ArrayElementInit aInit) {
  for (size_t i = 0; i < aBlocks.Length(); ++i) {
    size_t channelCount = aBlocks[i].ChannelCount();
    WorkletNodeEngine::Channels& portRef = aPorts->mPorts[i];

    auto& float32ArraysRef = portRef.mFloat32Arrays;
    for (auto& channelRef : float32ArraysRef) {
      uint32_t length = JS_GetTypedArrayLength(channelRef);
      if (length != WEBAUDIO_BLOCK_SIZE) {
        // Script has detached array buffers.  Create new objects.
        JSObject* array = JS_NewFloat32Array(aCx, WEBAUDIO_BLOCK_SIZE);
        if (NS_WARN_IF(!array)) {
          return false;
        }
        channelRef = array;
      } else if (aInit == ArrayElementInit::Zero) {
        // Need only zero existing arrays as new arrays are already zeroed.
        JS::AutoCheckCannotGC nogc;
        bool isShared;
        float* elementData =
            JS_GetFloat32ArrayData(channelRef, &isShared, nogc);
        MOZ_ASSERT(!isShared);  // Was created as unshared
        std::fill_n(elementData, WEBAUDIO_BLOCK_SIZE, 0.0f);
      }
    }
    // Enlarge if necessary...
    if (NS_WARN_IF(!float32ArraysRef.reserve(channelCount))) {
      return false;
    }
    while (float32ArraysRef.length() < channelCount) {
      JSObject* array = JS_NewFloat32Array(aCx, WEBAUDIO_BLOCK_SIZE);
      if (NS_WARN_IF(!array)) {
        return false;
      }
      float32ArraysRef.infallibleEmplaceBack(aCx, array);
    }
    // ... or shrink if necessary.
    float32ArraysRef.shrinkTo(channelCount);

    if (NS_WARN_IF(!PrepareArray(aCx, float32ArraysRef, &portRef.mJSArray))) {
      return false;
    }
  }

  return !(NS_WARN_IF(!PrepareArray(aCx, aPorts->mPorts, &aPorts->mJSArray)));
}

static void ProduceSilence(Span<AudioBlock> aOutput) {
  for (AudioBlock& output : aOutput) {
    output.SetNull(WEBAUDIO_BLOCK_SIZE);
  }
}

void WorkletNodeEngine::ProcessBlocksOnPorts(AudioNodeStream* aStream,
                                             Span<const AudioBlock> aInput,
                                             Span<AudioBlock> aOutput,
                                             bool* aFinished) {
  if (!mProcessor) {
    ProduceSilence(aOutput);
    return;
  }

  if (!mOutputChannelCount.IsEmpty()) {
    MOZ_ASSERT(mOutputChannelCount.Length() == aOutput.Length());
    for (size_t o = 0; o < aOutput.Length(); ++o) {
      aOutput[o].AllocateChannels(mOutputChannelCount[o]);
    }
  } else if (aInput.Length() == 1 && aOutput.Length() == 1) {
    aOutput[0].AllocateChannels(aInput[0].ChannelCount());
  } else {
    for (AudioBlock& output : aOutput) {
      output.AllocateChannels(1);
    }
  }

  AutoEntryScript aes(mGlobal, "Worklet Process");
  JSContext* cx = aes.cx();

  if (!PrepareBufferArrays(cx, aInput, &mInputs, ArrayElementInit::None) ||
      !PrepareBufferArrays(cx, aOutput, &mOutputs, ArrayElementInit::Zero)) {
    // OOM.  Give up.
    SendProcessorError();
    ProduceSilence(aOutput);
    return;
  }
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

  auto engine =
      new WorkletNodeEngine(audioWorkletNode, aOptions.mOutputChannelCount);
  audioWorkletNode->mStream = AudioNodeStream::Create(
      &aAudioContext, engine, AudioNodeStream::NO_STREAM_FLAGS,
      aAudioContext.Graph());

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
