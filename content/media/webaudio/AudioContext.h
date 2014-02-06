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
#include "mozilla/dom/TypedArray.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "js/TypeDecls.h"
#include "nsIMemoryReporter.h"
#include "mozilla/MemoryReporting.h"

// X11 has a #define for CurrentTime. Unbelievable :-(.
// See content/media/DOMMediaStream.h for more fun!
#ifdef CurrentTime
#undef CurrentTime
#endif

class nsPIDOMWindow;

namespace mozilla {

class DOMMediaStream;
class ErrorResult;
class MediaStream;
class MediaStreamGraph;

namespace dom {

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
class HTMLMediaElement;
class MediaElementAudioSourceNode;
class GlobalObject;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class OscillatorNode;
class PannerNode;
class ScriptProcessorNode;
class WaveShaperNode;
class PeriodicWave;

class AudioContext MOZ_FINAL : public nsDOMEventTargetHelper,
                               public nsIMemoryReporter
{
  AudioContext(nsPIDOMWindow* aParentWindow,
               bool aIsOffline,
               uint32_t aNumberOfChannels = 0,
               uint32_t aLength = 0,
               float aSampleRate = 0.0f);
  ~AudioContext();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioContext,
                                           nsDOMEventTargetHelper)
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  void Shutdown(); // idempotent
  void Suspend();
  void Resume();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  using nsDOMEventTargetHelper::DispatchTrustedEvent;

  // Constructor for regular AudioContext
  static already_AddRefed<AudioContext>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

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

  double CurrentTime() const;

  AudioListener* Listener();

  already_AddRefed<AudioBufferSourceNode> CreateBufferSource();

  already_AddRefed<AudioBuffer>
  CreateBuffer(JSContext* aJSContext, uint32_t aNumberOfChannels,
               uint32_t aLength, float aSampleRate,
               ErrorResult& aRv);

  already_AddRefed<AudioBuffer>
  CreateBuffer(JSContext* aJSContext, const ArrayBuffer& aBuffer,
               bool aMixToMono, ErrorResult& aRv);

  already_AddRefed<MediaStreamAudioDestinationNode>
  CreateMediaStreamDestination(ErrorResult& aRv);

  already_AddRefed<ScriptProcessorNode>
  CreateScriptProcessor(uint32_t aBufferSize,
                        uint32_t aNumberOfInputChannels,
                        uint32_t aNumberOfOutputChannels,
                        ErrorResult& aRv);

  already_AddRefed<ScriptProcessorNode>
  CreateJavaScriptNode(uint32_t aBufferSize,
                       uint32_t aNumberOfInputChannels,
                       uint32_t aNumberOfOutputChannels,
                       ErrorResult& aRv)
  {
    return CreateScriptProcessor(aBufferSize, aNumberOfInputChannels,
                                 aNumberOfOutputChannels, aRv);
  }

  already_AddRefed<AnalyserNode>
  CreateAnalyser();

  already_AddRefed<GainNode>
  CreateGain();

  already_AddRefed<WaveShaperNode>
  CreateWaveShaper();

  already_AddRefed<GainNode>
  CreateGainNode()
  {
    return CreateGain();
  }

  already_AddRefed<MediaElementAudioSourceNode>
  CreateMediaElementSource(HTMLMediaElement& aMediaElement, ErrorResult& aRv);
  already_AddRefed<MediaStreamAudioSourceNode>
  CreateMediaStreamSource(DOMMediaStream& aMediaStream, ErrorResult& aRv);

  already_AddRefed<DelayNode>
  CreateDelay(double aMaxDelayTime, ErrorResult& aRv);

  already_AddRefed<DelayNode>
  CreateDelayNode(double aMaxDelayTime, ErrorResult& aRv)
  {
    return CreateDelay(aMaxDelayTime, aRv);
  }

  already_AddRefed<PannerNode>
  CreatePanner();

  already_AddRefed<ConvolverNode>
  CreateConvolver();

  already_AddRefed<ChannelSplitterNode>
  CreateChannelSplitter(uint32_t aNumberOfOutputs, ErrorResult& aRv);

  already_AddRefed<ChannelMergerNode>
  CreateChannelMerger(uint32_t aNumberOfInputs, ErrorResult& aRv);

  already_AddRefed<DynamicsCompressorNode>
  CreateDynamicsCompressor();

  already_AddRefed<BiquadFilterNode>
  CreateBiquadFilter();

  already_AddRefed<OscillatorNode>
  CreateOscillator();

  already_AddRefed<PeriodicWave>
  CreatePeriodicWave(const Float32Array& aRealData, const Float32Array& aImagData,
                     ErrorResult& aRv);

  void DecodeAudioData(const ArrayBuffer& aBuffer,
                       DecodeSuccessCallback& aSuccessCallback,
                       const Optional<OwningNonNull<DecodeErrorCallback> >& aFailureCallback);

  // OfflineAudioContext methods
  void StartRendering(ErrorResult& aRv);
  IMPL_EVENT_HANDLER(complete)

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

  void Mute() const;
  void Unmute() const;

  JSContext* GetJSContext() const;

  AudioChannel MozAudioChannelType() const;
  void SetMozAudioChannelType(AudioChannel aValue, ErrorResult& aRv);

  void UpdateNodeCount(int32_t aDelta);

  double DOMTimeToStreamTime(double aTime) const
  {
    return aTime - ExtraCurrentTime();
  }

private:
  /**
   * Returns the amount of extra time added to the current time of the
   * AudioDestinationNode's MediaStream to get this AudioContext's currentTime.
   * Must be subtracted from all DOM API parameter times that are on the same
   * timeline as AudioContext's currentTime to get times we can pass to the
   * MediaStreamGraph.
   */
  double ExtraCurrentTime() const;

  void RemoveFromDecodeQueue(WebAudioDecodeJob* aDecodeJob);
  void ShutdownDecoder();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData);

  friend struct ::mozilla::WebAudioDecodeJob;

private:
  // Note that it's important for mSampleRate to be initialized before
  // mDestination, as mDestination's constructor needs to access it!
  const float mSampleRate;
  nsRefPtr<AudioDestinationNode> mDestination;
  nsRefPtr<AudioListener> mListener;
  MediaBufferDecoder mDecoder;
  nsTArray<nsRefPtr<WebAudioDecodeJob> > mDecodeJobs;
  // See RegisterActiveNode.  These will keep the AudioContext alive while it
  // is rendering and the window remains alive.
  nsTHashtable<nsRefPtrHashKey<AudioNode> > mActiveNodes;
  // Hashsets containing all the PannerNodes, to compute the doppler shift.
  // These are weak pointers.
  nsTHashtable<nsPtrHashKey<PannerNode> > mPannerNodes;
  // Number of channels passed in the OfflineAudioContext ctor.
  uint32_t mNumberOfChannels;
  // Number of nodes that currently exist for this AudioContext
  int32_t mNodeCount;
  bool mIsOffline;
  bool mIsStarted;
  bool mIsShutDown;
};

}
}

#endif

