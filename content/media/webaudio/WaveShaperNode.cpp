/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaveShaperNode.h"
#include "mozilla/dom/WaveShaperNodeBinding.h"
#include "AudioNode.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "mozilla/PodOperations.h"
#include "speex/speex_resampler.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(WaveShaperNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WaveShaperNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->ClearCurve();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WaveShaperNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WaveShaperNode)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCurve)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(WaveShaperNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(WaveShaperNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(WaveShaperNode, AudioNode)

static uint32_t ValueOf(OverSampleType aType)
{
  switch (aType) {
  case OverSampleType::None: return 1;
  case OverSampleType::_2x:  return 2;
  case OverSampleType::_4x:  return 4;
  default:
    NS_NOTREACHED("We should never reach here");
    return 1;
  }
}

class Resampler
{
public:
  Resampler()
    : mType(OverSampleType::None)
    , mUpSampler(nullptr)
    , mDownSampler(nullptr)
    , mChannels(0)
    , mSampleRate(0)
  {
  }

  ~Resampler()
  {
    Destroy();
  }

  void Reset(uint32_t aChannels, TrackRate aSampleRate, OverSampleType aType)
  {
    if (aChannels == mChannels &&
        aSampleRate == mSampleRate &&
        aType == mType) {
      return;
    }

    mChannels = aChannels;
    mSampleRate = aSampleRate;
    mType = aType;

    Destroy();

    if (aType == OverSampleType::None) {
      mBuffer.Clear();
      return;
    }

    mUpSampler = speex_resampler_init(aChannels,
                                      aSampleRate,
                                      aSampleRate * ValueOf(aType),
                                      SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                      nullptr);
    mDownSampler = speex_resampler_init(aChannels,
                                        aSampleRate * ValueOf(aType),
                                        aSampleRate,
                                        SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                        nullptr);
    mBuffer.SetLength(WEBAUDIO_BLOCK_SIZE*ValueOf(aType));
  }

  float* UpSample(uint32_t aChannel, const float* aInputData, uint32_t aBlocks)
  {
    uint32_t inSamples = WEBAUDIO_BLOCK_SIZE;
    uint32_t outSamples = WEBAUDIO_BLOCK_SIZE*aBlocks;
    float* outputData = mBuffer.Elements();

    MOZ_ASSERT(mBuffer.Length() == outSamples);

    speex_resampler_process_float(mUpSampler, aChannel,
                                  aInputData, &inSamples,
                                  outputData, &outSamples);

    MOZ_ASSERT(inSamples == WEBAUDIO_BLOCK_SIZE && outSamples == WEBAUDIO_BLOCK_SIZE*aBlocks);

    return outputData;
  }

  void DownSample(uint32_t aChannel, float* aOutputData, uint32_t aBlocks)
  {
    uint32_t inSamples = WEBAUDIO_BLOCK_SIZE*aBlocks;
    uint32_t outSamples = WEBAUDIO_BLOCK_SIZE;
    const float* inputData = mBuffer.Elements();

    MOZ_ASSERT(mBuffer.Length() == inSamples);

    speex_resampler_process_float(mDownSampler, aChannel,
                                  inputData, &inSamples,
                                  aOutputData, &outSamples);

    MOZ_ASSERT(inSamples == WEBAUDIO_BLOCK_SIZE*aBlocks && outSamples == WEBAUDIO_BLOCK_SIZE);
  }

private:
  void Destroy()
  {
    if (mUpSampler) {
      speex_resampler_destroy(mUpSampler);
      mUpSampler = nullptr;
    }
    if (mDownSampler) {
      speex_resampler_destroy(mDownSampler);
      mDownSampler = nullptr;
    }
  }

private:
  OverSampleType mType;
  SpeexResamplerState* mUpSampler;
  SpeexResamplerState* mDownSampler;
  uint32_t mChannels;
  TrackRate mSampleRate;
  nsTArray<float> mBuffer;
};

class WaveShaperNodeEngine : public AudioNodeEngine
{
public:
  explicit WaveShaperNodeEngine(AudioNode* aNode)
    : AudioNodeEngine(aNode)
    , mType(OverSampleType::None)
  {
  }

  enum Parameteres {
    TYPE
  };

  virtual void SetRawArrayData(nsTArray<float>& aCurve) MOZ_OVERRIDE
  {
    mCurve.SwapElements(aCurve);
  }

  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aValue) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case TYPE:
      mType = static_cast<OverSampleType>(aValue);
      break;
    default:
      NS_ERROR("Bad WaveShaperNode Int32Parameter");
    }
  }

  template <uint32_t blocks>
  void ProcessCurve(const float* aInputBuffer, float* aOutputBuffer)
  {
    for (uint32_t j = 0; j < WEBAUDIO_BLOCK_SIZE*blocks; ++j) {
      // Index into the curve array based on the amplitude of the
      // incoming signal by clamping the amplitude to [-1, 1] and
      // performing a linear interpolation of the neighbor values.
      float index = std::max(0.0f, std::min(float(mCurve.Length() - 1),
                                            mCurve.Length() * (aInputBuffer[j] + 1) / 2));
      uint32_t indexLower = uint32_t(index);
      uint32_t indexHigher = uint32_t(index + 1.0f);
      if (indexHigher == mCurve.Length()) {
        aOutputBuffer[j] = mCurve[indexLower];
      } else {
        float interpolationFactor = index - indexLower;
        aOutputBuffer[j] = (1.0f - interpolationFactor) * mCurve[indexLower] +
                                   interpolationFactor  * mCurve[indexHigher];
      }
    }
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished)
  {
    uint32_t channelCount = aInput.mChannelData.Length();
    if (!mCurve.Length() || !channelCount) {
      // Optimize the case where we don't have a curve buffer,
      // or the input is null.
      *aOutput = aInput;
      return;
    }

    AllocateAudioBlock(channelCount, aOutput);
    for (uint32_t i = 0; i < channelCount; ++i) {
      const float* inputBuffer = static_cast<const float*>(aInput.mChannelData[i]);
      float* outputBuffer = const_cast<float*> (static_cast<const float*>(aOutput->mChannelData[i]));
      float* sampleBuffer;

      switch (mType) {
      case OverSampleType::None:
        mResampler.Reset(channelCount, aStream->SampleRate(), OverSampleType::None);
        ProcessCurve<1>(inputBuffer, outputBuffer);
        break;
      case OverSampleType::_2x:
        mResampler.Reset(channelCount, aStream->SampleRate(), OverSampleType::_2x);
        sampleBuffer = mResampler.UpSample(i, inputBuffer, 2);
        ProcessCurve<2>(sampleBuffer, sampleBuffer);
        mResampler.DownSample(i, outputBuffer, 2);
        break;
      case OverSampleType::_4x:
        mResampler.Reset(channelCount, aStream->SampleRate(), OverSampleType::_4x);
        sampleBuffer = mResampler.UpSample(i, inputBuffer, 4);
        ProcessCurve<4>(sampleBuffer, sampleBuffer);
        mResampler.DownSample(i, outputBuffer, 4);
        break;
      default:
        NS_NOTREACHED("We should never reach here");
      }
    }
  }

private:
  nsTArray<float> mCurve;
  OverSampleType mType;
  Resampler mResampler;
};

WaveShaperNode::WaveShaperNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mCurve(nullptr)
  , mType(OverSampleType::None)
{
  NS_HOLD_JS_OBJECTS(this, WaveShaperNode);

  WaveShaperNodeEngine* engine = new WaveShaperNodeEngine(this);
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
}

WaveShaperNode::~WaveShaperNode()
{
  ClearCurve();
}

void
WaveShaperNode::ClearCurve()
{
  mCurve = nullptr;
  NS_DROP_JS_OBJECTS(this, WaveShaperNode);
}

JSObject*
WaveShaperNode::WrapObject(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return WaveShaperNodeBinding::Wrap(aCx, aScope, this);
}

void
WaveShaperNode::SetCurve(const Float32Array* aCurve)
{
  nsTArray<float> curve;
  if (aCurve) {
    mCurve = aCurve->Obj();

    curve.SetLength(aCurve->Length());
    PodCopy(curve.Elements(), aCurve->Data(), aCurve->Length());
  } else {
    mCurve = nullptr;
  }

  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  MOZ_ASSERT(ns, "Why don't we have a stream here?");
  ns->SetRawArrayData(curve);
}

void
WaveShaperNode::SetOversample(OverSampleType aType)
{
  mType = aType;
  SendInt32ParameterToStream(WaveShaperNodeEngine::TYPE, static_cast<int32_t>(aType));
}

}
}
