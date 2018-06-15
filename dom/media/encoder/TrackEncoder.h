/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrackEncoder_h_
#define TrackEncoder_h_

#include "AudioSegment.h"
#include "EncodedFrameContainer.h"
#include "MediaStreamGraph.h"
#include "StreamTracks.h"
#include "TrackMetadataBase.h"
#include "VideoSegment.h"

namespace mozilla {

class AbstractThread;
class TrackEncoder;

class TrackEncoderListener
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TrackEncoderListener)

  /**
   * Called when the TrackEncoder's underlying encoder has been successfully
   * initialized and there's non-null data ready to be encoded.
   */
  virtual void Initialized(TrackEncoder* aEncoder) = 0;

  /**
   * Called when there's new data ready to be encoded.
   * Always called after Initialized().
   */
  virtual void DataAvailable(TrackEncoder* aEncoder) = 0;

  /**
   * Called after the TrackEncoder hit an unexpected error, causing it to
   * abort operation.
   */
  virtual void Error(TrackEncoder* aEncoder) = 0;
protected:
  virtual ~TrackEncoderListener() {}
};

/**
 * Base class of AudioTrackEncoder and VideoTrackEncoder. Lifetime managed by
 * MediaEncoder. All methods are to be called only on the worker thread.
 *
 * MediaStreamTrackListeners will get store raw data in mIncomingBuffer, so
 * mIncomingBuffer is protected by a lock. The control APIs are all called by
 * MediaEncoder on its dedicated thread, where GetEncodedTrack is called
 * periodically to swap out mIncomingBuffer, feed it to the encoder, and return
 * the encoded data.
 */
class TrackEncoder
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TrackEncoder);

public:
  explicit TrackEncoder(TrackRate aTrackRate);

  virtual void Suspend(TimeStamp aTime) = 0;

  virtual void Resume(TimeStamp aTime) = 0;

  /**
   * Called by MediaEncoder to cancel the encoding.
   */
  virtual void Cancel() = 0;

  /**
   * Notifies us that we have reached the end of the stream and no more data
   * will be appended.
   */
  virtual void NotifyEndOfStream() = 0;

  /**
   * MediaStreamGraph notifies us about the time of the track's start.
   * This gets called on the MediaEncoder thread after a dispatch.
   */
  virtual void SetStartOffset(StreamTime aStartOffset) = 0;

  /**
   * Dispatched from MediaStreamGraph when it has run an iteration where the
   * input track of the track this TrackEncoder is associated with didn't have
   * any data.
   */
  virtual void AdvanceBlockedInput(StreamTime aDuration) = 0;

  /**
   * MediaStreamGraph notifies us about the duration of data that has just been
   * processed. This gets called on the MediaEncoder thread after a dispatch.
   */
  virtual void AdvanceCurrentTime(StreamTime aDuration) = 0;

  /**
   * Creates and sets up meta data for a specific codec, called on the worker
   * thread.
   */
  virtual already_AddRefed<TrackMetadataBase> GetMetadata() = 0;

  /**
   * Encodes raw segments. Result data is returned in aData, and called on the
   * worker thread.
   */
  virtual nsresult GetEncodedTrack(EncodedFrameContainer& aData) = 0;

  /**
   * Returns true once this TrackEncoder is initialized.
   */
  bool IsInitialized();

  /**
   * True if the track encoder has encoded all source segments coming from
   * MediaStreamGraph. Call on the worker thread.
   */
  bool IsEncodingComplete();

  /**
   * If this TrackEncoder was not already initialized, it is set to initialized
   * and listeners are notified.
   */
  void SetInitialized();

  /**
   * Notifies listeners that there is data available for encoding.
   */
  void OnDataAvailable();

  /**
   * Called after an error. Cancels the encoding and notifies listeners.
   */
  void OnError();

  /**
   * Registers a listener to events from this TrackEncoder.
   * We hold a strong reference to the listener.
   */
  void RegisterListener(TrackEncoderListener* aListener);

  /**
   * Unregisters a listener from events from this TrackEncoder.
   * The listener will stop receiving events synchronously.
   */
  bool UnregisterListener(TrackEncoderListener* aListener);

  virtual void SetBitrate(const uint32_t aBitrate) = 0;

  /**
   * It's optional to set the worker thread, but if you do we'll assert that
   * we are in the worker thread in every method that gets called.
   */
  void SetWorkerThread(AbstractThread* aWorkerThread);

  /**
   * Measure size of internal buffers.
   */
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) = 0;

protected:
  virtual ~TrackEncoder()
  {
    MOZ_ASSERT(mListeners.IsEmpty());
  }

  /**
   * True if the track encoder has encoded all source data.
   */
  bool mEncodingComplete;

  /**
   * True if flag of EOS or any form of indicating EOS has set in the codec-
   * encoder.
   */
  bool mEosSetInEncoder;

  /**
   * True if the track encoder has been initialized successfully.
   */
  bool mInitialized;

  /**
   * True once all data until the end of the input track has been received.
   */
  bool mEndOfStream;

  /**
   * True once this encoding has been cancelled.
   */
  bool mCanceled;

  /**
   * The latest current time reported to us from the MSG.
   */
  StreamTime mCurrentTime;

  // How many times we have tried to initialize the encoder.
  uint32_t mInitCounter;
  StreamTime mNotInitDuration;

  bool mSuspended;

  /**
   * The track rate of source media.
   */
  TrackRate mTrackRate;

  /**
   * If set we assert that all methods are called on this thread.
   */
  RefPtr<AbstractThread> mWorkerThread;

  nsTArray<RefPtr<TrackEncoderListener>> mListeners;
};

class AudioTrackEncoder : public TrackEncoder
{
public:
  explicit AudioTrackEncoder(TrackRate aTrackRate)
    : TrackEncoder(aTrackRate)
    , mChannels(0)
    , mSamplingRate(0)
    , mAudioBitrate(0)
    , mDirectConnected(false)
  {}

  /**
   * Suspends encoding from mCurrentTime, i.e., all audio data until the next
   * Resume() will be dropped.
   */
  void Suspend(TimeStamp aTime) override;

  /**
   * Resumes encoding starting at mCurrentTime.
   */
  void Resume(TimeStamp aTime) override;

  /**
   * Appends and consumes track data from aSegment.
   */
  void AppendAudioSegment(AudioSegment&& aSegment);

  /**
   * Takes track data from the last time TakeTrackData ran until mCurrentTime
   * and moves it to aSegment.
   */
  void TakeTrackData(AudioSegment& aSegment);

  template<typename T>
  static
  void InterleaveTrackData(nsTArray<const T*>& aInput,
                           int32_t aDuration,
                           uint32_t aOutputChannels,
                           AudioDataValue* aOutput,
                           float aVolume)
  {
    if (aInput.Length() < aOutputChannels) {
      // Up-mix. This might make the mChannelData have more than aChannels.
      AudioChannelsUpMix(&aInput, aOutputChannels, SilentChannel::ZeroChannel<T>());
    }

    if (aInput.Length() > aOutputChannels) {
      DownmixAndInterleave(aInput, aDuration,
                           aVolume, aOutputChannels, aOutput);
    } else {
      InterleaveAndConvertBuffer(aInput.Elements(), aDuration, aVolume,
                                 aOutputChannels, aOutput);
    }
  }

  /**
   * Interleaves the track data and stores the result into aOutput. Might need
   * to up-mix or down-mix the channel data if the channels number of this chunk
   * is different from aOutputChannels. The channel data from aChunk might be
   * modified by up-mixing.
   */
  static void InterleaveTrackData(AudioChunk& aChunk, int32_t aDuration,
                                  uint32_t aOutputChannels,
                                  AudioDataValue* aOutput);

  /**
   * De-interleaves the aInput data and stores the result into aOutput.
   * No up-mix or down-mix operations inside.
   */
  static void DeInterleaveTrackData(AudioDataValue* aInput, int32_t aDuration,
                                    int32_t aChannels, AudioDataValue* aOutput);

  /**
   * Measure size of internal buffers.
   */
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) override;

  void SetBitrate(const uint32_t aBitrate) override
  {
    mAudioBitrate = aBitrate;
  }

  /**
   * Tries to initiate the AudioEncoder based on data in aSegment.
   * This can be re-called often, as it will exit early should we already be
   * initiated. mInitiated will only be set if there was enough data in
   * aSegment to infer metadata. If mInitiated gets set, listeners are notified.
   *
   * Not having enough data in aSegment to initiate the encoder for an accumulated aDuration of one second will make us initiate with a default number of channels.
   *
   * If we attempt to initiate the underlying encoder but fail, we Cancel() and
   * notify listeners.
   */
  void TryInit(const AudioSegment& aSegment, StreamTime aDuration);

  void Cancel() override;

  /**
   * Dispatched from MediaStreamGraph when we have finished feeding data to
   * mIncomingBuffer.
   */
  void NotifyEndOfStream() override;

  void SetStartOffset(StreamTime aStartOffset) override;

  /**
   * Dispatched from MediaStreamGraph when it has run an iteration where the
   * input track of the track this TrackEncoder is associated with didn't have
   * any data.
   *
   * Since we sometimes use a direct listener for AudioSegments we miss periods
   * of time for which the source didn't have any data. This ensures that the
   * latest frame gets displayed while we wait for more data to be pushed.
   */
  void AdvanceBlockedInput(StreamTime aDuration) override;

  /**
   * Dispatched from MediaStreamGraph when it has run an iteration so we can
   * hand more data to the encoder.
   */
  void AdvanceCurrentTime(StreamTime aDuration) override;
protected:
  /**
   * Number of samples per channel in a pcm buffer. This is also the value of
   * frame size required by audio encoder, and listeners will be notified when
   * at least this much data has been added to mOutgoingBuffer.
   */
  virtual int GetPacketDuration() { return 0; }

  /**
   * Initializes the audio encoder. The call of this method is delayed until we
   * have received the first valid track from MediaStreamGraph.
   */
  virtual nsresult Init(int aChannels, int aSamplingRate) = 0;

  /**
   * The number of channels are used for processing PCM data in the audio encoder.
   * This value comes from the first valid audio chunk. If encoder can't support
   * the channels in the chunk, downmix PCM stream can be performed.
   * This value also be used to initialize the audio encoder.
   */
  int mChannels;

  /**
   * The sampling rate of source audio data.
   */
  int mSamplingRate;

  /**
   * A segment queue of incoming audio track data, from listeners.
   * The duration of mIncomingBuffer is strictly increasing as it gets fed more
   * data. Consumed data is replaced by null data.
   */
  AudioSegment mIncomingBuffer;

  /**
   * A segment queue of outgoing audio track data to the encoder.
   * The contents of mOutgoingBuffer will always be what has been consumed from
   * mIncomingBuffer (up to mCurrentTime) but not yet consumed by the encoder
   * sub class.
   */
  AudioSegment mOutgoingBuffer;

  uint32_t mAudioBitrate;

  // This may only be accessed on the MSG thread.
  // I.e., in the regular NotifyQueuedChanges for audio to avoid adding data
  // from that callback when the direct one is active.
  bool mDirectConnected;
};

enum class FrameDroppingMode {
  ALLOW, // Allowed to drop frames to keep up under load
  DISALLOW, // Must not drop any frames, even if it means we will OOM
};

class VideoTrackEncoder : public TrackEncoder
{
public:
  explicit VideoTrackEncoder(TrackRate aTrackRate, FrameDroppingMode aFrameDroppingMode);

  /**
   * Suspends encoding from aTime, i.e., all video frame with a timestamp
   * between aTime and the timestamp of the next Resume() will be dropped.
   */
  void Suspend(TimeStamp aTime) override;

  /**
   * Resumes encoding starting at aTime.
   */
  void Resume(TimeStamp aTime) override;

  /**
   * Appends source video frames to mIncomingBuffer. We only append the source
   * chunk if the image is different from mLastChunk's image. Called on the
   * MediaStreamGraph thread.
   */
  void AppendVideoSegment(VideoSegment&& aSegment);

  /**
   * Takes track data from the last time TakeTrackData ran until mCurrentTime
   * and moves it to aSegment.
   */
  void TakeTrackData(VideoSegment& aSegment);

  /**
   * Measure size of internal buffers.
   */
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) override;

  void SetBitrate(const uint32_t aBitrate) override
  {
    mVideoBitrate = aBitrate;
  }

  /**
   * Tries to initiate the VideoEncoder based on data in aSegment.
   * This can be re-called often, as it will exit early should we already be
   * initiated. mInitiated will only be set if there was enough data in
   * aSegment to infer metadata. If mInitiated gets set, listeners are notified.
   *
   * Failing to initiate the encoder for an accumulated aDuration of 30 seconds
   * is seen as an error and will cancel the current encoding.
   */
  void Init(const VideoSegment& aSegment, StreamTime aDuration);

  StreamTime SecondsToMediaTime(double aS) const
  {
    NS_ASSERTION(0 <= aS && aS <= TRACK_TICKS_MAX/TRACK_RATE_MAX,
                 "Bad seconds");
    return mTrackRate * aS;
  }

  void Cancel() override;

  /**
   * Notifies us that we have reached the end of the stream and no more data
   * will be appended to mIncomingBuffer.
   */
  void NotifyEndOfStream() override;

  void SetStartOffset(StreamTime aStartOffset) override;

  /**
   * Dispatched from MediaStreamGraph when it has run an iteration where the
   * input track of the track this TrackEncoder is associated with didn't have
   * any data.
   *
   * Since we use a direct listener for VideoSegments we miss periods of time
   * for which the source didn't have any data. This ensures that the latest
   * frame gets displayed while we wait for more data to be pushed.
   */
  void AdvanceBlockedInput(StreamTime aDuration) override;

  /**
   * Dispatched from MediaStreamGraph when it has run an iteration so we can
   * hand more data to the encoder.
   */
  void AdvanceCurrentTime(StreamTime aDuration) override;

  /**
   * Set desired keyframe interval defined in milliseconds.
   */
  void SetKeyFrameInterval(int32_t aKeyFrameInterval);

protected:
  /**
   * Initialize the video encoder. In order to collect the value of width and
   * height of source frames, this initialization is delayed until we have
   * received the first valid video frame from MediaStreamGraph.
   * Listeners will be notified after it has been successfully initialized.
   */
  virtual nsresult Init(int aWidth, int aHeight, int aDisplayWidth,
                        int aDisplayHeight) = 0;

  /**
   * The width of source video frame, ceiled if the source width is odd.
   */
  int mFrameWidth;

  /**
   * The height of source video frame, ceiled if the source height is odd.
   */
  int mFrameHeight;

  /**
   * The display width of source video frame.
   */
  int mDisplayWidth;

  /**
   * The display height of source video frame.
   */
  int mDisplayHeight;

  /**
   * The last unique frame and duration so far handled by NotifyAdvanceCurrentTime.
   * When a new frame is detected, mLastChunk is added to mOutgoingBuffer.
   */
  VideoChunk mLastChunk;

  /**
   * A segment queue of incoming video track data, from listeners.
   * The duration of mIncomingBuffer is strictly increasing as it gets fed more
   * data. Consumed data is replaced by null data.
   */
  VideoSegment mIncomingBuffer;

  /**
   * A segment queue of outgoing video track data to the encoder.
   * The contents of mOutgoingBuffer will always be what has been consumed from
   * mIncomingBuffer (up to mCurrentTime) but not yet consumed by the encoder
   * sub class. There won't be any null data at the beginning of mOutgoingBuffer
   * unless explicitly pushed by the producer.
   */
  VideoSegment mOutgoingBuffer;

  /**
   * The number of mTrackRate ticks we have passed to mOutgoingBuffer.
   */
  StreamTime mEncodedTicks;

  /**
   * The time of the first real video frame passed to mOutgoingBuffer (at t=0).
   *
   * Note that this time will progress during suspension, to make sure the
   * incoming frames stay in sync with the output.
   */
  TimeStamp mStartTime;

  /**
   * The time Suspend was called on the MediaRecorder, so we can calculate the
   * duration on the next Resume().
   */
  TimeStamp mSuspendTime;

  uint32_t mVideoBitrate;

  /**
   * ALLOW to drop frames under load.
   * DISALLOW to encode all frames, mainly for testing.
   */
  FrameDroppingMode mFrameDroppingMode;

  /**
   * The desired keyframe interval defined in milliseconds.
   */
  int32_t mKeyFrameInterval;
};

} // namespace mozilla

#endif
