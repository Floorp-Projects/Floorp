/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONODESTREAM_H_
#define MOZILLA_AUDIONODESTREAM_H_

#include "MediaStreamGraph.h"
#include "mozilla/dom/AudioNodeBinding.h"
#include "AudioSegment.h"

namespace mozilla {

namespace dom {
struct ThreeDPoint;
class AudioParamTimeline;
class AudioContext;
}

class ThreadSharedFloatArrayBufferList;
class AudioNodeEngine;

/**
 * An AudioNodeStream produces one audio track with ID AUDIO_TRACK.
 * The start time of the AudioTrack is aligned to the start time of the
 * AudioContext's destination node stream, plus some multiple of BLOCK_SIZE
 * samples.
 *
 * An AudioNodeStream has an AudioNodeEngine plugged into it that does the
 * actual audio processing. AudioNodeStream contains the glue code that
 * integrates audio processing with the MediaStreamGraph.
 */
class AudioNodeStream : public ProcessedMediaStream {
  typedef dom::ChannelCountMode ChannelCountMode;
  typedef dom::ChannelInterpretation ChannelInterpretation;

public:
  typedef mozilla::dom::AudioContext AudioContext;

  enum { AUDIO_TRACK = 1 };

  typedef nsAutoTArray<AudioChunk, 1> OutputChunks;

  /**
   * Transfers ownership of aEngine to the new AudioNodeStream.
   */
  AudioNodeStream(AudioNodeEngine* aEngine,
                  MediaStreamGraph::AudioNodeStreamKind aKind,
                  TrackRate aSampleRate)
    : ProcessedMediaStream(nullptr),
      mEngine(aEngine),
      mSampleRate(aSampleRate),
      mKind(aKind),
      mNumberOfInputChannels(2),
      mMarkAsFinishedAfterThisBlock(false),
      mAudioParamStream(false),
      mPassThrough(false)
  {
    MOZ_ASSERT(NS_IsMainThread());
    mChannelCountMode = ChannelCountMode::Max;
    mChannelInterpretation = ChannelInterpretation::Speakers;
    // AudioNodes are always producing data
    mHasCurrentData = true;
    MOZ_COUNT_CTOR(AudioNodeStream);
  }

protected:
  ~AudioNodeStream();

public:
  // Control API
  /**
   * Sets a parameter that's a time relative to some stream's played time.
   * This time is converted to a time relative to this stream when it's set.
   */
  void SetStreamTimeParameter(uint32_t aIndex, AudioContext* aContext,
                              double aStreamTime);
  void SetDoubleParameter(uint32_t aIndex, double aValue);
  void SetInt32Parameter(uint32_t aIndex, int32_t aValue);
  void SetTimelineParameter(uint32_t aIndex, const dom::AudioParamTimeline& aValue);
  void SetThreeDPointParameter(uint32_t aIndex, const dom::ThreeDPoint& aValue);
  void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList>&& aBuffer);
  // This consumes the contents of aData.  aData will be emptied after this returns.
  void SetRawArrayData(nsTArray<float>& aData);
  void SetChannelMixingParameters(uint32_t aNumberOfChannels,
                                  ChannelCountMode aChannelCountMoe,
                                  ChannelInterpretation aChannelInterpretation);
  void SetPassThrough(bool aPassThrough);
  ChannelInterpretation GetChannelInterpretation()
  {
    return mChannelInterpretation;
  }

  void SetAudioParamHelperStream()
  {
    MOZ_ASSERT(!mAudioParamStream, "Can only do this once");
    mAudioParamStream = true;
  }

  virtual AudioNodeStream* AsAudioNodeStream() MOZ_OVERRIDE { return this; }

  // Graph thread only
  void SetStreamTimeParameterImpl(uint32_t aIndex, MediaStream* aRelativeToStream,
                                  double aStreamTime);
  void SetChannelMixingParametersImpl(uint32_t aNumberOfChannels,
                                      ChannelCountMode aChannelCountMoe,
                                      ChannelInterpretation aChannelInterpretation);
  virtual void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) MOZ_OVERRIDE;
  /**
   * Produce the next block of output, before input is provided.
   * ProcessInput() will be called later, and it then should not change
   * the output.  This is used only for DelayNodeEngine in a feedback loop.
   */
  void ProduceOutputBeforeInput(GraphTime aFrom);
  TrackTicks GetCurrentPosition();
  bool IsAudioParamStream() const
  {
    return mAudioParamStream;
  }

  const OutputChunks& LastChunks() const
  {
    return mLastChunks;
  }
  virtual bool MainThreadNeedsUpdates() const MOZ_OVERRIDE
  {
    // Only source and external streams need updates on the main thread.
    return (mKind == MediaStreamGraph::SOURCE_STREAM && mFinished) ||
           mKind == MediaStreamGraph::EXTERNAL_STREAM;
  }
  virtual bool IsIntrinsicallyConsumed() const MOZ_OVERRIDE
  {
    return true;
  }

  // Any thread
  AudioNodeEngine* Engine() { return mEngine; }
  TrackRate SampleRate() const { return mSampleRate; }

  /**
   * Convert a time in seconds on the destination stream to ticks
   * on this stream, including fractional position between ticks.
   */
  double FractionalTicksFromDestinationTime(AudioNodeStream* aDestination,
                                            double aSeconds);
  /**
   * Convert a time in seconds on the destination stream to nearest TrackTicks
   * on this stream.
   */
  TrackTicks TicksFromDestinationTime(MediaStream* aDestination,
                                      double aSeconds);
  /**
   * Get the destination stream time in seconds corresponding to a position on
   * this stream.
   */
  double DestinationTimeFromTicks(AudioNodeStream* aDestination,
                                  TrackTicks aPosition);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE;

  void SizeOfAudioNodesIncludingThis(MallocSizeOf aMallocSizeOf,
                                     AudioNodeSizes& aUsage) const;

protected:
  void AdvanceOutputSegment();
  void FinishOutput();
  void AccumulateInputChunk(uint32_t aInputIndex, const AudioChunk& aChunk,
                            AudioChunk* aBlock,
                            nsTArray<float>* aDownmixBuffer);
  void UpMixDownMixChunk(const AudioChunk* aChunk, uint32_t aOutputChannelCount,
                         nsTArray<const void*>& aOutputChannels,
                         nsTArray<float>& aDownmixBuffer);

  uint32_t ComputedNumberOfChannels(uint32_t aInputChannelCount);
  void ObtainInputBlock(AudioChunk& aTmpChunk, uint32_t aPortIndex);

  // The engine that will generate output for this node.
  nsAutoPtr<AudioNodeEngine> mEngine;
  // The last block produced by this node.
  OutputChunks mLastChunks;
  // The stream's sampling rate
  const TrackRate mSampleRate;
  // Whether this is an internal or external stream
  MediaStreamGraph::AudioNodeStreamKind mKind;
  // The number of input channels that this stream requires. 0 means don't care.
  uint32_t mNumberOfInputChannels;
  // The mixing modes
  ChannelCountMode mChannelCountMode;
  ChannelInterpretation mChannelInterpretation;
  // Whether the stream should be marked as finished as soon
  // as the current time range has been computed block by block.
  bool mMarkAsFinishedAfterThisBlock;
  // Whether the stream is an AudioParamHelper stream.
  bool mAudioParamStream;
  // Whether the stream just passes its input through.
  bool mPassThrough;
};

}

#endif /* MOZILLA_AUDIONODESTREAM_H_ */
