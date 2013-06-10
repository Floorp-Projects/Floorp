/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioContext_h_
#define AudioContext_h_

#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "EnableWebAudioCheck.h"
#include "nsAutoPtr.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/AudioContextBinding.h"
#include "MediaBufferDecoder.h"
#include "StreamBuffer.h"
#include "MediaStreamGraph.h"
#include "nsTHashtable.h"

// X11 has a #define for CurrentTime. Unbelievable :-(.
// See content/media/DOMMediaStream.h for more fun!
#ifdef CurrentTime
#undef CurrentTime
#endif

struct JSContext;
class JSObject;
class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;
struct WebAudioDecodeJob;

namespace dom {

class AnalyserNode;
class AudioBuffer;
class AudioBufferSourceNode;
class AudioDestinationNode;
class AudioListener;
class BiquadFilterNode;
class ChannelMergerNode;
class ChannelSplitterNode;
class ConvolverNode;
class DelayNode;
class DynamicsCompressorNode;
class GainNode;
class GlobalObject;
class OfflineRenderSuccessCallback;
class PannerNode;
class ScriptProcessorNode;
class WaveShaperNode;
class WaveTable;

class AudioContext MOZ_FINAL : public nsDOMEventTargetHelper,
                               public EnableWebAudioCheck
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

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  void Shutdown();
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
  CreateBuffer(JSContext* aJSContext, ArrayBuffer& aBuffer,
               bool aMixToMono, ErrorResult& aRv);

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

  already_AddRefed<WaveTable>
  CreateWaveTable(const Float32Array& aRealData, const Float32Array& aImagData,
                  ErrorResult& aRv);

  void DecodeAudioData(const ArrayBuffer& aBuffer,
                       DecodeSuccessCallback& aSuccessCallback,
                       const Optional<OwningNonNull<DecodeErrorCallback> >& aFailureCallback);

  // OfflineAudioContext methods
  void StartRendering();
  IMPL_EVENT_HANDLER(complete)

  bool IsOffline() const { return mIsOffline; }

  MediaStreamGraph* Graph() const;
  MediaStream* DestinationStream() const;
  void UnregisterAudioBufferSourceNode(AudioBufferSourceNode* aNode);
  void UnregisterPannerNode(PannerNode* aNode);
  void UnregisterScriptProcessorNode(ScriptProcessorNode* aNode);
  void UpdatePannerSource();

  uint32_t MaxChannelCount() const;

  JSContext* GetJSContext() const;

private:
  void RemoveFromDecodeQueue(WebAudioDecodeJob* aDecodeJob);

  friend struct ::mozilla::WebAudioDecodeJob;

private:
  // Note that it's important for mSampleRate to be initialized before
  // mDestination, as mDestination's constructor needs to access it!
  const float mSampleRate;
  nsRefPtr<AudioDestinationNode> mDestination;
  nsRefPtr<AudioListener> mListener;
  MediaBufferDecoder mDecoder;
  nsTArray<nsAutoPtr<WebAudioDecodeJob> > mDecodeJobs;
  // Two hashsets containing all the PannerNodes and AudioBufferSourceNodes,
  // to compute the doppler shift, and also to stop AudioBufferSourceNodes.
  // These are all weak pointers.
  nsTHashtable<nsPtrHashKey<PannerNode> > mPannerNodes;
  nsTHashtable<nsPtrHashKey<AudioBufferSourceNode> > mAudioBufferSourceNodes;
  // Hashset containing all ScriptProcessorNodes in order to stop them.
  // These are all weak pointers.
  nsTHashtable<nsPtrHashKey<ScriptProcessorNode> > mScriptProcessorNodes;
  // Number of channels passed in the OfflineAudioContext ctor.
  uint32_t mNumberOfChannels;
  bool mIsOffline;
};

}
}

#endif

