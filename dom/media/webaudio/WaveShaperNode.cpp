/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaveShaperNode.h"
#include "mozilla/dom/WaveShaperNodeBinding.h"
#include "AlignmentUtils.h"
#include "AudioNode.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "mozilla/PodOperations.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(WaveShaperNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WaveShaperNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WaveShaperNode, AudioNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WaveShaperNode)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WaveShaperNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(WaveShaperNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(WaveShaperNode, AudioNode)

static uint32_t ValueOf(OverSampleType aType) {
  switch (aType) {
    case OverSampleType::None:
      return 1;
    case OverSampleType::_2x:
      return 2;
    case OverSampleType::_4x:
      return 4;
    default:
      MOZ_ASSERT_UNREACHABLE("We should never reach here");
      return 1;
  }
}

class Resampler final {
 public:
  Resampler()
      : mType(OverSampleType::None),
        mUpSampler(nullptr),
        mDownSampler(nullptr),
        mChannels(0),
        mSampleRate(0) {}

  ~Resampler() { Destroy(); }

  void Reset(uint32_t aChannels, TrackRate aSampleRate, OverSampleType aType) {
    if (aChannels == mChannels && aSampleRate == mSampleRate &&
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

    mUpSampler = speex_resampler_init(aChannels, aSampleRate,
                                      aSampleRate * ValueOf(aType),
                                      SPEEX_RESAMPLER_QUALITY_MIN, nullptr);
    mDownSampler =
        speex_resampler_init(aChannels, aSampleRate * ValueOf(aType),
                             aSampleRate, SPEEX_RESAMPLER_QUALITY_MIN, nullptr);
    mBuffer.SetLength(WEBAUDIO_BLOCK_SIZE * ValueOf(aType));
  }

  float* UpSample(uint32_t aChannel, const float* aInputData,
                  uint32_t aBlocks) {
    uint32_t inSamples = WEBAUDIO_BLOCK_SIZE;
    uint32_t outSamples = WEBAUDIO_BLOCK_SIZE * aBlocks;
    float* outputData = mBuffer.Elements();

    MOZ_ASSERT(mBuffer.Length() == outSamples);

    WebAudioUtils::SpeexResamplerProcess(mUpSampler, aChannel, aInputData,
                                         &inSamples, outputData, &outSamples);

    MOZ_ASSERT(inSamples == WEBAUDIO_BLOCK_SIZE &&
               outSamples == WEBAUDIO_BLOCK_SIZE * aBlocks);

    return outputData;
  }

  void DownSample(uint32_t aChannel, float* aOutputData, uint32_t aBlocks) {
    uint32_t inSamples = WEBAUDIO_BLOCK_SIZE * aBlocks;
    uint32_t outSamples = WEBAUDIO_BLOCK_SIZE;
    const float* inputData = mBuffer.Elements();

    MOZ_ASSERT(mBuffer.Length() == inSamples);

    WebAudioUtils::SpeexResamplerProcess(mDownSampler, aChannel, inputData,
                                         &inSamples, aOutputData, &outSamples);

    MOZ_ASSERT(inSamples == WEBAUDIO_BLOCK_SIZE * aBlocks &&
               outSamples == WEBAUDIO_BLOCK_SIZE);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t amount = 0;
    // Future: properly measure speex memory
    amount += aMallocSizeOf(mUpSampler);
    amount += aMallocSizeOf(mDownSampler);
    amount += mBuffer.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

 private:
  void Destroy() {
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

class WaveShaperNodeEngine final : public AudioNodeEngine {
 public:
  explicit WaveShaperNodeEngine(AudioNode* aNode)
      : AudioNodeEngine(aNode), mType(OverSampleType::None) {}

  enum Parameters { TYPE };

  void SetRawArrayData(nsTArray<float>&& aCurve) override {
    mCurve = std::move(aCurve);
  }

  void SetInt32Parameter(uint32_t aIndex, int32_t aValue) override {
    switch (aIndex) {
      case TYPE:
        mType = static_cast<OverSampleType>(aValue);
        break;
      default:
        NS_ERROR("Bad WaveShaperNode Int32Parameter");
    }
  }

  template <uint32_t blocks>
  void ProcessCurve(const float* aInputBuffer, float* aOutputBuffer) {
    for (uint32_t j = 0; j < WEBAUDIO_BLOCK_SIZE * blocks; ++j) {
      // Index into the curve array based on the amplitude of the
      // incoming signal by using an amplitude range of [-1, 1] and
      // performing a linear interpolation of the neighbor values.
      float index = (mCurve.Length() - 1) * (aInputBuffer[j] + 1.0f) / 2.0f;
      if (index < 0.0f) {
        aOutputBuffer[j] = mCurve[0];
      } else {
        int32_t indexLower = index;
        if (static_cast<uint32_t>(indexLower) >= mCurve.Length() - 1) {
          aOutputBuffer[j] = mCurve[mCurve.Length() - 1];
        } else {
          uint32_t indexHigher = indexLower + 1;
          float interpolationFactor = index - indexLower;
          aOutputBuffer[j] = (1.0f - interpolationFactor) * mCurve[indexLower] +
                             interpolationFactor * mCurve[indexHigher];
        }
      }
    }
  }

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    uint32_t channelCount = aInput.ChannelCount();
    if (!mCurve.Length()) {
      // Optimize the case where we don't have a curve buffer
      *aOutput = aInput;
      return;
    }

    // If the input is null, check to see if non-null output will be produced
    bool nullInput = false;
    if (channelCount == 0) {
      float index = (mCurve.Length() - 1) * 0.5;
      uint32_t indexLower = index;
      uint32_t indexHigher = indexLower + 1;
      float interpolationFactor = index - indexLower;
      if ((1.0f - interpolationFactor) * mCurve[indexLower] +
              interpolationFactor * mCurve[indexHigher] ==
          0.0) {
        *aOutput = aInput;
        return;
      }
      nullInput = true;
      channelCount = 1;
    }

    aOutput->AllocateChannels(channelCount);
    for (uint32_t i = 0; i < channelCount; ++i) {
      const float* inputSamples;
      float scaledInput[WEBAUDIO_BLOCK_SIZE + 4];
      float* alignedScaledInput = ALIGNED16(scaledInput);
      ASSERT_ALIGNED16(alignedScaledInput);
      if (!nullInput) {
        if (aInput.mVolume != 1.0f) {
          AudioBlockCopyChannelWithScale(
              static_cast<const float*>(aInput.mChannelData[i]), aInput.mVolume,
              alignedScaledInput);
          inputSamples = alignedScaledInput;
        } else {
          inputSamples = static_cast<const float*>(aInput.mChannelData[i]);
        }
      } else {
        PodZero(alignedScaledInput, WEBAUDIO_BLOCK_SIZE);
        inputSamples = alignedScaledInput;
      }
      float* outputBuffer = aOutput->ChannelFloatsForWrite(i);
      float* sampleBuffer;

      switch (mType) {
        case OverSampleType::None:
          mResampler.Reset(channelCount, aTrack->mSampleRate,
                           OverSampleType::None);
          ProcessCurve<1>(inputSamples, outputBuffer);
          break;
        case OverSampleType::_2x:
          mResampler.Reset(channelCount, aTrack->mSampleRate,
                           OverSampleType::_2x);
          sampleBuffer = mResampler.UpSample(i, inputSamples, 2);
          ProcessCurve<2>(sampleBuffer, sampleBuffer);
          mResampler.DownSample(i, outputBuffer, 2);
          break;
        case OverSampleType::_4x:
          mResampler.Reset(channelCount, aTrack->mSampleRate,
                           OverSampleType::_4x);
          sampleBuffer = mResampler.UpSample(i, inputSamples, 4);
          ProcessCurve<4>(sampleBuffer, sampleBuffer);
          mResampler.DownSample(i, outputBuffer, 4);
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("We should never reach here");
      }
    }
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mCurve.ShallowSizeOfExcludingThis(aMallocSizeOf);
    amount += mResampler.SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  nsTArray<float> mCurve;
  OverSampleType mType;
  Resampler mResampler;
};

WaveShaperNode::WaveShaperNode(AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers),
      mType(OverSampleType::None) {
  WaveShaperNodeEngine* engine = new WaveShaperNodeEngine(this);
  mTrack = AudioNodeTrack::Create(
      aContext, engine, AudioNodeTrack::NO_TRACK_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<WaveShaperNode> WaveShaperNode::Create(
    AudioContext& aAudioContext, const WaveShaperOptions& aOptions,
    ErrorResult& aRv) {
  RefPtr<WaveShaperNode> audioNode = new WaveShaperNode(&aAudioContext);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (aOptions.mCurve.WasPassed()) {
    audioNode->SetCurveInternal(aOptions.mCurve.Value(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  audioNode->SetOversample(aOptions.mOversample);
  return audioNode.forget();
}

JSObject* WaveShaperNode::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return WaveShaperNode_Binding::Wrap(aCx, this, aGivenProto);
}

void WaveShaperNode::SetCurve(const Nullable<Float32Array>& aCurve,
                              ErrorResult& aRv) {
  // Let's purge the cached value for the curve attribute.
  WaveShaperNode_Binding::ClearCachedCurveValue(this);

  if (aCurve.IsNull()) {
    CleanCurveInternal();
    return;
  }

  const Float32Array& floats = aCurve.Value();
  floats.ComputeState();

  nsTArray<float> curve;
  uint32_t argLength = floats.Length();
  if (!curve.SetLength(argLength, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  PodCopy(curve.Elements(), floats.Data(), argLength);
  SetCurveInternal(curve, aRv);
}

void WaveShaperNode::SetCurveInternal(const nsTArray<float>& aCurve,
                                      ErrorResult& aRv) {
  if (aCurve.Length() < 2) {
    aRv.ThrowInvalidStateError("Must have at least two entries");
    return;
  }

  mCurve = aCurve.Clone();
  SendCurveToTrack();
}

void WaveShaperNode::CleanCurveInternal() {
  mCurve.Clear();
  SendCurveToTrack();
}

void WaveShaperNode::SendCurveToTrack() {
  AudioNodeTrack* ns = mTrack;
  MOZ_ASSERT(ns, "Why don't we have a track here?");

  nsTArray<float> copyCurve(mCurve.Clone());
  ns->SetRawArrayData(std::move(copyCurve));
}

void WaveShaperNode::GetCurve(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetval) {
  // Let's return a null value if the list is empty.
  if (mCurve.IsEmpty()) {
    aRetval.set(nullptr);
    return;
  }

  MOZ_ASSERT(mCurve.Length() >= 2);
  aRetval.set(
      Float32Array::Create(aCx, this, mCurve.Length(), mCurve.Elements()));
}

void WaveShaperNode::SetOversample(OverSampleType aType) {
  mType = aType;
  SendInt32ParameterToTrack(WaveShaperNodeEngine::TYPE,
                            static_cast<int32_t>(aType));
}

}  // namespace mozilla::dom
