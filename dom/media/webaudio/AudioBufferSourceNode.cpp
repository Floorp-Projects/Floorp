/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBufferSourceNode.h"
#include "mozilla/dom/AudioBufferSourceNodeBinding.h"
#include "mozilla/dom/AudioParam.h"
#include "mozilla/FloatingPoint.h"
#include "nsMathUtils.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "AudioParamTimeline.h"
#include <limits>
#include <algorithm>

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioBufferSourceNode, AudioNode, mBuffer, mPlaybackRate, mDetune)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioBufferSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(AudioBufferSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(AudioBufferSourceNode, AudioNode)

/**
 * Media-thread playback engine for AudioBufferSourceNode.
 * Nothing is played until a non-null buffer has been set (via
 * AudioNodeStream::SetBuffer) and a non-zero mBufferEnd has been set (via
 * AudioNodeStream::SetInt32Parameter).
 */
class AudioBufferSourceNodeEngine final : public AudioNodeEngine
{
public:
  AudioBufferSourceNodeEngine(AudioNode* aNode,
                              AudioDestinationNode* aDestination) :
    AudioNodeEngine(aNode),
    mStart(0.0), mBeginProcessing(0),
    mStop(STREAM_TIME_MAX),
    mResampler(nullptr), mRemainingResamplerTail(0),
    mBufferEnd(0),
    mLoopStart(0), mLoopEnd(0),
    mBufferPosition(0), mBufferSampleRate(0),
    // mResamplerOutRate is initialized in UpdateResampler().
    mChannels(0),
    mDopplerShift(1.0f),
    mDestination(aDestination->Stream()),
    mPlaybackRateTimeline(1.0f),
    mDetuneTimeline(0.0f),
    mLoop(false)
  {}

  ~AudioBufferSourceNodeEngine()
  {
    if (mResampler) {
      speex_resampler_destroy(mResampler);
    }
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  virtual void SetTimelineParameter(uint32_t aIndex,
                                    const dom::AudioParamTimeline& aValue,
                                    TrackRate aSampleRate) override
  {
    switch (aIndex) {
    case AudioBufferSourceNode::PLAYBACKRATE:
      mPlaybackRateTimeline = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mPlaybackRateTimeline, mSource, mDestination);
      break;
    case AudioBufferSourceNode::DETUNE:
      mDetuneTimeline = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mDetuneTimeline, mSource, mDestination);
      break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine TimelineParameter");
    }
  }
  virtual void SetStreamTimeParameter(uint32_t aIndex, StreamTime aParam) override
  {
    switch (aIndex) {
    case AudioBufferSourceNode::STOP: mStop = aParam; break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine StreamTimeParameter");
    }
  }
  virtual void SetDoubleParameter(uint32_t aIndex, double aParam) override
  {
    switch (aIndex) {
    case AudioBufferSourceNode::START:
      MOZ_ASSERT(!mStart, "Another START?");
      mStart =
        mSource->FractionalTicksFromDestinationTime(mDestination, aParam);
      // Round to nearest
      mBeginProcessing = mStart + 0.5;
      break;
    case AudioBufferSourceNode::DOPPLERSHIFT:
      mDopplerShift = (aParam <= 0 || mozilla::IsNaN(aParam)) ? 1.0 : aParam;
      break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine double parameter.");
    };
  }
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override
  {
    switch (aIndex) {
    case AudioBufferSourceNode::SAMPLE_RATE:
      MOZ_ASSERT(aParam > 0);
      mBufferSampleRate = aParam;
      mSource->SetActive();
      break;
    case AudioBufferSourceNode::BUFFERSTART:
      MOZ_ASSERT(aParam >= 0);
      if (mBufferPosition == 0) {
        mBufferPosition = aParam;
      }
      break;
    case AudioBufferSourceNode::BUFFEREND:
      MOZ_ASSERT(aParam >= 0);
      mBufferEnd = aParam;
      break;
    case AudioBufferSourceNode::LOOP: mLoop = !!aParam; break;
    case AudioBufferSourceNode::LOOPSTART:
      MOZ_ASSERT(aParam >= 0);
      mLoopStart = aParam;
      break;
    case AudioBufferSourceNode::LOOPEND:
      MOZ_ASSERT(aParam >= 0);
      mLoopEnd = aParam;
      break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine Int32Parameter");
    }
  }
  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer) override
  {
    mBuffer = aBuffer;
  }

  bool BegunResampling()
  {
    return mBeginProcessing == -STREAM_TIME_MAX;
  }

  void UpdateResampler(int32_t aOutRate, uint32_t aChannels)
  {
    if (mResampler &&
        (aChannels != mChannels ||
         // If the resampler has begun, then it will have moved
         // mBufferPosition to after the samples it has read, but it hasn't
         // output its buffered samples.  Keep using the resampler, even if
         // the rates now match, so that this latent segment is output.
         (aOutRate == mBufferSampleRate && !BegunResampling()))) {
      speex_resampler_destroy(mResampler);
      mResampler = nullptr;
      mRemainingResamplerTail = 0;
      mBeginProcessing = mStart + 0.5;
    }

    if (aChannels == 0 ||
        (aOutRate == mBufferSampleRate && !mResampler)) {
      mResamplerOutRate = aOutRate;
      return;
    }

    if (!mResampler) {
      mChannels = aChannels;
      mResampler = speex_resampler_init(mChannels, mBufferSampleRate, aOutRate,
                                        SPEEX_RESAMPLER_QUALITY_MIN,
                                        nullptr);
    } else {
      if (mResamplerOutRate == aOutRate) {
        return;
      }
      speex_resampler_set_rate(mResampler, mBufferSampleRate, aOutRate);
    }

    mResamplerOutRate = aOutRate;

    if (!BegunResampling()) {
      // Low pass filter effects from the resampler mean that samples before
      // the start time are influenced by resampling the buffer.  The input
      // latency indicates half the filter width.
      int64_t inputLatency = speex_resampler_get_input_latency(mResampler);
      uint32_t ratioNum, ratioDen;
      speex_resampler_get_ratio(mResampler, &ratioNum, &ratioDen);
      // The output subsample resolution supported in aligning the resampler
      // is ratioNum.  First round the start time to the nearest subsample.
      int64_t subsample = mStart * ratioNum + 0.5;
      // Now include the leading effects of the filter, and round *up* to the
      // next whole tick, because there is no effect on samples outside the
      // filter width.
      mBeginProcessing =
        (subsample - inputLatency * ratioDen + ratioNum - 1) / ratioNum;
    }
  }

  // Borrow a full buffer of size WEBAUDIO_BLOCK_SIZE from the source buffer
  // at offset aSourceOffset.  This avoids copying memory.
  void BorrowFromInputBuffer(AudioBlock* aOutput,
                             uint32_t aChannels)
  {
    aOutput->SetBuffer(mBuffer);
    aOutput->mChannelData.SetLength(aChannels);
    for (uint32_t i = 0; i < aChannels; ++i) {
      aOutput->mChannelData[i] = mBuffer->GetData(i) + mBufferPosition;
    }
    aOutput->mVolume = 1.0f;
    aOutput->mBufferFormat = AUDIO_FORMAT_FLOAT32;
  }

  // Copy aNumberOfFrames frames from the source buffer at offset aSourceOffset
  // and put it at offset aBufferOffset in the destination buffer.
  void CopyFromInputBuffer(AudioBlock* aOutput,
                           uint32_t aChannels,
                           uintptr_t aOffsetWithinBlock,
                           uint32_t aNumberOfFrames) {
    for (uint32_t i = 0; i < aChannels; ++i) {
      float* baseChannelData = aOutput->ChannelFloatsForWrite(i);
      memcpy(baseChannelData + aOffsetWithinBlock,
             mBuffer->GetData(i) + mBufferPosition,
             aNumberOfFrames * sizeof(float));
    }
  }

  // Resamples input data to an output buffer, according to |mBufferSampleRate| and
  // the playbackRate/detune.
  // The number of frames consumed/produced depends on the amount of space
  // remaining in both the input and output buffer, and the playback rate (that
  // is, the ratio between the output samplerate and the input samplerate).
  void CopyFromInputBufferWithResampling(AudioNodeStream* aStream,
                                         AudioBlock* aOutput,
                                         uint32_t aChannels,
                                         uint32_t* aOffsetWithinBlock,
                                         uint32_t aAvailableInOutput,
                                         StreamTime* aCurrentPosition,
                                         uint32_t aBufferMax)
  {
    if (*aOffsetWithinBlock == 0) {
      aOutput->AllocateChannels(aChannels);
    }
    SpeexResamplerState* resampler = mResampler;
    MOZ_ASSERT(aChannels > 0);

    if (mBufferPosition < aBufferMax) {
      uint32_t availableInInputBuffer = aBufferMax - mBufferPosition;
      uint32_t ratioNum, ratioDen;
      speex_resampler_get_ratio(resampler, &ratioNum, &ratioDen);
      // Limit the number of input samples copied and possibly
      // format-converted for resampling by estimating how many will be used.
      // This may be a little small if still filling the resampler with
      // initial data, but we'll get called again and it will work out.
      uint32_t inputLimit = aAvailableInOutput * ratioNum / ratioDen + 10;
      if (!BegunResampling()) {
        // First time the resampler is used.
        uint32_t inputLatency = speex_resampler_get_input_latency(resampler);
        inputLimit += inputLatency;
        // If starting after mStart, then play from the beginning of the
        // buffer, but correct for input latency.  If starting before mStart,
        // then align the resampler so that the time corresponding to the
        // first input sample is mStart.
        uint32_t skipFracNum = inputLatency * ratioDen;
        double leadTicks = mStart - *aCurrentPosition;
        if (leadTicks > 0.0) {
          // Round to nearest output subsample supported by the resampler at
          // these rates.
          skipFracNum -= leadTicks * ratioNum + 0.5;
          MOZ_ASSERT(skipFracNum < INT32_MAX, "mBeginProcessing is wrong?");
        }
        speex_resampler_set_skip_frac_num(resampler, skipFracNum);

        mBeginProcessing = -STREAM_TIME_MAX;
      }
      inputLimit = std::min(inputLimit, availableInInputBuffer);

      for (uint32_t i = 0; true; ) {
        uint32_t inSamples = inputLimit;
        const float* inputData = mBuffer->GetData(i) + mBufferPosition;

        uint32_t outSamples = aAvailableInOutput;
        float* outputData =
          aOutput->ChannelFloatsForWrite(i) + *aOffsetWithinBlock;

        WebAudioUtils::SpeexResamplerProcess(resampler, i,
                                             inputData, &inSamples,
                                             outputData, &outSamples);
        if (++i == aChannels) {
          mBufferPosition += inSamples;
          MOZ_ASSERT(mBufferPosition <= mBufferEnd || mLoop);
          *aOffsetWithinBlock += outSamples;
          *aCurrentPosition += outSamples;
          if (inSamples == availableInInputBuffer && !mLoop) {
            // We'll feed in enough zeros to empty out the resampler's memory.
            // This handles the output latency as well as capturing the low
            // pass effects of the resample filter.
            mRemainingResamplerTail =
              2 * speex_resampler_get_input_latency(resampler) - 1;
          }
          return;
        }
      }
    } else {
      for (uint32_t i = 0; true; ) {
        uint32_t inSamples = mRemainingResamplerTail;
        uint32_t outSamples = aAvailableInOutput;
        float* outputData =
          aOutput->ChannelFloatsForWrite(i) + *aOffsetWithinBlock;

        // AudioDataValue* for aIn selects the function that does not try to
        // copy and format-convert input data.
        WebAudioUtils::SpeexResamplerProcess(resampler, i,
                         static_cast<AudioDataValue*>(nullptr), &inSamples,
                         outputData, &outSamples);
        if (++i == aChannels) {
          MOZ_ASSERT(inSamples <= mRemainingResamplerTail);
          mRemainingResamplerTail -= inSamples;
          *aOffsetWithinBlock += outSamples;
          *aCurrentPosition += outSamples;
          break;
        }
      }
    }
  }

  /**
   * Fill aOutput with as many zero frames as we can, and advance
   * aOffsetWithinBlock and aCurrentPosition based on how many frames we write.
   * This will never advance aOffsetWithinBlock past WEBAUDIO_BLOCK_SIZE or
   * aCurrentPosition past aMaxPos.  This function knows when it needs to
   * allocate the output buffer, and also optimizes the case where it can avoid
   * memory allocations.
   */
  void FillWithZeroes(AudioBlock* aOutput,
                      uint32_t aChannels,
                      uint32_t* aOffsetWithinBlock,
                      StreamTime* aCurrentPosition,
                      StreamTime aMaxPos)
  {
    MOZ_ASSERT(*aCurrentPosition < aMaxPos);
    uint32_t numFrames =
      std::min<StreamTime>(WEBAUDIO_BLOCK_SIZE - *aOffsetWithinBlock,
                           aMaxPos - *aCurrentPosition);
    if (numFrames == WEBAUDIO_BLOCK_SIZE || !aChannels) {
      aOutput->SetNull(numFrames);
    } else {
      if (*aOffsetWithinBlock == 0) {
        aOutput->AllocateChannels(aChannels);
      }
      WriteZeroesToAudioBlock(aOutput, *aOffsetWithinBlock, numFrames);
    }
    *aOffsetWithinBlock += numFrames;
    *aCurrentPosition += numFrames;
  }

  /**
   * Copy as many frames as possible from the source buffer to aOutput, and
   * advance aOffsetWithinBlock and aCurrentPosition based on how many frames
   * we write.  This will never advance aOffsetWithinBlock past
   * WEBAUDIO_BLOCK_SIZE, or aCurrentPosition past mStop.  It takes data from
   * the buffer at aBufferOffset, and never takes more data than aBufferMax.
   * This function knows when it needs to allocate the output buffer, and also
   * optimizes the case where it can avoid memory allocations.
   */
  void CopyFromBuffer(AudioNodeStream* aStream,
                      AudioBlock* aOutput,
                      uint32_t aChannels,
                      uint32_t* aOffsetWithinBlock,
                      StreamTime* aCurrentPosition,
                      uint32_t aBufferMax)
  {
    MOZ_ASSERT(*aCurrentPosition < mStop);
    uint32_t availableInOutput =
      std::min<StreamTime>(WEBAUDIO_BLOCK_SIZE - *aOffsetWithinBlock,
                           mStop - *aCurrentPosition);
    if (mResampler) {
      CopyFromInputBufferWithResampling(aStream, aOutput, aChannels,
                                        aOffsetWithinBlock, availableInOutput,
                                        aCurrentPosition, aBufferMax);
      return;
    }

    if (aChannels == 0) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      // There is no attempt here to limit advance so that mBufferPosition is
      // limited to aBufferMax.  The only observable affect of skipping the
      // check would be in the precise timing of the ended event if the loop
      // attribute is reset after playback has looped.
      *aOffsetWithinBlock += availableInOutput;
      *aCurrentPosition += availableInOutput;
      // Rounding at the start and end of the period means that fractional
      // increments essentially accumulate if outRate remains constant.  If
      // outRate is varying, then accumulation happens on average but not
      // precisely.
      TrackTicks start = *aCurrentPosition *
        mBufferSampleRate / mResamplerOutRate;
      TrackTicks end = (*aCurrentPosition + availableInOutput) *
        mBufferSampleRate / mResamplerOutRate;
      mBufferPosition += end - start;
      return;
    }

    uint32_t numFrames = std::min(aBufferMax - mBufferPosition,
                                  availableInOutput);
    if (numFrames == WEBAUDIO_BLOCK_SIZE) {
      MOZ_ASSERT(mBufferPosition < aBufferMax);
      BorrowFromInputBuffer(aOutput, aChannels);
    } else {
      if (*aOffsetWithinBlock == 0) {
        aOutput->AllocateChannels(aChannels);
      }
      MOZ_ASSERT(mBufferPosition < aBufferMax);
      CopyFromInputBuffer(aOutput, aChannels, *aOffsetWithinBlock, numFrames);
    }
    *aOffsetWithinBlock += numFrames;
    *aCurrentPosition += numFrames;
    mBufferPosition += numFrames;
  }

  int32_t ComputeFinalOutSampleRate(float aPlaybackRate, float aDetune)
  {
    float computedPlaybackRate = aPlaybackRate * pow(2, aDetune / 1200.f);
    // Make sure the playback rate and the doppler shift are something
    // our resampler can work with.
    int32_t rate = WebAudioUtils::
      TruncateFloatToInt<int32_t>(mSource->SampleRate() /
                                  (computedPlaybackRate * mDopplerShift));
    return rate ? rate : mBufferSampleRate;
  }

  void UpdateSampleRateIfNeeded(uint32_t aChannels)
  {
    float playbackRate;
    float detune;

    if (mPlaybackRateTimeline.HasSimpleValue()) {
      playbackRate = mPlaybackRateTimeline.GetValue();
    } else {
      playbackRate = mPlaybackRateTimeline.GetValueAtTime(mSource->GetCurrentPosition());
    }
    if (mDetuneTimeline.HasSimpleValue()) {
      detune = mDetuneTimeline.GetValue();
    } else {
      detune = mDetuneTimeline.GetValueAtTime(mSource->GetCurrentPosition());
    }
    if (playbackRate <= 0 || mozilla::IsNaN(playbackRate)) {
      playbackRate = 1.0f;
    }

    detune = std::min(std::max(-1200.f, detune), 1200.f);

    int32_t outRate = ComputeFinalOutSampleRate(playbackRate, detune);
    UpdateResampler(outRate, aChannels);
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioBlock& aInput,
                            AudioBlock* aOutput,
                            bool* aFinished) override
  {
    if (mBufferSampleRate == 0) {
      // start() has not yet been called or no buffer has yet been set
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    StreamTime streamPosition = aStream->GetCurrentPosition();
    // We've finished if we've gone past mStop, or if we're past mDuration when
    // looping is disabled.
    if (streamPosition >= mStop ||
        (!mLoop && mBufferPosition >= mBufferEnd && !mRemainingResamplerTail)) {
      *aFinished = true;
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    uint32_t channels = mBuffer ? mBuffer->GetChannels() : 0;

    UpdateSampleRateIfNeeded(channels);

    uint32_t written = 0;
    while (written < WEBAUDIO_BLOCK_SIZE) {
      if (mStop != STREAM_TIME_MAX &&
          streamPosition >= mStop) {
        FillWithZeroes(aOutput, channels, &written, &streamPosition, STREAM_TIME_MAX);
        continue;
      }
      if (streamPosition < mBeginProcessing) {
        FillWithZeroes(aOutput, channels, &written, &streamPosition,
                       mBeginProcessing);
        continue;
      }
      if (mLoop) {
        // mLoopEnd can become less than mBufferPosition when a LOOPEND engine
        // parameter is received after "loopend" is changed on the node or a
        // new buffer with lower samplerate is set.
        if (mBufferPosition >= mLoopEnd) {
          mBufferPosition = mLoopStart;
        }
        CopyFromBuffer(aStream, aOutput, channels, &written, &streamPosition, mLoopEnd);
      } else {
        if (mBufferPosition < mBufferEnd || mRemainingResamplerTail) {
          CopyFromBuffer(aStream, aOutput, channels, &written, &streamPosition, mBufferEnd);
        } else {
          FillWithZeroes(aOutput, channels, &written, &streamPosition, STREAM_TIME_MAX);
        }
      }
    }
  }

  virtual bool IsActive() const override
  {
    // Whether buffer has been set and start() has been called.
    return mBufferSampleRate != 0;
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    // Not owned:
    // - mBuffer - shared w/ AudioNode
    // - mPlaybackRateTimeline - shared w/ AudioNode
    // - mDetuneTimeline - shared w/ AudioNode

    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);

    // NB: We need to modify speex if we want the full memory picture, internal
    //     fields that need measuring noted below.
    // - mResampler->mem
    // - mResampler->sinc_table
    // - mResampler->last_sample
    // - mResampler->magic_samples
    // - mResampler->samp_frac_num
    amount += aMallocSizeOf(mResampler);

    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  double mStart; // including the fractional position between ticks
  // Low pass filter effects from the resampler mean that samples before the
  // start time are influenced by resampling the buffer.  mBeginProcessing
  // includes the extent of this filter.  The special value of -STREAM_TIME_MAX
  // indicates that the resampler has begun processing.
  StreamTime mBeginProcessing;
  StreamTime mStop;
  nsRefPtr<ThreadSharedFloatArrayBufferList> mBuffer;
  SpeexResamplerState* mResampler;
  // mRemainingResamplerTail, like mBufferPosition, and
  // mBufferEnd, is measured in input buffer samples.
  uint32_t mRemainingResamplerTail;
  uint32_t mBufferEnd;
  uint32_t mLoopStart;
  uint32_t mLoopEnd;
  uint32_t mBufferPosition;
  int32_t mBufferSampleRate;
  int32_t mResamplerOutRate;
  uint32_t mChannels;
  float mDopplerShift;
  AudioNodeStream* mDestination;
  AudioNodeStream* mSource;
  AudioParamTimeline mPlaybackRateTimeline;
  AudioParamTimeline mDetuneTimeline;
  bool mLoop;
};

AudioBufferSourceNode::AudioBufferSourceNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mLoopStart(0.0)
  , mLoopEnd(0.0)
  // mOffset and mDuration are initialized in Start().
  , mPlaybackRate(new AudioParam(this, SendPlaybackRateToStream, 1.0f, "playbackRate"))
  , mDetune(new AudioParam(this, SendDetuneToStream, 0.0f, "detune"))
  , mLoop(false)
  , mStartCalled(false)
{
  AudioBufferSourceNodeEngine* engine = new AudioBufferSourceNodeEngine(this, aContext->Destination());
  mStream = AudioNodeStream::Create(aContext, engine,
                                    AudioNodeStream::NEED_MAIN_THREAD_FINISHED);
  engine->SetSourceStream(mStream);
  mStream->AddMainThreadListener(this);
}

AudioBufferSourceNode::~AudioBufferSourceNode()
{
}

void
AudioBufferSourceNode::DestroyMediaStream()
{
  bool hadStream = mStream;
  if (hadStream) {
    mStream->RemoveMainThreadListener(this);
  }
  AudioNode::DestroyMediaStream();
  if (hadStream && Context()) {
    Context()->UnregisterAudioBufferSourceNode(this);
  }
}

size_t
AudioBufferSourceNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  if (mBuffer) {
    amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
  }

  amount += mPlaybackRate->SizeOfIncludingThis(aMallocSizeOf);
  amount += mDetune->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
AudioBufferSourceNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
AudioBufferSourceNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioBufferSourceNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
AudioBufferSourceNode::Start(double aWhen, double aOffset,
                             const Optional<double>& aDuration, ErrorResult& aRv)
{
  if (!WebAudioUtils::IsTimeValid(aWhen) ||
      (aDuration.WasPassed() && !WebAudioUtils::IsTimeValid(aDuration.Value()))) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  if (mStartCalled) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mStartCalled = true;

  AudioNodeStream* ns = mStream;
  if (!ns) {
    // Nothing to play, or we're already dead for some reason
    return;
  }

  // Remember our arguments so that we can use them when we get a new buffer.
  mOffset = aOffset;
  mDuration = aDuration.WasPassed() ? aDuration.Value()
                                    : std::numeric_limits<double>::min();
  // We can't send these parameters without a buffer because we don't know the
  // buffer's sample rate or length.
  if (mBuffer) {
    SendOffsetAndDurationParametersToStream(ns);
  }

  // Don't set parameter unnecessarily
  if (aWhen > 0.0) {
    ns->SetDoubleParameter(START, mContext->DOMTimeToStreamTime(aWhen));
  }
}

void
AudioBufferSourceNode::SendBufferParameterToStream(JSContext* aCx)
{
  AudioNodeStream* ns = mStream;
  if (!ns) {
    return;
  }

  if (mBuffer) {
    nsRefPtr<ThreadSharedFloatArrayBufferList> data =
      mBuffer->GetThreadSharedChannelsForRate(aCx);
    ns->SetBuffer(data.forget());

    if (mStartCalled) {
      SendOffsetAndDurationParametersToStream(ns);
    }
  } else {
    ns->SetInt32Parameter(BUFFEREND, 0);
    ns->SetBuffer(nullptr);

    MarkInactive();
  }
}

void
AudioBufferSourceNode::SendOffsetAndDurationParametersToStream(AudioNodeStream* aStream)
{
  NS_ASSERTION(mBuffer && mStartCalled,
               "Only call this when we have a buffer and start() has been called");

  float rate = mBuffer->SampleRate();
  aStream->SetInt32Parameter(SAMPLE_RATE, rate);

  int32_t bufferEnd = mBuffer->Length();
  int32_t offsetSamples = std::max(0, NS_lround(mOffset * rate));

  // Don't set parameter unnecessarily
  if (offsetSamples > 0) {
    aStream->SetInt32Parameter(BUFFERSTART, offsetSamples);
  }

  if (mDuration != std::numeric_limits<double>::min()) {
    MOZ_ASSERT(mDuration >= 0.0); // provided by Start()
    MOZ_ASSERT(rate >= 0.0f); // provided by AudioBuffer::Create()
    static_assert(std::numeric_limits<double>::digits >=
                  std::numeric_limits<decltype(bufferEnd)>::digits,
                  "bufferEnd should be represented exactly by double");
    // + 0.5 rounds mDuration to nearest sample when assigned to bufferEnd.
    bufferEnd = std::min<double>(bufferEnd,
                                 offsetSamples + mDuration * rate + 0.5);
  }
  aStream->SetInt32Parameter(BUFFEREND, bufferEnd);

  MarkActive();
}

void
AudioBufferSourceNode::Stop(double aWhen, ErrorResult& aRv)
{
  if (!WebAudioUtils::IsTimeValid(aWhen)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  if (!mStartCalled) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  AudioNodeStream* ns = mStream;
  if (!ns || !Context()) {
    // We've already stopped and had our stream shut down
    return;
  }

  ns->SetStreamTimeParameter(STOP, Context(), std::max(0.0, aWhen));
}

void
AudioBufferSourceNode::NotifyMainThreadStreamFinished()
{
  MOZ_ASSERT(mStream->IsFinished());

  class EndedEventDispatcher final : public nsRunnable
  {
  public:
    explicit EndedEventDispatcher(AudioBufferSourceNode* aNode)
      : mNode(aNode) {}
    NS_IMETHODIMP Run() override
    {
      // If it's not safe to run scripts right now, schedule this to run later
      if (!nsContentUtils::IsSafeToRunScript()) {
        nsContentUtils::AddScriptRunner(this);
        return NS_OK;
      }

      mNode->DispatchTrustedEvent(NS_LITERAL_STRING("ended"));
      // Release stream resources.
      mNode->DestroyMediaStream();
      return NS_OK;
    }
  private:
    nsRefPtr<AudioBufferSourceNode> mNode;
  };

  NS_DispatchToMainThread(new EndedEventDispatcher(this));

  // Drop the playing reference
  // Warning: The below line might delete this.
  MarkInactive();
}

void
AudioBufferSourceNode::SendPlaybackRateToStream(AudioNode* aNode)
{
  AudioBufferSourceNode* This = static_cast<AudioBufferSourceNode*>(aNode);
  if (!This->mStream) {
    return;
  }
  SendTimelineParameterToStream(This, PLAYBACKRATE, *This->mPlaybackRate);
}

void
AudioBufferSourceNode::SendDetuneToStream(AudioNode* aNode)
{
  AudioBufferSourceNode* This = static_cast<AudioBufferSourceNode*>(aNode);
  if (!This->mStream) {
    return;
  }
  SendTimelineParameterToStream(This, DETUNE, *This->mDetune);
}

void
AudioBufferSourceNode::SendDopplerShiftToStream(double aDopplerShift)
{
  MOZ_ASSERT(mStream, "Should have disconnected panner if no stream");
  SendDoubleParameterToStream(DOPPLERSHIFT, aDopplerShift);
}

void
AudioBufferSourceNode::SendLoopParametersToStream()
{
  if (!mStream) {
    return;
  }
  // Don't compute and set the loop parameters unnecessarily
  if (mLoop && mBuffer) {
    float rate = mBuffer->SampleRate();
    double length = (double(mBuffer->Length()) / mBuffer->SampleRate());
    double actualLoopStart, actualLoopEnd;
    if (mLoopStart >= 0.0 && mLoopEnd > 0.0 &&
        mLoopStart < mLoopEnd) {
      MOZ_ASSERT(mLoopStart != 0.0 || mLoopEnd != 0.0);
      actualLoopStart = (mLoopStart > length) ? 0.0 : mLoopStart;
      actualLoopEnd = std::min(mLoopEnd, length);
    } else {
      actualLoopStart = 0.0;
      actualLoopEnd = length;
    }
    int32_t loopStartTicks = NS_lround(actualLoopStart * rate);
    int32_t loopEndTicks = NS_lround(actualLoopEnd * rate);
    if (loopStartTicks < loopEndTicks) {
      SendInt32ParameterToStream(LOOPSTART, loopStartTicks);
      SendInt32ParameterToStream(LOOPEND, loopEndTicks);
      SendInt32ParameterToStream(LOOP, 1);
    } else {
      // Be explicit about looping not happening if the offsets make
      // looping impossible.
      SendInt32ParameterToStream(LOOP, 0);
    }
  } else {
    SendInt32ParameterToStream(LOOP, 0);
  }
}

} // namespace dom
} // namespace mozilla
