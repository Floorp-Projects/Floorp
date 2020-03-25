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
#include "mozilla/dom/ChannelMergerNodeBinding.h"
#include "mozilla/dom/ChannelSplitterNodeBinding.h"
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

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

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
  if (tmp->mSuspendCalled || !tmp->mIsStarted) {
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
  if (tmp->mSuspendCalled || !tmp->mIsStarted) {
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

static float GetSampleRateForAudioContext(bool aIsOffline, float aSampleRate) {
  if (aIsOffline || aSampleRate != 0.0) {
    return aSampleRate;
  } else {
    float rate = static_cast<float>(CubebUtils::PreferredSampleRate());
    if (StaticPrefs::privacy_resistFingerprinting()) {
      return 44100.f;
    }
    return rate;
  }
}

AudioContext::AudioContext(nsPIDOMWindowInner* aWindow, bool aIsOffline,
                           uint32_t aNumberOfChannels, uint32_t aLength,
                           float aSampleRate)
    : DOMEventTargetHelper(aWindow),
      mId(gAudioContextId++),
      mSampleRate(GetSampleRateForAudioContext(aIsOffline, aSampleRate)),
      mAudioContextState(AudioContextState::Suspended),
      mNumberOfChannels(aNumberOfChannels),
      mIsOffline(aIsOffline),
      mIsStarted(!aIsOffline),
      mIsShutDown(false),
      mCloseCalled(false),
      mSuspendCalled(false),
      mIsDisconnecting(false),
      mWasAllowedToStart(true),
      mSuspendedByContent(false),
      mWasEverAllowedToStart(false),
      mWasEverBlockedToStart(false),
      mWouldBeAllowedToStart(true) {
  bool mute = aWindow->AddAudioContext(this);

  // Note: AudioDestinationNode needs an AudioContext that must already be
  // bound to the window.
  const bool allowedToStart = AutoplayPolicy::IsAllowedToPlay(*this);
  // If an AudioContext is not allowed to start, we would postpone its state
  // transition from `suspended` to `running` until sites explicitly call
  // AudioContext.resume() or AudioScheduledSourceNode.start().
  if (!allowedToStart) {
    AUTOPLAY_LOG("AudioContext %p is not allowed to start", this);
    mSuspendCalled = true;
    ReportBlocked();
  }
  mDestination = new AudioDestinationNode(this, aIsOffline, allowedToStart,
                                          aNumberOfChannels, aLength);

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

  const bool isAllowedToPlay = AutoplayPolicy::IsAllowedToPlay(*this);
  AUTOPLAY_LOG("Trying to start AudioContext %p, IsAllowedToPlay=%d", this,
               isAllowedToPlay);

  // Only start the AudioContext if this resume() call was initiated by content,
  // not if it was a result of the AudioContext starting after having been
  // blocked because of the auto-play policy.
  if (isAllowedToPlay && !mSuspendedByContent) {
    ResumeInternal(AudioContextOperationFlags::SendStateChange);
  } else {
    ReportBlocked();
  }
}

nsresult AudioContext::Init() {
  if (!mIsOffline) {
    nsresult rv = mDestination->CreateAudioChannelAgent();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void AudioContext::DisconnectFromWindow() {
  nsPIDOMWindowInner* window = GetOwner();
  if (window) {
    window->RemoveAudioContext(this);
  }
}

AudioContext::~AudioContext() {
  DisconnectFromWindow();
  UnregisterWeakMemoryReporter(this);
}

JSObject* AudioContext::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  if (mIsOffline) {
    return OfflineAudioContext_Binding::Wrap(aCx, this, aGivenProto);
  } else {
    return AudioContext_Binding::Wrap(aCx, this, aGivenProto);
  }
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

  float sampleRate = MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE;
  if (aOptions.mSampleRate > 0 &&
      (aOptions.mSampleRate - WebAudioUtils::MinSampleRate < 0.0 ||
       WebAudioUtils::MaxSampleRate - aOptions.mSampleRate < 0.0)) {
    aRv.ThrowNotSupportedError(nsPrintfCString(
        "Sample rate %g is not in the range [%u, %u]", aOptions.mSampleRate,
        WebAudioUtils::MinSampleRate, WebAudioUtils::MaxSampleRate));
    return nullptr;
  }
  sampleRate = aOptions.mSampleRate;

  RefPtr<AudioContext> object =
      new AudioContext(window, false, 2, 0, sampleRate);
  aRv = object->Init();
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

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
    const Float32Array& aRealData, const Float32Array& aImagData,
    const PeriodicWaveConstraints& aConstraints, ErrorResult& aRv) {
  aRealData.ComputeState();
  aImagData.ComputeState();

  if (aRealData.Length() != aImagData.Length()) {
    aRv.ThrowIndexSizeError("\"real\" and \"imag\" must be the same length");
    return nullptr;
  }

  if (aRealData.Length() == 0) {
    aRv.ThrowIndexSizeError("\"real\" and \"imag\" are both empty arrays");
    return nullptr;
  }

  RefPtr<PeriodicWave> periodicWave = new PeriodicWave(
      this, aRealData.Data(), aImagData.Data(), aImagData.Length(),
      aConstraints.mDisableNormalization, aRv);
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
  // When reduceFingerprinting is enabled, return a latency figure that is
  // fixed, but plausible for the platform.
  double latency_s = 0.0;
  if (StaticPrefs::privacy_resistFingerprinting()) {
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

already_AddRefed<Promise> AudioContext::DecodeAudioData(
    const ArrayBuffer& aBuffer,
    const Optional<OwningNonNull<DecodeSuccessCallback>>& aSuccessCallback,
    const Optional<OwningNonNull<DecodeErrorCallback>>& aFailureCallback,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise;
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  // CheckedUnwrapStatic is OK, since we know we have an ArrayBuffer.
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrapStatic(aBuffer.Obj()));
  if (!obj) {
    aRv.ThrowSecurityError("Can't get audio data from cross-origin object");
    return nullptr;
  }

  JSAutoRealm ar(cx, obj);

  promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  aBuffer.ComputeState();

  if (!aBuffer.Data()) {
    // Throw if the buffer is detached
    aRv.ThrowTypeError("Buffer argument can't be a detached buffer");
    return nullptr;
  }

  // Detach the array buffer
  size_t length = aBuffer.Length();

  uint8_t* data = static_cast<uint8_t*>(JS::StealArrayBufferContents(cx, obj));

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
    mActiveNodes.PutEntry(aNode);
  }
}

void AudioContext::UnregisterActiveNode(AudioNode* aNode) {
  mActiveNodes.RemoveEntry(aNode);
}

uint32_t AudioContext::MaxChannelCount() const {
  if (StaticPrefs::privacy_resistFingerprinting()) {
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
  if ((128 / mSampleRate) * 1000.0 > nsRFPService::TimerResolution() / 1000.0) {
    return rawTime;
  }

  // The value of a MediaTrack's CurrentTime will always advance forward; it
  // will never reset (even if one rewinds a video.) Therefore we can use a
  // single Random Seed initialized at the same time as the object.
  return nsRFPService::ReduceTimePrecisionAsSecs(rawTime,
                                                 GetRandomTimelineSeed());
}

nsISerialEventTarget* AudioContext::GetMainThread() const {
  if (nsPIDOMWindowInner* window = GetParentObject()) {
    return window->AsGlobal()->EventTargetFor(TaskCategory::Other);
  }

  return GetCurrentThreadSerialEventTarget();
}

void AudioContext::DisconnectFromOwner() {
  mIsDisconnecting = true;
  Shutdown();
  DOMEventTargetHelper::DisconnectFromOwner();
}

void AudioContext::BindToOwner(nsIGlobalObject* aNew) {
  auto scopeExit =
      MakeScopeExit([&] { DOMEventTargetHelper::BindToOwner(aNew); });

  if (GetOwner()) {
    GetOwner()->RemoveAudioContext(this);
  }

  nsCOMPtr<nsPIDOMWindowInner> newWindow = do_QueryInterface(aNew);
  if (newWindow) {
    newWindow->AddAudioContext(this);
  }
}

void AudioContext::Shutdown() {
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

  // For offline contexts, we can destroy the MediaTrackGraph at this point.
  if (mIsOffline && mDestination) {
    mDestination->OfflineShutdown();
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
        doc, static_cast<DOMEventTargetHelper*>(mAudioContext),
        NS_LITERAL_STRING("statechange"), CanBubble::eNo, Cancelable::eNo);
  }

 private:
  RefPtr<AudioContext> mAudioContext;
};

void AudioContext::Dispatch(already_AddRefed<nsIRunnable>&& aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  // It can happen that this runnable took a long time to reach the main thread,
  // and the global is not valid anymore.
  if (parentObject) {
    parentObject->AbstractMainThreadFor(TaskCategory::Other)
        ->Dispatch(std::move(aRunnable));
  } else {
    RefPtr<nsIRunnable> runnable(aRunnable);
    runnable = nullptr;
  }
}

void AudioContext::OnStateChanged(void* aPromise, AudioContextState aNewState) {
  MOZ_ASSERT(NS_IsMainThread());

  // This can happen if close() was called right after creating the
  // AudioContext, before the context has switched to "running".
  if (mAudioContextState == AudioContextState::Closed &&
      aNewState == AudioContextState::Running && !aPromise) {
    return;
  }

  // This can happen if this is called in reaction to a
  // MediaTrackGraph shutdown, and a AudioContext was being
  // suspended at the same time, for example if a page was being
  // closed.
  if (mAudioContextState == AudioContextState::Closed &&
      aNewState == AudioContextState::Suspended) {
    return;
  }

#ifndef WIN32  // Bug 1170547
#  ifndef XP_MACOSX
#    ifdef DEBUG

  if (!((mAudioContextState == AudioContextState::Suspended &&
         aNewState == AudioContextState::Running) ||
        (mAudioContextState == AudioContextState::Running &&
         aNewState == AudioContextState::Suspended) ||
        (mAudioContextState == AudioContextState::Running &&
         aNewState == AudioContextState::Closed) ||
        (mAudioContextState == AudioContextState::Suspended &&
         aNewState == AudioContextState::Closed) ||
        (mAudioContextState == aNewState))) {
    fprintf(stderr,
            "Invalid transition: mAudioContextState: %d -> aNewState %d\n",
            static_cast<int>(mAudioContextState), static_cast<int>(aNewState));
    MOZ_ASSERT(false);
  }

#    endif  // DEBUG
#  endif    // XP_MACOSX
#endif      // WIN32

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
}

nsTArray<mozilla::MediaTrack*> AudioContext::GetAllTracks() const {
  nsTArray<mozilla::MediaTrack*> tracks;
  for (auto iter = mAllNodes.ConstIter(); !iter.Done(); iter.Next()) {
    AudioNode* node = iter.Get()->GetKey();
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
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise;
  promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  if (mIsOffline) {
    // XXXbz This is not reachable, since we don't implement this
    // method on OfflineAudioContext at all!
    promise->MaybeRejectWithNotSupportedError(
        "Can't suspend OfflineAudioContext yet");
    return promise.forget();
  }

  if (mAudioContextState == AudioContextState::Closed || mCloseCalled) {
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
  SuspendInternal(nullptr, AudioContextOperationFlags::None);
}

void AudioContext::SuspendInternal(void* aPromise,
                                   AudioContextOperationFlags aFlags) {
  MOZ_ASSERT(NS_IsMainThread());
  Destination()->Suspend();

  nsTArray<mozilla::MediaTrack*> tracks;
  // If mSuspendCalled is true then we already suspended all our tracks,
  // so don't suspend them again (since suspend(); suspend(); resume(); should
  // cancel both suspends). But we still need to do ApplyAudioContextOperation
  // to ensure our new promise is resolved.
  if (!mSuspendCalled) {
    tracks = GetAllTracks();
  }
  auto promise = Graph()->ApplyAudioContextOperation(
      DestinationTrack(), tracks, AudioContextOperation::Suspend);
  if ((aFlags & AudioContextOperationFlags::SendStateChange)) {
    promise->Then(
        GetMainThread(), "AudioContext::OnStateChanged",
        [self = RefPtr<AudioContext>(this),
         aPromise](AudioContextState aNewState) {
          self->OnStateChanged(aPromise, aNewState);
        },
        [] { MOZ_CRASH("Unexpected rejection"); });
  }

  mSuspendCalled = true;
}

void AudioContext::ResumeFromChrome() {
  if (mIsOffline || mIsShutDown) {
    return;
  }
  ResumeInternal(AudioContextOperationFlags::None);
}

already_AddRefed<Promise> AudioContext::Resume(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise;
  promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mIsOffline) {
    promise->MaybeRejectWithNotSupportedError(
        "Can't resume OfflineAudioContext");
    return promise.forget();
  }

  if (mAudioContextState == AudioContextState::Closed || mCloseCalled) {
    promise->MaybeRejectWithInvalidStateError(
        "Can't resume if the control thread state is \"closed\"");
    return promise.forget();
  }

  mSuspendedByContent = false;
  mPendingResumePromises.AppendElement(promise);

  const bool isAllowedToPlay = AutoplayPolicy::IsAllowedToPlay(*this);
  AUTOPLAY_LOG("Trying to resume AudioContext %p, IsAllowedToPlay=%d", this,
               isAllowedToPlay);
  if (isAllowedToPlay) {
    ResumeInternal(AudioContextOperationFlags::SendStateChange);
  } else {
    ReportBlocked();
  }

  MaybeUpdateAutoplayTelemetry();

  return promise.forget();
}

void AudioContext::ResumeInternal(AudioContextOperationFlags aFlags) {
  AUTOPLAY_LOG("Allow to resume AudioContext %p", this);
  mWasAllowedToStart = true;

  Destination()->Resume();

  nsTArray<mozilla::MediaTrack*> tracks;
  // If mSuspendCalled is false then we already resumed all our tracks,
  // so don't resume them again (since suspend(); resume(); resume(); should
  // be OK). But we still need to do ApplyAudioContextOperation
  // to ensure our new promise is resolved.
  if (mSuspendCalled) {
    tracks = GetAllTracks();
  }
  auto promise = Graph()->ApplyAudioContextOperation(
      DestinationTrack(), tracks, AudioContextOperation::Resume);
  if (aFlags & AudioContextOperationFlags::SendStateChange) {
    promise->Then(
        GetMainThread(), "AudioContext::OnStateChanged",
        [self = RefPtr<AudioContext>(this)](AudioContextState aNewState) {
          self->OnStateChanged(nullptr, aNewState);
        },
        [] { MOZ_CRASH("Unexpected rejection"); });
  }
  mSuspendCalled = false;
}

void AudioContext::UpdateAutoplayAssumptionStatus() {
  if (AutoplayPolicyTelemetryUtils::WouldBeAllowedToPlayIfAutoplayDisabled(
          *this)) {
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

  if (AutoplayPolicyTelemetryUtils::WouldBeAllowedToPlayIfAutoplayDisabled(
          *this) &&
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
        nsContentUtils::DispatchTrustedEvent(
            doc, static_cast<DOMEventTargetHelper*>(self),
            NS_LITERAL_STRING("blocked"), CanBubble::eNo, Cancelable::eNo);
      });
  Dispatch(r.forget());
}

already_AddRefed<Promise> AudioContext::Close(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise;
  promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mIsOffline) {
    // XXXbz This is not reachable, since we don't implement this
    // method on OfflineAudioContext at all!
    promise->MaybeRejectWithNotSupportedError(
        "Can't close OfflineAudioContext yet");
    return promise.forget();
  }

  if (mAudioContextState == AudioContextState::Closed) {
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
    Destination()->DestroyAudioChannelAgent();

    nsTArray<mozilla::MediaTrack*> tracks;
    // If mSuspendCalled or mCloseCalled are true then we already suspended
    // all our tracks, so don't suspend them again. But we still need to do
    // ApplyAudioContextOperation to ensure our new promise is resolved.
    if (!mSuspendCalled && !mCloseCalled) {
      tracks = GetAllTracks();
    }
    auto promise = Graph()->ApplyAudioContextOperation(
        ds, tracks, AudioContextOperation::Close);
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
  mCloseCalled = true;
  // Release references to active nodes.
  // Active AudioNodes don't unregister in destructors, at which point the
  // Node is already unregistered.
  mActiveNodes.Clear();
}

void AudioContext::RegisterNode(AudioNode* aNode) {
  MOZ_ASSERT(!mAllNodes.Contains(aNode));
  mAllNodes.PutEntry(aNode);
}

void AudioContext::UnregisterNode(AudioNode* aNode) {
  MOZ_ASSERT(mAllNodes.Contains(aNode));
  mAllNodes.RemoveEntry(aNode);
}

already_AddRefed<Promise> AudioContext::StartRendering(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());

  MOZ_ASSERT(mIsOffline, "This should only be called on OfflineAudioContext");
  if (mIsStarted) {
    aRv.ThrowInvalidStateError("Rendering already started");
    return nullptr;
  }

  mIsStarted = true;
  RefPtr<Promise> promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
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
  MOZ_ASSERT(!mWorkletParamDescriptors.GetValue(aName));
  Unused << mWorkletParamDescriptors.Put(aName, std::move(*aParamMap),
                                         fallible);
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
  for (auto iter = mAllNodes.ConstIter(); !iter.Done(); iter.Next()) {
    AudioNode* node = iter.Get()->GetKey();
    int64_t amount = node->SizeOfIncludingThis(MallocSizeOf);
    nsPrintfCString domNodePath("explicit/webaudio/audio-node/%s/dom-nodes",
                                node->NodeType());
    aHandleReport->Callback(EmptyCString(), domNodePath, KIND_HEAP, UNITS_BYTES,
                            amount, nodeDescription, aData);
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
  nsContentUtils::ReportToConsole(aErrorFlags, NS_LITERAL_CSTRING("Media"), doc,
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

}  // namespace dom
}  // namespace mozilla
