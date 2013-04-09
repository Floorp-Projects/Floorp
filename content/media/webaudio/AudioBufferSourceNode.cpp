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

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(AudioBufferSourceNode, AudioNode, mBuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioBufferSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(AudioBufferSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(AudioBufferSourceNode, AudioNode)

class AudioBufferSourceNodeEngine : public AudioNodeEngine
{
public:
  AudioBufferSourceNodeEngine() :
    mStart(0), mStop(TRACK_TICKS_MAX),
    mOffset(0), mDuration(0),
    mLoop(false), mLoopStart(0), mLoopEnd(0)
  {}

  // START, OFFSET and DURATION are always set by start() (along with setting
  // mBuffer to something non-null).
  // STOP is set by stop().
  enum Parameters {
    START,
    STOP,
    OFFSET,
    DURATION,
    LOOP,
    LOOPSTART,
    LOOPEND
  };
  virtual void SetStreamTimeParameter(uint32_t aIndex, TrackTicks aParam)
  {
    switch (aIndex) {
    case START: mStart = aParam; break;
    case STOP: mStop = aParam; break;
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine StreamTimeParameter");
    }
  }
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam)
  {
    switch (aIndex) {
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
    if (numFrames == WEBAUDIO_BLOCK_SIZE) {
      BorrowFromInputBuffer(aOutput, aChannels, aBufferOffset);
    } else {
      if (aOutput->IsNull()) {
        AllocateAudioBlock(aChannels, aOutput);
      }
      CopyFromInputBuffer(aOutput, aChannels, aBufferOffset, *aOffsetWithinBlock, numFrames);
    }
    *aOffsetWithinBlock += numFrames;
    *aCurrentPosition += numFrames;
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

    uint32_t written = 0;
    TrackTicks currentPosition = aStream->GetCurrentPosition();
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
  int32_t mOffset;
  int32_t mDuration;
  bool mLoop;
  int32_t mLoopStart;
  int32_t mLoopEnd;
};

AudioBufferSourceNode::AudioBufferSourceNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mLoopStart(0.0)
  , mLoopEnd(0.0)
  , mLoop(false)
  , mStartCalled(false)
{
  SetProduceOwnOutput(true);
  mStream = aContext->Graph()->CreateAudioNodeStream(new AudioBufferSourceNodeEngine(),
                                                     MediaStreamGraph::INTERNAL_STREAM);
  mStream->AddMainThreadListener(this);
}

AudioBufferSourceNode::~AudioBufferSourceNode()
{
  DestroyMediaStream();
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

  uint32_t rate = Context()->GetRate();
  uint32_t lengthSamples;
  nsRefPtr<ThreadSharedFloatArrayBufferList> data =
    mBuffer->GetThreadSharedChannelsForRate(aCx, rate, &lengthSamples);
  double length = double(lengthSamples)/rate;
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
    SetProduceOwnOutput(false);
  }
}

}
}
