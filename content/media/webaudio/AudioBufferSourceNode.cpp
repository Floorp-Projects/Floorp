/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBufferSourceNode.h"
#include "mozilla/dom/AudioBufferSourceNodeBinding.h"
#include "mozilla/dom/AudioParam.h"
#include "nsMathUtils.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "AudioParamTimeline.h"
#include "speex/speex_resampler.h"
#include <limits>

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioBufferSourceNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioBufferSourceNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBuffer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPlaybackRate)
  if (tmp->Context()) {
    // AudioNode's Unlink implementation disconnects us from the graph
    // too, but we need to do this right here to make sure that
    // UnregisterAudioBufferSourceNode can properly untangle us from
    // the possibly connected PannerNodes.
    tmp->DisconnectFromGraph();
    tmp->Context()->UnregisterAudioBufferSourceNode(tmp);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(AudioNode)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AudioBufferSourceNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlaybackRate)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioBufferSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(AudioBufferSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(AudioBufferSourceNode, AudioNode)

/**
 * Media-thread playback engine for AudioBufferSourceNode.
 * Nothing is played until a non-null buffer has been set (via
 * AudioNodeStream::SetBuffer) and a non-zero duration has been set (via
 * AudioNodeStream::SetInt32Parameter).
 */
class AudioBufferSourceNodeEngine : public AudioNodeEngine
{
public:
  explicit AudioBufferSourceNodeEngine(AudioNode* aNode,
                                       AudioDestinationNode* aDestination) :
    AudioNodeEngine(aNode),
    mStart(0), mStop(TRACK_TICKS_MAX),
    mResampler(nullptr), mRemainingResamplerTail(0),
    mOffset(0), mDuration(0),
    mLoopStart(0), mLoopEnd(0),
    mBufferSampleRate(0), mPosition(0), mChannels(0), mPlaybackRate(1.0f),
    mDopplerShift(1.0f),
    mDestination(static_cast<AudioNodeStream*>(aDestination->Stream())),
    mPlaybackRateTimeline(1.0f), mLoop(false)
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
                                    TrackRate aSampleRate) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case AudioBufferSourceNode::PLAYBACKRATE:
      mPlaybackRateTimeline = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mPlaybackRateTimeline, mSource, mDestination);
      break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine TimelineParameter");
    }
  }
  virtual void SetStreamTimeParameter(uint32_t aIndex, TrackTicks aParam)
  {
    switch (aIndex) {
    case AudioBufferSourceNode::START: mStart = aParam; break;
    case AudioBufferSourceNode::STOP: mStop = aParam; break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine StreamTimeParameter");
    }
  }
  virtual void SetDoubleParameter(uint32_t aIndex, double aParam)
  {
    switch (aIndex) {
      case AudioBufferSourceNode::DOPPLERSHIFT:
        mDopplerShift = aParam;
        break;
      default:
        NS_ERROR("Bad AudioBufferSourceNodeEngine double parameter.");
    };
  }
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam)
  {
    switch (aIndex) {
    case AudioBufferSourceNode::SAMPLE_RATE: mBufferSampleRate = aParam; break;
    case AudioBufferSourceNode::OFFSET: mOffset = aParam; break;
    case AudioBufferSourceNode::DURATION: mDuration = aParam; break;
    case AudioBufferSourceNode::LOOP: mLoop = !!aParam; break;
    case AudioBufferSourceNode::LOOPSTART: mLoopStart = aParam; break;
    case AudioBufferSourceNode::LOOPEND: mLoopEnd = aParam; break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine Int32Parameter");
    }
  }
  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
  {
    mBuffer = aBuffer;
  }

  SpeexResamplerState* Resampler(AudioNodeStream* aStream, uint32_t aChannels)
  {
    if (aChannels != mChannels && mResampler) {
      speex_resampler_destroy(mResampler);
      mResampler = nullptr;
    }

    if (!mResampler) {
      mChannels = aChannels;
      mResampler = speex_resampler_init(mChannels, mBufferSampleRate,
                                        ComputeFinalOutSampleRate(aStream->SampleRate()),
                                        SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                        nullptr);
      speex_resampler_skip_zeros(mResampler);
    }
    return mResampler;
  }

  // Borrow a full buffer of size WEBAUDIO_BLOCK_SIZE from the source buffer
  // at offset aSourceOffset.  This avoids copying memory.
  void BorrowFromInputBuffer(AudioChunk* aOutput,
                             uint32_t aChannels,
                             uintptr_t aSourceOffset)
  {
    aOutput->mDuration = WEBAUDIO_BLOCK_SIZE;
    aOutput->mBuffer = mBuffer;
    aOutput->mChannelData.SetLength(aChannels);
    for (uint32_t i = 0; i < aChannels; ++i) {
      aOutput->mChannelData[i] = mBuffer->GetData(i) + aSourceOffset;
    }
    aOutput->mVolume = 1.0f;
    aOutput->mBufferFormat = AUDIO_FORMAT_FLOAT32;
  }

  // Copy aNumberOfFrames frames from the source buffer at offset aSourceOffset
  // and put it at offset aBufferOffset in the destination buffer.
  void CopyFromInputBuffer(AudioChunk* aOutput,
                           uint32_t aChannels,
                           uintptr_t aSourceOffset,
                           uintptr_t aBufferOffset,
                           uint32_t aNumberOfFrames) {
    for (uint32_t i = 0; i < aChannels; ++i) {
      float* baseChannelData = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[i]));
      memcpy(baseChannelData + aBufferOffset,
             mBuffer->GetData(i) + aSourceOffset,
             aNumberOfFrames * sizeof(float));
    }
  }

  // Resamples input data to an output buffer, according to |mBufferSampleRate| and
  // the playbackRate.
  // The number of frames consumed/produced depends on the amount of space
  // remaining in both the input and output buffer, and the playback rate (that
  // is, the ratio between the output samplerate and the input samplerate).
  void CopyFromInputBufferWithResampling(AudioNodeStream* aStream,
                                         AudioChunk* aOutput,
                                         uint32_t aChannels,
                                         uintptr_t aSourceOffset,
                                         uintptr_t aBufferOffset,
                                         uint32_t aAvailableInInputBuffer,
                                         uint32_t& aFramesWritten) {
    // TODO: adjust for mStop (see bug 913854 comment 9).
    uint32_t availableInOutputBuffer = WEBAUDIO_BLOCK_SIZE - aBufferOffset;
    SpeexResamplerState* resampler = Resampler(aStream, aChannels);
    MOZ_ASSERT(aChannels > 0);

    if (aAvailableInInputBuffer) {
      // Limit the number of input samples copied and possibly
      // format-converted for resampling by estimating how many will be used.
      // This may be a little small when filling the resampler with initial
      // data, but we'll get called again and it will work out.
      uint32_t num, den;
      speex_resampler_get_ratio(resampler, &num, &den);
      uint32_t inputLimit = std::min(aAvailableInInputBuffer,
                                     availableInOutputBuffer * den / num + 10);
      for (uint32_t i = 0; true; ) {
        uint32_t inSamples = inputLimit;
        const float* inputData = mBuffer->GetData(i) + aSourceOffset;

        uint32_t outSamples = availableInOutputBuffer;
        float* outputData =
          static_cast<float*>(const_cast<void*>(aOutput->mChannelData[i])) +
          aBufferOffset;

        WebAudioUtils::SpeexResamplerProcess(resampler, i,
                                             inputData, &inSamples,
                                             outputData, &outSamples);
        if (++i == aChannels) {
          mPosition += inSamples;
          MOZ_ASSERT(mPosition <= mDuration || mLoop);
          aFramesWritten = outSamples;
          if (inSamples == aAvailableInInputBuffer && !mLoop) {
            // If the available output space were unbounded then the input
            // latency would always be the correct amount of extra input to
            // provide in order to advance the output position to align with
            // the final point in the buffer.  However, when the output space
            // becomes full, the resampler may read all available input
            // without writing out the corresponding output.  Add one more
            // input sample, so that we know that enough output has been
            // written when the last input sample has been read.  This may
            // often write more than necessary but the extra samples will be
            // based on (mostly) zero input.
            mRemainingResamplerTail =
              speex_resampler_get_input_latency(resampler) + 1;
          }
          return;
        }
      }
    } else {
      for (uint32_t i = 0; true; ) {
        uint32_t inSamples = mRemainingResamplerTail;
        uint32_t outSamples = availableInOutputBuffer;
        float* outputData =
          static_cast<float*>(const_cast<void*>(aOutput->mChannelData[i])) +
          aBufferOffset;

        // AudioDataValue* for aIn selects the function that does not try to
        // copy and format-convert input data.
        WebAudioUtils::SpeexResamplerProcess(resampler, i,
                         static_cast<AudioDataValue*>(nullptr), &inSamples,
                         outputData, &outSamples);
        if (++i == aChannels) {
          mRemainingResamplerTail -= inSamples;
          MOZ_ASSERT(mRemainingResamplerTail >= 0);
          aFramesWritten = outSamples;
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
  void FillWithZeroes(AudioChunk* aOutput,
                      uint32_t aChannels,
                      uint32_t* aOffsetWithinBlock,
                      TrackTicks* aCurrentPosition,
                      TrackTicks aMaxPos)
  {
    MOZ_ASSERT(*aCurrentPosition < aMaxPos);
    uint32_t numFrames =
      std::min<TrackTicks>(WEBAUDIO_BLOCK_SIZE - *aOffsetWithinBlock,
                           aMaxPos - *aCurrentPosition);
    if (numFrames == WEBAUDIO_BLOCK_SIZE) {
      aOutput->SetNull(numFrames);
    } else {
      if (aOutput->IsNull()) {
        AllocateAudioBlock(aChannels, aOutput);
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
                      AudioChunk* aOutput,
                      uint32_t aChannels,
                      uint32_t* aOffsetWithinBlock,
                      TrackTicks* aCurrentPosition,
                      uint32_t aBufferOffset,
                      uint32_t aBufferMax)
  {
    MOZ_ASSERT(*aCurrentPosition < mStop);
    uint32_t numFrames =
      std::min<TrackTicks>(std::min(WEBAUDIO_BLOCK_SIZE - *aOffsetWithinBlock,
                                    aBufferMax - aBufferOffset),
                           mStop - *aCurrentPosition);
    if (numFrames == WEBAUDIO_BLOCK_SIZE && !ShouldResample(aStream->SampleRate())) {
      BorrowFromInputBuffer(aOutput, aChannels, aBufferOffset);
      *aOffsetWithinBlock += numFrames;
      *aCurrentPosition += numFrames;
      mPosition += numFrames;
    } else {
      if (aOutput->IsNull()) {
        MOZ_ASSERT(*aOffsetWithinBlock == 0);
        AllocateAudioBlock(aChannels, aOutput);
      }
      if (!ShouldResample(aStream->SampleRate())) {
        CopyFromInputBuffer(aOutput, aChannels, aBufferOffset, *aOffsetWithinBlock, numFrames);
        *aOffsetWithinBlock += numFrames;
        *aCurrentPosition += numFrames;
        mPosition += numFrames;
      } else {
        uint32_t framesWritten, availableInInputBuffer;

        availableInInputBuffer = aBufferMax - aBufferOffset;

        CopyFromInputBufferWithResampling(aStream, aOutput, aChannels, aBufferOffset, *aOffsetWithinBlock, availableInInputBuffer, framesWritten);
        *aOffsetWithinBlock += framesWritten;
        *aCurrentPosition += framesWritten;
      }
    }
  }

  uint32_t ComputeFinalOutSampleRate(TrackRate aStreamSampleRate)
  {
    if (mPlaybackRate <= 0 || mPlaybackRate != mPlaybackRate) {
      mPlaybackRate = 1.0f;
    }
    if (mDopplerShift <= 0 || mDopplerShift != mDopplerShift) {
      mDopplerShift = 1.0f;
    }
    return WebAudioUtils::TruncateFloatToInt<uint32_t>(aStreamSampleRate /
                                                       (mPlaybackRate * mDopplerShift));
  }

  bool ShouldResample(TrackRate aStreamSampleRate) const
  {
    // There is latency in the resampler.  If there is already a resampler,
    // then it will have moved mPosition to after the samples it has read, but
    // it hasn't output its buffered samples.  Keep using the resampler, even
    // if the rates now match, so that this latency segment is output.
    return mResampler ||
      (mPlaybackRate * mDopplerShift * mBufferSampleRate != aStreamSampleRate);
  }

  void UpdateSampleRateIfNeeded(AudioNodeStream* aStream, uint32_t aChannels)
  {
    if (mPlaybackRateTimeline.HasSimpleValue()) {
      mPlaybackRate = mPlaybackRateTimeline.GetValue();
    } else {
      mPlaybackRate = mPlaybackRateTimeline.GetValueAtTime(aStream->GetCurrentPosition());
    }

    // Make sure the playback rate and the doppler shift are something
    // our resampler can work with.
    if (ComputeFinalOutSampleRate(aStream->SampleRate()) == 0) {
      mPlaybackRate = 1.0;
      mDopplerShift = 1.0;
    }

    if (mResampler) {
      SpeexResamplerState* resampler = Resampler(aStream, aChannels);
      uint32_t currentOutSampleRate, currentInSampleRate;
      speex_resampler_get_rate(resampler, &currentInSampleRate, &currentOutSampleRate);
      uint32_t finalSampleRate = ComputeFinalOutSampleRate(aStream->SampleRate());
      if (currentOutSampleRate != finalSampleRate) {
        speex_resampler_set_rate(resampler, currentInSampleRate, finalSampleRate);
      }
    }
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished)
  {
    if (!mBuffer || !mDuration) {
      return;
    }

    uint32_t channels = mBuffer->GetChannels();
    if (!channels) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    // WebKit treats the playbackRate as a k-rate parameter in their code,
    // despite the spec saying that it should be an a-rate parameter. We treat
    // it as k-rate. Spec bug: https://www.w3.org/Bugs/Public/show_bug.cgi?id=21592
    UpdateSampleRateIfNeeded(aStream, channels);

    uint32_t written = 0;
    TrackTicks streamPosition = aStream->GetCurrentPosition();
    while (written < WEBAUDIO_BLOCK_SIZE) {
      if (mStop != TRACK_TICKS_MAX &&
          streamPosition >= mStop) {
        FillWithZeroes(aOutput, channels, &written, &streamPosition, TRACK_TICKS_MAX);
        continue;
      }
      if (streamPosition < mStart) {
        FillWithZeroes(aOutput, channels, &written, &streamPosition, mStart);
        continue;
      }
      TrackTicks t = mPosition;
      if (mLoop) {
        if (mOffset + t < mLoopEnd) {
          CopyFromBuffer(aStream, aOutput, channels, &written, &streamPosition, mOffset + t, mLoopEnd);
        } else {
          uint32_t offsetInLoop = (mOffset + t - mLoopEnd) % (mLoopEnd - mLoopStart);
          CopyFromBuffer(aStream, aOutput, channels, &written, &streamPosition, mLoopStart + offsetInLoop, mLoopEnd);
        }
      } else {
        if (t < mDuration || mRemainingResamplerTail) {
          CopyFromBuffer(aStream, aOutput, channels, &written, &streamPosition, mOffset + t, mOffset + mDuration);
        } else {
          FillWithZeroes(aOutput, channels, &written, &streamPosition, TRACK_TICKS_MAX);
        }
      }
    }

    // We've finished if we've gone past mStop, or if we're past mDuration when
    // looping is disabled.
    if (streamPosition >= mStop ||
        (!mLoop && mPosition >= mDuration && !mRemainingResamplerTail)) {
      *aFinished = true;
    }
  }

  TrackTicks mStart;
  TrackTicks mStop;
  nsRefPtr<ThreadSharedFloatArrayBufferList> mBuffer;
  SpeexResamplerState* mResampler;
  // mRemainingResamplerTail, like mPosition, mOffset, and mDuration, is
  // measured in input buffer samples.
  int mRemainingResamplerTail;
  int32_t mOffset;
  int32_t mDuration;
  int32_t mLoopStart;
  int32_t mLoopEnd;
  int32_t mBufferSampleRate;
  int32_t mPosition;
  uint32_t mChannels;
  float mPlaybackRate;
  float mDopplerShift;
  AudioNodeStream* mDestination;
  AudioNodeStream* mSource;
  AudioParamTimeline mPlaybackRateTimeline;
  bool mLoop;
};

AudioBufferSourceNode::AudioBufferSourceNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mLoopStart(0.0)
  , mLoopEnd(0.0)
  , mOffset(0.0)
  , mDuration(std::numeric_limits<double>::min())
  , mPlaybackRate(new AudioParam(MOZ_THIS_IN_INITIALIZER_LIST(),
                  SendPlaybackRateToStream, 1.0f))
  , mLoop(false)
  , mStartCalled(false)
  , mStopped(false)
{
  AudioBufferSourceNodeEngine* engine = new AudioBufferSourceNodeEngine(this, aContext->Destination());
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::SOURCE_STREAM);
  engine->SetSourceStream(static_cast<AudioNodeStream*>(mStream.get()));
  mStream->AddMainThreadListener(this);
}

AudioBufferSourceNode::~AudioBufferSourceNode()
{
  if (Context()) {
    Context()->UnregisterAudioBufferSourceNode(this);
  }
}

JSObject*
AudioBufferSourceNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AudioBufferSourceNodeBinding::Wrap(aCx, aScope, this);
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

  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  if (!ns) {
    // Nothing to play, or we're already dead for some reason
    return;
  }

  if (mBuffer) {
    double duration = aDuration.WasPassed() ?
                      aDuration.Value() :
                      std::numeric_limits<double>::min();
    SendOffsetAndDurationParametersToStream(ns, aOffset, duration);
  } else {
    // Remember our arguments so that we can use them once we have a buffer.
    // We can't send these parameters now because we don't know the buffer
    // sample rate.
    mOffset = aOffset;
    mDuration = aDuration.WasPassed() ?
                aDuration.Value() :
                std::numeric_limits<double>::min();
  }

  // Don't set parameter unnecessarily
  if (aWhen > 0.0) {
    ns->SetStreamTimeParameter(START, Context()->DestinationStream(), aWhen);
  }

  MarkActive();
}

void
AudioBufferSourceNode::SendBufferParameterToStream(JSContext* aCx)
{
  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  MOZ_ASSERT(ns, "Why don't we have a stream here?");

  if (mBuffer) {
    float rate = mBuffer->SampleRate();
    nsRefPtr<ThreadSharedFloatArrayBufferList> data =
      mBuffer->GetThreadSharedChannelsForRate(aCx);
    ns->SetBuffer(data.forget());
    ns->SetInt32Parameter(SAMPLE_RATE, rate);

    if (mStartCalled) {
      SendOffsetAndDurationParametersToStream(ns, mOffset, mDuration);
    }
  } else {
    ns->SetBuffer(nullptr);
  }
}

void
AudioBufferSourceNode::SendOffsetAndDurationParametersToStream(AudioNodeStream* aStream,
                                                               double aOffset,
                                                               double aDuration)
{
  NS_ASSERTION(mBuffer && mStartCalled,
               "Only call this when we have a buffer and start() has been called");

  float rate = mBuffer->SampleRate();
  int32_t bufferLength = mBuffer->Length();
  int32_t offsetSamples = std::max(0, NS_lround(aOffset * rate));

  if (offsetSamples >= bufferLength) {
    // The offset falls past the end of the buffer.  In this case, we need to
    // stop the playback immediately if it's in progress.
    // Note that we can't call Stop() here since that might be overridden if
    // web content calls Stop() too, so we just null out the buffer.
    if (mStartCalled) {
      aStream->SetBuffer(nullptr);
    }
    return;
  }
  // Don't set parameter unnecessarily
  if (offsetSamples > 0) {
    aStream->SetInt32Parameter(OFFSET, offsetSamples);
  }

  int32_t playingLength = bufferLength - offsetSamples;
  if (aDuration != std::numeric_limits<double>::min()) {
    playingLength = std::min(NS_lround(aDuration * rate), playingLength);
  }
  aStream->SetInt32Parameter(DURATION, playingLength);
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

  if (!mBuffer) {
    // We don't have a buffer, so the stream is never marked as finished.
    // Therefore we need to drop our playing ref right now.
    MarkInactive();
  }

  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  if (!ns || !Context()) {
    // We've already stopped and had our stream shut down
    return;
  }

  ns->SetStreamTimeParameter(STOP, Context()->DestinationStream(),
                             std::max(0.0, aWhen));
}

void
AudioBufferSourceNode::NotifyMainThreadStateChanged()
{
  if (mStream->IsFinished()) {
    class EndedEventDispatcher : public nsRunnable
    {
    public:
      explicit EndedEventDispatcher(AudioBufferSourceNode* aNode)
        : mNode(aNode) {}
      NS_IMETHODIMP Run()
      {
        // If it's not safe to run scripts right now, schedule this to run later
        if (!nsContentUtils::IsSafeToRunScript()) {
          nsContentUtils::AddScriptRunner(this);
          return NS_OK;
        }

        mNode->DispatchTrustedEvent(NS_LITERAL_STRING("ended"));
        return NS_OK;
      }
    private:
      nsRefPtr<AudioBufferSourceNode> mNode;
    };
    if (!mStopped) {
      // Only dispatch the ended event once
      NS_DispatchToMainThread(new EndedEventDispatcher(this));
      mStopped = true;
    }

    // Drop the playing reference
    // Warning: The below line might delete this.
    MarkInactive();
  }
}

void
AudioBufferSourceNode::SendPlaybackRateToStream(AudioNode* aNode)
{
  AudioBufferSourceNode* This = static_cast<AudioBufferSourceNode*>(aNode);
  SendTimelineParameterToStream(This, PLAYBACKRATE, *This->mPlaybackRate);
}

void
AudioBufferSourceNode::SendDopplerShiftToStream(double aDopplerShift)
{
  SendDoubleParameterToStream(DOPPLERSHIFT, aDopplerShift);
}

void
AudioBufferSourceNode::SendLoopParametersToStream()
{
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
  } else if (!mLoop) {
    SendInt32ParameterToStream(LOOP, 0);
  }
}

}
}
