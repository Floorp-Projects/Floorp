/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioWorkletNode.h"

#include "AudioParamMap.h"
#include "js/Array.h"  // JS::{Get,Set}ArrayLength, JS::NewArrayLength
#include "mozilla/dom/AudioWorkletNodeBinding.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePort.h"
#include "PlayingRefChangeHandler.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(AudioWorkletNode, AudioNode)
NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioWorkletNode, AudioNode, mPort)

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
  void ConstructProcessor(AudioWorkletImpl* aWorkletImpl,
                          const nsAString& aName,
                          NotNull<StructuredCloneHolder*> aSerializedOptions,
                          UniqueMessagePortId& aPortIdentifier);

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    MOZ_ASSERT(InputCount() <= 1);
    MOZ_ASSERT(OutputCount() <= 1);
    ProcessBlocksOnPorts(aTrack, MakeSpan(&aInput, InputCount()),
                         MakeSpan(aOutput, OutputCount()), aFinished);
  }

  void ProcessBlocksOnPorts(AudioNodeTrack* aTrack,
                            Span<const AudioBlock> aInput,
                            Span<AudioBlock> aOutput, bool* aFinished) override;

  void NotifyForcedShutdown() override { ReleaseJSResources(); }

  bool IsActive() const override { return mKeepEngineActive; }

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
  bool CallProcess(AudioNodeTrack* aTrack, JSContext* aCx,
                   JS::Handle<JS::Value> aCallable);
  void ProduceSilence(AudioNodeTrack* aTrack, Span<AudioBlock> aOutput);

  void ReleaseJSResources() {
    mInputs.mPorts.clearAndFree();
    mOutputs.mPorts.clearAndFree();
    mInputs.mJSArray.reset();
    mOutputs.mJSArray.reset();
    mGlobal = nullptr;
    // This is equivalent to setting [[callable process]] to false.
    mProcessor.reset();
  }

  nsTArray<uint32_t> mOutputChannelCount;
  // The AudioWorkletGlobalScope-associated objects referenced from
  // WorkletNodeEngine are typically kept alive as long as the
  // AudioWorkletNode in the main-thread global.  The objects must be released
  // on the rendering thread, which usually happens simply because
  // AudioWorkletNode is such that the last AudioNodeTrack reference is
  // released by the MTG.  That occurs on the rendering thread except during
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

  // mProcessorIsActive is named [[active source]] in the spec.
  // It is initially true and so at least the first process()
  // call will not be skipped when there are no active inputs.
  bool mProcessorIsActive = true;
  // mKeepEngineActive ensures another call to ProcessBlocksOnPorts(), even if
  // there are no active inputs.  Its transitions to false lag those of
  // mProcessorIsActive by one call to ProcessBlocksOnPorts() so that
  // downstream engines can addref their nodes before this engine's node is
  // released.
  bool mKeepEngineActive = true;
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
    NotNull<StructuredCloneHolder*> aSerializedOptions,
    UniqueMessagePortId& aPortIdentifier) {
  MOZ_ASSERT(mInputs.mPorts.empty() && mOutputs.mPorts.empty());
  RefPtr<AudioWorkletGlobalScope> global = aWorkletImpl->GetGlobalScope();
  MOZ_ASSERT(global);  // global has already been used to register processor
  JS::RootingContext* cx = RootingCx();
  mProcessor.init(cx);
  if (!global->ConstructProcessor(aName, aSerializedOptions, aPortIdentifier,
                                  &mProcessor) ||
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
    if (JS::GetArrayLength(aCx, aArray, &oldLength) &&
        (oldLength == length || JS::SetArrayLength(aCx, aArray, length)) &&
        SetArrayElements(aCx, aElements, aArray)) {
      return true;
    }
    // Script may have frozen the array.  Try again with a new Array.
    JS_ClearPendingException(aCx);
  }
  JSObject* array = JS::NewArrayObject(aCx, length);
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
  MOZ_ASSERT(aBlocks.Length() == aPorts->mPorts.length());
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

// This runs JS script.  MediaTrackGraph control messages, which would
// potentially destroy the WorkletNodeEngine and its AudioNodeTrack, cannot
// be triggered by script.  They are not run from an nsIThread event loop and
// do not run until after ProcessBlocksOnPorts() has returned.
bool WorkletNodeEngine::CallProcess(AudioNodeTrack* aTrack, JSContext* aCx,
                                    JS::Handle<JS::Value> aCallable) {
  JS::RootedVector<JS::Value> argv(aCx);
  if (NS_WARN_IF(!argv.resize(3))) {
    return false;
  }
  argv[0].setObject(*mInputs.mJSArray);
  argv[1].setObject(*mOutputs.mJSArray);
  // TODO: argv[2].setObject() for parameters.
  JS::Rooted<JS::Value> rval(aCx);
  if (!JS::Call(aCx, mProcessor, aCallable, argv, &rval)) {
    return false;
  }

  mProcessorIsActive = JS::ToBoolean(rval);
  // Transitions of mProcessorIsActive to false do not trigger
  // PlayingRefChangeHandler::RELEASE until silence is produced in the next
  // block.  This allows downstream engines receiving this non-silence block
  // to take a reference to their nodes before this engine's node releases its
  // down node references.
  if (mProcessorIsActive && !mKeepEngineActive) {
    mKeepEngineActive = true;
    RefPtr<PlayingRefChangeHandler> refchanged =
        new PlayingRefChangeHandler(aTrack, PlayingRefChangeHandler::ADDREF);
    aTrack->Graph()->DispatchToMainThreadStableState(refchanged.forget());
  }
  return true;
}

void WorkletNodeEngine::ProduceSilence(AudioNodeTrack* aTrack,
                                       Span<AudioBlock> aOutput) {
  if (mKeepEngineActive) {
    mKeepEngineActive = false;
    aTrack->ScheduleCheckForInactive();
    RefPtr<PlayingRefChangeHandler> refchanged =
        new PlayingRefChangeHandler(aTrack, PlayingRefChangeHandler::RELEASE);
    aTrack->Graph()->DispatchToMainThreadStableState(refchanged.forget());
  }
  for (AudioBlock& output : aOutput) {
    output.SetNull(WEBAUDIO_BLOCK_SIZE);
  }
}

void WorkletNodeEngine::ProcessBlocksOnPorts(AudioNodeTrack* aTrack,
                                             Span<const AudioBlock> aInput,
                                             Span<AudioBlock> aOutput,
                                             bool* aFinished) {
  MOZ_ASSERT(aInput.Length() == InputCount());
  MOZ_ASSERT(aOutput.Length() == OutputCount());

  bool isSilent = true;
  if (mProcessor) {
    if (mProcessorIsActive) {
      isSilent = false;  // call process()
    } else {             // [[active source]] is false.
      // Call process() only if an input is actively processing.
      for (const AudioBlock& input : aInput) {
        if (!input.IsNull()) {
          isSilent = false;
          break;
        }
      }
    }
  }
  if (isSilent) {
    ProduceSilence(aTrack, aOutput);
    return;
  }

  if (!mOutputChannelCount.IsEmpty()) {
    MOZ_ASSERT(mOutputChannelCount.Length() == aOutput.Length());
    for (size_t o = 0; o < aOutput.Length(); ++o) {
      aOutput[o].AllocateChannels(mOutputChannelCount[o]);
    }
  } else if (aInput.Length() == 1 && aOutput.Length() == 1) {
    uint32_t channelCount = std::max(aInput[0].ChannelCount(), 1U);
    aOutput[0].AllocateChannels(channelCount);
  } else {
    for (AudioBlock& output : aOutput) {
      output.AllocateChannels(1);
    }
  }

  AutoEntryScript aes(mGlobal, "Worklet Process");
  JSContext* cx = aes.cx();

  JS::Rooted<JS::Value> process(cx);
  if (!JS_GetProperty(cx, mProcessor, "process", &process) ||
      !process.isObject() || !JS::IsCallable(&process.toObject()) ||
      !PrepareBufferArrays(cx, aInput, &mInputs, ArrayElementInit::None) ||
      !PrepareBufferArrays(cx, aOutput, &mOutputs, ArrayElementInit::Zero)) {
    // process() not callable or OOM.
    SendProcessorError();
    ProduceSilence(aTrack, aOutput);
    return;
  }

  // Copy input values to JS objects.
  for (size_t i = 0; i < aInput.Length(); ++i) {
    const AudioBlock& input = aInput[i];
    size_t channelCount = input.ChannelCount();
    if (channelCount == 0) {
      // Null blocks have AUDIO_FORMAT_SILENCE.
      // Don't call ChannelData<float>().
      continue;
    }
    float volume = input.mVolume;
    const auto& channelData = input.ChannelData<float>();
    const auto& float32Arrays = mInputs.mPorts[i].mFloat32Arrays;
    JS::AutoCheckCannotGC nogc;
    for (size_t c = 0; c < channelCount; ++c) {
      bool isShared;
      float* dest = JS_GetFloat32ArrayData(float32Arrays[c], &isShared, nogc);
      MOZ_ASSERT(!isShared);  // Was created as unshared
      AudioBlockCopyChannelWithScale(channelData[c], volume, dest);
    }
  }

  if (!CallProcess(aTrack, cx, process)) {
    // An exception occurred.
    SendProcessorError();
    /**
     * https://webaudio.github.io/web-audio-api/#dom-audioworkletnode-onprocessorerror
     * Note that once an exception is thrown, the processor will output silence
     * throughout its lifetime.
     */
    ProduceSilence(aTrack, aOutput);
    return;
  }

  // Copy output values from JS objects.
  for (size_t o = 0; o < aOutput.Length(); ++o) {
    AudioBlock* output = &aOutput[o];
    size_t channelCount = output->ChannelCount();
    const auto& float32Arrays = mOutputs.mPorts[o].mFloat32Arrays;
    JS::AutoCheckCannotGC nogc;
    for (size_t c = 0; c < channelCount; ++c) {
      bool isShared;
      const float* src =
          JS_GetFloat32ArrayData(float32Arrays[c], &isShared, nogc);
      MOZ_ASSERT(!isShared);  // Was created as unshared
      PodCopy(output->ChannelFloatsForWrite(c), src, WEBAUDIO_BLOCK_SIZE);
    }
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
  /**
   * 1. If nodeName does not exist as a key in the BaseAudioContext’s node
   *    name to parameter descriptor map, throw a NotSupportedError exception
   *    and abort these steps.
   */
  const AudioParamDescriptorMap* parameterDescriptors =
      aAudioContext.GetParamMapForWorkletName(aName);
  if (!parameterDescriptors) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  // See https://github.com/WebAudio/web-audio-api/issues/2074 for ordering.
  RefPtr<AudioWorkletNode> audioWorkletNode =
      new AudioWorkletNode(&aAudioContext, aName, aOptions);
  audioWorkletNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  /**
   * 3. Configure input, output and output channels of node with options.
   */
  if (aOptions.mNumberOfInputs == 0 && aOptions.mNumberOfOutputs == 0) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (aOptions.mOutputChannelCount.WasPassed()) {
    /**
     * 1. If any value in outputChannelCount is zero or greater than the
     *    implementation’s maximum number of channels, throw a
     *    NotSupportedError and abort the remaining steps.
     */
    for (uint32_t channelCount : aOptions.mOutputChannelCount.Value()) {
      if (channelCount == 0 || channelCount > WebAudioUtils::MaxChannelCount) {
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
      }
    }
    /**
     * 2. If the length of outputChannelCount does not equal numberOfOutputs,
     *    throw an IndexSizeError and abort the remaining steps.
     */
    if (aOptions.mOutputChannelCount.Value().Length() !=
        aOptions.mNumberOfOutputs) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return nullptr;
    }
  }
  // MTG does not support more than UINT16_MAX inputs or outputs.
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

  /**
   * 4. Let messageChannel be a new MessageChannel.
   */
  RefPtr<MessageChannel> messageChannel =
      MessageChannel::Constructor(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  /* 5. Let nodePort be the value of messageChannel’s port1 attribute.
   * 6. Let processorPortOnThisSide be the value of messageChannel’s port2
   *    attribute.
   * 7. Let serializedProcessorPort be the result of
   *    StructuredSerializeWithTransfer(processorPortOnThisSide,
   *                                    « processorPortOnThisSide »).
   */
  UniqueMessagePortId processorPortId;
  messageChannel->Port2()->CloneAndDisentangle(processorPortId);
  /**
   * 8. Convert options dictionary to optionsObject.
   */
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JS::Value> optionsVal(cx);
  if (NS_WARN_IF(!ToJSValue(cx, aOptions, &optionsVal))) {
    aRv.NoteJSContextException(cx);
    return nullptr;
  }
  /**
   * 9. Let serializedOptions be the result of
   *    StructuredSerialize(optionsObject).
   */
  // StructuredCloneHolder does not have a move constructor.  Instead allocate
  // memory so that the pointer can be passed to the rendering thread.
  UniquePtr<StructuredCloneHolder> serializedOptions =
      MakeUnique<StructuredCloneHolder>(
          StructuredCloneHolder::CloningSupported,
          StructuredCloneHolder::TransferringNotSupported,
          JS::StructuredCloneScope::SameProcess);
  serializedOptions->Write(cx, optionsVal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  /**
   * 10. Set node’s port to nodePort.
   */
  audioWorkletNode->mPort = messageChannel->Port1();

  auto engine =
      new WorkletNodeEngine(audioWorkletNode, aOptions.mOutputChannelCount);
  audioWorkletNode->mTrack = AudioNodeTrack::Create(
      &aAudioContext, engine, AudioNodeTrack::NO_TRACK_FLAGS,
      aAudioContext.Graph());

  /**
   * 12. Queue a control message to invoke the constructor of the
   *     corresponding AudioWorkletProcessor with the processor construction
   *     data that consists of: nodeName, node, serializedOptions, and
   *     serializedProcessorPort.
   */
  Worklet* worklet = aAudioContext.GetAudioWorklet(aRv);
  MOZ_ASSERT(worklet, "Worklet already existed and so getter shouldn't fail.");
  auto workletImpl = static_cast<AudioWorkletImpl*>(worklet->Impl());
  audioWorkletNode->mTrack->SendRunnable(NS_NewRunnableFunction(
      "WorkletNodeEngine::ConstructProcessor",
      // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.
      // See bug 1535398.
      [track = audioWorkletNode->mTrack,
       workletImpl = RefPtr<AudioWorkletImpl>(workletImpl),
       name = nsString(aName), options = std::move(serializedOptions),
       portId = std::move(processorPortId)]()
          MOZ_CAN_RUN_SCRIPT_BOUNDARY mutable {
            auto engine = static_cast<WorkletNodeEngine*>(track->Engine());
            engine->ConstructProcessor(workletImpl, name,
                                       WrapNotNull(options.get()), portId);
          }));

  // [[active source]] is initially true and so at least the first process()
  // call will not be skipped when there are no active inputs.
  audioWorkletNode->MarkActive();

  return audioWorkletNode.forget();
}

AudioParamMap* AudioWorkletNode::GetParameters(ErrorResult& aRv) const {
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
