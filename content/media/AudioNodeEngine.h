/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIONODEENGINE_H_
#define MOZILLA_AUDIONODEENGINE_H_

#include "AudioSegment.h"
#include "mozilla/dom/AudioParam.h"

namespace mozilla {

class AudioNodeStream;

// We ensure that the graph advances in steps that are multiples of the Web
// Audio block size
const uint32_t WEBAUDIO_BLOCK_SIZE_BITS = 7;
const uint32_t WEBAUDIO_BLOCK_SIZE = 1 << WEBAUDIO_BLOCK_SIZE_BITS;

/**
 * This class holds onto a set of immutable channel buffers. The storage
 * for the buffers must be malloced, but the buffer pointers and the malloc
 * pointers can be different (e.g. if the buffers are contained inside
 * some malloced object).
 */
class ThreadSharedFloatArrayBufferList : public ThreadSharedObject {
public:
  /**
   * Construct with null data.
   */
  ThreadSharedFloatArrayBufferList(uint32_t aCount)
  {
    mContents.SetLength(aCount);
  }

  struct Storage {
    Storage()
    {
      mDataToFree = nullptr;
      mSampleData = nullptr;
    }
    ~Storage() { free(mDataToFree); }
    void* mDataToFree;
    const float* mSampleData;
  };

  /**
   * This can be called on any thread.
   */
  uint32_t GetChannels() const { return mContents.Length(); }
  /**
   * This can be called on any thread.
   */
  const float* GetData(uint32_t aIndex) const { return mContents[aIndex].mSampleData; }

  /**
   * Call this only during initialization, before the object is handed to
   * any other thread.
   */
  void SetData(uint32_t aIndex, void* aDataToFree, const float* aData)
  {
    Storage* s = &mContents[aIndex];
    free(s->mDataToFree);
    s->mDataToFree = aDataToFree;
    s->mSampleData = aData;
  }

  /**
   * Put this object into an error state where there are no channels.
   */
  void Clear() { mContents.Clear(); }

private:
  AutoFallibleTArray<Storage,2> mContents;
};

/**
 * Allocates an AudioChunk with fresh buffers of WEBAUDIO_BLOCK_SIZE float samples.
 * AudioChunk::mChannelData's entries can be cast to float* for writing.
 */
void AllocateAudioBlock(uint32_t aChannelCount, AudioChunk* aChunk);

/**
 * aChunk must have been allocated by AllocateAudioBlock.
 */
void WriteZeroesToAudioBlock(AudioChunk* aChunk, uint32_t aStart, uint32_t aLength);

/**
 * Pointwise multiply-add operation. aScale == 1.0f should be optimized.
 */
void AudioBlockAddChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                   float aScale,
                                   float aOutput[WEBAUDIO_BLOCK_SIZE]);

/**
 * Pointwise copy-scaled operation. aScale == 1.0f should be optimized.
 */
void AudioBlockCopyChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                    float aScale,
                                    float aOutput[WEBAUDIO_BLOCK_SIZE]);

/**
 * All methods of this class and its subclasses are called on the
 * MediaStreamGraph thread.
 */
class AudioNodeEngine {
public:
  AudioNodeEngine() {}
  virtual ~AudioNodeEngine() {}

  virtual void SetStreamTimeParameter(uint32_t aIndex, TrackTicks aParam)
  {
    NS_ERROR("Invalid SetStreamTimeParameter index");
  }
  virtual void SetDoubleParameter(uint32_t aIndex, double aParam)
  {
    NS_ERROR("Invalid SetDoubleParameter index");
  }
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam)
  {
    NS_ERROR("Invalid SetInt32Parameter index");
  }
  virtual void SetTimelineParameter(uint32_t aIndex,
                                    const dom::AudioParamTimeline& aValue)
  {
    NS_ERROR("Invalid SetTimelineParameter index");
  }
  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
  {
    NS_ERROR("SetBuffer called on engine that doesn't support it");
  }

  /**
   * Produce the next block of audio samples, given input samples aInput
   * (the mixed data for input 0).
   * By default, simply returns the mixed input.
   * aInput is guaranteed to have float sample format (if it has samples at all)
   * and to have been resampled to IdealAudioRate(), and to have exactly
   * WEBAUDIO_BLOCK_SIZE samples.
   * *aFinished is set to false by the caller. If the callee sets it to true,
   * we'll finish the stream and not call this again.
   */
  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished)
  {
    *aOutput = aInput;
  }
};

}

#endif /* MOZILLA_AUDIONODEENGINE_H_ */
