/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrackEncoder_h_
#define TrackEncoder_h_

#include "mozilla/ReentrantMonitor.h"

#include "AudioSegment.h"
#include "EncodedFrameContainer.h"
#include "StreamBuffer.h"
#include "TrackMetadataBase.h"
#include "VideoSegment.h"

namespace mozilla {

class MediaStreamGraph;

/**
 * Base class of AudioTrackEncoder and VideoTrackEncoder. Lifetimes managed by
 * MediaEncoder. Most methods can only be called on the MediaEncoder's thread,
 * but some subclass methods can be called on other threads when noted.
 *
 * NotifyQueuedTrackChanges is called on subclasses of this class from the
 * MediaStreamGraph thread, and AppendAudioSegment/AppendVideoSegment is then
 * called to store media data in the TrackEncoder. Later on, GetEncodedTrack is
 * called on MediaEncoder's thread to encode and retrieve the encoded data.
 */
class TrackEncoder
{
public:
  TrackEncoder()
    : mReentrantMonitor("media.TrackEncoder")
    , mEncodingComplete(false)
    , mEosSetInEncoder(false)
    , mInitialized(false)
    , mEndOfStream(false)
    , mCanceled(false)
  {}

  virtual ~TrackEncoder() {}

  /**
   * Notified by the same callbcak of MediaEncoder when it has received a track
   * change from MediaStreamGraph. Called on the MediaStreamGraph thread.
   */
  virtual void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                        TrackRate aTrackRate,
                                        TrackTicks aTrackOffset,
                                        uint32_t aTrackEvents,
                                        const MediaSegment& aQueuedMedia) = 0;

  /**
   * Notified by the same callback of MediaEncoder when it has been removed from
   * MediaStreamGraph. Called on the MediaStreamGraph thread.
   */
  void NotifyRemoved(MediaStreamGraph* aGraph) { NotifyEndOfStream(); }

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
   * True if the track encoder has encoded all source segments coming from
   * MediaStreamGraph. Call on the worker thread.
   */
  bool IsEncodingComplete() { return mEncodingComplete; }

  /**
   * Notifies from MediaEncoder to cancel the encoding, and wakes up
   * mReentrantMonitor if encoder is waiting on it.
   */
  void NotifyCancel()
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mCanceled = true;
    mReentrantMonitor.NotifyAll();
  }

protected:
  /**
   * Notifies track encoder that we have reached the end of source stream, and
   * wakes up mReentrantMonitor if encoder is waiting for any source data.
   */
  virtual void NotifyEndOfStream() = 0;

  /**
   * A ReentrantMonitor to protect the pushing and pulling of mRawSegment which
   * is declared in its subclasses, and the following flags: mInitialized,
   * EndOfStream and mCanceled. The control of protection is managed by its
   * subclasses.
   */
  ReentrantMonitor mReentrantMonitor;

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
   * True if the track encoder has initialized successfully, protected by
   * mReentrantMonitor.
   */
  bool mInitialized;

  /**
   * True if the TrackEncoder has received an event of TRACK_EVENT_ENDED from
   * MediaStreamGraph, or the MediaEncoder is removed from its source stream,
   * protected by mReentrantMonitor.
   */
  bool mEndOfStream;

  /**
   * True if a cancellation of encoding is sent from MediaEncoder, protected by
   * mReentrantMonitor.
   */
  bool mCanceled;
};

class AudioTrackEncoder : public TrackEncoder
{
public:
  AudioTrackEncoder()
    : TrackEncoder()
    , mChannels(0)
    , mSamplingRate(0)
  {}

  void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                TrackRate aTrackRate,
                                TrackTicks aTrackOffset,
                                uint32_t aTrackEvents,
                                const MediaSegment& aQueuedMedia) MOZ_OVERRIDE;

protected:
  /**
   * Number of samples per channel in a pcm buffer. This is also the value of
   * frame size required by audio encoder, and mReentrantMonitor will be
   * notified when at least this much data has been added to mRawSegment.
   */
  virtual int GetPacketDuration() { return 0; }

  /**
   * Initializes the audio encoder. The call of this method is delayed until we
   * have received the first valid track from MediaStreamGraph, and the
   * mReentrantMonitor will be notified if other methods is waiting for encoder
   * to be completely initialized. This method is called on the MediaStreamGraph
   * thread.
   */
  virtual nsresult Init(int aChannels, int aSamplingRate) = 0;

  /**
   * Appends and consumes track data from aSegment, this method is called on
   * the MediaStreamGraph thread. mReentrantMonitor will be notified when at
   * least GetPacketDuration() data has been added to mRawSegment, wake up other
   * method which is waiting for more data from mRawSegment.
   */
  nsresult AppendAudioSegment(const AudioSegment& aSegment);

  /**
   * Notifies the audio encoder that we have reached the end of source stream,
   * and wakes up mReentrantMonitor if encoder is waiting for more track data.
   */
  virtual void NotifyEndOfStream() MOZ_OVERRIDE;

  /**
   * Interleaves the track data and stores the result into aOutput. Might need
   * to up-mix or down-mix the channel data if the channels number of this chunk
   * is different from mChannels. The channel data from aChunk might be modified
   * by up-mixing.
   */
  void InterleaveTrackData(AudioChunk& aChunk, int32_t aDuration,
                           uint32_t aOutputChannels, AudioDataValue* aOutput);

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
   * A segment queue of audio track data, protected by mReentrantMonitor.
   */
  AudioSegment mRawSegment;
};

class VideoTrackEncoder : public TrackEncoder
{
public:
  VideoTrackEncoder()
    : TrackEncoder()
    , mFrameWidth(0)
    , mFrameHeight(0)
    , mTrackRate(0)
    , mTotalFrameDuration(0)
  {}

  /**
   * Notified by the same callbcak of MediaEncoder when it has received a track
   * change from MediaStreamGraph. Called on the MediaStreamGraph thread.
   */
  void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                TrackRate aTrackRate,
                                TrackTicks aTrackOffset,
                                uint32_t aTrackEvents,
                                const MediaSegment& aQueuedMedia) MOZ_OVERRIDE;

protected:
  /**
   * Initialized the video encoder. In order to collect the value of width and
   * height of source frames, this initialization is delayed until we have
   * received the first valid video frame from MediaStreamGraph;
   * mReentrantMonitor will be notified after it has successfully initialized,
   * and this method is called on the MediaStramGraph thread.
   */
  virtual nsresult Init(int aWidth, int aHeight, TrackRate aTrackRate) = 0;

  /**
   * Appends source video frames to mRawSegment. We only append the source chunk
   * if it is unique to mLastChunk. Called on the MediaStreamGraph thread.
   */
  nsresult AppendVideoSegment(const VideoSegment& aSegment);

  /**
   * Tells the video track encoder that we've reached the end of source stream,
   * and wakes up mReentrantMonitor if encoder is waiting for more track data.
   * Called on the MediaStreamGraph thread.
   */
  virtual void NotifyEndOfStream() MOZ_OVERRIDE;

  /**
   * Create a buffer of black image in format of YUV:420. Called on the worker
   * thread.
   */
  void CreateMutedFrame(nsTArray<uint8_t>* aOutputBuffer);

  /**
   * The width of source video frame, ceiled if the source width is odd.
   */
  int mFrameWidth;

  /**
   * The height of source video frame, ceiled if the source height is odd.
   */
  int mFrameHeight;

  /**
   * The track rate of source video.
   */
  TrackRate mTrackRate;

  /**
   * The total duration of frames in encoded video in TrackTicks, kept track of
   * in subclasses.
   */
  TrackTicks mTotalFrameDuration;

  /**
   * The last unique frame we've sent to track encoder, kept track of in
   * subclasses.
   */
  VideoFrame mLastFrame;

  /**
   * A segment queue of audio track data, protected by mReentrantMonitor.
   */
  VideoSegment mRawSegment;
};

}
#endif
