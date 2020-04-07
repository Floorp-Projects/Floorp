/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioWorkletNode.h"

#include "AudioParamMap.h"
#include "js/Array.h"  // JS::{Get,Set}ArrayLength, JS::NewArrayLength
#include "mozilla/dom/AudioWorkletNodeBinding.h"
#include "mozilla/dom/AudioParamMapBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ErrorEvent.h"
#include "nsIScriptGlobalObject.h"
#include "AudioParam.h"
#include "AudioDestinationNode.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePort.h"
#include "nsReadableUtils.h"
#include "mozilla/Span.h"
#include "PlayingRefChangeHandler.h"
#include "nsPrintfCString.h"
#include "Tracing.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(AudioWorkletNode, AudioNode)
NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioWorkletNode, AudioNode, mPort,
                                   mParameters)

struct NamedAudioParamTimeline {
  explicit NamedAudioParamTimeline(const AudioParamDescriptor& aParamDescriptor)
      : mName(aParamDescriptor.mName),
        mTimeline(aParamDescriptor.mDefaultValue) {}
  const nsString mName;
  AudioParamTimeline mTimeline;
};

struct ProcessorErrorDetails {
  ProcessorErrorDetails() : mLineno(0), mColno(0) {}
  unsigned mLineno;
  unsigned mColno;
  nsString mFilename;
  nsString mMessage;
};

class WorkletNodeEngine final : public AudioNodeEngine {
 public:
  WorkletNodeEngine(AudioWorkletNode* aNode,
                    AudioDestinationNode* aDestinationNode,
                    nsTArray<NamedAudioParamTimeline>&& aParamTimelines,
                    const Optional<Sequence<uint32_t>>& aOutputChannelCount)
      : AudioNodeEngine(aNode),
        mDestination(aDestinationNode->Track()),
        mParamTimelines(aParamTimelines) {
    if (aOutputChannelCount.WasPassed()) {
      mOutputChannelCount = aOutputChannelCount.Value();
    }
  }

  MOZ_CAN_RUN_SCRIPT
  void ConstructProcessor(AudioWorkletImpl* aWorkletImpl,
                          const nsAString& aName,
                          NotNull<StructuredCloneHolder*> aSerializedOptions,
                          UniqueMessagePortId& aPortIdentifier,
                          AudioNodeTrack* aTrack);

  void RecvTimelineEvent(uint32_t aIndex, AudioTimelineEvent& aEvent) override {
    MOZ_ASSERT(mDestination);
    WebAudioUtils::ConvertAudioTimelineEventToTicks(aEvent, mDestination);

    if (aIndex < mParamTimelines.Length()) {
      mParamTimelines[aIndex].mTimeline.InsertEvent<int64_t>(aEvent);
    } else {
      NS_ERROR("Bad WorkletNodeEngine timeline event index");
    }
  }

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    MOZ_ASSERT(InputCount() <= 1);
    MOZ_ASSERT(OutputCount() <= 1);
    ProcessBlocksOnPorts(aTrack, aFrom, MakeSpan(&aInput, InputCount()),
                         MakeSpan(aOutput, OutputCount()), aFinished);
  }

  void ProcessBlocksOnPorts(AudioNodeTrack* aTrack, GraphTime aFrom,
                            Span<const AudioBlock> aInput,
                            Span<AudioBlock> aOutput, bool* aFinished) override;

  void NotifyForcedShutdown() override { ReleaseJSResources(); }

  bool IsActive() const override { return mKeepEngineActive; }

  // Vector<T> supports non-memmovable types such as PersistentRooted
  // (without any need to jump through hoops like
  // MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR_FOR_TEMPLATE for nsTArray).
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
  struct ParameterValues {
    Vector<JS::PersistentRooted<JSObject*>> mFloat32Arrays;
    JS::PersistentRooted<JSObject*> mJSObject;
  };

 private:
  size_t ParameterCount() { return mParamTimelines.Length(); }
  void SendProcessorError(AudioNodeTrack* aTrack, JSContext* aCx);
  bool CallProcess(AudioNodeTrack* aTrack, JSContext* aCx,
                   JS::Handle<JS::Value> aCallable);
  void ProduceSilence(AudioNodeTrack* aTrack, Span<AudioBlock> aOutput);
  void SendErrorToMainThread(AudioNodeTrack* aTrack,
                             const ProcessorErrorDetails& aDetails);

  void ReleaseJSResources() {
    mInputs.mPorts.clearAndFree();
    mOutputs.mPorts.clearAndFree();
    mParameters.mFloat32Arrays.clearAndFree();
    mInputs.mJSArray.reset();
    mOutputs.mJSArray.reset();
    mParameters.mJSObject.reset();
    mGlobal = nullptr;
    // This is equivalent to setting [[callable process]] to false.
    mProcessor.reset();
  }

  RefPtr<AudioNodeTrack> mDestination;
  nsTArray<uint32_t> mOutputChannelCount;
  nsTArray<NamedAudioParamTimeline> mParamTimelines;
  // The AudioWorkletGlobalScope-associated objects referenced from
  // WorkletNodeEngine are typically kept alive as long as the
  // AudioWorkletNode in the main-thread global.  The objects must be released
  // on the rendering thread, which usually happens simply because
  // AudioWorkletNode is such that the last AudioNodeTrack reference is
  // released by the MTG.  That occurs on the rendering thread except during
  // process shutdown, in which case NotifyForcedShutdown() is called on the
  // rendering thread.
  //
  // mInputs, mOutputs and mParameters keep references to all objects passed to
  // process(), for reuse of the same objects.  The JS objects are all in the
  // compartment of the realm of mGlobal.  Properties on the objects may be
  // replaced by script, so don't assume that getting indexed properties on the
  // JS arrays will return the same objects.  Only objects and buffers created
  // by the implementation are modified or read by the implementation.
  Ports mInputs;
  Ports mOutputs;
  ParameterValues mParameters;

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

void WorkletNodeEngine::SendErrorToMainThread(
    AudioNodeTrack* aTrack, const ProcessorErrorDetails& aDetails) {
  RefPtr<AudioNodeTrack> track = aTrack;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "WorkletNodeEngine::SendProcessorError",
      [track = std::move(track), aDetails]() mutable {
        AudioWorkletNode* node =
            static_cast<AudioWorkletNode*>(track->Engine()->NodeMainThread());
        if (!node) {
          return;
        }
        node->DispatchProcessorErrorEvent(aDetails);
      }));
}

void WorkletNodeEngine::SendProcessorError(AudioNodeTrack* aTrack,
                                           JSContext* aCx) {
  // Note that once an exception is thrown, the processor will output silence
  // throughout its lifetime.
  ReleaseJSResources();
  // The processor errored out while getting a context, try to tell the node
  // anyways.
  if (!aCx) {
    ProcessorErrorDetails details;
    details.mMessage.Assign(u"Unknown processor error");
    SendErrorToMainThread(aTrack, details);
    return;
  }

  js::ErrorReport jsReport(aCx);
  JS::Rooted<JS::Value> exn(aCx);
  JS::Rooted<JSObject*> exnStack(aCx);
  if (JS_GetPendingException(aCx, &exn)) {
    exnStack.set(JS::GetPendingExceptionStack(aCx));
    JS_ClearPendingException(aCx);
    if (!jsReport.init(aCx, exn, js::ErrorReport::WithSideEffects)) {
      ProcessorErrorDetails details;
      details.mMessage.Assign(u"Unknown processor error");
      SendErrorToMainThread(aTrack, details);
      // Set the exception and stack back to have it in the console with a stack
      // trace.
      JS::SetPendingExceptionAndStack(aCx, exn, exnStack);
      return;
    }

    ProcessorErrorDetails details;

    CopyUTF8toUTF16(mozilla::MakeStringSpan(jsReport.report()->filename),
                    details.mFilename);

    xpc::ErrorReport::ErrorReportToMessageString(jsReport.report(),
                                                 details.mMessage);
    details.mLineno = jsReport.report()->lineno;
    details.mColno = jsReport.report()->column;
    MOZ_ASSERT(!jsReport.report()->isMuted);

    SendErrorToMainThread(aTrack, details);

    // Set the exception and stack back to have it in the console with a stack
    // trace.
    JS::SetPendingExceptionAndStack(aCx, exn, exnStack);
  } else {
    NS_WARNING("No exception, but processor errored out?");
  }
}

void WorkletNodeEngine::ConstructProcessor(
    AudioWorkletImpl* aWorkletImpl, const nsAString& aName,
    NotNull<StructuredCloneHolder*> aSerializedOptions,
    UniqueMessagePortId& aPortIdentifier, AudioNodeTrack* aTrack) {
  MOZ_ASSERT(mInputs.mPorts.empty() && mOutputs.mPorts.empty());
  RefPtr<AudioWorkletGlobalScope> global = aWorkletImpl->GetGlobalScope();
  MOZ_ASSERT(global);  // global has already been used to register processor
  AutoJSAPI api;
  if (NS_WARN_IF(!api.Init(global))) {
    SendProcessorError(aTrack, nullptr);
    return;
  }
  JSContext* cx = api.cx();
  mProcessor.init(cx);
  if (!global->ConstructProcessor(cx, aName, aSerializedOptions,
                                  aPortIdentifier, &mProcessor) ||
      // mInputs and mOutputs outer arrays are fixed length and so much of the
      // initialization need only be performed once (i.e. here).
      NS_WARN_IF(!mInputs.mPorts.growBy(InputCount())) ||
      NS_WARN_IF(!mOutputs.mPorts.growBy(OutputCount()))) {
    SendProcessorError(aTrack, cx);
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
  JSObject* object = JS_NewPlainObject(cx);
  if (NS_WARN_IF(!object)) {
    SendProcessorError(aTrack, cx);
    return;
  }

  mParameters.mJSObject.init(cx, object);
  if (NS_WARN_IF(!mParameters.mFloat32Arrays.growBy(ParameterCount()))) {
    SendProcessorError(aTrack, cx);
    return;
  }
  for (size_t i = 0; i < mParamTimelines.Length(); i++) {
    auto& float32ArraysRef = mParameters.mFloat32Arrays;
    float32ArraysRef[i].init(cx);
    JSObject* array = JS_NewFloat32Array(cx, WEBAUDIO_BLOCK_SIZE);
    if (NS_WARN_IF(!array)) {
      SendProcessorError(aTrack, cx);
      return;
    }

    float32ArraysRef[i] = array;
    if (NS_WARN_IF(!JS_DefineUCProperty(
            cx, mParameters.mJSObject, mParamTimelines[i].mName.get(),
            mParamTimelines[i].mName.Length(), float32ArraysRef[i],
            JSPROP_ENUMERATE))) {
      SendProcessorError(aTrack, cx);
      return;
    }
  }
  if (NS_WARN_IF(!JS_FreezeObject(cx, mParameters.mJSObject))) {
    SendProcessorError(aTrack, cx);
    return;
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
  TRACE();

  JS::RootedVector<JS::Value> argv(aCx);
  if (NS_WARN_IF(!argv.resize(3))) {
    return false;
  }
  argv[0].setObject(*mInputs.mJSArray);
  argv[1].setObject(*mOutputs.mJSArray);
  argv[2].setObject(*mParameters.mJSObject);
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
                                             GraphTime aFrom,
                                             Span<const AudioBlock> aInput,
                                             Span<AudioBlock> aOutput,
                                             bool* aFinished) {
  MOZ_ASSERT(aInput.Length() == InputCount());
  MOZ_ASSERT(aOutput.Length() == OutputCount());
  TRACE();

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
    SendProcessorError(aTrack, cx);
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

  TrackTime tick = mDestination->GraphTimeToTrackTime(aFrom);
  // Compute and copy parameter values to JS objects.
  for (size_t i = 0; i < mParamTimelines.Length(); ++i) {
    const auto& float32Arrays = mParameters.mFloat32Arrays[i];
    uint32_t length = JS_GetTypedArrayLength(float32Arrays);

    // If the Float32Array that is supposed to hold the values for a particular
    // AudioParam has been detached, error out. This is being worked on in
    // https://github.com/WebAudio/web-audio-api/issues/1933 and
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1619486
    if (length != WEBAUDIO_BLOCK_SIZE) {
      SendProcessorError(aTrack, cx);
      ProduceSilence(aTrack, aOutput);
      return;
    }
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    float* dest = JS_GetFloat32ArrayData(float32Arrays, &isShared, nogc);
    MOZ_ASSERT(!isShared);  // Was created as unshared

    size_t frames =
        mParamTimelines[i].mTimeline.HasSimpleValue() ? 1 : WEBAUDIO_BLOCK_SIZE;
    mParamTimelines[i].mTimeline.GetValuesAtTime(tick, dest, frames);
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1616599
    if (frames == 1) {
      std::fill_n(dest + 1, WEBAUDIO_BLOCK_SIZE - 1, dest[0]);
    }
  }

  if (!CallProcess(aTrack, cx, process)) {
    // An exception occurred.
    SendProcessorError(aTrack, cx);
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

void AudioWorkletNode::InitializeParameters(
    nsTArray<NamedAudioParamTimeline>* aParamTimelines, ErrorResult& aRv) {
  MOZ_ASSERT(!mParameters, "Only initialize the `parameters` attribute once.");
  MOZ_ASSERT(aParamTimelines);

  AudioContext* context = Context();
  const AudioParamDescriptorMap* parameterDescriptors =
      context->GetParamMapForWorkletName(mNodeName);
  MOZ_ASSERT(parameterDescriptors);
  nsPIDOMWindowInner* window = context->GetParentObject();
  MOZ_ASSERT(window);

  mParameters = new AudioParamMap(window);
  size_t audioParamIndex = 0;
  aParamTimelines->SetCapacity(parameterDescriptors->Length());

  for (size_t i = 0; i < parameterDescriptors->Length(); i++) {
    auto& paramEntry = (*parameterDescriptors)[i];
    RefPtr<AudioParam> param = nullptr;
    // There are no ways to remove elements from ParamMapForWorkletName, so the
    // string contained in it and used here have a lifetime that is strictly
    // longer than the lifetime of the AudioParam constructed below.
    // Additionally, AudioParam keep a reference to their AudioNode.
    CreateAudioParam(param, audioParamIndex++, paramEntry.mName.get(),
                     paramEntry.mDefaultValue, paramEntry.mMinValue,
                     paramEntry.mMaxValue);
    AudioParamMap_Binding::MaplikeHelpers::Set(mParameters, paramEntry.mName,
                                               *param, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
    aParamTimelines->AppendElement(paramEntry);
  }
}

void AudioWorkletNode::SendParameterData(
    const Optional<Record<nsString, double>>& aParameterData) {
  MOZ_ASSERT(mTrack, "This method only works if the track has been created.");
  nsAutoString name;
  if (aParameterData.WasPassed()) {
    const auto& paramData = aParameterData.Value();
    for (const auto& paramDataEntry : paramData.Entries()) {
      for (auto& audioParam : mParams) {
        audioParam->GetName(name);
        if (paramDataEntry.mKey.Equals(name)) {
          audioParam->SetValue(paramDataEntry.mValue);
        }
      }
    }
  }
}

/* static */
already_AddRefed<AudioWorkletNode> AudioWorkletNode::Constructor(
    const GlobalObject& aGlobal, AudioContext& aAudioContext,
    const nsAString& aName, const AudioWorkletNodeOptions& aOptions,
    ErrorResult& aRv) {
  /**
   * 1. If nodeName does not exist as a key in the BaseAudioContext’s node
   *    name to parameter descriptor map, throw a InvalidStateError exception
   *    and abort these steps.
   */
  const AudioParamDescriptorMap* parameterDescriptors =
      aAudioContext.GetParamMapForWorkletName(aName);
  if (!parameterDescriptors) {
    // Not using nsPrintfCString in case aName has embedded nulls.
    aRv.ThrowInvalidStateError(
        NS_LITERAL_CSTRING("Unknown AudioWorklet name '") +
        NS_ConvertUTF16toUTF8(aName) + NS_LITERAL_CSTRING("'"));
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
    aRv.ThrowNotSupportedError(
        "Must have nonzero numbers of inputs or outputs");
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
        aRv.ThrowNotSupportedError(
            nsPrintfCString("Channel count (%u) must be in the range [1, max "
                            "supported channel count]",
                            channelCount));
        return nullptr;
      }
    }
    /**
     * 2. If the length of outputChannelCount does not equal numberOfOutputs,
     *    throw an IndexSizeError and abort the remaining steps.
     */
    if (aOptions.mOutputChannelCount.Value().Length() !=
        aOptions.mNumberOfOutputs) {
      aRv.ThrowIndexSizeError(
          nsPrintfCString("Length of outputChannelCount (%zu) does not match "
                          "numberOfOutputs (%u)",
                          aOptions.mOutputChannelCount.Value().Length(),
                          aOptions.mNumberOfOutputs));
      return nullptr;
    }
  }
  // MTG does not support more than UINT16_MAX inputs or outputs.
  if (aOptions.mNumberOfInputs > UINT16_MAX) {
    aRv.ThrowRangeError<MSG_VALUE_OUT_OF_RANGE>("numberOfInputs");
    return nullptr;
  }
  if (aOptions.mNumberOfOutputs > UINT16_MAX) {
    aRv.ThrowRangeError<MSG_VALUE_OUT_OF_RANGE>("numberOfOutputs");
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

  // This context and the worklet are part of the same agent cluster and they
  // can share memory.
  JS::CloneDataPolicy cloneDataPolicy;
  cloneDataPolicy.allowIntraClusterClonableSharedObjects();
  cloneDataPolicy.allowSharedMemoryObjects();

  // StructuredCloneHolder does not have a move constructor.  Instead allocate
  // memory so that the pointer can be passed to the rendering thread.
  UniquePtr<StructuredCloneHolder> serializedOptions =
      MakeUnique<StructuredCloneHolder>(
          StructuredCloneHolder::CloningSupported,
          StructuredCloneHolder::TransferringNotSupported,
          JS::StructuredCloneScope::SameProcess);
  serializedOptions->Write(cx, optionsVal, JS::UndefinedHandleValue,
                           cloneDataPolicy, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  /**
   * 10. Set node’s port to nodePort.
   */
  audioWorkletNode->mPort = messageChannel->Port1();

  /**
   * 11. Let parameterDescriptors be the result of retrieval of nodeName from
   * node name to parameter descriptor map.
   */
  nsTArray<NamedAudioParamTimeline> paramTimelines;
  audioWorkletNode->InitializeParameters(&paramTimelines, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  auto engine = new WorkletNodeEngine(
      audioWorkletNode, aAudioContext.Destination(), std::move(paramTimelines),
      aOptions.mOutputChannelCount);
  audioWorkletNode->mTrack = AudioNodeTrack::Create(
      &aAudioContext, engine, AudioNodeTrack::NO_TRACK_FLAGS,
      aAudioContext.Graph());

  audioWorkletNode->SendParameterData(aOptions.mParameterData);

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
            engine->ConstructProcessor(
                workletImpl, name, WrapNotNull(options.get()), portId, track);
          }));

  // [[active source]] is initially true and so at least the first process()
  // call will not be skipped when there are no active inputs.
  audioWorkletNode->MarkActive();

  return audioWorkletNode.forget();
}

AudioParamMap* AudioWorkletNode::GetParameters(ErrorResult& aRv) const {
  return mParameters.get();
}

void AudioWorkletNode::DispatchProcessorErrorEvent(
    const ProcessorErrorDetails& aDetails) {
  if (HasListenersFor(nsGkAtoms::onprocessorerror)) {
    RootedDictionary<ErrorEventInit> init(RootingCx());
    init.mMessage = aDetails.mMessage;
    init.mFilename = aDetails.mFilename;
    init.mLineno = aDetails.mLineno;
    init.mColno = aDetails.mColno;
    RefPtr<ErrorEvent> errorEvent = ErrorEvent::Constructor(
        this, NS_LITERAL_STRING("processorerror"), init);
    MOZ_ASSERT(errorEvent);
    DispatchTrustedEvent(errorEvent);
  }
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
