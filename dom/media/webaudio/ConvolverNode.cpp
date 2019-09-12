/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConvolverNode.h"
#include "mozilla/dom/ConvolverNodeBinding.h"
#include "nsAutoPtr.h"
#include "AlignmentUtils.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "blink/Reverb.h"
#include "PlayingRefChangeHandler.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ConvolverNode, AudioNode, mBuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ConvolverNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(ConvolverNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(ConvolverNode, AudioNode)

class ConvolverNodeEngine final : public AudioNodeEngine {
  typedef PlayingRefChangeHandler PlayingRefChanged;

 public:
  ConvolverNodeEngine(AudioNode* aNode, bool aNormalize, uint64_t aWindowID)
      : AudioNodeEngine(aNode),
        mWindowID(aWindowID),
        mUseBackgroundThreads(!aNode->Context()->IsOffline()),
        mNormalize(aNormalize) {}

  // Indicates how the right output channel is generated.
  enum class RightConvolverMode {
    // A right convolver is always used when there is more than one impulse
    // response channel.
    Always,
    // With a single response channel, the mode may be either Direct or
    // Difference.  The decision on which to use is made when stereo input is
    // received.  Once the right convolver is in use, convolver state is
    // suitable only for the selected mode, and so the mode cannot change
    // until the right convolver contains only silent history.
    //
    // With Direct mode, each convolver processes a corresponding channel.
    // This mode is selected when input is initially stereo or
    // channelInterpretation is "discrete" at the time or starting the right
    // convolver when input changes from non-silent mono to stereo.
    Direct,
    // Difference mode is selected if channelInterpretation is "speakers" at
    // the time starting the right convolver when the input changes from mono
    // to stereo.
    //
    // When non-silent input is initially mono, with a single response
    // channel, the right output channel is not produced until input becomes
    // stereo.  Only a single convolver is used for mono processing.  When
    // stereo input arrives after mono input, output must be as if the mono
    // signal remaining in the left convolver is up-mixed, but the right
    // convolver has not been initialized with the history of the mono input.
    // Copying the state of the left convolver into the right convolver is not
    // desirable, because there is considerable state to copy, and the
    // different convolvers are intended to process out of phase, which means
    // that state from one convolver would not directly map to state in
    // another convolver.
    //
    // Instead the distributive property of convolution is used to generate
    // the right output channel using information in the left output channel.
    // Using l and r to denote the left and right channel input signals, g the
    // impulse response, and * convolution, the convolution of the right
    // channel can be given by
    //
    //   r * g = (l + (r - l)) * g
    //         = l * g + (r - l) * g
    //
    // The left convolver continues to process the left channel l to produce
    // l * g.  The right convolver processes the difference of input channel
    // signals r - l to produce (r - l) * g.  The outputs of the two
    // convolvers are added to generate the right channel output r * g.
    //
    // The benefit of doing this is that the history of the r - l input for a
    // "speakers" up-mixed mono signal is zero, and so an empty convolver
    // already has exactly the right history for mixing the previous mono
    // signal with the new stereo signal.
    Difference
  };

  enum Parameters { SAMPLE_RATE, NORMALIZE };
  void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override {
    switch (aIndex) {
      case NORMALIZE:
        mNormalize = !!aParam;
        break;
      default:
        NS_ERROR("Bad ConvolverNodeEngine Int32Parameter");
    }
  }
  void SetDoubleParameter(uint32_t aIndex, double aParam) override {
    switch (aIndex) {
      case SAMPLE_RATE:
        mSampleRate = aParam;
        // The buffer is passed after the sample rate.
        // mReverb will be set using this sample rate when the buffer is
        // received.
        break;
      default:
        NS_ERROR("Bad ConvolverNodeEngine DoubleParameter");
    }
  }
  void SetBuffer(AudioChunk&& aBuffer) override {
    // Note about empirical tuning (this is copied from Blink)
    // The maximum FFT size affects reverb performance and accuracy.
    // If the reverb is single-threaded and processes entirely in the real-time
    // audio thread, it's important not to make this too high.  In this case
    // 8192 is a good value. But, the Reverb object is multi-threaded, so we
    // want this as high as possible without losing too much accuracy. Very
    // large FFTs will have worse phase errors. Given these constraints 32768 is
    // a good compromise.
    const size_t MaxFFTSize = 32768;

    // Reset.
    mRemainingLeftOutput = INT32_MIN;
    mRemainingRightOutput = 0;
    mRemainingRightHistory = 0;

    if (aBuffer.IsNull() || !mSampleRate) {
      mReverb = nullptr;
      return;
    }

    // Assume for now that convolution of channel difference is not required.
    // Direct may change to Difference during processing.
    mRightConvolverMode = aBuffer.ChannelCount() == 1
                              ? RightConvolverMode::Direct
                              : RightConvolverMode::Always;

    bool allocationFailure = false;
    mReverb = new WebCore::Reverb(aBuffer, MaxFFTSize, mUseBackgroundThreads,
                                  mNormalize, mSampleRate, &allocationFailure);
    if (allocationFailure) {
      // If the allocation failed, this AudioNodeEngine is going to output
      // silence. This is signaled to developers in the console.
      mReverb = nullptr;
      WebAudioUtils::LogToDeveloperConsole(mWindowID,
                                           "ConvolverNodeAllocationError");
    }
  }

  void AllocateReverbInput(const AudioBlock& aInput,
                           uint32_t aTotalChannelCount) {
    uint32_t inputChannelCount = aInput.ChannelCount();
    MOZ_ASSERT(inputChannelCount <= aTotalChannelCount);
    mReverbInput.AllocateChannels(aTotalChannelCount);
    // Pre-multiply the input's volume
    for (uint32_t i = 0; i < inputChannelCount; ++i) {
      const float* src = static_cast<const float*>(aInput.mChannelData[i]);
      float* dest = mReverbInput.ChannelFloatsForWrite(i);
      AudioBlockCopyChannelWithScale(src, aInput.mVolume, dest);
    }
    // Fill remaining channels with silence
    for (uint32_t i = inputChannelCount; i < aTotalChannelCount; ++i) {
      float* dest = mReverbInput.ChannelFloatsForWrite(i);
      std::fill_n(dest, WEBAUDIO_BLOCK_SIZE, 0.0f);
    }
  }

  void ProcessBlock(AudioNodeStream* aStream, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override;

  bool IsActive() const override { return mRemainingLeftOutput != INT32_MIN; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);

    amount += mReverbInput.SizeOfExcludingThis(aMallocSizeOf, false);

    if (mReverb) {
      amount += mReverb->sizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  // Keeping mReverbInput across process calls avoids unnecessary reallocation.
  AudioBlock mReverbInput;
  nsAutoPtr<WebCore::Reverb> mReverb;
  uint64_t mWindowID;
  // Tracks samples of the tail remaining to be output.  INT32_MIN is a
  // special value to indicate that the end of any previous tail has been
  // handled.
  int32_t mRemainingLeftOutput = INT32_MIN;
  // mRemainingRightOutput and mRemainingRightHistory are only used when
  // mRightOutputMode != Always.  There is no special handling required at the
  // end of tail times and so INT32_MIN is not used.
  // mRemainingRightOutput tracks how much longer this node needs to continue
  // to produce a right output channel.
  int32_t mRemainingRightOutput = 0;
  // mRemainingRightHistory tracks how much silent input would be required to
  // drain the right convolver, which may sometimes be longer than the period
  // a right output channel is required.
  int32_t mRemainingRightHistory = 0;
  float mSampleRate = 0.0f;
  RightConvolverMode mRightConvolverMode = RightConvolverMode::Always;
  bool mUseBackgroundThreads;
  bool mNormalize;
};

static void AddScaledLeftToRight(AudioBlock* aBlock, float aScale) {
  const float* left = static_cast<const float*>(aBlock->mChannelData[0]);
  float* right = aBlock->ChannelFloatsForWrite(1);
  AudioBlockAddChannelWithScale(left, aScale, right);
}

void ConvolverNodeEngine::ProcessBlock(AudioNodeStream* aStream,
                                       GraphTime aFrom,
                                       const AudioBlock& aInput,
                                       AudioBlock* aOutput, bool* aFinished) {
  if (!mReverb) {
    aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
    return;
  }

  uint32_t inputChannelCount = aInput.ChannelCount();
  if (aInput.IsNull()) {
    if (mRemainingLeftOutput > 0) {
      mRemainingLeftOutput -= WEBAUDIO_BLOCK_SIZE;
      AllocateReverbInput(aInput, 1);  // floats for silence
    } else {
      if (mRemainingLeftOutput != INT32_MIN) {
        mRemainingLeftOutput = INT32_MIN;
        MOZ_ASSERT(mRemainingRightOutput <= 0);
        MOZ_ASSERT(mRemainingRightHistory <= 0);
        aStream->ScheduleCheckForInactive();
        RefPtr<PlayingRefChanged> refchanged =
            new PlayingRefChanged(aStream, PlayingRefChanged::RELEASE);
        aStream->Graph()->DispatchToMainThreadStableState(refchanged.forget());
      }
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }
  } else {
    if (mRemainingLeftOutput <= 0) {
      RefPtr<PlayingRefChanged> refchanged =
          new PlayingRefChanged(aStream, PlayingRefChanged::ADDREF);
      aStream->Graph()->DispatchToMainThreadStableState(refchanged.forget());
    }

    // Use mVolume as a flag to detect whether AllocateReverbInput() gets
    // called.
    mReverbInput.mVolume = 0.0f;

    // Special handling of input channel count changes is used when there is
    // only a single impulse response channel.  See RightConvolverMode.
    if (mRightConvolverMode != RightConvolverMode::Always) {
      ChannelInterpretation channelInterpretation =
          aStream->GetChannelInterpretation();
      if (inputChannelCount == 2) {
        if (mRemainingRightHistory <= 0) {
          // Will start the second convolver.  Choose to convolve the right
          // channel directly if there is no left tail to up-mix or up-mixing
          // is "discrete".
          mRightConvolverMode =
              (mRemainingLeftOutput <= 0 ||
               channelInterpretation == ChannelInterpretation::Discrete)
                  ? RightConvolverMode::Direct
                  : RightConvolverMode::Difference;
        }
        // The extra WEBAUDIO_BLOCK_SIZE is subtracted below.
        mRemainingRightOutput =
            mReverb->impulseResponseLength() + WEBAUDIO_BLOCK_SIZE;
        mRemainingRightHistory = mRemainingRightOutput;
        if (mRightConvolverMode == RightConvolverMode::Difference) {
          AllocateReverbInput(aInput, 2);
          // Subtract left from right.
          AddScaledLeftToRight(&mReverbInput, -1.0f);
        }
      } else if (mRemainingRightHistory > 0) {
        // There is one channel of input, but a second convolver also
        // requires input.  Up-mix appropriately for the second convolver.
        if ((mRightConvolverMode == RightConvolverMode::Difference) ^
            (channelInterpretation == ChannelInterpretation::Discrete)) {
          MOZ_ASSERT(
              (mRightConvolverMode == RightConvolverMode::Difference &&
               channelInterpretation == ChannelInterpretation::Speakers) ||
              (mRightConvolverMode == RightConvolverMode::Direct &&
               channelInterpretation == ChannelInterpretation::Discrete));
          // The state is one of the following combinations:
          // 1) Difference and speakers.
          //    Up-mixing gives r = l.
          //    The input to the second convolver is r - l.
          // 2) Direct and discrete.
          //    Up-mixing gives r = 0.
          //    The input to the second convolver is r.
          //
          // In each case the input for the second convolver is silence, which
          // will drain the convolver.
          AllocateReverbInput(aInput, 2);
        } else {
          if (channelInterpretation == ChannelInterpretation::Discrete) {
            MOZ_ASSERT(mRightConvolverMode == RightConvolverMode::Difference);
            // channelInterpretation has changed since the second convolver
            // was added.  "discrete" up-mixing of input would produce a
            // silent right channel r = 0, but the second convolver needs
            // r - l for RightConvolverMode::Difference.
            AllocateReverbInput(aInput, 2);
            AddScaledLeftToRight(&mReverbInput, -1.0f);
          } else {
            MOZ_ASSERT(channelInterpretation ==
                       ChannelInterpretation::Speakers);
            MOZ_ASSERT(mRightConvolverMode == RightConvolverMode::Direct);
            // The Reverb will essentially up-mix the single input channel by
            // feeding it into both convolvers.
          }
          // The second convolver does not have silent input, and so it will
          // not drain.  It will need to continue processing up-mixed input
          // because the next input block may be stereo, which would be mixed
          // with the signal remaining in the convolvers.
          // The extra WEBAUDIO_BLOCK_SIZE is subtracted below.
          mRemainingRightHistory =
              mReverb->impulseResponseLength() + WEBAUDIO_BLOCK_SIZE;
        }
      }
    }

    if (mReverbInput.mVolume == 0.0f) {  // not yet set
      if (aInput.mVolume != 1.0f) {
        AllocateReverbInput(aInput, inputChannelCount);  // pre-multiply
      } else {
        mReverbInput = aInput;
      }
    }

    mRemainingLeftOutput = mReverb->impulseResponseLength();
    MOZ_ASSERT(mRemainingLeftOutput > 0);
  }

  // "The ConvolverNode produces a mono output only in the single case where
  // there is a single input channel and a single-channel buffer."
  uint32_t outputChannelCount = 2;
  uint32_t reverbOutputChannelCount = 2;
  if (mRightConvolverMode != RightConvolverMode::Always) {
    // When the input changes from stereo to mono, the output continues to be
    // stereo for the length of the tail time, during which the two channels
    // may differ.
    if (mRemainingRightOutput > 0) {
      MOZ_ASSERT(mRemainingRightHistory > 0);
      mRemainingRightOutput -= WEBAUDIO_BLOCK_SIZE;
    } else {
      outputChannelCount = 1;
    }
    // The second convolver keeps processing until it drains.
    if (mRemainingRightHistory > 0) {
      mRemainingRightHistory -= WEBAUDIO_BLOCK_SIZE;
    } else {
      reverbOutputChannelCount = 1;
    }
  }

  // If there are two convolvers, then they each need an output buffer, even
  // if the second convolver is only processing to keep history of up-mixed
  // input.
  aOutput->AllocateChannels(reverbOutputChannelCount);

  mReverb->process(&mReverbInput, aOutput);

  if (mRightConvolverMode == RightConvolverMode::Difference &&
      outputChannelCount == 2) {
    // Add left to right.
    AddScaledLeftToRight(aOutput, 1.0f);
  } else {
    // Trim if outputChannelCount < reverbOutputChannelCount
    aOutput->mChannelData.TruncateLength(outputChannelCount);
  }
}

ConvolverNode::ConvolverNode(AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Clamped_max,
                ChannelInterpretation::Speakers),
      mNormalize(true) {
  uint64_t windowID;
  if (aContext->GetParentObject()) {
    windowID = aContext->GetParentObject()->WindowID();
  } else {
    // This is used to send a message to the developer console, but the page is
    // being closed so it doesn't matter too much.
    windowID = 0;
  }
  ConvolverNodeEngine* engine =
      new ConvolverNodeEngine(this, mNormalize, windowID);
  mStream = AudioNodeStream::Create(
      aContext, engine, AudioNodeStream::NO_STREAM_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<ConvolverNode> ConvolverNode::Create(
    JSContext* aCx, AudioContext& aAudioContext,
    const ConvolverOptions& aOptions, ErrorResult& aRv) {
  RefPtr<ConvolverNode> audioNode = new ConvolverNode(&aAudioContext);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // This must be done before setting the buffer.
  audioNode->SetNormalize(!aOptions.mDisableNormalization);

  if (aOptions.mBuffer.WasPassed()) {
    MOZ_ASSERT(aCx);
    audioNode->SetBuffer(aCx, aOptions.mBuffer.Value(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  return audioNode.forget();
}

size_t ConvolverNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  if (mBuffer) {
    // NB: mBuffer might be shared with the associated engine, by convention
    //     the AudioNode will report.
    amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
  }
  return amount;
}

size_t ConvolverNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject* ConvolverNode::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return ConvolverNode_Binding::Wrap(aCx, this, aGivenProto);
}

void ConvolverNode::SetBuffer(JSContext* aCx, AudioBuffer* aBuffer,
                              ErrorResult& aRv) {
  if (aBuffer) {
    switch (aBuffer->NumberOfChannels()) {
      case 1:
      case 2:
      case 4:
        // Supported number of channels
        break;
      default:
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return;
    }
  }

  // Send the buffer to the stream
  AudioNodeStream* ns = mStream;
  MOZ_ASSERT(ns, "Why don't we have a stream here?");
  if (aBuffer) {
    AudioChunk data = aBuffer->GetThreadSharedChannelsForRate(aCx);
    if (data.mBufferFormat == AUDIO_FORMAT_S16) {
      // Reverb expects data in float format.
      // Convert on the main thread so as to minimize allocations on the audio
      // thread.
      // Reverb will dispose of the buffer once initialized, so convert here
      // and leave the smaller arrays in the AudioBuffer.
      // There is currently no value in providing 16/32-byte aligned data
      // because PadAndMakeScaledDFT() will copy the data (without SIMD
      // instructions) to aligned arrays for the FFT.
      RefPtr<SharedBuffer> floatBuffer = SharedBuffer::Create(
          sizeof(float) * data.mDuration * data.ChannelCount());
      if (!floatBuffer) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
      auto floatData = static_cast<float*>(floatBuffer->Data());
      for (size_t i = 0; i < data.ChannelCount(); ++i) {
        ConvertAudioSamples(data.ChannelData<int16_t>()[i], floatData,
                            data.mDuration);
        data.mChannelData[i] = floatData;
        floatData += data.mDuration;
      }
      data.mBuffer = std::move(floatBuffer);
      data.mBufferFormat = AUDIO_FORMAT_FLOAT32;
    }
    SendDoubleParameterToStream(ConvolverNodeEngine::SAMPLE_RATE,
                                aBuffer->SampleRate());
    ns->SetBuffer(std::move(data));
  } else {
    ns->SetBuffer(AudioChunk());
  }

  mBuffer = aBuffer;
}

void ConvolverNode::SetNormalize(bool aNormalize) {
  mNormalize = aNormalize;
  SendInt32ParameterToStream(ConvolverNodeEngine::NORMALIZE, aNormalize);
}

}  // namespace dom
}  // namespace mozilla
