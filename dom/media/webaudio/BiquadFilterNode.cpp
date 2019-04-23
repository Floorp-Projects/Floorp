/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BiquadFilterNode.h"
#include "AlignmentUtils.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "PlayingRefChangeHandler.h"
#include "WebAudioUtils.h"
#include "blink/Biquad.h"
#include "mozilla/UniquePtr.h"
#include "AudioParamTimeline.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(BiquadFilterNode, AudioNode, mFrequency,
                                   mDetune, mQ, mGain)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BiquadFilterNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(BiquadFilterNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(BiquadFilterNode, AudioNode)

static void SetParamsOnBiquad(WebCore::Biquad& aBiquad, float aSampleRate,
                              BiquadFilterType aType, double aFrequency,
                              double aQ, double aGain, double aDetune) {
  const double nyquist = aSampleRate * 0.5;
  double normalizedFrequency = aFrequency / nyquist;

  if (aDetune) {
    normalizedFrequency *= std::exp2(aDetune / 1200);
  }

  switch (aType) {
    case BiquadFilterType::Lowpass:
      aBiquad.setLowpassParams(normalizedFrequency, aQ);
      break;
    case BiquadFilterType::Highpass:
      aBiquad.setHighpassParams(normalizedFrequency, aQ);
      break;
    case BiquadFilterType::Bandpass:
      aBiquad.setBandpassParams(normalizedFrequency, aQ);
      break;
    case BiquadFilterType::Lowshelf:
      aBiquad.setLowShelfParams(normalizedFrequency, aGain);
      break;
    case BiquadFilterType::Highshelf:
      aBiquad.setHighShelfParams(normalizedFrequency, aGain);
      break;
    case BiquadFilterType::Peaking:
      aBiquad.setPeakingParams(normalizedFrequency, aQ, aGain);
      break;
    case BiquadFilterType::Notch:
      aBiquad.setNotchParams(normalizedFrequency, aQ);
      break;
    case BiquadFilterType::Allpass:
      aBiquad.setAllpassParams(normalizedFrequency, aQ);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("We should never see the alternate names here");
      break;
  }
}

class BiquadFilterNodeEngine final : public AudioNodeEngine {
 public:
  BiquadFilterNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination,
                         uint64_t aWindowID)
      : AudioNodeEngine(aNode),
        mDestination(aDestination->Stream())
        // Keep the default values in sync with the default values in
        // BiquadFilterNode::BiquadFilterNode
        ,
        mType(BiquadFilterType::Lowpass),
        mFrequency(350.f),
        mDetune(0.f),
        mQ(1.f),
        mGain(0.f),
        mWindowID(aWindowID) {}

  enum Parameters { TYPE, FREQUENCY, DETUNE, Q, GAIN };
  void SetInt32Parameter(uint32_t aIndex, int32_t aValue) override {
    switch (aIndex) {
      case TYPE:
        mType = static_cast<BiquadFilterType>(aValue);
        break;
      default:
        NS_ERROR("Bad BiquadFilterNode Int32Parameter");
    }
  }
  void RecvTimelineEvent(uint32_t aIndex, AudioTimelineEvent& aEvent) override {
    MOZ_ASSERT(mDestination);

    WebAudioUtils::ConvertAudioTimelineEventToTicks(aEvent, mDestination);

    switch (aIndex) {
      case FREQUENCY:
        mFrequency.InsertEvent<int64_t>(aEvent);
        break;
      case DETUNE:
        mDetune.InsertEvent<int64_t>(aEvent);
        break;
      case Q:
        mQ.InsertEvent<int64_t>(aEvent);
        break;
      case GAIN:
        mGain.InsertEvent<int64_t>(aEvent);
        break;
      default:
        NS_ERROR("Bad BiquadFilterNodeEngine TimelineParameter");
    }
  }

  void ProcessBlock(AudioNodeStream* aStream, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    float inputBuffer[WEBAUDIO_BLOCK_SIZE + 4];
    float* alignedInputBuffer = ALIGNED16(inputBuffer);
    ASSERT_ALIGNED16(alignedInputBuffer);

    if (aInput.IsNull()) {
      bool hasTail = false;
      for (uint32_t i = 0; i < mBiquads.Length(); ++i) {
        if (mBiquads[i].hasTail()) {
          hasTail = true;
          break;
        }
      }
      if (!hasTail) {
        if (!mBiquads.IsEmpty()) {
          mBiquads.Clear();
          aStream->ScheduleCheckForInactive();

          RefPtr<PlayingRefChangeHandler> refchanged =
              new PlayingRefChangeHandler(aStream,
                                          PlayingRefChangeHandler::RELEASE);
          aStream->Graph()->DispatchToMainThreadStableState(
              refchanged.forget());
        }

        aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
        return;
      }

      PodArrayZero(inputBuffer);

    } else if (mBiquads.Length() != aInput.ChannelCount()) {
      if (mBiquads.IsEmpty()) {
        RefPtr<PlayingRefChangeHandler> refchanged =
            new PlayingRefChangeHandler(aStream,
                                        PlayingRefChangeHandler::ADDREF);
        aStream->Graph()->DispatchToMainThreadStableState(refchanged.forget());
      } else {  // Help people diagnose bug 924718
        WebAudioUtils::LogToDeveloperConsole(
            mWindowID, "BiquadFilterChannelCountChangeWarning");
      }

      // Adjust the number of biquads based on the number of channels
      mBiquads.SetLength(aInput.ChannelCount());
    }

    uint32_t numberOfChannels = mBiquads.Length();
    aOutput->AllocateChannels(numberOfChannels);

    StreamTime pos = mDestination->GraphTimeToStreamTime(aFrom);

    double freq = mFrequency.GetValueAtTime(pos);
    double q = mQ.GetValueAtTime(pos);
    double gain = mGain.GetValueAtTime(pos);
    double detune = mDetune.GetValueAtTime(pos);

    for (uint32_t i = 0; i < numberOfChannels; ++i) {
      const float* input;
      if (aInput.IsNull()) {
        input = alignedInputBuffer;
      } else {
        input = static_cast<const float*>(aInput.mChannelData[i]);
        if (aInput.mVolume != 1.0) {
          AudioBlockCopyChannelWithScale(input, aInput.mVolume,
                                         alignedInputBuffer);
          input = alignedInputBuffer;
        }
      }
      SetParamsOnBiquad(mBiquads[i], aStream->SampleRate(), mType, freq, q,
                        gain, detune);

      mBiquads[i].process(input, aOutput->ChannelFloatsForWrite(i),
                          aInput.GetDuration());
    }
  }

  bool IsActive() const override { return !mBiquads.IsEmpty(); }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    // Not owned:
    // - mDestination - probably not owned
    // - AudioParamTimelines - counted in the AudioNode
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mBiquads.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  RefPtr<AudioNodeStream> mDestination;
  BiquadFilterType mType;
  AudioParamTimeline mFrequency;
  AudioParamTimeline mDetune;
  AudioParamTimeline mQ;
  AudioParamTimeline mGain;
  nsTArray<WebCore::Biquad> mBiquads;
  uint64_t mWindowID;
};

BiquadFilterNode::BiquadFilterNode(AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers),
      mType(BiquadFilterType::Lowpass) {
  CreateAudioParam(mFrequency, BiquadFilterNodeEngine::FREQUENCY, "frequency",
                   350.f, -(aContext->SampleRate() / 2),
                   aContext->SampleRate() / 2);
  CreateAudioParam(mDetune, BiquadFilterNodeEngine::DETUNE, "detune", 0.f);
  CreateAudioParam(mQ, BiquadFilterNodeEngine::Q, "Q", 1.f);
  CreateAudioParam(mGain, BiquadFilterNodeEngine::GAIN, "gain", 0.f);

  uint64_t windowID = 0;
  if (aContext->GetParentObject()) {
    windowID = aContext->GetParentObject()->WindowID();
  }
  BiquadFilterNodeEngine* engine =
      new BiquadFilterNodeEngine(this, aContext->Destination(), windowID);
  mStream = AudioNodeStream::Create(
      aContext, engine, AudioNodeStream::NO_STREAM_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<BiquadFilterNode> BiquadFilterNode::Create(
    AudioContext& aAudioContext, const BiquadFilterOptions& aOptions,
    ErrorResult& aRv) {
  RefPtr<BiquadFilterNode> audioNode = new BiquadFilterNode(&aAudioContext);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  audioNode->SetType(aOptions.mType);
  audioNode->Q()->SetValue(aOptions.mQ);
  audioNode->Detune()->SetValue(aOptions.mDetune);
  audioNode->Frequency()->SetValue(aOptions.mFrequency);
  audioNode->Gain()->SetValue(aOptions.mGain);

  return audioNode.forget();
}

size_t BiquadFilterNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);

  if (mFrequency) {
    amount += mFrequency->SizeOfIncludingThis(aMallocSizeOf);
  }

  if (mDetune) {
    amount += mDetune->SizeOfIncludingThis(aMallocSizeOf);
  }

  if (mQ) {
    amount += mQ->SizeOfIncludingThis(aMallocSizeOf);
  }

  if (mGain) {
    amount += mGain->SizeOfIncludingThis(aMallocSizeOf);
  }

  return amount;
}

size_t BiquadFilterNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject* BiquadFilterNode::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return BiquadFilterNode_Binding::Wrap(aCx, this, aGivenProto);
}

void BiquadFilterNode::SetType(BiquadFilterType aType) {
  mType = aType;
  SendInt32ParameterToStream(BiquadFilterNodeEngine::TYPE,
                             static_cast<int32_t>(aType));
}

void BiquadFilterNode::GetFrequencyResponse(
    const Float32Array& aFrequencyHz, const Float32Array& aMagResponse,
    const Float32Array& aPhaseResponse) {
  aFrequencyHz.ComputeLengthAndData();
  aMagResponse.ComputeLengthAndData();
  aPhaseResponse.ComputeLengthAndData();

  uint32_t length =
      std::min(std::min(aFrequencyHz.Length(), aMagResponse.Length()),
               aPhaseResponse.Length());
  if (!length) {
    return;
  }

  auto frequencies = MakeUnique<float[]>(length);
  float* frequencyHz = aFrequencyHz.Data();
  const double nyquist = Context()->SampleRate() * 0.5;

  // Normalize the frequencies
  for (uint32_t i = 0; i < length; ++i) {
    if (frequencyHz[i] >= 0 && frequencyHz[i] <= nyquist) {
      frequencies[i] = static_cast<float>(frequencyHz[i] / nyquist);
    } else {
      frequencies[i] = std::numeric_limits<float>::quiet_NaN();
    }
  }

  const double currentTime = Context()->CurrentTime();

  double freq = mFrequency->GetValueAtTime(currentTime);
  double q = mQ->GetValueAtTime(currentTime);
  double gain = mGain->GetValueAtTime(currentTime);
  double detune = mDetune->GetValueAtTime(currentTime);

  WebCore::Biquad biquad;
  SetParamsOnBiquad(biquad, Context()->SampleRate(), mType, freq, q, gain,
                    detune);
  biquad.getFrequencyResponse(int(length), frequencies.get(),
                              aMagResponse.Data(), aPhaseResponse.Data());
}

}  // namespace dom
}  // namespace mozilla
