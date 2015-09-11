/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConvolverNode.h"
#include "mozilla/dom/ConvolverNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "blink/Reverb.h"
#include "PlayingRefChangeHandler.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ConvolverNode, AudioNode, mBuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ConvolverNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(ConvolverNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(ConvolverNode, AudioNode)

class ConvolverNodeEngine final : public AudioNodeEngine
{
  typedef PlayingRefChangeHandler PlayingRefChanged;
public:
  ConvolverNodeEngine(AudioNode* aNode, bool aNormalize)
    : AudioNodeEngine(aNode)
    , mBufferLength(0)
    , mLeftOverData(INT32_MIN)
    , mSampleRate(0.0f)
    , mUseBackgroundThreads(!aNode->Context()->IsOffline())
    , mNormalize(aNormalize)
  {
  }

  enum Parameters {
    BUFFER_LENGTH,
    SAMPLE_RATE,
    NORMALIZE
  };
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override
  {
    switch (aIndex) {
    case BUFFER_LENGTH:
      // BUFFER_LENGTH is the first parameter that we set when setting a new buffer,
      // so we should be careful to invalidate the rest of our state here.
      mBuffer = nullptr;
      mSampleRate = 0.0f;
      mBufferLength = aParam;
      mLeftOverData = INT32_MIN;
      break;
    case SAMPLE_RATE:
      mSampleRate = aParam;
      break;
    case NORMALIZE:
      mNormalize = !!aParam;
      break;
    default:
      NS_ERROR("Bad ConvolverNodeEngine Int32Parameter");
    }
  }
  virtual void SetDoubleParameter(uint32_t aIndex, double aParam) override
  {
    switch (aIndex) {
    case SAMPLE_RATE:
      mSampleRate = aParam;
      AdjustReverb();
      break;
    default:
      NS_ERROR("Bad ConvolverNodeEngine DoubleParameter");
    }
  }
  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer) override
  {
    mBuffer = aBuffer;
    AdjustReverb();
  }

  void AdjustReverb()
  {
    // Note about empirical tuning (this is copied from Blink)
    // The maximum FFT size affects reverb performance and accuracy.
    // If the reverb is single-threaded and processes entirely in the real-time audio thread,
    // it's important not to make this too high.  In this case 8192 is a good value.
    // But, the Reverb object is multi-threaded, so we want this as high as possible without losing too much accuracy.
    // Very large FFTs will have worse phase errors. Given these constraints 32768 is a good compromise.
    const size_t MaxFFTSize = 32768;

    if (!mBuffer || !mBufferLength || !mSampleRate) {
      mReverb = nullptr;
      mLeftOverData = INT32_MIN;
      return;
    }

    mReverb = new WebCore::Reverb(mBuffer, mBufferLength,
                                  WEBAUDIO_BLOCK_SIZE,
                                  MaxFFTSize, 2, mUseBackgroundThreads,
                                  mNormalize, mSampleRate);
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioBlock& aInput,
                            AudioBlock* aOutput,
                            bool* aFinished) override
  {
    if (!mReverb) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    AudioBlock input = aInput;
    if (aInput.IsNull()) {
      if (mLeftOverData > 0) {
        mLeftOverData -= WEBAUDIO_BLOCK_SIZE;
        input.AllocateChannels(1);
        WriteZeroesToAudioBlock(&input, 0, WEBAUDIO_BLOCK_SIZE);
      } else {
        if (mLeftOverData != INT32_MIN) {
          mLeftOverData = INT32_MIN;
          nsRefPtr<PlayingRefChanged> refchanged =
            new PlayingRefChanged(aStream, PlayingRefChanged::RELEASE);
          aStream->Graph()->
            DispatchToMainThreadAfterStreamStateUpdate(refchanged.forget());
        }
        aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
        return;
      }
    } else {
      if (aInput.mVolume != 1.0f) {
        // Pre-multiply the input's volume
        uint32_t numChannels = aInput.ChannelCount();
        input.AllocateChannels(numChannels);
        for (uint32_t i = 0; i < numChannels; ++i) {
          const float* src = static_cast<const float*>(aInput.mChannelData[i]);
          float* dest = input.ChannelFloatsForWrite(i);
          AudioBlockCopyChannelWithScale(src, aInput.mVolume, dest);
        }
      }

      if (mLeftOverData <= 0) {
        nsRefPtr<PlayingRefChanged> refchanged =
          new PlayingRefChanged(aStream, PlayingRefChanged::ADDREF);
        aStream->Graph()->
          DispatchToMainThreadAfterStreamStateUpdate(refchanged.forget());
      }
      mLeftOverData = mBufferLength;
      MOZ_ASSERT(mLeftOverData > 0);
    }
    aOutput->AllocateChannels(2);

    mReverb->process(&input, aOutput, WEBAUDIO_BLOCK_SIZE);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    if (mBuffer && !mBuffer->IsShared()) {
      amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
    }

    if (mReverb) {
      amount += mReverb->sizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  nsRefPtr<ThreadSharedFloatArrayBufferList> mBuffer;
  nsAutoPtr<WebCore::Reverb> mReverb;
  int32_t mBufferLength;
  int32_t mLeftOverData;
  float mSampleRate;
  bool mUseBackgroundThreads;
  bool mNormalize;
};

ConvolverNode::ConvolverNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Clamped_max,
              ChannelInterpretation::Speakers)
  , mNormalize(true)
{
  ConvolverNodeEngine* engine = new ConvolverNodeEngine(this, mNormalize);
  mStream = AudioNodeStream::Create(aContext->Graph(), engine,
                                    AudioNodeStream::NO_STREAM_FLAGS);
}

ConvolverNode::~ConvolverNode()
{
}

size_t
ConvolverNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  if (mBuffer) {
    // NB: mBuffer might be shared with the associated engine, by convention
    //     the AudioNode will report.
    amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
  }
  return amount;
}

size_t
ConvolverNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
ConvolverNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ConvolverNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
ConvolverNode::SetBuffer(JSContext* aCx, AudioBuffer* aBuffer, ErrorResult& aRv)
{
  if (aBuffer) {
    switch (aBuffer->NumberOfChannels()) {
    case 1:
    case 2:
    case 4:
      // Supported number of channels
      break;
    default:
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }
  }

  mBuffer = aBuffer;

  // Send the buffer to the stream
  AudioNodeStream* ns = mStream;
  MOZ_ASSERT(ns, "Why don't we have a stream here?");
  if (mBuffer) {
    uint32_t length = mBuffer->Length();
    nsRefPtr<ThreadSharedFloatArrayBufferList> data =
      mBuffer->GetThreadSharedChannelsForRate(aCx);
    if (data && length < WEBAUDIO_BLOCK_SIZE) {
      // For very small impulse response buffers, we need to pad the
      // buffer with 0 to make sure that the Reverb implementation
      // has enough data to compute FFTs from.
      length = WEBAUDIO_BLOCK_SIZE;
      nsRefPtr<ThreadSharedFloatArrayBufferList> paddedBuffer =
        new ThreadSharedFloatArrayBufferList(data->GetChannels());
      float* channelData = (float*) malloc(sizeof(float) * length * data->GetChannels());
      for (uint32_t i = 0; i < data->GetChannels(); ++i) {
        PodCopy(channelData + length * i, data->GetData(i), mBuffer->Length());
        PodZero(channelData + length * i + mBuffer->Length(), WEBAUDIO_BLOCK_SIZE - mBuffer->Length());
        paddedBuffer->SetData(i, (i == 0) ? channelData : nullptr, free, channelData);
      }
      data = paddedBuffer;
    }
    SendInt32ParameterToStream(ConvolverNodeEngine::BUFFER_LENGTH, length);
    SendDoubleParameterToStream(ConvolverNodeEngine::SAMPLE_RATE,
                                mBuffer->SampleRate());
    ns->SetBuffer(data.forget());
  } else {
    ns->SetBuffer(nullptr);
  }
}

void
ConvolverNode::SetNormalize(bool aNormalize)
{
  mNormalize = aNormalize;
  SendInt32ParameterToStream(ConvolverNodeEngine::NORMALIZE, aNormalize);
}

} // namespace dom
} // namespace mozilla

