/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIONODEENGINE_H_
#define MOZILLA_AUDIONODEENGINE_H_

#include "AudioSegment.h"
#include "mozilla/dom/AudioNode.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"

namespace WebCore {
class Reverb;
}  // namespace WebCore

namespace mozilla {

namespace dom {
struct ThreeDPoint;
class AudioParamTimeline;
class DelayNodeEngine;
struct AudioTimelineEvent;
}  // namespace dom

class AbstractThread;
class AudioBlock;
class AudioNodeTrack;

/**
 * This class holds onto a set of immutable channel buffers. The storage
 * for the buffers must be malloced, but the buffer pointers and the malloc
 * pointers can be different (e.g. if the buffers are contained inside
 * some malloced object).
 */
class ThreadSharedFloatArrayBufferList final : public ThreadSharedObject {
 public:
  /**
   * Construct with null channel data pointers.
   */
  explicit ThreadSharedFloatArrayBufferList(uint32_t aCount) {
    mContents.SetLength(aCount);
  }
  /**
   * Create with buffers suitable for transfer to
   * JS::NewArrayBufferWithContents().  The buffer contents are uninitialized
   * and so should be set using GetDataForWrite().
   */
  static already_AddRefed<ThreadSharedFloatArrayBufferList> Create(
      uint32_t aChannelCount, size_t aLength, const mozilla::fallible_t&);

  ThreadSharedFloatArrayBufferList* AsThreadSharedFloatArrayBufferList()
      override {
    return this;
  };

  struct Storage final {
    Storage() : mDataToFree(nullptr), mFree(nullptr), mSampleData(nullptr) {}
    ~Storage() {
      if (mFree) {
        mFree(mDataToFree);
      } else {
        MOZ_ASSERT(!mDataToFree);
      }
    }
    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
      // NB: mSampleData might not be owned, if it is it just points to
      //     mDataToFree.
      return aMallocSizeOf(mDataToFree);
    }
    void* mDataToFree;
    void (*mFree)(void*);
    float* mSampleData;
  };

  /**
   * This can be called on any thread.
   */
  uint32_t GetChannels() const { return mContents.Length(); }
  /**
   * This can be called on any thread.
   */
  const float* GetData(uint32_t aIndex) const {
    return mContents[aIndex].mSampleData;
  }
  /**
   * This can be called on any thread, but only when the calling thread is the
   * only owner.
   */
  float* GetDataForWrite(uint32_t aIndex) {
    MOZ_ASSERT(!IsShared());
    return mContents[aIndex].mSampleData;
  }

  /**
   * Call this only during initialization, before the object is handed to
   * any other thread.
   */
  void SetData(uint32_t aIndex, void* aDataToFree, void (*aFreeFunc)(void*),
               float* aData) {
    Storage* s = &mContents[aIndex];
    if (s->mFree) {
      s->mFree(s->mDataToFree);
    } else {
      MOZ_ASSERT(!s->mDataToFree);
    }

    s->mDataToFree = aDataToFree;
    s->mFree = aFreeFunc;
    s->mSampleData = aData;
  }

  /**
   * Put this object into an error state where there are no channels.
   */
  void Clear() { mContents.Clear(); }

  size_t SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override {
    size_t amount = ThreadSharedObject::SizeOfExcludingThis(aMallocSizeOf);
    amount += mContents.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < mContents.Length(); i++) {
      amount += mContents[i].SizeOfExcludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  AutoTArray<Storage, 2> mContents;
};

/**
 * aChunk must have been allocated by AllocateAudioBlock.
 */
void WriteZeroesToAudioBlock(AudioBlock* aChunk, uint32_t aStart,
                             uint32_t aLength);

/**
 * Copy with scale. aScale == 1.0f should be optimized.
 */
void AudioBufferCopyWithScale(const float* aInput, float aScale, float* aOutput,
                              uint32_t aSize);

/**
 * Pointwise multiply-add operation. aScale == 1.0f should be optimized.
 */
void AudioBufferAddWithScale(const float* aInput, float aScale, float* aOutput,
                             uint32_t aSize);

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
void AudioBlockCopyChannelWithScale(const float* aInput, float aScale,
                                    float* aOutput);

/**
 * Vector copy-scaled operation.
 */
void AudioBlockCopyChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                                    const float aScale[WEBAUDIO_BLOCK_SIZE],
                                    float aOutput[WEBAUDIO_BLOCK_SIZE]);

/**
 * Vector complex multiplication on arbitrary sized buffers.
 */
void BufferComplexMultiply(const float* aInput, const float* aScale,
                           float* aOutput, uint32_t aSize);

/**
 * Vector maximum element magnitude ( max(abs(aInput)) ).
 */
float AudioBufferPeakValue(const float* aInput, uint32_t aSize);

/**
 * In place gain. aScale == 1.0f should be optimized.
 */
void AudioBlockInPlaceScale(float aBlock[WEBAUDIO_BLOCK_SIZE], float aScale);

/**
 * In place gain. aScale == 1.0f should be optimized.
 */
void AudioBufferInPlaceScale(float* aBlock, float aScale, uint32_t aSize);

/**
 * a-rate in place gain.
 */
void AudioBlockInPlaceScale(float aBlock[WEBAUDIO_BLOCK_SIZE],
                            float aScale[WEBAUDIO_BLOCK_SIZE]);
/**
 * a-rate in place gain.
 */
void AudioBufferInPlaceScale(float* aBlock, float* aScale, uint32_t aSize);

/**
 * Upmix a mono input to a stereo output, scaling the two output channels by two
 * different gain value.
 * This algorithm is specified in the WebAudio spec.
 */
void AudioBlockPanMonoToStereo(const float aInput[WEBAUDIO_BLOCK_SIZE],
                               float aGainL, float aGainR,
                               float aOutputL[WEBAUDIO_BLOCK_SIZE],
                               float aOutputR[WEBAUDIO_BLOCK_SIZE]);

void AudioBlockPanMonoToStereo(const float aInput[WEBAUDIO_BLOCK_SIZE],
                               float aGainL[WEBAUDIO_BLOCK_SIZE],
                               float aGainR[WEBAUDIO_BLOCK_SIZE],
                               float aOutputL[WEBAUDIO_BLOCK_SIZE],
                               float aOutputR[WEBAUDIO_BLOCK_SIZE]);
/**
 * Pan a stereo source according to right and left gain, and the position
 * (whether the listener is on the left of the source or not).
 * This algorithm is specified in the WebAudio spec.
 */
void AudioBlockPanStereoToStereo(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                 const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                 float aGainL, float aGainR, bool aIsOnTheLeft,
                                 float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputR[WEBAUDIO_BLOCK_SIZE]);
void AudioBlockPanStereoToStereo(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                                 const float aInputR[WEBAUDIO_BLOCK_SIZE],
                                 const float aGainL[WEBAUDIO_BLOCK_SIZE],
                                 const float aGainR[WEBAUDIO_BLOCK_SIZE],
                                 const bool aIsOnTheLeft[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputL[WEBAUDIO_BLOCK_SIZE],
                                 float aOutputR[WEBAUDIO_BLOCK_SIZE]);

/**
 * Replace NaN by zeros in aSamples.
 */
void NaNToZeroInPlace(float* aSamples, size_t aCount);

/**
 * Return the sum of squares of all of the samples in the input.
 */
float AudioBufferSumOfSquares(const float* aInput, uint32_t aLength);

/**
 * All methods of this class and its subclasses are called on the
 * MediaTrackGraph thread.
 */
class AudioNodeEngine {
 public:
  // This should be compatible with AudioNodeTrack::OutputChunks.
  typedef AutoTArray<AudioBlock, 1> OutputChunks;

  explicit AudioNodeEngine(dom::AudioNode* aNode);

  virtual ~AudioNodeEngine() {
    MOZ_ASSERT(!mNode, "The node reference must be already cleared");
    MOZ_COUNT_DTOR(AudioNodeEngine);
  }

  virtual dom::DelayNodeEngine* AsDelayNodeEngine() { return nullptr; }

  virtual void SetTrackTimeParameter(uint32_t aIndex, TrackTime aParam) {
    NS_ERROR("Invalid SetTrackTimeParameter index");
  }
  virtual void SetDoubleParameter(uint32_t aIndex, double aParam) {
    NS_ERROR("Invalid SetDoubleParameter index");
  }
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam) {
    NS_ERROR("Invalid SetInt32Parameter index");
  }
  virtual void RecvTimelineEvent(uint32_t aIndex,
                                 dom::AudioTimelineEvent& aValue) {
    NS_ERROR("Invalid RecvTimelineEvent index");
  }
  virtual void SetBuffer(AudioChunk&& aBuffer) {
    NS_ERROR("SetBuffer called on engine that doesn't support it");
  }
  // This consumes the contents of aData.  aData will be emptied after this
  // returns.
  virtual void SetRawArrayData(nsTArray<float>& aData) {
    NS_ERROR("SetRawArrayData called on an engine that doesn't support it");
  }

  virtual void SetReverb(WebCore::Reverb* aBuffer,
                         uint32_t aImpulseChannelCount) {
    NS_ERROR("SetReverb called on engine that doesn't support it");
  }

  /**
   * Produce the next block of audio samples, given input samples aInput
   * (the mixed data for input 0).
   * aInput is guaranteed to have float sample format (if it has samples at all)
   * and to have been resampled to the sampling rate for the track, and to have
   * exactly WEBAUDIO_BLOCK_SIZE samples.
   * *aFinished is set to false by the caller. The callee must not set this to
   * true unless silent output is produced. If set to true, we'll finish the
   * track, consider this input inactive on any downstream nodes, and not
   * call this again.
   */
  virtual void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                            const AudioBlock& aInput, AudioBlock* aOutput,
                            bool* aFinished);
  /**
   * Produce the next block of audio samples, before input is provided.
   * ProcessBlock() will be called later, and it then should not change
   * aOutput.  This is used only for DelayNodeEngine in a feedback loop.
   */
  virtual void ProduceBlockBeforeInput(AudioNodeTrack* aTrack, GraphTime aFrom,
                                       AudioBlock* aOutput) {
    MOZ_ASSERT_UNREACHABLE("ProduceBlockBeforeInput called on wrong engine");
  }

  /**
   * Produce the next block of audio samples, given input samples in the aInput
   * array.  There is one input sample per port in aInput, in order.
   * This is the multi-input/output version of ProcessBlock.  Only one kind
   * of ProcessBlock is called on each node.  ProcessBlocksOnPorts() is called
   * instead of ProcessBlock() if either the number of inputs or the number of
   * outputs is greater than 1.
   *
   * The numbers of AudioBlocks in aInput and aOutput are always guaranteed to
   * match the numbers of inputs and outputs for the node.
   */
  virtual void ProcessBlocksOnPorts(AudioNodeTrack* aTrack, GraphTime aFrom,
                                    Span<const AudioBlock> aInput,
                                    Span<AudioBlock> aOutput, bool* aFinished);

  // IsActive() returns true if the engine needs to continue processing an
  // unfinished track even when it has silent or no input connections.  This
  // includes tail-times and when sources have been scheduled to start.  If
  // returning false, then the track can be suspended.
  virtual bool IsActive() const { return false; }

  // Called on forced shutdown of the MediaTrackGraph before handing ownership
  // from graph thread to main thread.
  virtual void NotifyForcedShutdown() {}

  bool HasNode() const {
    MOZ_ASSERT(NS_IsMainThread());
    return !!mNode;
  }

  dom::AudioNode* NodeMainThread() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mNode;
  }

  void ClearNode() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mNode != nullptr);
    mNode = nullptr;
  }

  uint16_t InputCount() const { return mInputCount; }
  uint16_t OutputCount() const { return mOutputCount; }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    // NB: |mNode| is tracked separately so it is excluded here.
    return 0;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  void SizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                           AudioNodeSizes& aUsage) const {
    aUsage.mEngine = SizeOfIncludingThis(aMallocSizeOf);
    aUsage.mNodeType = mNodeType;
  }

 private:
  // This is cleared from AudioNode::DestroyMediaTrack()
  dom::AudioNode* MOZ_NON_OWNING_REF mNode;  // main thread only
  const char* const mNodeType;
  const uint16_t mInputCount;
  const uint16_t mOutputCount;

 protected:
  const RefPtr<AbstractThread> mAbstractMainThread;
};

}  // namespace mozilla

#endif /* MOZILLA_AUDIONODEENGINE_H_ */
