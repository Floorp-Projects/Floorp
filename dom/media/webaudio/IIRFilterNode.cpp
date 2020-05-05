/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IIRFilterNode.h"
#include "AudioNodeEngine.h"
#include "AudioDestinationNode.h"
#include "blink/IIRFilter.h"
#include "PlayingRefChangeHandler.h"
#include "AlignmentUtils.h"
#include "nsPrintfCString.h"

#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

class IIRFilterNodeEngine final : public AudioNodeEngine {
 public:
  IIRFilterNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination,
                      const AudioDoubleArray& aFeedforward,
                      const AudioDoubleArray& aFeedback, uint64_t aWindowID)
      : AudioNodeEngine(aNode),
        mDestination(aDestination->Track()),
        mFeedforward(aFeedforward.Clone()),
        mFeedback(aFeedback.Clone()),
        mWindowID(aWindowID) {}

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    float inputBuffer[WEBAUDIO_BLOCK_SIZE + 4];
    float* alignedInputBuffer = ALIGNED16(inputBuffer);
    ASSERT_ALIGNED16(alignedInputBuffer);

    if (aInput.IsNull()) {
      if (!mIIRFilters.IsEmpty()) {
        bool allZero = true;
        for (uint32_t i = 0; i < mIIRFilters.Length(); ++i) {
          allZero &= mIIRFilters[i]->buffersAreZero();
        }

        // all filter buffer values are zero, so the output will be zero
        // as well.
        if (allZero) {
          mIIRFilters.Clear();
          aTrack->ScheduleCheckForInactive();

          RefPtr<PlayingRefChangeHandler> refchanged =
              new PlayingRefChangeHandler(aTrack,
                                          PlayingRefChangeHandler::RELEASE);
          aTrack->Graph()->DispatchToMainThreadStableState(refchanged.forget());

          aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
          return;
        }

        PodZero(alignedInputBuffer, WEBAUDIO_BLOCK_SIZE);
      }
    } else if (mIIRFilters.Length() != aInput.ChannelCount()) {
      if (mIIRFilters.IsEmpty()) {
        RefPtr<PlayingRefChangeHandler> refchanged =
            new PlayingRefChangeHandler(aTrack,
                                        PlayingRefChangeHandler::ADDREF);
        aTrack->Graph()->DispatchToMainThreadStableState(refchanged.forget());
      } else {
        WebAudioUtils::LogToDeveloperConsole(
            mWindowID, "IIRFilterChannelCountChangeWarning");
      }

      // Adjust the number of filters based on the number of channels
      mIIRFilters.SetLength(aInput.ChannelCount());
      for (size_t i = 0; i < aInput.ChannelCount(); ++i) {
        mIIRFilters[i] =
            MakeUnique<blink::IIRFilter>(&mFeedforward, &mFeedback);
      }
    }

    uint32_t numberOfChannels = mIIRFilters.Length();
    aOutput->AllocateChannels(numberOfChannels);

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

      mIIRFilters[i]->process(input, aOutput->ChannelFloatsForWrite(i),
                              aInput.GetDuration());
    }
  }

  bool IsActive() const override { return !mIIRFilters.IsEmpty(); }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    // Not owned:
    // - mDestination - probably not owned
    // - AudioParamTimelines - counted in the AudioNode
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mIIRFilters.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  RefPtr<AudioNodeTrack> mDestination;
  nsTArray<UniquePtr<blink::IIRFilter>> mIIRFilters;
  AudioDoubleArray mFeedforward;
  AudioDoubleArray mFeedback;
  uint64_t mWindowID;
};

IIRFilterNode::IIRFilterNode(AudioContext* aContext,
                             const Sequence<double>& aFeedforward,
                             const Sequence<double>& aFeedback)
    : AudioNode(aContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers) {
  mFeedforward.SetLength(aFeedforward.Length());
  PodCopy(mFeedforward.Elements(), aFeedforward.Elements(),
          aFeedforward.Length());
  mFeedback.SetLength(aFeedback.Length());
  PodCopy(mFeedback.Elements(), aFeedback.Elements(), aFeedback.Length());

  // Scale coefficients -- we guarantee that mFeedback != 0 when creating
  // the IIRFilterNode.
  double scale = mFeedback[0];
  double* elements = mFeedforward.Elements();
  for (size_t i = 0; i < mFeedforward.Length(); ++i) {
    elements[i] /= scale;
  }

  elements = mFeedback.Elements();
  for (size_t i = 0; i < mFeedback.Length(); ++i) {
    elements[i] /= scale;
  }

  // We check that this is exactly equal to one later in blink/IIRFilter.cpp
  elements[0] = 1.0;

  uint64_t windowID = 0;
  if (aContext->GetParentObject()) {
    windowID = aContext->GetParentObject()->WindowID();
  }
  IIRFilterNodeEngine* engine = new IIRFilterNodeEngine(
      this, aContext->Destination(), mFeedforward, mFeedback, windowID);
  mTrack = AudioNodeTrack::Create(
      aContext, engine, AudioNodeTrack::NO_TRACK_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<IIRFilterNode> IIRFilterNode::Create(
    AudioContext& aAudioContext, const IIRFilterOptions& aOptions,
    ErrorResult& aRv) {
  if (aOptions.mFeedforward.Length() == 0 ||
      aOptions.mFeedforward.Length() > 20) {
    aRv.ThrowNotSupportedError(
        nsPrintfCString("\"feedforward\" length %zu is not in the range [1,20]",
                        aOptions.mFeedforward.Length()));
    return nullptr;
  }

  if (aOptions.mFeedback.Length() == 0 || aOptions.mFeedback.Length() > 20) {
    aRv.ThrowNotSupportedError(
        nsPrintfCString("\"feedback\" length %zu is not in the range [1,20]",
                        aOptions.mFeedforward.Length()));
    return nullptr;
  }

  bool feedforwardAllZeros = true;
  for (size_t i = 0; i < aOptions.mFeedforward.Length(); ++i) {
    if (aOptions.mFeedforward.Elements()[i] != 0.0) {
      feedforwardAllZeros = false;
      break;
    }
  }

  if (feedforwardAllZeros) {
    aRv.ThrowInvalidStateError(
        "\"feedforward\" must contain some nonzero values");
    return nullptr;
  }

  if (aOptions.mFeedback[0] == 0.0) {
    aRv.ThrowInvalidStateError("First value in \"feedback\" must be nonzero");
    return nullptr;
  }

  RefPtr<IIRFilterNode> audioNode = new IIRFilterNode(
      &aAudioContext, aOptions.mFeedforward, aOptions.mFeedback);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return audioNode.forget();
}

size_t IIRFilterNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t IIRFilterNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject* IIRFilterNode::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return IIRFilterNode_Binding::Wrap(aCx, this, aGivenProto);
}

void IIRFilterNode::GetFrequencyResponse(const Float32Array& aFrequencyHz,
                                         const Float32Array& aMagResponse,
                                         const Float32Array& aPhaseResponse) {
  aFrequencyHz.ComputeState();
  aMagResponse.ComputeState();
  aPhaseResponse.ComputeState();

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

  blink::IIRFilter filter(&mFeedforward, &mFeedback);
  filter.getFrequencyResponse(int(length), frequencies.get(),
                              aMagResponse.Data(), aPhaseResponse.Data());
}

}  // namespace dom
}  // namespace mozilla
