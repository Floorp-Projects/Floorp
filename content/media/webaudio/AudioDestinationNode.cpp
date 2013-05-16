/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDestinationNode.h"
#include "mozilla/dom/AudioDestinationNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "MediaStreamGraph.h"
#include "OfflineAudioCompletionEvent.h"

namespace mozilla {
namespace dom {

class OfflineDestinationNodeEngine : public AudioNodeEngine
{
public:
  typedef AutoFallibleTArray<nsAutoArrayPtr<float>, 2> InputChannels;

  OfflineDestinationNodeEngine(AudioDestinationNode* aNode,
                               uint32_t aNumberOfChannels,
                               uint32_t aLength,
                               float aSampleRate)
    : AudioNodeEngine(aNode)
    , mWriteIndex(0)
    , mLength(aLength)
    , mSampleRate(aSampleRate)
  {
    // These allocations might fail if content provides a huge number of
    // channels or size, but it's OK since we'll deal with the failure
    // gracefully.
    if (mInputChannels.SetLength(aNumberOfChannels)) {
      static const fallible_t fallible = fallible_t();
      for (uint32_t i = 0; i < aNumberOfChannels; ++i) {
        mInputChannels[i] = new(fallible) float[aLength];
        if (!mInputChannels[i]) {
          mInputChannels.Clear();
          break;
        }
      }
    }
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished) MOZ_OVERRIDE
  {
    // Do this just for the sake of political correctness; this output
    // will not go anywhere.
    *aOutput = aInput;

    // Handle the case of allocation failure in the input buffer
    if (mInputChannels.IsEmpty()) {
      return;
    }

    // Record our input buffer
    MOZ_ASSERT(mWriteIndex < mLength, "How did this happen?");
    const uint32_t duration = std::min(WEBAUDIO_BLOCK_SIZE, mLength - mWriteIndex);
    for (uint32_t i = 0; i < mInputChannels.Length(); ++i) {
      if (aInput.IsNull()) {
        PodZero(mInputChannels[i] + mWriteIndex, duration);
      } else {
        const float* inputBuffer = static_cast<const float*>(aInput.mChannelData[i]);
        if (duration == WEBAUDIO_BLOCK_SIZE) {
          // Use the optimized version of the copy with scale operation
          AudioBlockCopyChannelWithScale(inputBuffer, aInput.mVolume,
                                         mInputChannels[i] + mWriteIndex);
        } else {
          if (aInput.mVolume == 1.0f) {
            PodCopy(mInputChannels[i] + mWriteIndex, inputBuffer, duration);
          } else {
            for (uint32_t j = 0; j < duration; ++j) {
              mInputChannels[i][mWriteIndex + j] = aInput.mVolume * inputBuffer[j];
            }
          }
        }
      }
    }
    mWriteIndex += duration;

    if (mWriteIndex == mLength) {
      SendBufferToMainThread(aStream);
      *aFinished = true;
    }
  }

  void SendBufferToMainThread(AudioNodeStream* aStream)
  {
    class Command : public nsRunnable
    {
    public:
      Command(AudioNodeStream* aStream,
              InputChannels& aInputChannels,
              uint32_t aLength,
              float aSampleRate)
        : mStream(aStream)
        , mLength(aLength)
        , mSampleRate(aSampleRate)
      {
        mInputChannels.SwapElements(aInputChannels);
      }

      NS_IMETHODIMP Run()
      {
        // If it's not safe to run scripts right now, schedule this to run later
        if (!nsContentUtils::IsSafeToRunScript()) {
          nsContentUtils::AddScriptRunner(this);
          return NS_OK;
        }

        nsRefPtr<AudioContext> context;
        {
          MutexAutoLock lock(mStream->Engine()->NodeMutex());
          AudioNode* node = mStream->Engine()->Node();
          if (node) {
            context = node->Context();
          }
        }
        if (!context) {
          return NS_OK;
        }

        AutoPushJSContext cx(context->GetJSContext());
        if (cx) {
          JSAutoRequest ar(cx);

          // Create the input buffer
          nsRefPtr<AudioBuffer> renderedBuffer = new AudioBuffer(context,
                                                                 mLength,
                                                                 mSampleRate);
          if (!renderedBuffer->InitializeBuffers(mInputChannels.Length(), cx)) {
            return NS_OK;
          }
          for (uint32_t i = 0; i < mInputChannels.Length(); ++i) {
            renderedBuffer->SetRawChannelContents(cx, i, mInputChannels[i]);
          }

          nsRefPtr<OfflineAudioCompletionEvent> event =
              new OfflineAudioCompletionEvent(context, nullptr, nullptr);
          event->InitEvent(renderedBuffer);
          context->DispatchTrustedEvent(event);
        }

        return NS_OK;
      }
    private:
      nsRefPtr<AudioNodeStream> mStream;
      InputChannels mInputChannels;
      uint32_t mLength;
      float mSampleRate;
    };

    // Empty out the source array to make sure we don't attempt to collect
    // more input data in the future.
    NS_DispatchToMainThread(new Command(aStream, mInputChannels, mLength, mSampleRate));
  }

private:
  // The input to the destination node is recorded in the mInputChannels buffer.
  // When this buffer fills up with mLength frames, the buffered input is sent
  // to the main thread in order to dispatch OfflineAudioCompletionEvent.
  InputChannels mInputChannels;
  // An index representing the next offset in mInputChannels to be written to.
  uint32_t mWriteIndex;
  // How many frames the OfflineAudioContext intends to produce.
  uint32_t mLength;
  float mSampleRate;
};

NS_IMPL_ISUPPORTS_INHERITED0(AudioDestinationNode, AudioNode)

AudioDestinationNode::AudioDestinationNode(AudioContext* aContext,
                                           bool aIsOffline,
                                           uint32_t aNumberOfChannels,
                                           uint32_t aLength,
                                           float aSampleRate)
  : AudioNode(aContext,
              aIsOffline ? aNumberOfChannels : 2,
              ChannelCountMode::Explicit,
              ChannelInterpretation::Speakers)
  , mFramesToProduce(aLength)
{
  MediaStreamGraph* graph = aIsOffline ?
                            MediaStreamGraph::CreateNonRealtimeInstance() :
                            MediaStreamGraph::GetInstance();
  AudioNodeEngine* engine = aIsOffline ?
                            new OfflineDestinationNodeEngine(this, aNumberOfChannels,
                                                             aLength, aSampleRate) :
                            new AudioNodeEngine(this);
  mStream = graph->CreateAudioNodeStream(engine, MediaStreamGraph::EXTERNAL_STREAM);
}

void
AudioDestinationNode::DestroyGraph()
{
  MOZ_ASSERT(Context() && Context()->IsOffline(),
             "Should only be called on a valid OfflineAudioContext");
  MediaStreamGraph::DestroyNonRealtimeInstance(mStream->Graph());
}

JSObject*
AudioDestinationNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AudioDestinationNodeBinding::Wrap(aCx, aScope, this);
}

void
AudioDestinationNode::StartRendering()
{
  mStream->Graph()->StartNonRealtimeProcessing(mFramesToProduce);
}

}
}
