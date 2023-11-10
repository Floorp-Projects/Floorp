/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrackEncoder_h_
#define TrackEncoder_h_

#include "AudioSegment.h"
#include "EncodedFrame.h"
#include "MediaQueue.h"
#include "MediaTrackGraph.h"
#include "TrackMetadataBase.h"
#include "VideoSegment.h"

namespace mozilla {

class AbstractThread;
class DriftCompensator;
class TrackEncoder;

class TrackEncoderListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TrackEncoderListener)

  /**
   * Called when the TrackEncoder has received its first real data.
   */
  virtual void Started(TrackEncoder* aEncoder) = 0;

  /**
   * Called when the TrackEncoder's underlying encoder has been successfully
   * initialized and there's non-null data ready to be encoded.
   */
  virtual void Initialized(TrackEncoder* aEncoder) = 0;

  /**
   * Called after the TrackEncoder hit an unexpected error, causing it to
   * abort operation.
   */
  virtual void Error(TrackEncoder* aEncoder) = 0;

 protected:
  virtual ~TrackEncoderListener() = default;
};

/**
 * Base class of AudioTrackEncoder and VideoTrackEncoder. Lifetime managed by
 * MediaEncoder. All methods are to be called only on the worker thread.
 *
 * The control APIs are all called by MediaEncoder on its dedicated thread. Data
 * is encoded as soon as it has been appended (and time has advanced past its
 * end in case of video) and pushed to mEncodedDataQueue.
 */
class TrackEncoder {
 public:
  TrackEncoder(TrackRate aTrackRate,
               MediaQueue<EncodedFrame>& aEncodedDataQueue);

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
   * Creates and sets up meta data for a specific codec, called on the worker
   * thread.
   */
  virtual already_AddRefed<TrackMetadataBase> GetMetadata() = 0;

  /**
   * MediaQueue containing encoded data, that is pushed as soon as it's ready.
   */
  MediaQueue<EncodedFrame>& EncodedDataQueue() { return mEncodedDataQueue; }

  /**
   * Returns true once this TrackEncoder is initialized.
   */
  bool IsInitialized();

  /**
   * Returns true once this TrackEncoder has received some data.
   */
  bool IsStarted();

  /**
   * True if the track encoder has encoded all source segments coming from
   * MediaTrackGraph. Call on the worker thread.
   */
  bool IsEncodingComplete() const;

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
  virtual ~TrackEncoder() { MOZ_ASSERT(mListeners.IsEmpty()); }

  /**
   * If this TrackEncoder was not already initialized, it is set to initialized
   * and listeners are notified.
   */
  void SetInitialized();

  /**
   * If this TrackEncoder was not already marked started, its started state is
   * set and listeners are notified.
   */
  void SetStarted();

  /**
   * Called after an error. Cancels the encoding and notifies listeners.
   */
  void OnError();

  /**
   * True if the track encoder has been initialized successfully.
   */
  bool mInitialized;

  /**
   * True if the track encoder has received data.
   */
  bool mStarted;

  /**
   * True once all data until the end of the input track has been received.
   */
  bool mEndOfStream;

  /**
   * True once this encoding has been cancelled.
   */
  bool mCanceled;

  // How many times we have tried to initialize the encoder.
  uint32_t mInitCounter;

  /**
   * True if this TrackEncoder is currently suspended.
   */
  bool mSuspended;

  /**
   * The track rate of source media.
   */
  const TrackRate mTrackRate;

  /**
   * If set we assert that all methods are called on this thread.
   */
  RefPtr<AbstractThread> mWorkerThread;

  /**
   * MediaQueue where encoded data ends up. Note that metadata goes out of band.
   */
  MediaQueue<EncodedFrame>& mEncodedDataQueue;

  nsTArray<RefPtr<TrackEncoderListener>> mListeners;
};

class AudioTrackEncoder : public TrackEncoder {
 public:
  AudioTrackEncoder(TrackRate aTrackRate,
                    MediaQueue<EncodedFrame>& aEncodedDataQueue)
      : TrackEncoder(aTrackRate, aEncodedDataQueue),
        mChannels(0),
        mNotInitDuration(0),
        mAudioBitrate(0) {}

  /**
   * Suspends encoding from now, i.e., all future audio data received through
   * AppendAudioSegment() until the next Resume() will be dropped.
   */
  void Suspend();

  /**
   * Resumes encoding starting now, i.e., data from the next
   * AppendAudioSegment() will get encoded.
   */
  void Resume();

  /**
   * Appends and consumes track data from aSegment.
   */
  void AppendAudioSegment(AudioSegment&& aSegment);

  template <typename T>
  static void InterleaveTrackData(nsTArray<const T*>& aInput, int32_t aDuration,
                                  uint32_t aOutputChannels,
                                  AudioDataValue* aOutput, float aVolume) {
    if (aInput.Length() < aOutputChannels) {
      // Up-mix. This might make the mChannelData have more than aChannels.
      AudioChannelsUpMix(&aInput, aOutputChannels,
                         SilentChannel::ZeroChannel<T>());
    }

    if (aInput.Length() > aOutputChannels) {
      DownmixAndInterleave<T>(aInput, aDuration, aVolume, aOutputChannels,
                              aOutput);
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

  void SetBitrate(const uint32_t aBitrate) override {
    mAudioBitrate = aBitrate;
  }

  /**
   * Tries to initiate the AudioEncoder based on data in aSegment.
   * This can be re-called often, as it will exit early should we already be
   * initiated. mInitiated will only be set if there was enough data in
   * aSegment to infer metadata. If mInitiated gets set, listeners are notified.
   *
   * Not having enough data in aSegment to initiate the encoder for an
   * accumulated aDuration of one second will make us initiate with a default
   * number of channels.
   *
   * If we attempt to initiate the underlying encoder but fail, we Cancel() and
   * notify listeners.
   */
  void TryInit(const AudioSegment& aSegment, TrackTime aDuration);

  void Cancel() override;

  /**
   * Dispatched from MediaTrackGraph when we have finished feeding data to
   * mOutgoingBuffer.
   */
  void NotifyEndOfStream() override;

 protected:
  /**
   * Number of samples per channel in a pcm buffer. This is also the value of
   * frame size required by audio encoder, and listeners will be notified when
   * at least this much data has been added to mOutgoingBuffer.
   */
  virtual int NumInputFramesPerPacket() const { return 0; }

  /**
   * Initializes the audio encoder. The call of this method is delayed until we
   * have received the first valid track from MediaTrackGraph.
   */
  virtual nsresult Init(int aChannels) = 0;

  /**
   * Encodes buffered data and pushes it to mEncodedDataQueue.
   */
  virtual nsresult Encode(AudioSegment* aSegment) = 0;

  /**
   * The number of channels are used for processing PCM data in the audio
   * encoder. This value comes from the first valid audio chunk. If encoder
   * can't support the channels in the chunk, downmix PCM stream can be
   * performed. This value also be used to initialize the audio encoder.
   */
  int mChannels;

  /**
   * A segment queue of outgoing audio track data to the encoder.
   * The contents of mOutgoingBuffer will always be what has been appended on
   * the encoder thread but not yet consumed by the encoder sub class.
   */
  AudioSegment mOutgoingBuffer;

  TrackTime mNotInitDuration;

  uint32_t mAudioBitrate;
};

enum class FrameDroppingMode {
  ALLOW,     // Allowed to drop frames to keep up under load
  DISALLOW,  // Must not drop any frames, even if it means we will OOM
};

class VideoTrackEncoder : public TrackEncoder {
 public:
  VideoTrackEncoder(RefPtr<DriftCompensator> aDriftCompensator,
                    TrackRate aTrackRate,
                    MediaQueue<EncodedFrame>& aEncodedDataQueue,
                    FrameDroppingMode aFrameDroppingMode);

  /**
   * Suspends encoding from aTime, i.e., all video frame with a timestamp
   * between aTime and the timestamp of the next Resume() will be dropped.
   */
  void Suspend(const TimeStamp& aTime);

  /**
   * Resumes encoding starting at aTime.
   */
  void Resume(const TimeStamp& aTime);

  /**
   * Makes the video black from aTime.
   */
  void Disable(const TimeStamp& aTime);

  /**
   * Makes the video non-black from aTime.
   *
   * NB that it could still be forced black for other reasons, like principals.
   */
  void Enable(const TimeStamp& aTime);

  /**
   * Appends source video frames to mIncomingBuffer. We only append the source
   * chunk if the image is different from mLastChunk's image. Called on the
   * MediaTrackGraph thread.
   */
  void AppendVideoSegment(VideoSegment&& aSegment);

  /**
   * Measure size of internal buffers.
   */
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) override;

  void SetBitrate(const uint32_t aBitrate) override {
    mVideoBitrate = aBitrate;
  }

  /**
   * Tries to initiate the VideoEncoder based on data in aSegment.
   * This can be re-called often, as it will exit early should we already be
   * initiated. mInitiated will only be set if there was enough data in
   * aSegment to infer metadata. If mInitiated gets set, listeners are notified.
   * The amount of chunks needed can be controlled by
   * aFrameRateDetectionMinChunks which denotes the minimum number of chunks
   * needed to infer the framerate.
   *
   * Failing to initiate the encoder for an accumulated aDuration of 30 seconds
   * is seen as an error and will cancel the current encoding.
   */
  void Init(const VideoSegment& aSegment, const TimeStamp& aTime,
            size_t aFrameRateDetectionMinChunks);

  TrackTime SecondsToMediaTime(double aS) const {
    NS_ASSERTION(0 <= aS && aS <= TRACK_TICKS_MAX / TRACK_RATE_MAX,
                 "Bad seconds");
    return mTrackRate * aS;
  }

  /**
   * MediaTrackGraph notifies us about the time of the track's start.
   * This gets called on the MediaEncoder thread after a dispatch.
   */
  void SetStartOffset(const TimeStamp& aStartOffset);

  void Cancel() override;

  /**
   * Notifies us that we have reached the end of the stream and no more data
   * will be appended to mIncomingBuffer.
   */
  void NotifyEndOfStream() override;

  /**
   * Dispatched from MediaTrackGraph when it has run an iteration so we can
   * hand more data to the encoder.
   */
  void AdvanceCurrentTime(const TimeStamp& aTime);

 protected:
  /**
   * Initialize the video encoder. In order to collect the value of width and
   * height of source frames, this initialization is delayed until we have
   * received the first valid video frame from MediaTrackGraph.
   * Listeners will be notified after it has been successfully initialized.
   */
  virtual nsresult Init(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                        int32_t aDisplayHeight, float aEstimatedFrameRate) = 0;

  /**
   * Encodes data in the outgoing buffer and pushes it to mEncodedDataQueue.
   */
  virtual nsresult Encode(VideoSegment* aSegment) = 0;

  /**
   * Drift compensator for re-clocking incoming video frame wall-clock
   * timestamps to audio time.
   */
  const RefPtr<DriftCompensator> mDriftCompensator;

  /**
   * The last unique frame and duration so far handled by
   * NotifyAdvanceCurrentTime. When a new frame is detected, mLastChunk is added
   * to mOutgoingBuffer.
   */
  VideoChunk mLastChunk;

  /**
   * A segment queue of incoming video track data, from listeners.
   * The duration of mIncomingBuffer is irrelevant as we only look at TimeStamps
   * of frames. Consumed data is replaced by null data.
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
  TrackTime mEncodedTicks;

  /**
   * The time up to which we have forwarded data from mIncomingBuffer to
   * mOutgoingBuffer.
   */
  TimeStamp mCurrentTime;

  /**
   * The time the video track started, so the start of the video track can be
   * synced to the start of the audio track.
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
   * True if the video MediaTrackTrack this VideoTrackEncoder is attached to is
   * currently enabled. While false, we encode all frames as black.
   */
  bool mEnabled;
};

}  // namespace mozilla

#endif
