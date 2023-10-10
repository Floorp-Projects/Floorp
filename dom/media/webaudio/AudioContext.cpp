/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioContext.h"

#include "blink/PeriodicWave.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/NotNull.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"

#include "mozilla/dom/AnalyserNode.h"
#include "mozilla/dom/AnalyserNodeBinding.h"
#include "mozilla/dom/AudioBufferSourceNodeBinding.h"
#include "mozilla/dom/AudioContextBinding.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/dom/BiquadFilterNodeBinding.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ChannelMergerNodeBinding.h"
#include "mozilla/dom/ChannelSplitterNodeBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ConvolverNodeBinding.h"
#include "mozilla/dom/DelayNodeBinding.h"
#include "mozilla/dom/DynamicsCompressorNodeBinding.h"
#include "mozilla/dom/GainNodeBinding.h"
#include "mozilla/dom/IIRFilterNodeBinding.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/MediaElementAudioSourceNodeBinding.h"
#include "mozilla/dom/MediaStreamAudioSourceNodeBinding.h"
#include "mozilla/dom/MediaStreamTrackAudioSourceNodeBinding.h"
#include "mozilla/dom/OfflineAudioContextBinding.h"
#include "mozilla/dom/OscillatorNodeBinding.h"
#include "mozilla/dom/PannerNodeBinding.h"
#include "mozilla/dom/PeriodicWaveBinding.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StereoPannerNodeBinding.h"
#include "mozilla/dom/WaveShaperNodeBinding.h"
#include "mozilla/dom/Worklet.h"

#include "AudioBuffer.h"
#include "AudioBufferSourceNode.h"
#include "AudioChannelService.h"
#include "AudioDestinationNode.h"
#include "AudioListener.h"
#include "AudioNodeTrack.h"
#include "AudioStream.h"
#include "AudioWorkletImpl.h"
#include "AutoplayPolicy.h"
#include "BiquadFilterNode.h"
#include "ChannelMergerNode.h"
#include "ChannelSplitterNode.h"
#include "ConstantSourceNode.h"
#include "ConvolverNode.h"
#include "DelayNode.h"
#include "DynamicsCompressorNode.h"
#include "GainNode.h"
#include "IIRFilterNode.h"
#include "js/ArrayBuffer.h"  // JS::StealArrayBufferContents
#include "MediaElementAudioSourceNode.h"
#include "MediaStreamAudioDestinationNode.h"
#include "MediaStreamAudioSourceNode.h"
#include "MediaTrackGraph.h"
#include "MediaStreamTrackAudioSourceNode.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsRFPService.h"
#include "OscillatorNode.h"
#include "PannerNode.h"
#include "PeriodicWave.h"
#include "ScriptProcessorNode.h"
#include "StereoPannerNode.h"
#include "WaveShaperNode.h"
#include "Tracing.h"

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla::dom {

// 0 is a special value that MediaTracks use to denote they are not part of a
// AudioContext.
static dom::AudioContext::AudioContextId gAudioContextId = 1;

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioContext)
  // The destination node and AudioContext form a cycle and so the destination
  // track will be destroyed.  mWorklet must be shut down before the track
  // is destroyed.  Do this before clearing mWorklet.
  tmp->ShutdownWorklet();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDestination)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWorklet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromiseGripArray)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingResumePromises)
  if (tmp->mTracksAreSuspended || !tmp->mIsStarted) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mActiveNodes)
  }
  // mDecodeJobs owns the WebAudioDecodeJob objects whose lifetime is managed
  // explicitly. mAllNodes is an array of weak pointers, ignore it here.
  // mBasicWaveFormCache cannot participate in cycles, ignore it here.

  // Remove weak reference on the global window as the context is not usable
  // without mDestination.
  tmp->DisconnectFromWindow();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AudioContext,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDestination)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWorklet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromiseGripArray)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingResumePromises)
  if (tmp->mTracksAreSuspended || !tmp->mIsStarted) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActiveNodes)
  }
  // mDecodeJobs owns the WebAudioDecodeJob objects whose lifetime is managed
  // explicitly. mAllNodes is an array of weak pointers, ignore it here.
  // mBasicWaveFormCache cannot participate in cycles, ignore it here.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(AudioContext, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioContext, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioContext)
  NS_INTERFACE_MAP_ENTRY(nsIMemoryReporter)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

static float GetSampleRateForAudioContext(bool aIsOffline, float aSampleRate,
                                          bool aShouldResistFingerprinting) {
  if (aIsOffline || aSampleRate != 0.0) {
    return aSampleRate;
  } else {
    return static_cast<float>(
        CubebUtils::PreferredSampleRate(aShouldResistFingerprinting));
  }
}

AudioContext::AudioContext(nsPIDOMWindowInner* aWindow, bool aIsOffline,
                           uint32_t aNumberOfChannels, uint32_t aLength,
                           float aSampleRate)
    : DOMEventTargetHelper(aWindow),
      mId(gAudioContextId++),
      mSampleRate(GetSampleRateForAudioContext(
          aIsOffline, aSampleRate,
          aWindow->AsGlobal()->ShouldResistFingerprinting(
              RFPTarget::AudioSampleRate))),
      mAudioContextState(AudioContextState::Suspended),
      mNumberOfChannels(aNumberOfChannels),
      mRTPCallerType(aWindow->AsGlobal()->GetRTPCallerType()),
      mShouldResistFingerprinting(
          aWindow->AsGlobal()->ShouldResistFingerprinting(
              RFPTarget::AudioContext)),
      mIsOffline(aIsOffline),
      mIsStarted(!aIsOffline),
      mIsShutDown(false),
      mIsDisconnecting(false),
      mCloseCalled(false),
      // Realtime contexts start with suspended tracks until an
      // AudioCallbackDriver is running.
      mTracksAreSuspended(!aIsOffline),
      mWasAllowedToStart(true),
      mSuspendedByContent(false),
      mSuspendedByChrome(aWindow->IsSuspended()),
      mWasEverAllowedToStart(false),
      mWasEverBlockedToStart(false),
      mWouldBeAllowedToStart(true) {
  bool mute = aWindow->AddAudioContext(this);

  // Note: AudioDestinationNode needs an AudioContext that must already be
  // bound to the window.
  const bool allowedToStart = media::AutoplayPolicy::IsAllowedToPlay(*this);
  mDestination =
      new AudioDestinationNode(this, aIsOffline, aNumberOfChannels, aLength);
  mDestination->Init();
  // If an AudioContext is not allowed to start, we would postpone its state
  // transition from `suspended` to `running` until sites explicitly call
  // AudioContext.resume() or AudioScheduledSourceNode.start().
  if (!allowedToStart) {
    MOZ_ASSERT(!mIsOffline);
    AUTOPLAY_LOG("AudioContext %p is not allowed to start", this);
    ReportBlocked();
  } else if (!mIsOffline) {
    ResumeInternal();
  }

  // The context can't be muted until it has a destination.
  if (mute) {
    Mute();
  }

  UpdateAutoplayAssumptionStatus();

  FFTBlock::MainThreadInit();
}

void AudioContext::StartBlockedAudioContextIfAllowed() {
  MOZ_ASSERT(NS_IsMainThread());
  MaybeUpdateAutoplayTelemetry();
  // Only try to start AudioContext when AudioContext was not allowed to start.
  if (mWasAllowedToStart) {
    return;
  }

  const bool isAllowedToPlay = media::AutoplayPolicy::IsAllowedToPlay(*this);
  AUTOPLAY_LOG("Trying to start AudioContext %p, IsAllowedToPlay=%d", this,
               isAllowedToPlay);

  // Only start the AudioContext if this resume() call was initiated by content,
  // not if it was a result of the AudioContext starting after having been
  // blocked because of the auto-play policy.
  if (isAllowedToPlay && !mSuspendedByContent) {
    ResumeInternal();
  } else {
    ReportBlocked();
  }
}

void AudioContext::DisconnectFromWindow() {
  MaybeClearPageAwakeRequest();
  nsPIDOMWindowInner* window = GetOwner();
  if (window) {
    window->RemoveAudioContext(this);
  }
}

AudioContext::~AudioContext() {
  DisconnectFromWindow();
  UnregisterWeakMemoryReporter(this);
  MOZ_ASSERT(!mSetPageAwakeRequest, "forgot to revoke for page awake?");
}

JSObject* AudioContext::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  if (mIsOffline) {
    return OfflineAudioContext_Binding::Wrap(aCx, this, aGivenProto);
  } else {
    return AudioContext_Binding::Wrap(aCx, this, aGivenProto);
  }
}

static bool CheckFullyActive(nsPIDOMWindowInner* aWindow, ErrorResult& aRv) {
  if (!aWindow->IsFullyActive()) {
    aRv.ThrowInvalidStateError("The document is not fully active.");
    return false;
  }
  return true;
}

/* static */
already_AddRefed<AudioContext> AudioContext::Constructor(
    const GlobalObject& aGlobal, const AudioContextOptions& aOptions,
    ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  /**
   * If the current settings object’s responsible document is NOT fully
   * active, throw an InvalidStateError and abort these steps.
   */
  if (!CheckFullyActive(window, aRv)) {
    return nullptr;
  }

  if (aOptions.mSampleRate.WasPassed() &&
      (aOptions.mSampleRate.Value() < WebAudioUtils::MinSampleRate ||
       aOptions.mSampleRate.Value() > WebAudioUtils::MaxSampleRate)) {
    aRv.ThrowNotSupportedError(nsPrintfCString(
        "Sample rate %g is not in the range [%u, %u]",
        aOptions.mSampleRate.Value(), WebAudioUtils::MinSampleRate,
        WebAudioUtils::MaxSampleRate));
    return nullptr;
  }
  float sampleRate = aOptions.mSampleRate.WasPassed()
                         ? aOptions.mSampleRate.Value()
                         : MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE;

  RefPtr<AudioContext> object =
      new AudioContext(window, false, 2, 0, sampleRate);

  RegisterWeakMemoryReporter(object);

  return object.forget();
}

/* static */
already_AddRefed<AudioContext> AudioContext::Constructor(
    const GlobalObject& aGlobal, const OfflineAudioContextOptions& aOptions,
    ErrorResult& aRv) {
  return Constructor(aGlobal, aOptions.mNumberOfChannels, aOptions.mLength,
                     aOptions.mSampleRate, aRv);
}

/* static */
already_AddRefed<AudioContext> AudioContext::Constructor(
    const GlobalObject& aGlobal, uint32_t aNumberOfChannels, uint32_t aLength,
    float aSampleRate, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  /**
   * If the current settings object’s responsible document is NOT fully
   * active, throw an InvalidStateError and abort these steps.
   */
  if (!CheckFullyActive(window, aRv)) {
    return nullptr;
  }

  if (aNumberOfChannels == 0 ||
      aNumberOfChannels > WebAudioUtils::MaxChannelCount) {
    aRv.ThrowNotSupportedError(
        nsPrintfCString("%u is not a valid channel count", aNumberOfChannels));
    return nullptr;
  }

  if (aLength == 0) {
    aRv.ThrowNotSupportedError("Length must be nonzero");
    return nullptr;
  }

  if (aSampleRate < WebAudioUtils::MinSampleRate ||
      aSampleRate > WebAudioUtils::MaxSampleRate) {
    // The DOM binding protects us against infinity and NaN
    aRv.ThrowNotSupportedError(nsPrintfCString(
        "Sample rate %g is not in the range [%u, %u]", aSampleRate,
        WebAudioUtils::MinSampleRate, WebAudioUtils::MaxSampleRate));
    return nullptr;
  }

  RefPtr<AudioContext> object =
      new AudioContext(window, true, aNumberOfChannels, aLength, aSampleRate);

  RegisterWeakMemoryReporter(object);

  return object.forget();
}

already_AddRefed<AudioBufferSourceNode> AudioContext::CreateBufferSource() {
  return AudioBufferSourceNode::Create(nullptr, *this,
                                       AudioBufferSourceOptions());
}

already_AddRefed<ConstantSourceNode> AudioContext::CreateConstantSource() {
  RefPtr<ConstantSourceNode> constantSourceNode = new ConstantSourceNode(this);
  return constantSourceNode.forget();
}

already_AddRefed<AudioBuffer> AudioContext::CreateBuffer(
    uint32_t aNumberOfChannels, uint32_t aLength, float aSampleRate,
    ErrorResult& aRv) {
  if (!aNumberOfChannels) {
    aRv.ThrowNotSupportedError("Number of channels must be nonzero");
    return nullptr;
  }

  return AudioBuffer::Create(GetOwner(), aNumberOfChannels, aLength,
                             aSampleRate, aRv);
}

namespace {

bool IsValidBufferSize(uint32_t aBufferSize) {
  switch (aBufferSize) {
    case 0:  // let the implementation choose the buffer size
    case 256:
    case 512:
    case 1024:
    case 2048:
    case 4096:
    case 8192:
    case 16384:
      return true;
    default:
      return false;
  }
}

}  // namespace

already_AddRefed<MediaStreamAudioDestinationNode>
AudioContext::CreateMediaStreamDestination(ErrorResult& aRv) {
  return MediaStreamAudioDestinationNode::Create(*this, AudioNodeOptions(),
                                                 aRv);
}

already_AddRefed<ScriptProcessorNode> AudioContext::CreateScriptProcessor(
    uint32_t aBufferSize, uint32_t aNumberOfInputChannels,
    uint32_t aNumberOfOutputChannels, ErrorResult& aRv) {
  if (aNumberOfInputChannels == 0 && aNumberOfOutputChannels == 0) {
    aRv.ThrowIndexSizeError(
        "At least one of numberOfInputChannels and numberOfOutputChannels must "
        "be nonzero");
    return nullptr;
  }

  if (aNumberOfInputChannels > WebAudioUtils::MaxChannelCount) {
    aRv.ThrowIndexSizeError(nsPrintfCString(
        "%u is not a valid number of input channels", aNumberOfInputChannels));
    return nullptr;
  }

  if (aNumberOfOutputChannels > WebAudioUtils::MaxChannelCount) {
    aRv.ThrowIndexSizeError(
        nsPrintfCString("%u is not a valid number of output channels",
                        aNumberOfOutputChannels));
    return nullptr;
  }

  if (!IsValidBufferSize(aBufferSize)) {
    aRv.ThrowIndexSizeError(
        nsPrintfCString("%u is not a valid bufferSize", aBufferSize));
    return nullptr;
  }

  RefPtr<ScriptProcessorNode> scriptProcessor = new ScriptProcessorNode(
      this, aBufferSize, aNumberOfInputChannels, aNumberOfOutputChannels);
  return scriptProcessor.forget();
}

already_AddRefed<AnalyserNode> AudioContext::CreateAnalyser(ErrorResult& aRv) {
  return AnalyserNode::Create(*this, AnalyserOptions(), aRv);
}

already_AddRefed<StereoPannerNode> AudioContext::CreateStereoPanner(
    ErrorResult& aRv) {
  return StereoPannerNode::Create(*this, StereoPannerOptions(), aRv);
}

already_AddRefed<MediaElementAudioSourceNode>
AudioContext::CreateMediaElementSource(HTMLMediaElement& aMediaElement,
                                       ErrorResult& aRv) {
  MediaElementAudioSourceOptions options;
  options.mMediaElement = aMediaElement;

  return MediaElementAudioSourceNode::Create(*this, options, aRv);
}

already_AddRefed<MediaStreamAudioSourceNode>
AudioContext::CreateMediaStreamSource(DOMMediaStream& aMediaStream,
                                      ErrorResult& aRv) {
  MediaStreamAudioSourceOptions options;
  options.mMediaStream = aMediaStream;

  return MediaStreamAudioSourceNode::Create(*this, options, aRv);
}

already_AddRefed<MediaStreamTrackAudioSourceNode>
AudioContext::CreateMediaStreamTrackSource(MediaStreamTrack& aMediaStreamTrack,
                                           ErrorResult& aRv) {
  MediaStreamTrackAudioSourceOptions options;
  options.mMediaStreamTrack = aMediaStreamTrack;

  return MediaStreamTrackAudioSourceNode::Create(*this, options, aRv);
}

already_AddRefed<GainNode> AudioContext::CreateGain(ErrorResult& aRv) {
  return GainNode::Create(*this, GainOptions(), aRv);
}

already_AddRefed<WaveShaperNode> AudioContext::CreateWaveShaper(
    ErrorResult& aRv) {
  return WaveShaperNode::Create(*this, WaveShaperOptions(), aRv);
}

already_AddRefed<DelayNode> AudioContext::CreateDelay(double aMaxDelayTime,
                                                      ErrorResult& aRv) {
  DelayOptions options;
  options.mMaxDelayTime = aMaxDelayTime;
  return DelayNode::Create(*this, options, aRv);
}

already_AddRefed<PannerNode> AudioContext::CreatePanner(ErrorResult& aRv) {
  return PannerNode::Create(*this, PannerOptions(), aRv);
}

already_AddRefed<ConvolverNode> AudioContext::CreateConvolver(
    ErrorResult& aRv) {
  return ConvolverNode::Create(nullptr, *this, ConvolverOptions(), aRv);
}

already_AddRefed<ChannelSplitterNode> AudioContext::CreateChannelSplitter(
    uint32_t aNumberOfOutputs, ErrorResult& aRv) {
  ChannelSplitterOptions options;
  options.mNumberOfOutputs = aNumberOfOutputs;
  return ChannelSplitterNode::Create(*this, options, aRv);
}

already_AddRefed<ChannelMergerNode> AudioContext::CreateChannelMerger(
    uint32_t aNumberOfInputs, ErrorResult& aRv) {
  ChannelMergerOptions options;
  options.mNumberOfInputs = aNumberOfInputs;
  return ChannelMergerNode::Create(*this, options, aRv);
}

already_AddRefed<DynamicsCompressorNode> AudioContext::CreateDynamicsCompressor(
    ErrorResult& aRv) {
  return DynamicsCompressorNode::Create(*this, DynamicsCompressorOptions(),
                                        aRv);
}

already_AddRefed<BiquadFilterNode> AudioContext::CreateBiquadFilter(
    ErrorResult& aRv) {
  return BiquadFilterNode::Create(*this, BiquadFilterOptions(), aRv);
}

already_AddRefed<IIRFilterNode> AudioContext::CreateIIRFilter(
    const Sequence<double>& aFeedforward, const Sequence<double>& aFeedback,
    mozilla::ErrorResult& aRv) {
  IIRFilterOptions options;
  options.mFeedforward = aFeedforward;
  options.mFeedback = aFeedback;
  return IIRFilterNode::Create(*this, options, aRv);
}

already_AddRefed<OscillatorNode> AudioContext::CreateOscillator(
    ErrorResult& aRv) {
  return OscillatorNode::Create(*this, OscillatorOptions(), aRv);
}

already_AddRefed<PeriodicWave> AudioContext::CreatePeriodicWave(
    const Sequence<float>& aRealData, const Sequence<float>& aImagData,
    const PeriodicWaveConstraints& aConstraints, ErrorResult& aRv) {
  RefPtr<PeriodicWave> periodicWave = new PeriodicWave(
      this, aRealData.Elements(), aRealData.Length(), aImagData.Elements(),
      aImagData.Length(), aConstraints.mDisableNormalization, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return periodicWave.forget();
}

AudioListener* AudioContext::Listener() {
  if (!mListener) {
    mListener = new AudioListener(this);
  }
  return mListener;
}

double AudioContext::OutputLatency() {
  if (mIsShutDown) {
    return 0.0;
  }
  // When reduceFingerprinting is enabled, return a latency figure that is
  // fixed, but plausible for the platform.
  double latency_s = 0.0;
  if (mShouldResistFingerprinting) {
#ifdef XP_MACOSX
    latency_s = 512. / mSampleRate;
#elif MOZ_WIDGET_ANDROID
    latency_s = 0.020;
#elif XP_WIN
    latency_s = 0.04;
#else  // Catchall for other OSes, including Linux.
    latency_s = 0.025;
#endif
  } else {
    return Graph()->AudioOutputLatency();
  }
  return latency_s;
}

void AudioContext::GetOutputTimestamp(AudioTimestamp& aTimeStamp) {
  if (!Destination()) {
    aTimeStamp.mContextTime.Construct(0.0);
    aTimeStamp.mPerformanceTime.Construct(0.0);
    return;
  }

  // The currentTime currently being output is the currentTime minus the audio
  // output latency. The resolution of CurrentTime() is already reduced.
  aTimeStamp.mContextTime.Construct(
      std::max(0.0, CurrentTime() - OutputLatency()));
  nsPIDOMWindowInner* parent = GetParentObject();
  Performance* perf = parent ? parent->GetPerformance() : nullptr;
  if (perf) {
    // perf->Now() already has reduced resolution here, no need to do it again.
    aTimeStamp.mPerformanceTime.Construct(
        std::max(0., perf->Now() - (OutputLatency() * 1000.)));
  } else {
    aTimeStamp.mPerformanceTime.Construct(0.0);
  }
}

Worklet* AudioContext::GetAudioWorklet(ErrorResult& aRv) {
  if (!mWorklet) {
    mWorklet = AudioWorkletImpl::CreateWorklet(this, aRv);
  }

  return mWorklet;
}
bool AudioContext::IsRunning() const {
  return mAudioContextState == AudioContextState::Running;
}

already_AddRefed<Promise> AudioContext::CreatePromise(ErrorResult& aRv) {
  // Get the relevant global for the promise from the wrapper cache because
  // DOMEventTargetHelper::GetOwner() returns null if the document is unloaded.
  // We know the wrapper exists because it is being used for |this| from JS.
  // See https://github.com/heycam/webidl/issues/932 for why the relevant
  // global is used instead of the current global.
  nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(GetWrapper());
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  /**
   * If this's relevant global object's associated Document is not fully
   * active then return a promise rejected with "InvalidStateError"
   * DOMException.
   */
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
  if (!window->IsFullyActive()) {
    promise->MaybeRejectWithInvalidStateError(
        "The document is not fully active.");
  }
  return promise.forget();
}

already_AddRefed<Promise> AudioContext::DecodeAudioData(
    const ArrayBuffer& aBuffer,
    const Optional<OwningNonNull<DecodeSuccessCallback>>& aSuccessCallback,
    const Optional<OwningNonNull<DecodeErrorCallback>>& aFailureCallback,
    ErrorResult& aRv) {
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  // CheckedUnwrapStatic is OK, since we know we have an ArrayBuffer.
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrapStatic(aBuffer.Obj()));
  if (!obj) {
    aRv.ThrowSecurityError("Can't get audio data from cross-origin object");
    return nullptr;
  }

  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed() || promise->State() == Promise::PromiseState::Rejected) {
    return promise.forget();
  }

  JSAutoRealm ar(cx, obj);

  // Detach the array buffer
  size_t length = JS::GetArrayBufferByteLength(obj);
  uint8_t* data = static_cast<uint8_t*>(JS::StealArrayBufferContents(cx, obj));
  if (!data) {
    JS_ClearPendingException(cx);

    // Throw if the buffer is detached
    aRv.ThrowTypeError("Buffer argument can't be a detached buffer");
    return nullptr;
  }

  // Sniff the content of the media.
  // Failed type sniffing will be handled by AsyncDecodeWebAudio.
  nsAutoCString contentType;
  NS_SniffContent(NS_DATA_SNIFFER_CATEGORY, nullptr, data, length, contentType);

  RefPtr<DecodeErrorCallback> failureCallback;
  RefPtr<DecodeSuccessCallback> successCallback;
  if (aFailureCallback.WasPassed()) {
    failureCallback = &aFailureCallback.Value();
  }
  if (aSuccessCallback.WasPassed()) {
    successCallback = &aSuccessCallback.Value();
  }
  UniquePtr<WebAudioDecodeJob> job(
      new WebAudioDecodeJob(this, promise, successCallback, failureCallback));
  AsyncDecodeWebAudio(contentType.get(), data, length, *job);
  // Transfer the ownership to mDecodeJobs
  mDecodeJobs.AppendElement(std::move(job));

  return promise.forget();
}

void AudioContext::RemoveFromDecodeQueue(WebAudioDecodeJob* aDecodeJob) {
  // Since UniquePtr doesn't provide an operator== which allows you to compare
  // against raw pointers, we need to iterate manually.
  for (uint32_t i = 0; i < mDecodeJobs.Length(); ++i) {
    if (mDecodeJobs[i].get() == aDecodeJob) {
      mDecodeJobs.RemoveElementAt(i);
      break;
    }
  }
}

void AudioContext::RegisterActiveNode(AudioNode* aNode) {
  if (!mCloseCalled) {
    mActiveNodes.Insert(aNode);
  }
}

void AudioContext::UnregisterActiveNode(AudioNode* aNode) {
  mActiveNodes.Remove(aNode);
}

uint32_t AudioContext::MaxChannelCount() const {
  if (mShouldResistFingerprinting) {
    return 2;
  }
  return std::min<uint32_t>(
      WebAudioUtils::MaxChannelCount,
      mIsOffline ? mNumberOfChannels : CubebUtils::MaxNumberOfChannels());
}

uint32_t AudioContext::ActiveNodeCount() const { return mActiveNodes.Count(); }

MediaTrackGraph* AudioContext::Graph() const {
  return Destination()->Track()->Graph();
}

AudioNodeTrack* AudioContext::DestinationTrack() const {
  if (Destination()) {
    return Destination()->Track();
  }
  return nullptr;
}

void AudioContext::ShutdownWorklet() {
  if (mWorklet) {
    mWorklet->Impl()->NotifyWorkletFinished();
  }
}

double AudioContext::CurrentTime() {
  mozilla::MediaTrack* track = Destination()->Track();

  double rawTime = track->TrackTimeToSeconds(track->GetCurrentTime());

  // CurrentTime increments in intervals of 128/sampleRate. If the Timer
  // Precision Reduction is smaller than this interval, the jittered time
  // can always be reversed to the raw step of the interval. In that case
  // we can simply return the un-reduced time; and avoid breaking tests.
  // We have to convert each variable into a common magnitude, we choose ms.
  if ((128 / mSampleRate) * 1000.0 >
      nsRFPService::TimerResolution(mRTPCallerType) / 1000.0) {
    return rawTime;
  }

  // The value of a MediaTrack's CurrentTime will always advance forward; it
  // will never reset (even if one rewinds a video.) Therefore we can use a
  // single Random Seed initialized at the same time as the object.
  return nsRFPService::ReduceTimePrecisionAsSecs(
      rawTime, GetRandomTimelineSeed(), mRTPCallerType);
}

nsISerialEventTarget* AudioContext::GetMainThread() const {
  if (nsPIDOMWindowInner* window = GetParentObject()) {
    return window->AsGlobal()->SerialEventTarget();
  }
  return GetCurrentSerialEventTarget();
}

void AudioContext::DisconnectFromOwner() {
  mIsDisconnecting = true;
  MaybeClearPageAwakeRequest();
  OnWindowDestroy();
  DOMEventTargetHelper::DisconnectFromOwner();
}

void AudioContext::OnWindowDestroy() {
  // Avoid resend the Telemetry data.
  if (!mIsShutDown) {
    MaybeUpdateAutoplayTelemetryWhenShutdown();
  }
  mIsShutDown = true;

  CloseInternal(nullptr, AudioContextOperationFlags::None);

  // We don't want to touch promises if the global is going away soon.
  if (!mIsDisconnecting) {
    for (auto p : mPromiseGripArray) {
      p->MaybeRejectWithInvalidStateError("Navigated away from page");
    }

    mPromiseGripArray.Clear();

    for (const auto& p : mPendingResumePromises) {
      p->MaybeRejectWithInvalidStateError("Navigated away from page");
    }
    mPendingResumePromises.Clear();
  }

  // On process shutdown, the MTG thread shuts down before the destination
  // track is destroyed, but AudioWorklet needs to release objects on the MTG
  // thread.  AudioContext::Shutdown() is invoked on processing the
  // PBrowser::Destroy() message before xpcom shutdown begins.
  ShutdownWorklet();

  if (mDestination) {
    // We can destroy the MediaTrackGraph at this point.
    // Although there may be other clients using the graph, this graph is used
    // only for clients in the same window and this window is going away.
    // This will also interrupt any worklet script still running on the graph
    // thread.
    Graph()->ForceShutDown();
    // AudioDestinationNodes on rendering offline contexts have a
    // self-reference which needs removal.
    if (mIsOffline) {
      mDestination->OfflineShutdown();
    }
  }
}

/* This runnable allows to fire the "statechange" event */
class OnStateChangeTask final : public Runnable {
 public:
  explicit OnStateChangeTask(AudioContext* aAudioContext)
      : Runnable("dom::OnStateChangeTask"), mAudioContext(aAudioContext) {}

  NS_IMETHODIMP
  Run() override {
    nsPIDOMWindowInner* parent = mAudioContext->GetParentObject();
    if (!parent) {
      return NS_ERROR_FAILURE;
    }

    Document* doc = parent->GetExtantDoc();
    if (!doc) {
      return NS_ERROR_FAILURE;
    }

    return nsContentUtils::DispatchTrustedEvent(
        doc, mAudioContext, u"statechange"_ns, CanBubble::eNo, Cancelable::eNo);
  }

 private:
  RefPtr<AudioContext> mAudioContext;
};

void AudioContext::Dispatch(already_AddRefed<nsIRunnable>&& aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  // It can happen that this runnable took a long time to reach the main thread,
  // and the global is not valid anymore.
  if (GetParentObject()) {
    AbstractThread::MainThread()->Dispatch(std::move(aRunnable));
  } else {
    RefPtr<nsIRunnable> runnable(aRunnable);
    runnable = nullptr;
  }
}

void AudioContext::OnStateChanged(void* aPromise, AudioContextState aNewState) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mAudioContextState == AudioContextState::Closed) {
    fprintf(stderr,
            "Invalid transition: mAudioContextState: %d -> aNewState %d\n",
            static_cast<int>(mAudioContextState), static_cast<int>(aNewState));
    MOZ_ASSERT(false);
  }

  if (aPromise) {
    Promise* promise = reinterpret_cast<Promise*>(aPromise);
    // It is possible for the promise to have been removed from
    // mPromiseGripArray if the cycle collector has severed our connections. DO
    // NOT dereference the promise pointer in that case since it may point to
    // already freed memory.
    if (mPromiseGripArray.Contains(promise)) {
      promise->MaybeResolveWithUndefined();
      DebugOnly<bool> rv = mPromiseGripArray.RemoveElement(promise);
      MOZ_ASSERT(rv, "Promise wasn't in the grip array?");
    }
  }

  // Resolve all pending promises once the audio context has been allowed to
  // start.
  if (aNewState == AudioContextState::Running) {
    for (const auto& p : mPendingResumePromises) {
      p->MaybeResolveWithUndefined();
    }
    mPendingResumePromises.Clear();
  }

  if (mAudioContextState != aNewState) {
    RefPtr<OnStateChangeTask> task = new OnStateChangeTask(this);
    Dispatch(task.forget());
  }

  mAudioContextState = aNewState;
  Destination()->NotifyAudioContextStateChanged();
  MaybeUpdatePageAwakeRequest();
}

BrowsingContext* AudioContext::GetTopLevelBrowsingContext() {
  nsCOMPtr<nsPIDOMWindowInner> window = GetParentObject();
  if (!window) {
    return nullptr;
  }
  BrowsingContext* bc = window->GetBrowsingContext();
  if (!bc || bc->IsDiscarded()) {
    return nullptr;
  }
  return bc->Top();
}

void AudioContext::MaybeUpdatePageAwakeRequest() {
  // No need to keep page awake for offline context.
  if (IsOffline()) {
    return;
  }

  if (mIsShutDown) {
    return;
  }

  if (IsRunning() && !mSetPageAwakeRequest) {
    SetPageAwakeRequest(true);
  } else if (!IsRunning() && mSetPageAwakeRequest) {
    SetPageAwakeRequest(false);
  }
}

void AudioContext::SetPageAwakeRequest(bool aShouldSet) {
  mSetPageAwakeRequest = aShouldSet;
  BrowsingContext* bc = GetTopLevelBrowsingContext();
  if (!bc) {
    return;
  }
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendAddOrRemovePageAwakeRequest(bc, aShouldSet);
    return;
  }
  if (aShouldSet) {
    bc->Canonical()->AddPageAwakeRequest();
  } else {
    bc->Canonical()->RemovePageAwakeRequest();
  }
}

void AudioContext::MaybeClearPageAwakeRequest() {
  if (mSetPageAwakeRequest) {
    SetPageAwakeRequest(false);
  }
}

nsTArray<RefPtr<mozilla::MediaTrack>> AudioContext::GetAllTracks() const {
  nsTArray<RefPtr<mozilla::MediaTrack>> tracks;
  for (AudioNode* node : mAllNodes) {
    mozilla::MediaTrack* t = node->GetTrack();
    if (t) {
      tracks.AppendElement(t);
    }
    // Add the tracks of AudioParam.
    const nsTArray<RefPtr<AudioParam>>& audioParams = node->GetAudioParams();
    if (!audioParams.IsEmpty()) {
      for (auto& param : audioParams) {
        t = param->GetTrack();
        if (t && !tracks.Contains(t)) {
          tracks.AppendElement(t);
        }
      }
    }
  }
  return tracks;
}

already_AddRefed<Promise> AudioContext::Suspend(ErrorResult& aRv) {
  TRACE("AudioContext::Suspend");
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed() || promise->State() == Promise::PromiseState::Rejected) {
    return promise.forget();
  }
  if (mIsOffline) {
    // XXXbz This is not reachable, since we don't implement this
    // method on OfflineAudioContext at all!
    promise->MaybeRejectWithNotSupportedError(
        "Can't suspend OfflineAudioContext yet");
    return promise.forget();
  }

  if (mCloseCalled) {
    promise->MaybeRejectWithInvalidStateError(
        "Can't suspend if the control thread state is \"closed\"");
    return promise.forget();
  }

  mSuspendedByContent = true;
  mPromiseGripArray.AppendElement(promise);
  SuspendInternal(promise, AudioContextOperationFlags::SendStateChange);
  return promise.forget();
}

void AudioContext::SuspendFromChrome() {
  if (mIsOffline || mIsShutDown) {
    return;
  }
  MOZ_ASSERT(!mSuspendedByChrome);
  mSuspendedByChrome = true;
  SuspendInternal(nullptr, Preferences::GetBool("dom.audiocontext.testing")
                               ? AudioContextOperationFlags::SendStateChange
                               : AudioContextOperationFlags::None);
}

void AudioContext::SuspendInternal(void* aPromise,
                                   AudioContextOperationFlags aFlags) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsOffline);
  Destination()->Suspend();

  nsTArray<RefPtr<mozilla::MediaTrack>> tracks;
  // If mTracksAreSuspended is true then we already suspended all our tracks,
  // so don't suspend them again (since suspend(); suspend(); resume(); should
  // cancel both suspends). But we still need to do ApplyAudioContextOperation
  // to ensure our new promise is resolved.
  if (!mTracksAreSuspended) {
    mTracksAreSuspended = true;
    tracks = GetAllTracks();
  }
  auto promise = Graph()->ApplyAudioContextOperation(
      DestinationTrack(), std::move(tracks), AudioContextOperation::Suspend);
  if ((aFlags & AudioContextOperationFlags::SendStateChange)) {
    promise->Then(
        GetMainThread(), "AudioContext::OnStateChanged",
        [self = RefPtr<AudioContext>(this),
         aPromise](AudioContextState aNewState) {
          self->OnStateChanged(aPromise, aNewState);
        },
        [] { MOZ_CRASH("Unexpected rejection"); });
  }
}

void AudioContext::ResumeFromChrome() {
  if (mIsOffline || mIsShutDown) {
    return;
  }
  MOZ_ASSERT(mSuspendedByChrome);
  mSuspendedByChrome = false;
  if (!mWasAllowedToStart) {
    return;
  }
  ResumeInternal();
}

already_AddRefed<Promise> AudioContext::Resume(ErrorResult& aRv) {
  TRACE("AudioContext::Resume");
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed() || promise->State() == Promise::PromiseState::Rejected) {
    return promise.forget();
  }

  if (mIsOffline) {
    promise->MaybeRejectWithNotSupportedError(
        "Can't resume OfflineAudioContext");
    return promise.forget();
  }

  if (mCloseCalled) {
    promise->MaybeRejectWithInvalidStateError(
        "Can't resume if the control thread state is \"closed\"");
    return promise.forget();
  }

  mSuspendedByContent = false;
  mPendingResumePromises.AppendElement(promise);

  const bool isAllowedToPlay = media::AutoplayPolicy::IsAllowedToPlay(*this);
  AUTOPLAY_LOG("Trying to resume AudioContext %p, IsAllowedToPlay=%d", this,
               isAllowedToPlay);
  if (isAllowedToPlay) {
    ResumeInternal();
  } else {
    ReportBlocked();
  }

  MaybeUpdateAutoplayTelemetry();

  return promise.forget();
}

void AudioContext::ResumeInternal() {
  MOZ_ASSERT(!mIsOffline);
  AUTOPLAY_LOG("Allow to resume AudioContext %p", this);
  mWasAllowedToStart = true;

  if (mSuspendedByChrome || mSuspendedByContent || mCloseCalled) {
    MOZ_ASSERT(mTracksAreSuspended);
    return;
  }

  Destination()->Resume();

  nsTArray<RefPtr<mozilla::MediaTrack>> tracks;
  // If mTracksAreSuspended is false then we already resumed all our tracks,
  // so don't resume them again (since suspend(); resume(); resume(); should
  // be OK). But we still need to do ApplyAudioContextOperation
  // to ensure our new promise is resolved.
  if (mTracksAreSuspended) {
    mTracksAreSuspended = false;
    tracks = GetAllTracks();
  }
  // Check for statechange even when resumed from chrome because content may
  // have called Resume() before chrome resumed the window.
  Graph()
      ->ApplyAudioContextOperation(DestinationTrack(), std::move(tracks),
                                   AudioContextOperation::Resume)
      ->Then(
          GetMainThread(), "AudioContext::OnStateChanged",
          [self = RefPtr<AudioContext>(this)](AudioContextState aNewState) {
            self->OnStateChanged(nullptr, aNewState);
          },
          [] {});  // Promise may be rejected after graph shutdown.
}

void AudioContext::UpdateAutoplayAssumptionStatus() {
  if (media::AutoplayPolicyTelemetryUtils::
          WouldBeAllowedToPlayIfAutoplayDisabled(*this)) {
    mWasEverAllowedToStart |= true;
    mWouldBeAllowedToStart = true;
  } else {
    mWasEverBlockedToStart |= true;
    mWouldBeAllowedToStart = false;
  }
}

void AudioContext::MaybeUpdateAutoplayTelemetry() {
  // Exclude offline AudioContext because it's always allowed to start.
  if (mIsOffline) {
    return;
  }

  if (media::AutoplayPolicyTelemetryUtils::
          WouldBeAllowedToPlayIfAutoplayDisabled(*this) &&
      !mWouldBeAllowedToStart) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_WEB_AUDIO_AUTOPLAY::AllowedAfterBlocked);
  }
  UpdateAutoplayAssumptionStatus();
}

void AudioContext::MaybeUpdateAutoplayTelemetryWhenShutdown() {
  // Exclude offline AudioContext because it's always allowed to start.
  if (mIsOffline) {
    return;
  }

  if (mWasEverAllowedToStart && !mWasEverBlockedToStart) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_WEB_AUDIO_AUTOPLAY::NeverBlocked);
  } else if (!mWasEverAllowedToStart && mWasEverBlockedToStart) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_WEB_AUDIO_AUTOPLAY::NeverAllowed);
  }
}

void AudioContext::ReportBlocked() {
  ReportToConsole(nsIScriptError::warningFlag,
                  "BlockAutoplayWebAudioStartError");
  mWasAllowedToStart = false;

  if (!StaticPrefs::media_autoplay_block_event_enabled()) {
    return;
  }

  RefPtr<AudioContext> self = this;
  RefPtr<nsIRunnable> r =
      NS_NewRunnableFunction("AudioContext::AutoplayBlocked", [self]() {
        nsPIDOMWindowInner* parent = self->GetParentObject();
        if (!parent) {
          return;
        }

        Document* doc = parent->GetExtantDoc();
        if (!doc) {
          return;
        }

        AUTOPLAY_LOG("Dispatch `blocked` event for AudioContext %p",
                     self.get());
        nsContentUtils::DispatchTrustedEvent(doc, self, u"blocked"_ns,
                                             CanBubble::eNo, Cancelable::eNo);
      });
  Dispatch(r.forget());
}

already_AddRefed<Promise> AudioContext::Close(ErrorResult& aRv) {
  TRACE("AudioContext::Close");
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed() || promise->State() == Promise::PromiseState::Rejected) {
    return promise.forget();
  }

  if (mIsOffline) {
    // XXXbz This is not reachable, since we don't implement this
    // method on OfflineAudioContext at all!
    promise->MaybeRejectWithNotSupportedError(
        "Can't close OfflineAudioContext yet");
    return promise.forget();
  }

  if (mCloseCalled) {
    promise->MaybeRejectWithInvalidStateError(
        "Can't close an AudioContext twice");
    return promise.forget();
  }

  mPromiseGripArray.AppendElement(promise);

  CloseInternal(promise, AudioContextOperationFlags::SendStateChange);

  return promise.forget();
}

void AudioContext::OfflineClose() {
  CloseInternal(nullptr, AudioContextOperationFlags::None);
}

void AudioContext::CloseInternal(void* aPromise,
                                 AudioContextOperationFlags aFlags) {
  // This can be called when freeing a document, and the tracks are dead at
  // this point, so we need extra null-checks.
  AudioNodeTrack* ds = DestinationTrack();
  if (ds && !mIsOffline) {
    Destination()->Close();

    nsTArray<RefPtr<mozilla::MediaTrack>> tracks;
    // If mTracksAreSuspended or mCloseCalled are true then we already suspended
    // all our tracks, so don't suspend them again. But we still need to do
    // ApplyAudioContextOperation to ensure our new promise is resolved.
    if (!mTracksAreSuspended && !mCloseCalled) {
      tracks = GetAllTracks();
    }
    auto promise = Graph()->ApplyAudioContextOperation(
        ds, std::move(tracks), AudioContextOperation::Close);
    if ((aFlags & AudioContextOperationFlags::SendStateChange)) {
      promise->Then(
          GetMainThread(), "AudioContext::OnStateChanged",
          [self = RefPtr<AudioContext>(this),
           aPromise](AudioContextState aNewState) {
            self->OnStateChanged(aPromise, aNewState);
          },
          [] {});  // Promise may be rejected after graph shutdown.
    }
  }
  mCloseCalled = true;
  // Release references to active nodes.
  // Active AudioNodes don't unregister in destructors, at which point the
  // Node is already unregistered.
  mActiveNodes.Clear();
}

void AudioContext::RegisterNode(AudioNode* aNode) {
  MOZ_ASSERT(!mAllNodes.Contains(aNode));
  mAllNodes.Insert(aNode);
}

void AudioContext::UnregisterNode(AudioNode* aNode) {
  MOZ_ASSERT(mAllNodes.Contains(aNode));
  mAllNodes.Remove(aNode);
}

already_AddRefed<Promise> AudioContext::StartRendering(ErrorResult& aRv) {
  MOZ_ASSERT(mIsOffline, "This should only be called on OfflineAudioContext");
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed() || promise->State() == Promise::PromiseState::Rejected) {
    return promise.forget();
  }
  if (mIsStarted) {
    aRv.ThrowInvalidStateError("Rendering already started");
    return nullptr;
  }

  mIsStarted = true;
  mDestination->StartRendering(promise);

  OnStateChanged(nullptr, AudioContextState::Running);

  return promise.forget();
}

unsigned long AudioContext::Length() {
  MOZ_ASSERT(mIsOffline);
  return mDestination->Length();
}

void AudioContext::Mute() const {
  MOZ_ASSERT(!mIsOffline);
  if (mDestination) {
    mDestination->Mute();
  }
}

void AudioContext::Unmute() const {
  MOZ_ASSERT(!mIsOffline);
  if (mDestination) {
    mDestination->Unmute();
  }
}

void AudioContext::SetParamMapForWorkletName(
    const nsAString& aName, AudioParamDescriptorMap* aParamMap) {
  MOZ_ASSERT(!mWorkletParamDescriptors.Contains(aName));
  Unused << mWorkletParamDescriptors.InsertOrUpdate(
      aName, std::move(*aParamMap), fallible);
}

size_t AudioContext::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  // AudioNodes are tracked separately because we do not want the AudioContext
  // to track all of the AudioNodes it creates, so we wouldn't be able to
  // traverse them from here.

  size_t amount = aMallocSizeOf(this);
  if (mListener) {
    amount += mListener->SizeOfIncludingThis(aMallocSizeOf);
  }
  amount += mDecodeJobs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mDecodeJobs.Length(); ++i) {
    amount += mDecodeJobs[i]->SizeOfIncludingThis(aMallocSizeOf);
  }
  amount += mActiveNodes.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

NS_IMETHODIMP
AudioContext::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize) {
  const nsLiteralCString nodeDescription(
      "Memory used by AudioNode DOM objects (Web Audio).");
  for (AudioNode* node : mAllNodes) {
    int64_t amount = node->SizeOfIncludingThis(MallocSizeOf);
    nsPrintfCString domNodePath("explicit/webaudio/audio-node/%s/dom-nodes",
                                node->NodeType());
    aHandleReport->Callback(""_ns, domNodePath, KIND_HEAP, UNITS_BYTES, amount,
                            nodeDescription, aData);
  }

  int64_t amount = SizeOfIncludingThis(MallocSizeOf);
  MOZ_COLLECT_REPORT("explicit/webaudio/audiocontext", KIND_HEAP, UNITS_BYTES,
                     amount,
                     "Memory used by AudioContext objects (Web Audio).");

  return NS_OK;
}

BasicWaveFormCache* AudioContext::GetBasicWaveFormCache() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mBasicWaveFormCache) {
    mBasicWaveFormCache = new BasicWaveFormCache(SampleRate());
  }
  return mBasicWaveFormCache;
}

void AudioContext::ReportToConsole(uint32_t aErrorFlags,
                                   const char* aMsg) const {
  MOZ_ASSERT(aMsg);
  Document* doc =
      GetParentObject() ? GetParentObject()->GetExtantDoc() : nullptr;
  nsContentUtils::ReportToConsole(aErrorFlags, "Media"_ns, doc,
                                  nsContentUtils::eDOM_PROPERTIES, aMsg);
}

BasicWaveFormCache::BasicWaveFormCache(uint32_t aSampleRate)
    : mSampleRate(aSampleRate) {
  MOZ_ASSERT(NS_IsMainThread());
}
BasicWaveFormCache::~BasicWaveFormCache() = default;

WebCore::PeriodicWave* BasicWaveFormCache::GetBasicWaveForm(
    OscillatorType aType) {
  MOZ_ASSERT(!NS_IsMainThread());
  if (aType == OscillatorType::Sawtooth) {
    if (!mSawtooth) {
      mSawtooth = WebCore::PeriodicWave::createSawtooth(mSampleRate);
    }
    return mSawtooth;
  }
  if (aType == OscillatorType::Square) {
    if (!mSquare) {
      mSquare = WebCore::PeriodicWave::createSquare(mSampleRate);
    }
    return mSquare;
  }
  if (aType == OscillatorType::Triangle) {
    if (!mTriangle) {
      mTriangle = WebCore::PeriodicWave::createTriangle(mSampleRate);
    }
    return mTriangle;
  }
  MOZ_ASSERT(false, "Not reached");
  return nullptr;
}

}  // namespace mozilla::dom
