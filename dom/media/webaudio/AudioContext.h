/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioContext_h_
#define AudioContext_h_

#include "X11UndefineNone.h"
#include "AudioParamDescriptorMap.h"
#include "mozilla/dom/OfflineAudioContextBinding.h"
#include "mozilla/dom/AudioContextBinding.h"
#include "MediaBufferDecoder.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/RelativeTimeline.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"
#include "nsTHashSet.h"
#include "js/TypeDecls.h"
#include "nsIMemoryReporter.h"

namespace WebCore {
class PeriodicWave;
}  // namespace WebCore

class nsPIDOMWindowInner;

namespace mozilla {

class DOMMediaStream;
class ErrorResult;
class MediaTrack;
class MediaTrackGraph;
class AudioNodeTrack;

namespace dom {

enum class AudioContextState : uint8_t;
class AnalyserNode;
class AudioBuffer;
class AudioBufferSourceNode;
class AudioDestinationNode;
class AudioListener;
class AudioNode;
class BiquadFilterNode;
class BrowsingContext;
class ChannelMergerNode;
class ChannelSplitterNode;
class ConstantSourceNode;
class ConvolverNode;
class DelayNode;
class DynamicsCompressorNode;
class GainNode;
class GlobalObject;
class HTMLMediaElement;
class IIRFilterNode;
class MediaElementAudioSourceNode;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class MediaStreamTrack;
class MediaStreamTrackAudioSourceNode;
class OscillatorNode;
class PannerNode;
class ScriptProcessorNode;
class StereoPannerNode;
class WaveShaperNode;
class Worklet;
class PeriodicWave;
struct PeriodicWaveConstraints;
class Promise;
enum class OscillatorType : uint8_t;

// This is addrefed by the OscillatorNodeEngine on the main thread
// and then used from the MTG thread.
// It can be released either from the graph thread or the main thread.
class BasicWaveFormCache {
 public:
  explicit BasicWaveFormCache(uint32_t aSampleRate);
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BasicWaveFormCache)
  WebCore::PeriodicWave* GetBasicWaveForm(OscillatorType aType);

 private:
  ~BasicWaveFormCache();
  RefPtr<WebCore::PeriodicWave> mSawtooth;
  RefPtr<WebCore::PeriodicWave> mSquare;
  RefPtr<WebCore::PeriodicWave> mTriangle;
  uint32_t mSampleRate;
};

/* This runnable allows the MTG to notify the main thread when audio is actually
 * flowing */
class StateChangeTask final : public Runnable {
 public:
  /* This constructor should be used when this event is sent from the main
   * thread. */
  StateChangeTask(AudioContext* aAudioContext, void* aPromise,
                  AudioContextState aNewState);

  /* This constructor should be used when this event is sent from the audio
   * thread. */
  StateChangeTask(AudioNodeTrack* aTrack, void* aPromise,
                  AudioContextState aNewState);

  NS_IMETHOD Run() override;

 private:
  RefPtr<AudioContext> mAudioContext;
  void* mPromise;
  RefPtr<AudioNodeTrack> mAudioNodeTrack;
  AudioContextState mNewState;
};

enum class AudioContextOperation : uint8_t { Suspend, Resume, Close };
static const char* const kAudioContextOptionsStrings[] = {"Suspend", "Resume",
                                                          "Close"};
// When suspending or resuming an AudioContext, some operations have to notify
// the main thread, so that the Promise is resolved, the state is modified, and
// the statechanged event is sent. Some other operations don't go back to the
// main thread, for example when the AudioContext is paused by something that is
// not caused by the page itself: opening a debugger, breaking on a breakpoint,
// reloading a document.
enum class AudioContextOperationFlags { None, SendStateChange };
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(AudioContextOperationFlags);

struct AudioContextOptions;

class AudioContext final : public DOMEventTargetHelper,
                           public nsIMemoryReporter,
                           public RelativeTimeline {
  AudioContext(nsPIDOMWindowInner* aParentWindow, bool aIsOffline,
               uint32_t aNumberOfChannels = 0, uint32_t aLength = 0,
               float aSampleRate = 0.0f);
  ~AudioContext();

 public:
  typedef uint64_t AudioContextId;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioContext, DOMEventTargetHelper)
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  nsISerialEventTarget* GetMainThread() const;

  virtual void DisconnectFromOwner() override;

  void OnWindowDestroy();  // idempotent

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  using DOMEventTargetHelper::DispatchTrustedEvent;

  // Constructor for regular AudioContext
  static already_AddRefed<AudioContext> Constructor(
      const GlobalObject& aGlobal, const AudioContextOptions& aOptions,
      ErrorResult& aRv);

  // Constructor for offline AudioContext with options object
  static already_AddRefed<AudioContext> Constructor(
      const GlobalObject& aGlobal, const OfflineAudioContextOptions& aOptions,
      ErrorResult& aRv);

  // Constructor for offline AudioContext
  static already_AddRefed<AudioContext> Constructor(const GlobalObject& aGlobal,
                                                    uint32_t aNumberOfChannels,
                                                    uint32_t aLength,
                                                    float aSampleRate,
                                                    ErrorResult& aRv);

  // AudioContext methods

  AudioDestinationNode* Destination() const { return mDestination; }

  float SampleRate() const { return mSampleRate; }

  bool ShouldSuspendNewTrack() const { return mSuspendCalled || mCloseCalled; }

  double CurrentTime();

  AudioListener* Listener();

  AudioContextState State() const { return mAudioContextState; }

  double BaseLatency() const {
    // Gecko does not do any buffering between rendering the audio and sending
    // it to the audio subsystem.
    return 0.0;
  }

  double OutputLatency();

  void GetOutputTimestamp(AudioTimestamp& aTimeStamp);

  Worklet* GetAudioWorklet(ErrorResult& aRv);

  bool IsRunning() const;

  // Called when an AudioScheduledSourceNode started or the source node starts,
  // this method might resume the AudioContext if it was not allowed to start.
  void StartBlockedAudioContextIfAllowed();

  // Those three methods return a promise to content, that is resolved when an
  // (possibly long) operation is completed on the MTG (and possibly other)
  // thread(s). To avoid having to match the calls and asychronous result when
  // the operation is completed, we keep a reference to the promises on the main
  // thread, and then send the promises pointers down the MTG thread, as a void*
  // (to make it very clear that the pointer is to merely be treated as an ID).
  // When back on the main thread, we can resolve or reject the promise, by
  // casting it back to a `Promise*` while asserting we're back on the main
  // thread and removing the reference we added.
  already_AddRefed<Promise> Suspend(ErrorResult& aRv);
  already_AddRefed<Promise> Resume(ErrorResult& aRv);
  already_AddRefed<Promise> Close(ErrorResult& aRv);
  IMPL_EVENT_HANDLER(statechange)

  // These two functions are similar with Suspend() and Resume(), the difference
  // is they are designed for calling from chrome side, not content side. eg.
  // calling from inner window, so we won't need to return promise for caller.
  void SuspendFromChrome();
  void ResumeFromChrome();
  // Called on completion of offline rendering:
  void OfflineClose();

  already_AddRefed<AudioBufferSourceNode> CreateBufferSource();

  already_AddRefed<ConstantSourceNode> CreateConstantSource();

  already_AddRefed<AudioBuffer> CreateBuffer(uint32_t aNumberOfChannels,
                                             uint32_t aLength,
                                             float aSampleRate,
                                             ErrorResult& aRv);

  already_AddRefed<MediaStreamAudioDestinationNode>
  CreateMediaStreamDestination(ErrorResult& aRv);

  already_AddRefed<ScriptProcessorNode> CreateScriptProcessor(
      uint32_t aBufferSize, uint32_t aNumberOfInputChannels,
      uint32_t aNumberOfOutputChannels, ErrorResult& aRv);

  already_AddRefed<StereoPannerNode> CreateStereoPanner(ErrorResult& aRv);

  already_AddRefed<AnalyserNode> CreateAnalyser(ErrorResult& aRv);

  already_AddRefed<GainNode> CreateGain(ErrorResult& aRv);

  already_AddRefed<WaveShaperNode> CreateWaveShaper(ErrorResult& aRv);

  already_AddRefed<MediaElementAudioSourceNode> CreateMediaElementSource(
      HTMLMediaElement& aMediaElement, ErrorResult& aRv);
  already_AddRefed<MediaStreamAudioSourceNode> CreateMediaStreamSource(
      DOMMediaStream& aMediaStream, ErrorResult& aRv);
  already_AddRefed<MediaStreamTrackAudioSourceNode>
  CreateMediaStreamTrackSource(MediaStreamTrack& aMediaStreamTrack,
                               ErrorResult& aRv);

  already_AddRefed<DelayNode> CreateDelay(double aMaxDelayTime,
                                          ErrorResult& aRv);

  already_AddRefed<PannerNode> CreatePanner(ErrorResult& aRv);

  already_AddRefed<ConvolverNode> CreateConvolver(ErrorResult& aRv);

  already_AddRefed<ChannelSplitterNode> CreateChannelSplitter(
      uint32_t aNumberOfOutputs, ErrorResult& aRv);

  already_AddRefed<ChannelMergerNode> CreateChannelMerger(
      uint32_t aNumberOfInputs, ErrorResult& aRv);

  already_AddRefed<DynamicsCompressorNode> CreateDynamicsCompressor(
      ErrorResult& aRv);

  already_AddRefed<BiquadFilterNode> CreateBiquadFilter(ErrorResult& aRv);

  already_AddRefed<IIRFilterNode> CreateIIRFilter(
      const Sequence<double>& aFeedforward, const Sequence<double>& aFeedback,
      mozilla::ErrorResult& aRv);

  already_AddRefed<OscillatorNode> CreateOscillator(ErrorResult& aRv);

  already_AddRefed<PeriodicWave> CreatePeriodicWave(
      const Sequence<float>& aRealData, const Sequence<float>& aImagData,
      const PeriodicWaveConstraints& aConstraints, ErrorResult& aRv);

  already_AddRefed<Promise> DecodeAudioData(
      const ArrayBuffer& aBuffer,
      const Optional<OwningNonNull<DecodeSuccessCallback>>& aSuccessCallback,
      const Optional<OwningNonNull<DecodeErrorCallback>>& aFailureCallback,
      ErrorResult& aRv);

  // OfflineAudioContext methods
  already_AddRefed<Promise> StartRendering(ErrorResult& aRv);
  IMPL_EVENT_HANDLER(complete)
  unsigned long Length();

  bool IsOffline() const { return mIsOffline; }

  MediaTrackGraph* Graph() const;
  AudioNodeTrack* DestinationTrack() const;

  // Nodes register here if they will produce sound even if they have silent
  // or no input connections.  The AudioContext will keep registered nodes
  // alive until the context is collected.  This takes care of "playing"
  // references and "tail-time" references.
  void RegisterActiveNode(AudioNode* aNode);
  // Nodes unregister when they have finished producing sound for the
  // foreseeable future.
  // Do NOT call UnregisterActiveNode from an AudioNode destructor.
  // If the destructor is called, then the Node has already been unregistered.
  // The destructor may be called during hashtable enumeration, during which
  // unregistering would not be safe.
  void UnregisterActiveNode(AudioNode* aNode);

  uint32_t MaxChannelCount() const;

  uint32_t ActiveNodeCount() const;

  void Mute() const;
  void Unmute() const;

  void RegisterNode(AudioNode* aNode);
  void UnregisterNode(AudioNode* aNode);

  void OnStateChanged(void* aPromise, AudioContextState aNewState);

  BasicWaveFormCache* GetBasicWaveFormCache();

  void ShutdownWorklet();
  // Steals from |aParamMap|
  void SetParamMapForWorkletName(const nsAString& aName,
                                 AudioParamDescriptorMap* aParamMap);
  const AudioParamDescriptorMap* GetParamMapForWorkletName(
      const nsAString& aName) {
    return mWorkletParamDescriptors.Lookup(aName).DataPtrOrNull();
  }

  void Dispatch(already_AddRefed<nsIRunnable>&& aRunnable);

 private:
  void DisconnectFromWindow();
  already_AddRefed<Promise> CreatePromise(ErrorResult& aRv);
  void RemoveFromDecodeQueue(WebAudioDecodeJob* aDecodeJob);
  void ShutdownDecoder();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  NS_DECL_NSIMEMORYREPORTER

  friend struct ::mozilla::WebAudioDecodeJob;

  nsTArray<RefPtr<mozilla::MediaTrack>> GetAllTracks() const;

  void ResumeInternal(AudioContextOperationFlags aFlags);
  void SuspendInternal(void* aPromise, AudioContextOperationFlags aFlags);
  void CloseInternal(void* aPromise, AudioContextOperationFlags aFlags);

  // Will report error message to console and dispatch testing event if needed
  // when AudioContext is blocked by autoplay policy.
  void ReportBlocked();

  void ReportToConsole(uint32_t aErrorFlags, const char* aMsg) const;

  // This function should be called everytime we decide whether allow to start
  // audio context, it's used to update Telemetry related variables.
  void UpdateAutoplayAssumptionStatus();

  // These functions are used for updating Telemetry.
  // - MaybeUpdateAutoplayTelemetry: update category 'AllowedAfterBlocked'
  // - MaybeUpdateAutoplayTelemetryWhenShutdown: update category 'NeverBlocked'
  //   and 'NeverAllowed', so we need to call it when shutdown AudioContext
  void MaybeUpdateAutoplayTelemetry();
  void MaybeUpdateAutoplayTelemetryWhenShutdown();

  // If the pref `dom.suspend_inactive.enabled` is enabled, the dom window will
  // be suspended when the window becomes inactive. In order to keep audio
  // context running still, we will ask pages to keep awake in that situation.
  void MaybeUpdatePageAwakeRequest();
  void MaybeClearPageAwakeRequest();
  void SetPageAwakeRequest(bool aShouldSet);

  BrowsingContext* GetTopLevelBrowsingContext();

 private:
  // Each AudioContext has an id, that is passed down the MediaTracks that
  // back the AudioNodes, so we can easily compute the set of all the
  // MediaTracks for a given context, on the MediasTrackGraph side.
  const AudioContextId mId;
  // Note that it's important for mSampleRate to be initialized before
  // mDestination, as mDestination's constructor needs to access it!
  const float mSampleRate;
  AudioContextState mAudioContextState;
  RefPtr<AudioDestinationNode> mDestination;
  RefPtr<AudioListener> mListener;
  RefPtr<Worklet> mWorklet;
  nsTArray<UniquePtr<WebAudioDecodeJob>> mDecodeJobs;
  // This array is used to keep the suspend/close promises alive until
  // they are resolved, so we can safely pass them accross threads.
  nsTArray<RefPtr<Promise>> mPromiseGripArray;
  // This array is used to onlly keep the resume promises alive until they are
  // resolved, so we can safely pass them accross threads. If the audio context
  // is not allowed to play, the promise would be pending in this array and be
  // resolved until audio context has been allowed and user call resume() again.
  nsTArray<RefPtr<Promise>> mPendingResumePromises;
  // See RegisterActiveNode.  These will keep the AudioContext alive while it
  // is rendering and the window remains alive.
  nsTHashSet<RefPtr<AudioNode>> mActiveNodes;
  // Raw (non-owning) references to all AudioNodes for this AudioContext.
  nsTHashSet<AudioNode*> mAllNodes;
  nsTHashMap<nsStringHashKey, AudioParamDescriptorMap> mWorkletParamDescriptors;
  // Cache to avoid recomputing basic waveforms all the time.
  RefPtr<BasicWaveFormCache> mBasicWaveFormCache;
  // Number of channels passed in the OfflineAudioContext ctor.
  uint32_t mNumberOfChannels;
  bool mIsOffline;
  bool mIsStarted;
  bool mIsShutDown;
  // Close has been called, reject suspend and resume call.
  bool mCloseCalled;
  // Suspend has been called with no following resume.
  bool mSuspendCalled;
  bool mIsDisconnecting;
  // This flag stores the value of previous status of `allowed-to-start`.
  bool mWasAllowedToStart;

  // True if this AudioContext has been suspended by the page.
  bool mSuspendedByContent;

  // These variables are used for telemetry, they're not reflect the actual
  // status of AudioContext, they are based on the "assumption" of enabling
  // blocking web audio. Because we want to record Telemetry no matter user
  // enable blocking autoplay or not.
  // - 'mWasEverAllowedToStart' would be true when AudioContext had ever been
  //   allowed to start if we enable blocking web audio.
  // - 'mWasEverBlockedToStart' would be true when AudioContext had ever been
  //   blocked to start if we enable blocking web audio.
  // - 'mWouldBeAllowedToStart' stores the value of previous status of
  //   `allowed-to-start` if we enable blocking web audio.
  bool mWasEverAllowedToStart;
  bool mWasEverBlockedToStart;
  bool mWouldBeAllowedToStart;

  // Whether we have set the page awake reqeust when non-offline audio context
  // is running. That will keep the audio context being able to continue running
  // even if the window is inactive.
  bool mSetPageAwakeRequest = false;
};

static const dom::AudioContext::AudioContextId NO_AUDIO_CONTEXT = 0;

}  // namespace dom
}  // namespace mozilla

inline nsISupports* ToSupports(mozilla::dom::AudioContext* p) {
  return NS_CYCLE_COLLECTION_CLASSNAME(mozilla::dom::AudioContext)::Upcast(p);
}

#endif
