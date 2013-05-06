/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioContext.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "mozilla/ErrorResult.h"
#include "MediaStreamGraph.h"
#include "mozilla/dom/AnalyserNode.h"
#include "AudioDestinationNode.h"
#include "AudioBufferSourceNode.h"
#include "AudioBuffer.h"
#include "GainNode.h"
#include "DelayNode.h"
#include "PannerNode.h"
#include "AudioListener.h"
#include "DynamicsCompressorNode.h"
#include "BiquadFilterNode.h"
#include "ScriptProcessorNode.h"
#include "ChannelMergerNode.h"
#include "ChannelSplitterNode.h"
#include "nsNetUtil.h"

// Note that this number is an arbitrary large value to protect against OOM
// attacks.
const unsigned MAX_SCRIPT_PROCESSOR_CHANNELS = 10000;
const unsigned MAX_CHANNEL_SPLITTER_OUTPUTS = UINT16_MAX;
const unsigned MAX_CHANNEL_MERGER_INPUTS = UINT16_MAX;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(AudioContext, nsDOMEventTargetHelper,
                                     mDestination, mListener)

NS_IMPL_ADDREF_INHERITED(AudioContext, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioContext, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioContext)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

static uint8_t gWebAudioOutputKey;

AudioContext::AudioContext(nsPIDOMWindow* aWindow)
  : mDestination(new AudioDestinationNode(this, MediaStreamGraph::GetInstance()))
{
  // Actually play audio
  mDestination->Stream()->AddAudioOutput(&gWebAudioOutputKey);
  nsDOMEventTargetHelper::BindToOwner(aWindow);
  SetIsDOMBinding();

  mPannerNodes.Init();
  mAudioBufferSourceNodes.Init();
  mScriptProcessorNodes.Init();
}

AudioContext::~AudioContext()
{
}

JSObject*
AudioContext::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AudioContextBinding::Wrap(aCx, aScope, this);
}

/* static */ already_AddRefed<AudioContext>
AudioContext::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.Get());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<AudioContext> object = new AudioContext(window);
  window->AddAudioContext(object);
  return object.forget();
}

already_AddRefed<AudioBufferSourceNode>
AudioContext::CreateBufferSource()
{
  nsRefPtr<AudioBufferSourceNode> bufferNode =
    new AudioBufferSourceNode(this);
  mAudioBufferSourceNodes.PutEntry(bufferNode);
  return bufferNode.forget();
}

already_AddRefed<AudioBuffer>
AudioContext::CreateBuffer(JSContext* aJSContext, uint32_t aNumberOfChannels,
                           uint32_t aLength, float aSampleRate,
                           ErrorResult& aRv)
{
  if (aSampleRate < 8000 || aSampleRate > 96000) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  if (aLength > INT32_MAX) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsRefPtr<AudioBuffer> buffer =
    new AudioBuffer(this, int32_t(aLength), aSampleRate);
  if (!buffer->InitializeBuffers(aNumberOfChannels, aJSContext)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  return buffer.forget();
}

already_AddRefed<AudioBuffer>
AudioContext::CreateBuffer(JSContext* aJSContext, ArrayBuffer& aBuffer,
                          bool aMixToMono, ErrorResult& aRv)
{
  // Sniff the content of the media.
  // Failed type sniffing will be handled by SyncDecodeMedia.
  nsAutoCString contentType;
  NS_SniffContent(NS_DATA_SNIFFER_CATEGORY, nullptr,
                  aBuffer.Data(), aBuffer.Length(),
                  contentType);

  WebAudioDecodeJob job(contentType, aBuffer, this);

  if (mDecoder.SyncDecodeMedia(contentType.get(),
                               job.mBuffer, job.mLength, job) &&
      job.mOutput) {
    nsRefPtr<AudioBuffer> buffer = job.mOutput.forget();
    if (aMixToMono) {
      buffer->MixToMono(aJSContext);
    }
    return buffer.forget();
  }

  return nullptr;
}

namespace {

bool IsValidBufferSize(uint32_t aBufferSize) {
  switch (aBufferSize) {
  case 0:       // let the implementation choose the buffer size
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

}

already_AddRefed<ScriptProcessorNode>
AudioContext::CreateScriptProcessor(uint32_t aBufferSize,
                                    uint32_t aNumberOfInputChannels,
                                    uint32_t aNumberOfOutputChannels,
                                    ErrorResult& aRv)
{
  if (aNumberOfInputChannels == 0 || aNumberOfOutputChannels == 0 ||
      aNumberOfInputChannels > MAX_SCRIPT_PROCESSOR_CHANNELS ||
      aNumberOfOutputChannels > MAX_SCRIPT_PROCESSOR_CHANNELS ||
      !IsValidBufferSize(aBufferSize)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsRefPtr<ScriptProcessorNode> scriptProcessor =
    new ScriptProcessorNode(this, aBufferSize, aNumberOfInputChannels,
                            aNumberOfOutputChannels);
  mScriptProcessorNodes.PutEntry(scriptProcessor);
  return scriptProcessor.forget();
}

already_AddRefed<AnalyserNode>
AudioContext::CreateAnalyser()
{
  nsRefPtr<AnalyserNode> analyserNode = new AnalyserNode(this);
  return analyserNode.forget();
}

already_AddRefed<GainNode>
AudioContext::CreateGain()
{
  nsRefPtr<GainNode> gainNode = new GainNode(this);
  return gainNode.forget();
}

already_AddRefed<DelayNode>
AudioContext::CreateDelay(double aMaxDelayTime, ErrorResult& aRv)
{
  if (aMaxDelayTime > 0. && aMaxDelayTime < 180.) {
    nsRefPtr<DelayNode> delayNode = new DelayNode(this, aMaxDelayTime);
    return delayNode.forget();
  }
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

already_AddRefed<PannerNode>
AudioContext::CreatePanner()
{
  nsRefPtr<PannerNode> pannerNode = new PannerNode(this);
  mPannerNodes.PutEntry(pannerNode);
  return pannerNode.forget();
}

already_AddRefed<ChannelSplitterNode>
AudioContext::CreateChannelSplitter(uint32_t aNumberOfOutputs, ErrorResult& aRv)
{
  if (aNumberOfOutputs == 0 ||
      aNumberOfOutputs > MAX_CHANNEL_SPLITTER_OUTPUTS) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsRefPtr<ChannelSplitterNode> splitterNode =
    new ChannelSplitterNode(this, aNumberOfOutputs);
  return splitterNode.forget();
}

already_AddRefed<ChannelMergerNode>
AudioContext::CreateChannelMerger(uint32_t aNumberOfInputs, ErrorResult& aRv)
{
  if (aNumberOfInputs == 0 ||
      aNumberOfInputs > MAX_CHANNEL_MERGER_INPUTS) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsRefPtr<ChannelMergerNode> mergerNode =
    new ChannelMergerNode(this, aNumberOfInputs);
  return mergerNode.forget();
}

already_AddRefed<DynamicsCompressorNode>
AudioContext::CreateDynamicsCompressor()
{
  nsRefPtr<DynamicsCompressorNode> compressorNode =
    new DynamicsCompressorNode(this);
  return compressorNode.forget();
}

already_AddRefed<BiquadFilterNode>
AudioContext::CreateBiquadFilter()
{
  nsRefPtr<BiquadFilterNode> filterNode =
    new BiquadFilterNode(this);
  return filterNode.forget();
}

AudioListener*
AudioContext::Listener()
{
  if (!mListener) {
    mListener = new AudioListener(this);
  }
  return mListener;
}

void
AudioContext::DecodeAudioData(const ArrayBuffer& aBuffer,
                              DecodeSuccessCallback& aSuccessCallback,
                              const Optional<OwningNonNull<DecodeErrorCallback> >& aFailureCallback)
{
  // Sniff the content of the media.
  // Failed type sniffing will be handled by AsyncDecodeMedia.
  nsAutoCString contentType;
  NS_SniffContent(NS_DATA_SNIFFER_CATEGORY, nullptr,
                  aBuffer.Data(), aBuffer.Length(),
                  contentType);

  nsCOMPtr<DecodeErrorCallback> failureCallback;
  if (aFailureCallback.WasPassed()) {
    failureCallback = aFailureCallback.Value().get();
  }
  nsAutoPtr<WebAudioDecodeJob> job(
    new WebAudioDecodeJob(contentType, aBuffer, this,
                          &aSuccessCallback, failureCallback));
  mDecoder.AsyncDecodeMedia(contentType.get(),
                            job->mBuffer, job->mLength, *job);
  // Transfer the ownership to mDecodeJobs
  mDecodeJobs.AppendElement(job.forget());
}

void
AudioContext::RemoveFromDecodeQueue(WebAudioDecodeJob* aDecodeJob)
{
  mDecodeJobs.RemoveElement(aDecodeJob);
}

void
AudioContext::UnregisterAudioBufferSourceNode(AudioBufferSourceNode* aNode)
{
  mAudioBufferSourceNodes.RemoveEntry(aNode);
  UpdatePannerSource();
}

void
AudioContext::UnregisterPannerNode(PannerNode* aNode)
{
  mPannerNodes.RemoveEntry(aNode);
}

void
AudioContext::UnregisterScriptProcessorNode(ScriptProcessorNode* aNode)
{
  mScriptProcessorNodes.RemoveEntry(aNode);
}

static PLDHashOperator
FindConnectedSourcesOn(nsPtrHashKey<PannerNode>* aEntry, void* aData)
{
  aEntry->GetKey()->FindConnectedSources();
  return PL_DHASH_NEXT;
}

void
AudioContext::UpdatePannerSource()
{
  mPannerNodes.EnumerateEntries(FindConnectedSourcesOn, nullptr);
}

MediaStreamGraph*
AudioContext::Graph() const
{
  return Destination()->Stream()->Graph();
}

MediaStream*
AudioContext::DestinationStream() const
{
  return Destination()->Stream();
}

double
AudioContext::CurrentTime() const
{
  return MediaTimeToSeconds(Destination()->Stream()->GetCurrentTime());
}

template <class T>
static PLDHashOperator
GetHashtableEntry(nsPtrHashKey<T>* aEntry, void* aData)
{
  nsTArray<T*>* array = static_cast<nsTArray<T*>*>(aData);
  array->AppendElement(aEntry->GetKey());
  return PL_DHASH_NEXT;
}

template <class T>
static void
GetHashtableElements(nsTHashtable<nsPtrHashKey<T> >& aHashtable, nsTArray<T*>& aArray)
{
  aHashtable.EnumerateEntries(&GetHashtableEntry<T>, &aArray);
}

void
AudioContext::Shutdown()
{
  Suspend();
  mDecoder.Shutdown();

  // Stop all audio buffer source nodes, to make sure that they release
  // their self-references.
  // We first gather an array of the nodes and then call Stop on each one,
  // since Stop may delete the object and therefore trigger a re-entrant
  // hashtable call to remove the pointer from the hashtable, which is
  // not safe.
  nsTArray<AudioBufferSourceNode*> sourceNodes;
  GetHashtableElements(mAudioBufferSourceNodes, sourceNodes);
  for (uint32_t i = 0; i < sourceNodes.Length(); ++i) {
    ErrorResult rv;
    sourceNodes[i]->Stop(0.0, rv);
  }
  // Stop all script processor nodes, to make sure that they release
  // their self-references.
  nsTArray<ScriptProcessorNode*> spNodes;
  GetHashtableElements(mScriptProcessorNodes, spNodes);
  for (uint32_t i = 0; i < spNodes.Length(); ++i) {
    spNodes[i]->Stop();
  }
}

void
AudioContext::Suspend()
{
  MediaStream* ds = DestinationStream();
  if (ds) {
    ds->ChangeExplicitBlockerCount(1);
  }
}

void
AudioContext::Resume()
{
  MediaStream* ds = DestinationStream();
  if (ds) {
    ds->ChangeExplicitBlockerCount(-1);
  }
}

JSContext*
AudioContext::GetJSContext() const
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
    do_QueryInterface(GetParentObject());
  if (!scriptGlobal) {
    return nullptr;
  }
  nsIScriptContext* scriptContext = scriptGlobal->GetContext();
  if (!scriptContext) {
    return nullptr;
  }
  return scriptContext->GetNativeContext();
}

}
}
