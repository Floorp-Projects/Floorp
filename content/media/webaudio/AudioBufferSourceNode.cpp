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

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(AudioBufferSourceNode, AudioSourceNode, mBuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioBufferSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioSourceNode)

NS_IMPL_ADDREF_INHERITED(AudioBufferSourceNode, AudioSourceNode)
NS_IMPL_RELEASE_INHERITED(AudioBufferSourceNode, AudioSourceNode)

class AudioBufferSourceNodeEngine : public AudioNodeEngine
{
public:
  AudioBufferSourceNodeEngine() :
    mStart(0), mStop(TRACK_TICKS_MAX), mOffset(0), mDuration(0) {}

  // START, OFFSET and DURATION are always set by start() (along with setting
  // mBuffer to something non-null).
  // STOP is set by stop().
  enum Parameters {
    START,
    STOP,
    OFFSET,
    DURATION
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
    default:
      NS_ERROR("Bad AudioBufferSourceNodeEngine Int32Parameter");
    }
  }
  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
  {
    mBuffer = aBuffer;
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished)
  {
    if (!mBuffer)
      return;
    TrackTicks currentPosition = aStream->GetCurrentPosition();
    if (currentPosition + WEBAUDIO_BLOCK_SIZE <= mStart) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }
    TrackTicks endTime = std::min(mStart + mDuration, mStop);
    // Don't set *aFinished just because we passed mStop. Maybe someone
    // will call stop() again with a different value.
    if (currentPosition + WEBAUDIO_BLOCK_SIZE >= mStart + mDuration) {
      *aFinished = true;
    }
    if (currentPosition >= endTime || mStart >= endTime) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    uint32_t channels = mBuffer->GetChannels();
    if (!channels) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    if (currentPosition >= mStart &&
        currentPosition + WEBAUDIO_BLOCK_SIZE <= endTime) {
      // Data is entirely within the buffer. Avoid copying it.
      aOutput->mDuration = WEBAUDIO_BLOCK_SIZE;
      aOutput->mBuffer = mBuffer;
      aOutput->mChannelData.SetLength(channels);
      for (uint32_t i = 0; i < channels; ++i) {
        aOutput->mChannelData[i] =
          mBuffer->GetData(i) + uintptr_t(currentPosition - mStart + mOffset);
      }
      aOutput->mVolume = 1.0f;
      aOutput->mBufferFormat = AUDIO_FORMAT_FLOAT32;
      return;
    }

    AllocateAudioBlock(channels, aOutput);
    TrackTicks start = std::max(currentPosition, mStart);
    TrackTicks end = std::min(currentPosition + WEBAUDIO_BLOCK_SIZE, endTime);
    WriteZeroesToAudioBlock(aOutput, 0, uint32_t(start - currentPosition));
    for (uint32_t i = 0; i < channels; ++i) {
      memcpy(static_cast<float*>(const_cast<void*>(aOutput->mChannelData[i])) +
             uint32_t(start - currentPosition),
             mBuffer->GetData(i) +
             uintptr_t(start - mStart + mOffset),
             uint32_t(end - start));
    }
    uint32_t endOffset = uint32_t(end - currentPosition);
    WriteZeroesToAudioBlock(aOutput, endOffset, WEBAUDIO_BLOCK_SIZE - endOffset);
  }

  TrackTicks mStart;
  TrackTicks mStop;
  nsRefPtr<ThreadSharedFloatArrayBufferList> mBuffer;
  int32_t mOffset;
  int32_t mDuration;
};

AudioBufferSourceNode::AudioBufferSourceNode(AudioContext* aContext)
  : AudioSourceNode(aContext)
  , mStartCalled(false)
{
  SetProduceOwnOutput(true);
  mStream = aContext->Graph()->CreateAudioNodeStream(new AudioBufferSourceNodeEngine());
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
