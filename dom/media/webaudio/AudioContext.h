/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioContext_h_
#define AudioContext_h_

#include "mozilla/dom/AudioChannelBinding.h"
#include "MediaBufferDecoder.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/TypedArray.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "js/TypeDecls.h"
#include "nsIMemoryReporter.h"

// X11 has a #define for CurrentTime. Unbelievable :-(.
// See dom/media/DOMMediaStream.h for more fun!
#ifdef CurrentTime
#undef CurrentTime
#endif

namespace WebCore {
  class PeriodicWave;
} // namespace WebCore

class nsPIDOMWindowInner;

namespace mozilla {

class DOMMediaStream;
class ErrorResult;
class MediaStream;
class MediaStreamGraph;
class AudioNodeStream;

namespace dom {

enum class AudioContextState : uint32_t;
class AnalyserNode;
class AudioBuffer;
class AudioBufferSourceNode;
class AudioDestinationNode;
class AudioListener;
class AudioNode;
class BiquadFilterNode;
class ChannelMergerNode;
class ChannelSplitterNode;
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
class OscillatorNode;
class PannerNode;
class ScriptProcessorNode;
class StereoPannerNode;
class WaveShaperNode;
class PeriodicWave;
struct PeriodicWaveConstraints;
class Promise;
enum class OscillatorType : uint32_t;

// This is addrefed by the OscillatorNodeEngine on the main thread
// and then used from the MSG thread.
// It can be released either from the graph thread or the main thread.
class BasicWaveFormCache
{
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


/* This runnable allows the MSG to notify the main thread when audio is actually
 * flowing */
class StateChangeTask final : public Runnable
{
public:
  /* This constructor should be used when this event is sent from the main
   * thread. */
  StateChangeTask(AudioContext* aAudioContext, void* aPromise, AudioContextState aNewState);

  /* This constructor should be used when this event is sent from the audio
   * thread. */
  StateChangeTask(AudioNodeStream* aStream, void* aPromise, AudioContextState aNewState);

  NS_IMETHOD Run() override;

private:
  RefPtr<AudioContext> mAudioContext;
  void* mPromise;
  RefPtr<AudioNodeStream> mAudioNodeStream;
  AudioContextState mNewState;
};

enum class AudioContextOperation { Suspend, Resume, Close };

class AudioContext final : public DOMEventTargetHelper,
                           public nsIMemoryReporter
{
  AudioContext(nsPIDOMWindowInner* aParentWindow,
               bool aIsOffline,
               AudioChannel aChannel,
               uint32_t aNumberOfChannels = 0,
               uint32_t aLength = 0,
               float aSampleRate = 0.0f);
  ~AudioContext();

  nsresult Init();

public:
  typedef uint64_t AudioContextId;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioContext,
                                           DOMEventTargetHelper)
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  nsPIDOMWindowInner* GetParentObject() const
  {
    return GetOwner();
  }

  void Shutdown(); // idempotent

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  using DOMEventTargetHelper::DispatchTrustedEvent;

  // Constructor for regular AudioContext
  static already_AddRefed<AudioContext>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  // Constructor for regular AudioContext. A default audio channel is needed.
  static already_AddRefed<AudioContext>
  Constructor(const GlobalObject& aGlobal,
              AudioChannel aChannel,
              ErrorResult& aRv);

  // Constructor for offline AudioContext
  static already_AddRefed<AudioContext>
  Constructor(const GlobalObject& aGlobal,
              uint32_t aNumberOfChannels,
              uint32_t aLength,
              float aSampleRate,
              ErrorResult& aRv);

  // AudioContext methods

  AudioDestinationNode* Destination() const
  {
    return mDestination;
  }

  float SampleRate() const
  {
    return mSampleRate;
  }

  bool ShouldSuspendNewStream() const { return mSuspendCalled; }

  double CurrentTime() const;

  AudioListener* Listener();

  AudioContextState State() const { return mAudioContextState; }

  // Those three methods return a promise to content, that is resolved when an
  // (possibly long) operation is completed on the MSG (and possibly other)
  // thread(s). To avoid having to match the calls and asychronous result when
  // the operation is completed, we keep a reference to the promises on the main
  // thread, and then send the promises pointers down the MSG thread, as a void*
  // (to make it very clear that the pointer is to merely be treated as an ID).
  // When back on the main thread, we can resolve or reject the promise, by
  // casting it back to a `Promise*` while asserting we're back on the main
  // thread and removing the reference we added.
  already_AddRefed<Promise> Suspend(ErrorResult& aRv);
  already_AddRefed<Promise> Resume(ErrorResult& aRv);
  already_AddRefed<Promise> Close(ErrorResult& aRv);
  IMPL_EVENT_HANDLER(statechange)

  already_AddRefed<AudioBufferSourceNode> CreateBufferSource(ErrorResult& aRv);

  already_AddRefed<AudioBuffer>
  CreateBuffer(uint32_t aNumberOfChannels, uint32_t aLength, float aSampleRate,
               ErrorResult& aRv);

  already_AddRefed<MediaStreamAudioDestinationNode>
  CreateMediaStreamDestination(ErrorResult& aRv);

  already_AddRefed<ScriptProcessorNode>
  CreateScriptProcessor(uint32_t aBufferSize,
                        uint32_t aNumberOfInputChannels,
                        uint32_t aNumberOfOutputChannels,
                        ErrorResult& aRv);

  already_AddRefed<StereoPannerNode>
  CreateStereoPanner(ErrorResult& aRv);

  already_AddRefed<AnalyserNode>
  CreateAnalyser(ErrorResult& aRv);

  already_AddRefed<GainNode>
  CreateGain(ErrorResult& aRv);

  already_AddRefed<WaveShaperNode>
  CreateWaveShaper(ErrorResult& aRv);

  already_AddRefed<MediaElementAudioSourceNode>
  CreateMediaElementSource(HTMLMediaElement& aMediaElement, ErrorResult& aRv);
  already_AddRefed<MediaStreamAudioSourceNode>
  CreateMediaStreamSource(DOMMediaStream& aMediaStream, ErrorResult& aRv);

  already_AddRefed<DelayNode>
  CreateDelay(double aMaxDelayTime, ErrorResult& aRv);

  already_AddRefed<PannerNode>
  CreatePanner(ErrorResult& aRv);

  already_AddRefed<ConvolverNode>
  CreateConvolver(ErrorResult& aRv);

  already_AddRefed<ChannelSplitterNode>
  CreateChannelSplitter(uint32_t aNumberOfOutputs, ErrorResult& aRv);

  already_AddRefed<ChannelMergerNode>
  CreateChannelMerger(uint32_t aNumberOfInputs, ErrorResult& aRv);

  already_AddRefed<DynamicsCompressorNode>
  CreateDynamicsCompressor(ErrorResult& aRv);

  already_AddRefed<BiquadFilterNode>
  CreateBiquadFilter(ErrorResult& aRv);

  already_AddRefed<IIRFilterNode>
  CreateIIRFilter(const mozilla::dom::binding_detail::AutoSequence<double>& aFeedforward,
                  const mozilla::dom::binding_detail::AutoSequence<double>& aFeedback,
                  mozilla::ErrorResult& aRv);

  already_AddRefed<OscillatorNode>
  CreateOscillator(ErrorResult& aRv);

  already_AddRefed<PeriodicWave>
  CreatePeriodicWave(const Float32Array& aRealData, const Float32Array& aImagData,
                     const PeriodicWaveConstraints& aConstraints,
                     ErrorResult& aRv);

  already_AddRefed<Promise>
  DecodeAudioData(const ArrayBuffer& aBuffer,
                  const Optional<OwningNonNull<DecodeSuccessCallback> >& aSuccessCallback,
                  const Optional<OwningNonNull<DecodeErrorCallback> >& aFailureCallback,
                  ErrorResult& aRv);

  // OfflineAudioContext methods
  already_AddRefed<Promise> StartRendering(ErrorResult& aRv);
  IMPL_EVENT_HANDLER(complete)
  unsigned long Length();

  bool IsOffline() const { return mIsOffline; }

  MediaStreamGraph* Graph() const;
  MediaStream* DestinationStream() const;

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

  void UnregisterAudioBufferSourceNode(AudioBufferSourceNode* aNode);
  void UnregisterPannerNode(PannerNode* aNode);
  void UpdatePannerSource();

  uint32_t MaxChannelCount() const;

  uint32_t ActiveNodeCount() const;

  void Mute() const;
  void Unmute() const;

  JSObject* GetGlobalJSObject() const;

  AudioChannel MozAudioChannelType() const;

  AudioChannel TestAudioChannelInAudioNodeStream();

  void RegisterNode(AudioNode* aNode);
  void UnregisterNode(AudioNode* aNode);

  void OnStateChanged(void* aPromise, AudioContextState aNewState);

  BasicWaveFormCache* GetBasicWaveFormCache();

  IMPL_EVENT_HANDLER(mozinterruptbegin)
  IMPL_EVENT_HANDLER(mozinterruptend)

private:
  void DisconnectFromWindow();
  void RemoveFromDecodeQueue(WebAudioDecodeJob* aDecodeJob);
  void ShutdownDecoder();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override;

  friend struct ::mozilla::WebAudioDecodeJob;

  bool CheckClosed(ErrorResult& aRv);

  nsTArray<MediaStream*> GetAllStreams() const;

private:
  // Each AudioContext has an id, that is passed down the MediaStreams that
  // back the AudioNodes, so we can easily compute the set of all the
  // MediaStreams for a given context, on the MediasStreamGraph side.
  const AudioContextId mId;
  // Note that it's important for mSampleRate to be initialized before
  // mDestination, as mDestination's constructor needs to access it!
  const float mSampleRate;
  AudioContextState mAudioContextState;
  RefPtr<AudioDestinationNode> mDestination;
  RefPtr<AudioListener> mListener;
  nsTArray<RefPtr<WebAudioDecodeJob> > mDecodeJobs;
  // This array is used to keep the suspend/resume/close promises alive until
  // they are resolved, so we can safely pass them accross threads.
  nsTArray<RefPtr<Promise>> mPromiseGripArray;
  // See RegisterActiveNode.  These will keep the AudioContext alive while it
  // is rendering and the window remains alive.
  nsTHashtable<nsRefPtrHashKey<AudioNode> > mActiveNodes;
  // Raw (non-owning) references to all AudioNodes for this AudioContext.
  nsTHashtable<nsPtrHashKey<AudioNode> > mAllNodes;
  // Hashsets containing all the PannerNodes, to compute the doppler shift.
  // These are weak pointers.
  nsTHashtable<nsPtrHashKey<PannerNode> > mPannerNodes;
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
};

static const dom::AudioContext::AudioContextId NO_AUDIO_CONTEXT = 0;

} // namespace dom
} // namespace mozilla

#endif

