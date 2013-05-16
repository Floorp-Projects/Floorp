/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIONODEENGINE_H_
#define MOZILLA_AUDIONODEENGINE_H_

#include "AudioSegment.h"
#include "mozilla/dom/AudioParam.h"
#include "mozilla/Mutex.h"

namespace mozilla {

namespace dom {
class AudioNode;
struct ThreeDPoint;
}

class AudioNodeStream;

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
 *
 * Buffer size is implicitly assumed to be WEBAUDIO_BLOCK_SIZE.
 */
void AudioBlockCopyChannelWithScale(const float* aInput,
                                    float aScale,
                                    float* aOutput);

/**
 * Vector copy-scaled operation.
 */
void AudioBlockCopyChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                    const float aScale[WEBAUDIO_BLOCK_SIZE],
                                    float aOutput[WEBAUDIO_BLOCK_SIZE]);

/**
 * In place gain. aScale == 1.0f should be optimized.
 */
void AudioBlockInPlaceScale(float aBlock[WEBAUDIO_BLOCK_SIZE],
                            uint32_t aChannelCount,
                            float aScale);

/**
 * Upmix a mono input to a stereo output, scaling the two output channels by two
 * different gain value.
 * This algorithm is specified in the WebAudio spec.
 */
void
AudioBlockPanMonoToStereo(const float aInput[WEBAUDIO_BLOCK_SIZE],
                          float aGainL, float aGainR,
                          float aOutputL[WEBAUDIO_BLOCK_SIZE],
                          float aOutputR[WEBAUDIO_BLOCK_SIZE]);
/**
 * Pan a stereo source according to right and left gain, and the position
 * (whether the listener is on the left of the source or not).
 * This algorithm is specified in the WebAudio spec.
 */
void
AudioBlockPanStereoToStereo(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                            const float aInputR[WEBAUDIO_BLOCK_SIZE],
                            float aGainL, float aGainR, bool aIsOnTheLeft,
                            float aOutputL[WEBAUDIO_BLOCK_SIZE],
                            float aOutputR[WEBAUDIO_BLOCK_SIZE]);

/**
 * All methods of this class and its subclasses are called on the
 * MediaStreamGraph thread.
 */
class AudioNodeEngine {
public:
  // This should be compatible with AudioNodeStream::OutputChunks.
  typedef nsAutoTArray<AudioChunk, 1> OutputChunks;

  explicit AudioNodeEngine(dom::AudioNode* aNode)
    : mNode(aNode)
    , mNodeMutex("AudioNodeEngine::mNodeMutex")
    , mInputCount(aNode ? aNode->NumberOfInputs() : 1)
    , mOutputCount(aNode ? aNode->NumberOfOutputs() : 0)
  {
    MOZ_COUNT_CTOR(AudioNodeEngine);
  }
  virtual ~AudioNodeEngine()
  {
    MOZ_ASSERT(!mNode, "The node reference must be already cleared");
    MOZ_COUNT_DTOR(AudioNodeEngine);
  }

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
  virtual void SetThreeDPointParameter(uint32_t aIndex,
                                       const dom::ThreeDPoint& aValue)
  {
    NS_ERROR("Invalid SetThreeDPointParameter index");
  }
  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
  {
    NS_ERROR("SetBuffer called on engine that doesn't support it");
  }
  // This consumes the contents of aData.  aData will be emptied after this returns.
  virtual void SetRawArrayData(nsTArray<float>& aData)
  {
    NS_ERROR("SetRawArrayData called on an engine that doesn't support it");
  }

  /**
   * Produce the next block of audio samples, given input samples aInput
   * (the mixed data for input 0).
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
    MOZ_ASSERT(mInputCount <= 1 && mOutputCount <= 1);
    *aOutput = aInput;
  }

  /**
   * Produce the next block of audio samples, given input samples in the aInput
   * array.  There is one input sample per active port in aInput, in order.
   * This is the multi-input/output version of ProduceAudioBlock.  Only one kind
   * of ProduceAudioBlock is called on each node, depending on whether the
   * number of inputs and outputs are both 1 or not.
   *
   * aInput is always guaranteed to not contain more input AudioChunks than the
   * maximum number of inputs for the node.  It is the responsibility of the
   * overrides of this function to make sure they will only add a maximum number
   * of AudioChunks to aOutput as advertized by the AudioNode implementation.
   * An engine may choose to produce fewer inputs than advertizes by the
   * corresponding AudioNode, in which case it will be interpreted as a channel
   * of silence.
   */
  virtual void ProduceAudioBlocksOnPorts(AudioNodeStream* aStream,
                                         const OutputChunks& aInput,
                                         OutputChunks& aOutput,
                                         bool* aFinished)
  {
    MOZ_ASSERT(mInputCount > 1 || mOutputCount > 1);
    // Only produce one output port, and drop all other input ports.
    aOutput[0] = aInput[0];
  }

  Mutex& NodeMutex() { return mNodeMutex;}

  bool HasNode() const
  {
    return !!mNode;
  }

  dom::AudioNode* Node() const
  {
    mNodeMutex.AssertCurrentThreadOwns();
    return mNode;
  }

  dom::AudioNode* NodeMainThread() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mNode;
  }

  void ClearNode()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mNode != nullptr);
    mNodeMutex.AssertCurrentThreadOwns();
    mNode = nullptr;
  }

  uint16_t InputCount() const { return mInputCount; }
  uint16_t OutputCount() const { return mOutputCount; }

private:
  dom::AudioNode* mNode;
  Mutex mNodeMutex;
  const uint16_t mInputCount;
  const uint16_t mOutputCount;
};

}

#endif /* MOZILLA_AUDIONODEENGINE_H_ */
