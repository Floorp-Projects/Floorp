/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBufferSourceNode.h"
#include "mozilla/dom/AudioBufferSourceNodeBinding.h"
#include "nsMathUtils.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "PannerNode.h"
#include "speex/speex_resampler.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AudioBufferSourceNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBuffer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPlaybackRate)
  if (tmp->Context()) {
    tmp->Context()->UnregisterAudioBufferSourceNode(tmp);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AudioBufferSourceNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlaybackRate)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioBufferSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(AudioBufferSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(AudioBufferSourceNode, AudioNode)

class AudioBufferSourceNodeEngine : public AudioNodeEngine
{
public:
  explicit AudioBufferSourceNodeEngine(AudioNode* aNode,
                                       AudioDestinationNode* aDestination) :
    AudioNodeEngine(aNode),
    mStart(0), mStop(TRACK_TICKS_MAX),
    mResampler(nullptr),
    mOffset(0), mDuration(0),
    mLoopStart(0), mLoopEnd(0),
    mSampleRate(0), mPosition(0), mChannels(0), mPlaybackRate(1.0f),
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

  // START, OFFSET and DURATION are always set by start() (along with setting
  // mBuffer to something non-null).
  // STOP is set by stop().
  enum Parameters {
    SAMPLE_RATE,
    START,
    STOP,
    OFFSET,
    DURATION,
    LOOP,
    LOOPSTART,
    LOOPEND,
    PLAYBACKRATE,
    DOPPLERSHIFT
  };
  virtual void SetTimelineParameter(uint32_t aIndex, const dom::AudioParamTimeline& aValue)
  {
    switch (aIndex) {
    case PLAYBACKRATE:
      mPlaybackRateTimeline = aValue;
      // If we have a simple value that is 1.0 (i.e. intrinsic speed), and our
      // input buffer is already at the ideal audio rate, and we have a
      // resampler, we can release it.
      if (mResampler && mPlaybackRateTimeline.HasSimpleValue() &&
          mPlaybackRateTimeline.GetValue() == 1.0 &&
          mSampleRate == IdealAudioRate()) {
        speex_resampler_destroy(mResampler);
        mResampler = nullptr;
      }
      WebAudioUtils::ConvertAudioParamToTicks(mPlaybackRateTimeline, nullptr, mDestination);
      break;
    default:
      NS_ERROR("Bad GainNodeEngine TimelineParameter");
    }
  }
  virtual void SetStreamTimeParameter(uint32_t aIndex, TrackTicks aParam)
  {
    switch (aIndex) {
    case START: mStart = aParam; break;
    case STOP: mStop = aParam; break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine StreamTimeParameter");
    }
  }
  virtual void SetDoubleParameter(uint32_t aIndex, double aParam)
  {
    switch (aIndex) {
      case DOPPLERSHIFT:
        mDopplerShift = aParam;
        break;
      default:
        NS_ERROR("Bad AudioBufferSourceNodeEngine double parameter.");
    };
  }
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam)
  {
    switch (aIndex) {
    case SAMPLE_RATE: mSampleRate = aParam; break;
    case OFFSET: mOffset = aParam; break;
    case DURATION: mDuration = aParam; break;
    case LOOP: mLoop = !!aParam; break;
    case LOOPSTART: mLoopStart = aParam; break;
    case LOOPEND: mLoopEnd = aParam; break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine Int32Parameter");
    }
  }
  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
  {
    mBuffer = aBuffer;
  }

  SpeexResamplerState* Resampler(uint32_t aChannels)
  {
    if (aChannels != mChannels && mResampler) {
      speex_resampler_destroy(mResampler);
      mResampler = nullptr;
    }

    if (!mResampler) {
      mChannels = aChannels;
      mResampler = speex_resampler_init(mChannels, mSampleRate,
                                        ComputeFinalOutSampleRate(),
                                        SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                        nullptr);
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

  // Resamples input data to an output buffer, according to |mSampleRate| and
  // the playbackRate.
  // The number of frames consumed/produced depends on the amount of space
  // remaining in both the input and output buffer, and the playback rate (that
  // is, the ratio between the output samplerate and the input samplerate).
  void CopyFromInputBufferWithResampling(AudioChunk* aOutput,
                                         uint32_t aChannels,
                                         uintptr_t aSourceOffset,
                                         uintptr_t aBufferOffset,
                                         uint32_t aAvailableInInputBuffer,
                                         uint32_t& aFramesRead,
                                         uint32_t& aFramesWritten) {
    double finalPlaybackRate = static_cast<double>(mSampleRate) / ComputeFinalOutSampleRate();
    uint32_t availableInOuputBuffer = WEBAUDIO_BLOCK_SIZE - aBufferOffset;
    uint32_t inputSamples, outputSamples;

    // Check if we are short on input or output buffer.
    if (aAvailableInInputBuffer < availableInOuputBuffer * finalPlaybackRate) {
      outputSamples = ceil(aAvailableInInputBuffer / finalPlaybackRate);
      inputSamples = aAvailableInInputBuffer;
    } else {
      inputSamples = ceil(availableInOuputBuffer * finalPlaybackRate);
      outputSamples = availableInOuputBuffer;
    }

    SpeexResamplerState* resampler = Resampler(aChannels);

    for (uint32_t i = 0; i < aChannels; ++i) {
      uint32_t inSamples = inputSamples;
      uint32_t outSamples = outputSamples;

      const float* inputData = mBuffer->GetData(i) + aSourceOffset;
      float* outputData =
        static_cast<float*>(const_cast<void*>(aOutput->mChannelData[i])) +
        aBufferOffset;

      speex_resampler_process_float(resampler, i,
                                    inputData, &inSamples,
                                    outputData, &outSamples);

      aFramesRead = inSamples;
      aFramesWritten = outSamples;
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
    uint32_t numFrames = std::min(WEBAUDIO_BLOCK_SIZE - *aOffsetWithinBlock,
                                  uint32_t(aMaxPos - *aCurrentPosition));
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
   * we copy.  This will never advance aOffsetWithinBlock past
   * WEBAUDIO_BLOCK_SIZE, or aCurrentPosition past mStop.  It takes data from
   * the buffer at aBufferOffset, and never takes more data than aBufferMax.
   * This function knows when it needs to allocate the output buffer, and also
   * optimizes the case where it can avoid memory allocations.
   */
  void CopyFromBuffer(AudioChunk* aOutput,
                      uint32_t aChannels,
                      uint32_t* aOffsetWithinBlock,
                      TrackTicks* aCurrentPosition,
                      uint32_t aBufferOffset,
                      uint32_t aBufferMax)
  {
    uint32_t numFrames = std::min(std::min(WEBAUDIO_BLOCK_SIZE - *aOffsetWithinBlock,
                                           aBufferMax - aBufferOffset),
                                  uint32_t(mStop - *aCurrentPosition));
    if (numFrames == WEBAUDIO_BLOCK_SIZE && !ShouldResample()) {
      BorrowFromInputBuffer(aOutput, aChannels, aBufferOffset);
      *aOffsetWithinBlock += numFrames;
      *aCurrentPosition += numFrames;
      mPosition += numFrames;
    } else {
      if (aOutput->IsNull()) {
        MOZ_ASSERT(*aOffsetWithinBlock == 0);
        AllocateAudioBlock(aChannels, aOutput);
      }
      if (!ShouldResample()) {
        CopyFromInputBuffer(aOutput, aChannels, aBufferOffset, *aOffsetWithinBlock, numFrames);
        *aOffsetWithinBlock += numFrames;
        *aCurrentPosition += numFrames;
        mPosition += numFrames;
      } else {
        uint32_t framesRead, framesWritten, availableInInputBuffer;

        availableInInputBuffer = aBufferMax - aBufferOffset;

        CopyFromInputBufferWithResampling(aOutput, aChannels, aBufferOffset, *aOffsetWithinBlock, availableInInputBuffer, framesRead, framesWritten);
        *aOffsetWithinBlock += framesWritten;
        *aCurrentPosition += framesRead;
        mPosition += framesRead;
      }
    }
  }

  TrackTicks GetPosition(AudioNodeStream* aStream)
  {
    if (aStream->GetCurrentPosition() < mStart) {
      return aStream->GetCurrentPosition();
    }
    return mStart + mPosition;
  }

  int32_t ComputeFinalOutSampleRate() const
  {
    return static_cast<uint32_t>(IdealAudioRate() / (mPlaybackRate * mDopplerShift));
  }

  bool ShouldResample() const
  {
    return !(mPlaybackRate == 1.0 &&
             mDopplerShift == 1.0 &&
             mSampleRate == IdealAudioRate());
  }

  void UpdateSampleRateIfNeeded(AudioNodeStream* aStream)
  {
    if (mPlaybackRateTimeline.HasSimpleValue()) {
      mPlaybackRate = mPlaybackRateTimeline.GetValue();
    } else {
      mPlaybackRate = mPlaybackRateTimeline.GetValueAtTime<TrackTicks>(aStream->GetCurrentPosition());
    }

    uint32_t currentOutSampleRate, currentInSampleRate;
    if (ShouldResample()) {
      SpeexResamplerState* resampler = Resampler(mChannels);
      speex_resampler_get_rate(resampler, &currentInSampleRate, &currentOutSampleRate);
      uint32_t finalSampleRate = ComputeFinalOutSampleRate();
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
    if (!mBuffer)
      return;

    uint32_t channels = mBuffer->GetChannels();
    if (!channels) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    // WebKit treats the playbackRate as a k-rate parameter in their code,
    // despite the spec saying that it should be an a-rate parameter. We treat
    // it as k-rate. Spec bug: https://www.w3.org/Bugs/Public/show_bug.cgi?id=21592
    UpdateSampleRateIfNeeded(aStream);

    uint32_t written = 0;
    TrackTicks currentPosition = GetPosition(aStream);
    while (written < WEBAUDIO_BLOCK_SIZE) {
      if (mStop != TRACK_TICKS_MAX &&
          currentPosition >= mStop) {
        FillWithZeroes(aOutput, channels, &written, &currentPosition, TRACK_TICKS_MAX);
        continue;
      }
      if (currentPosition < mStart) {
        FillWithZeroes(aOutput, channels, &written, &currentPosition, mStart);
        continue;
      }
      TrackTicks t = currentPosition - mStart;
      if (mLoop) {
        if (mOffset + t < mLoopEnd) {
          CopyFromBuffer(aOutput, channels, &written, &currentPosition, mOffset + t, mLoopEnd);
        } else {
          uint32_t offsetInLoop = (mOffset + t - mLoopEnd) % (mLoopEnd - mLoopStart);
          CopyFromBuffer(aOutput, channels, &written, &currentPosition, mLoopStart + offsetInLoop, mLoopEnd);
        }
      } else {
        if (mOffset + t < mDuration) {
          CopyFromBuffer(aOutput, channels, &written, &currentPosition, mOffset + t, mDuration);
        } else {
          FillWithZeroes(aOutput, channels, &written, &currentPosition, TRACK_TICKS_MAX);
        }
      }
    }

    // We've finished if we've gone past mStop, or if we're past mDuration when
    // looping is disabled.
    if (currentPosition >= mStop ||
        (!mLoop && currentPosition - mStart + mOffset > mDuration)) {
      *aFinished = true;
    }
  }

  TrackTicks mStart;
  TrackTicks mStop;
  nsRefPtr<ThreadSharedFloatArrayBufferList> mBuffer;
  SpeexResamplerState* mResampler;
  int32_t mOffset;
  int32_t mDuration;
  int32_t mLoopStart;
  int32_t mLoopEnd;
  int32_t mSampleRate;
  uint32_t mPosition;
  uint32_t mChannels;
  float mPlaybackRate;
  float mDopplerShift;
  AudioNodeStream* mDestination;
  AudioParamTimeline mPlaybackRateTimeline;
  bool mLoop;
};

AudioBufferSourceNode::AudioBufferSourceNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mLoopStart(0.0)
  , mLoopEnd(0.0)
  , mPlaybackRate(new AudioParam(this, SendPlaybackRateToStream, 1.0f))
  , mPannerNode(nullptr)
  , mLoop(false)
  , mStartCalled(false)
{
  mStream = aContext->Graph()->CreateAudioNodeStream(
      new AudioBufferSourceNodeEngine(this, aContext->Destination()),
      MediaStreamGraph::INTERNAL_STREAM);
  mStream->AddMainThreadListener(this);
}

AudioBufferSourceNode::~AudioBufferSourceNode()
{
  if (Context()) {
    Context()->UnregisterAudioBufferSourceNode(this);
  }
}

JSObject*
AudioBufferSourceNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return AudioBufferSourceNodeBinding::Wrap(aCx, aScope, this);
}

void
AudioBufferSourceNode::Start(JSContext* aCx, double aWhen, double aOffset,
                             const Optional<double>& aDuration, ErrorResult& aRv)
{
  if (mStartCalled) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mStartCalled = true;

  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  if (!mBuffer || !ns) {
    // Nothing to play, or we're already dead for some reason
    return;
  }

  float rate = mBuffer->SampleRate();
  int32_t lengthSamples = mBuffer->Length();
  nsRefPtr<ThreadSharedFloatArrayBufferList> data =
    mBuffer->GetThreadSharedChannelsForRate(aCx);
  double length = double(lengthSamples) / rate;
  double offset = std::max(0.0, aOffset);
  double endOffset = aDuration.WasPassed() ?
      std::min(aOffset + aDuration.Value(), length) : length;

  if (offset >= endOffset) {
    return;
  }

  // Don't compute and set the loop parameters unnecessarily
  if (mLoop) {
    double actualLoopStart, actualLoopEnd;
    if (((mLoopStart != 0.0) || (mLoopEnd != 0.0)) &&
        mLoopStart >= 0.0 && mLoopEnd > 0.0 &&
        mLoopStart < mLoopEnd) {
      actualLoopStart = (mLoopStart > length) ? 0.0 : mLoopStart;
      actualLoopEnd = std::min(mLoopEnd, length);
    } else {
      actualLoopStart = 0.0;
      actualLoopEnd = length;
    }
    int32_t loopStartTicks = NS_lround(actualLoopStart * rate);
    int32_t loopEndTicks = NS_lround(actualLoopEnd * rate);
    ns->SetInt32Parameter(AudioBufferSourceNodeEngine::LOOP, 1);
    ns->SetInt32Parameter(AudioBufferSourceNodeEngine::LOOPSTART, loopStartTicks);
    ns->SetInt32Parameter(AudioBufferSourceNodeEngine::LOOPEND, loopEndTicks);
  }

  ns->SetBuffer(data.forget());
  // Don't set parameter unnecessarily
  if (aWhen > 0.0) {
    ns->SetStreamTimeParameter(AudioBufferSourceNodeEngine::START,
                               Context()->DestinationStream(),
                               aWhen);
  }
  int32_t offsetTicks = NS_lround(offset*rate);
  // Don't set parameter unnecessarily
  if (offsetTicks > 0) {
    ns->SetInt32Parameter(AudioBufferSourceNodeEngine::OFFSET, offsetTicks);
  }
  ns->SetInt32Parameter(AudioBufferSourceNodeEngine::DURATION,
      NS_lround(endOffset*rate) - offsetTicks);
  ns->SetInt32Parameter(AudioBufferSourceNodeEngine::SAMPLE_RATE, rate);

  MOZ_ASSERT(!mPlayingRef, "We can only accept a successful start() call once");
  mPlayingRef.Take(this);
}

void
AudioBufferSourceNode::Stop(double aWhen, ErrorResult& aRv)
{
  if (!mStartCalled) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  if (!ns) {
    // We've already stopped and had our stream shut down
    return;
  }

  ns->SetStreamTimeParameter(AudioBufferSourceNodeEngine::STOP,
                             Context()->DestinationStream(),
                             std::max(0.0, aWhen));
}

void
AudioBufferSourceNode::NotifyMainThreadStateChanged()
{
  if (mStream->IsFinished()) {
    // Drop the playing reference
    // Warning: The below line might delete this.
    mPlayingRef.Drop(this);
  }
}

void
AudioBufferSourceNode::SendPlaybackRateToStream(AudioNode* aNode)
{
  AudioBufferSourceNode* This = static_cast<AudioBufferSourceNode*>(aNode);
  SendTimelineParameterToStream(This, AudioBufferSourceNodeEngine::PLAYBACKRATE, *This->mPlaybackRate);
}

void
AudioBufferSourceNode::SendDopplerShiftToStream(double aDopplerShift)
{
  SendDoubleParameterToStream(AudioBufferSourceNodeEngine::DOPPLERSHIFT, aDopplerShift);
}

}
}
