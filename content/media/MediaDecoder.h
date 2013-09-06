/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
Each video element based on MediaDecoder has a state machine to manage
its play state and keep the current frame up to date. All state machines
share time in a single shared thread. Each decoder also has one thread
dedicated to decoding audio and video data. This thread is shutdown when
playback is paused. Each decoder also has a thread to push decoded audio
to the hardware. This thread is not created until playback starts, but
currently is not destroyed when paused, only when playback ends.

The decoder owns the resources for downloading the media file, and the
high level state. It holds an owning reference to the state machine that
owns all the resources related to decoding data, and manages the low level
decoding operations and A/V sync.

Each state machine runs on the shared state machine thread. Every time some
action is required for a state machine, it is scheduled to run on the shared
the state machine thread. The state machine runs one "cycle" on the state
machine thread, and then returns. If necessary, it will schedule itself to
run again in future. While running this cycle, it must not block the
thread, as other state machines' events may need to run. State shared
between a state machine's threads is synchronised via the monitor owned
by its MediaDecoder object.

The Main thread controls the decode state machine by setting the value
of a mPlayState variable and notifying on the monitor based on the
high level player actions required (Seek, Pause, Play, etc).

The player states are the states requested by the client through the
DOM API.  They represent the desired state of the player, while the
decoder's state represents the actual state of the decoder.

The high level state of the player is maintained via a PlayState value.
It can have the following states:

START
  The decoder has been initialized but has no resource loaded.
PAUSED
  A request via the API has been received to pause playback.
LOADING
  A request via the API has been received to load a resource.
PLAYING
  A request via the API has been received to start playback.
SEEKING
  A request via the API has been received to start seeking.
COMPLETED
  Playback has completed.
SHUTDOWN
  The decoder is about to be destroyed.

State transition occurs when the Media Element calls the Play, Seek,
etc methods on the MediaDecoder object. When the transition
occurs MediaDecoder then calls the methods on the decoder state
machine object to cause it to behave as required by the play state.
State transitions will likely schedule the state machine to run to
affect the change.

An implementation of the MediaDecoderStateMachine class is the event
that gets dispatched to the state machine thread. Each time the event is run,
the state machine must cycle the state machine once, and then return.

The state machine has the following states:

DECODING_METADATA
  The media headers are being loaded, and things like framerate, etc are
  being determined, and the first frame of audio/video data is being decoded.
DECODING
  The decode has started. If the PlayState is PLAYING, the decode thread
  should be alive and decoding video and audio frame, the audio thread
  should be playing audio, and the state machine should run periodically
  to update the video frames being displayed.
SEEKING
  A seek operation is in progress. The decode thread should be seeking.
BUFFERING
  Decoding is paused while data is buffered for smooth playback. If playback
  is paused (PlayState transitions to PAUSED) we'll destory the decode thread.
COMPLETED
  The resource has completed decoding, but possibly not finished playback.
  The decode thread will be destroyed. Once playback finished, the audio
  thread will also be destroyed.
SHUTDOWN
  The decoder object and its state machine are about to be destroyed.
  Once the last state machine has been destroyed, the shared state machine
  thread will also be destroyed. It will be recreated later if needed.

The following result in state transitions.

Shutdown()
  Clean up any resources the MediaDecoderStateMachine owns.
Play()
  Start decoding and playback of media data.
Buffer
  This is not user initiated. It occurs when the
  available data in the stream drops below a certain point.
Complete
  This is not user initiated. It occurs when the
  stream is completely decoded.
Seek(double)
  Seek to the time position given in the resource.

A state transition diagram:

DECODING_METADATA
  |      |
  v      | Shutdown()
  |      |
  v      -->-------------------->--------------------------|
  |---------------->----->------------------------|        v
DECODING             |          |  |              |        |
  ^                  v Seek(t)  |  |              |        |
  |         Play()   |          v  |              |        |
  ^-----------<----SEEKING      |  v Complete     v        v
  |                  |          |  |              |        |
  |                  |          |  COMPLETED    SHUTDOWN-<-|
  ^                  ^          |  |Shutdown()    |
  |                  |          |  >-------->-----^
  |          Play()  |Seek(t)   |Buffer()         |
  -----------<--------<-------BUFFERING           |
                                |                 ^
                                v Shutdown()      |
                                |                 |
                                ------------>-----|

The following represents the states that the MediaDecoder object
can be in, and the valid states the MediaDecoderStateMachine can be in at that
time:

player LOADING   decoder DECODING_METADATA
player PLAYING   decoder DECODING, BUFFERING, SEEKING, COMPLETED
player PAUSED    decoder DECODING, BUFFERING, SEEKING, COMPLETED
player SEEKING   decoder SEEKING
player COMPLETED decoder SHUTDOWN
player SHUTDOWN  decoder SHUTDOWN

The general sequence of events is:

1) The video element calls Load on MediaDecoder. This creates the
   state machine and starts the channel for downloading the
   file. It instantiates and schedules the MediaDecoderStateMachine. The
   high level LOADING state is entered, which results in the decode
   thread being created and starting to decode metadata. These are
   the headers that give the video size, framerate, etc. Load() returns
   immediately to the calling video element.

2) When the metadata has been loaded by the decode thread, the state machine
   will call a method on the video element object to inform it that this
   step is done, so it can do the things required by the video specification
   at this stage. The decode thread then continues to decode the first frame
   of data.

3) When the first frame of data has been successfully decoded the state
   machine calls a method on the video element object to inform it that
   this step has been done, once again so it can do the required things
   by the video specification at this stage.

   This results in the high level state changing to PLAYING or PAUSED
   depending on any user action that may have occurred.

   While the play state is PLAYING, the decode thread will decode
   data, and the audio thread will push audio data to the hardware to
   be played. The state machine will run periodically on the shared
   state machine thread to ensure video frames are played at the
   correct time; i.e. the state machine manages A/V sync.

The Shutdown method on MediaDecoder closes the download channel, and
signals to the state machine that it should shutdown. The state machine
shuts down asynchronously, and will release the owning reference to the
state machine once its threads are shutdown.

The owning object of a MediaDecoder object *MUST* call Shutdown when
destroying the MediaDecoder object.

*/
#if !defined(MediaDecoder_h_)
#define MediaDecoder_h_

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"
#include "MediaResource.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/TimeStamp.h"
#include "MediaStreamGraph.h"
#include "AudioChannelCommon.h"
#include "AbstractMediaDecoder.h"

class nsIStreamListener;
class nsIMemoryReporter;
class nsIPrincipal;
class nsITimer;

namespace mozilla {
namespace dom {
class TimeRanges;
}
}

using namespace mozilla::dom;

namespace mozilla {
namespace layers {
class Image;
} //namespace layers

class VideoFrameContainer;
class MediaDecoderStateMachine;
class MediaDecoderOwner;

// The size to use for audio data frames in MozAudioAvailable events.
// This value is per channel, and is chosen to give ~43 fps of events,
// for example, 44100 with 2 channels, 2*1024 = 2048.
static const uint32_t FRAMEBUFFER_LENGTH_PER_CHANNEL = 1024;

// The total size of the framebuffer used for MozAudioAvailable events
// has to be within the following range.
static const uint32_t FRAMEBUFFER_LENGTH_MIN = 512;
static const uint32_t FRAMEBUFFER_LENGTH_MAX = 16384;

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaDecoder::GetCurrentTime implementation.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

class MediaDecoder : public nsIObserver,
                     public AbstractMediaDecoder
{
public:
  typedef mozilla::layers::Image Image;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Enumeration for the valid play states (see mPlayState)
  enum PlayState {
    PLAY_STATE_START,
    PLAY_STATE_LOADING,
    PLAY_STATE_PAUSED,
    PLAY_STATE_PLAYING,
    PLAY_STATE_SEEKING,
    PLAY_STATE_ENDED,
    PLAY_STATE_SHUTDOWN
  };

  MediaDecoder();
  virtual ~MediaDecoder();

  // Create a new decoder of the same type as this one.
  // Subclasses must implement this.
  virtual MediaDecoder* Clone() = 0;
  // Create a new state machine to run this decoder.
  // Subclasses must implement this.
  virtual MediaDecoderStateMachine* CreateStateMachine() = 0;

  // Call on the main thread only.
  // Perform any initialization required for the decoder.
  // Return true on successful initialisation, false
  // on failure.
  virtual bool Init(MediaDecoderOwner* aOwner);

  // Cleanup internal data structures. Must be called on the main
  // thread by the owning object before that object disposes of this object.
  virtual void Shutdown();

  // Start downloading the media. Decode the downloaded data up to the
  // point of the first frame of data.
  // This is called at most once per decoder, after Init().
  virtual nsresult Load(nsIStreamListener** aListener,
                        MediaDecoder* aCloneDonor);

  // Called in |Load| to open mResource.
  nsresult OpenResource(nsIStreamListener** aStreamListener);

  // Called when the video file has completed downloading.
  virtual void ResourceLoaded();

  // Called if the media file encounters a network error.
  virtual void NetworkError();

  // Get the current MediaResource being used. Its URI will be returned
  // by currentSrc. Returns what was passed to Load(), if Load() has been called.
  // Note: The MediaResource is refcounted, but it outlives the MediaDecoder,
  // so it's OK to use the reference returned by this function without
  // refcounting, *unless* you need to store and use the reference after the
  // MediaDecoder has been destroyed. You might need to do this if you're
  // wrapping the MediaResource in some kind of byte stream interface to be
  // passed to a platform decoder.
  MediaResource* GetResource() const MOZ_FINAL MOZ_OVERRIDE
  {
    return mResource;
  }
  void SetResource(MediaResource* aResource)
  {
    NS_ASSERTION(NS_IsMainThread(), "Should only be called on main thread");
    mResource = aResource;
  }

  // Return the principal of the current URI being played or downloaded.
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();

  // Return the time position in the video stream being
  // played measured in seconds.
  virtual double GetCurrentTime();

  // Seek to the time position in (seconds) from the start of the video.
  virtual nsresult Seek(double aTime);

  // Enables decoders to supply an enclosing byte range for a seek offset.
  // E.g. used by ChannelMediaResource to download a whole cluster for
  // DASH-WebM.
  virtual nsresult GetByteRangeForSeek(int64_t const aOffset,
                                       MediaByteRange &aByteRange) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Initialize state machine and schedule it.
  nsresult InitializeStateMachine(MediaDecoder* aCloneDonor);

  // Start playback of a video. 'Load' must have previously been
  // called.
  virtual nsresult Play();

  // Set/Unset dormant state if necessary.
  // Dormant state is a state to free all scarce media resources
  //  (like hw video codec), did not decoding and stay dormant.
  // It is used to share scarece media resources in system.
  virtual void SetDormantIfNecessary(bool aDormant);

  // Pause video playback.
  virtual void Pause();
  // Adjust the speed of the playback, optionally with pitch correction,
  virtual void SetVolume(double aVolume);
  // Sets whether audio is being captured. If it is, we won't play any
  // of our audio.
  virtual void SetAudioCaptured(bool aCaptured);

  void SetPlaybackRate(double aPlaybackRate);
  void SetPreservesPitch(bool aPreservesPitch);

  // All MediaStream-related data is protected by mReentrantMonitor.
  // We have at most one DecodedStreamData per MediaDecoder. Its stream
  // is used as the input for each ProcessedMediaStream created by calls to
  // captureStream(UntilEnded). Seeking creates a new source stream, as does
  // replaying after the input as ended. In the latter case, the new source is
  // not connected to streams created by captureStreamUntilEnded.

  struct DecodedStreamData MOZ_FINAL : public MainThreadMediaStreamListener {
    DecodedStreamData(MediaDecoder* aDecoder,
                      int64_t aInitialTime, SourceMediaStream* aStream);
    ~DecodedStreamData();

    // The following group of fields are protected by the decoder's monitor
    // and can be read or written on any thread.
    int64_t mLastAudioPacketTime; // microseconds
    int64_t mLastAudioPacketEndTime; // microseconds
    // Count of audio frames written to the stream
    int64_t mAudioFramesWritten;
    // Saved value of aInitialTime. Timestamp of the first audio and/or
    // video packet written.
    int64_t mInitialTime; // microseconds
    // mNextVideoTime is the end timestamp for the last packet sent to the stream.
    // Therefore video packets starting at or after this time need to be copied
    // to the output stream.
    int64_t mNextVideoTime; // microseconds
    MediaDecoder* mDecoder;
    // The last video image sent to the stream. Useful if we need to replicate
    // the image.
    nsRefPtr<Image> mLastVideoImage;
    gfxIntSize mLastVideoImageDisplaySize;
    // This is set to true when the stream is initialized (audio and
    // video tracks added).
    bool mStreamInitialized;
    bool mHaveSentFinish;
    bool mHaveSentFinishAudio;
    bool mHaveSentFinishVideo;

    // The decoder is responsible for calling Destroy() on this stream.
    // Can be read from any thread.
    const nsRefPtr<SourceMediaStream> mStream;
    // True when we've explicitly blocked this stream because we're
    // not in PLAY_STATE_PLAYING. Used on the main thread only.
    bool mHaveBlockedForPlayState;

    virtual void NotifyMainThreadStateChanged() MOZ_OVERRIDE;
  };
  struct OutputStreamData {
    void Init(ProcessedMediaStream* aStream, bool aFinishWhenEnded)
    {
      mStream = aStream;
      mFinishWhenEnded = aFinishWhenEnded;
    }
    nsRefPtr<ProcessedMediaStream> mStream;
    // mPort connects mDecodedStream->mStream to our mStream.
    nsRefPtr<MediaInputPort> mPort;
    bool mFinishWhenEnded;
  };
  /**
   * Connects mDecodedStream->mStream to aStream->mStream.
   */
  void ConnectDecodedStreamToOutputStream(OutputStreamData* aStream);
  /**
   * Disconnects mDecodedStream->mStream from all outputs and clears
   * mDecodedStream.
   */
  void DestroyDecodedStream();
  /**
   * Recreates mDecodedStream. Call this to create mDecodedStream at first,
   * and when seeking, to ensure a new stream is set up with fresh buffers.
   * aStartTimeUSecs is relative to the state machine's mStartTime.
   */
  void RecreateDecodedStream(int64_t aStartTimeUSecs);
  /**
   * Called when the state of mDecodedStream as visible on the main thread
   * has changed. In particular we want to know when the stream has finished
   * so we can call PlaybackEnded.
   */
  void NotifyDecodedStreamMainThreadStateChanged();
  nsTArray<OutputStreamData>& OutputStreams()
  {
    GetReentrantMonitor().AssertCurrentThreadIn();
    return mOutputStreams;
  }
  DecodedStreamData* GetDecodedStream()
  {
    GetReentrantMonitor().AssertCurrentThreadIn();
    return mDecodedStream;
  }

  // Add an output stream. All decoder output will be sent to the stream.
  // The stream is initially blocked. The decoder is responsible for unblocking
  // it while it is playing back.
  virtual void AddOutputStream(ProcessedMediaStream* aStream, bool aFinishWhenEnded);

  // Return the duration of the video in seconds.
  virtual double GetDuration();

  // Return the duration of the video in seconds.
  int64_t GetMediaDuration() MOZ_FINAL MOZ_OVERRIDE;

  // A media stream is assumed to be infinite if the metadata doesn't
  // contain the duration, and range requests are not supported, and
  // no headers give a hint of a possible duration (Content-Length,
  // Content-Duration, and variants), and we cannot seek in the media
  // stream to determine the duration.
  //
  // When the media stream ends, we can know the duration, thus the stream is
  // no longer considered to be infinite.
  virtual void SetInfinite(bool aInfinite);

  // Return true if the stream is infinite (see SetInfinite).
  virtual bool IsInfinite();

  // Called by MediaResource when the "cache suspended" status changes.
  // If MediaResource::IsSuspendedByCache returns true, then the decoder
  // should stop buffering or otherwise waiting for download progress and
  // start consuming data, if possible, because the cache is full.
  virtual void NotifySuspendedStatusChanged();

  // Called by MediaResource when some data has been received.
  // Call on the main thread only.
  virtual void NotifyBytesDownloaded();

  // Called by nsChannelToPipeListener or MediaResource when the
  // download has ended. Called on the main thread only. aStatus is
  // the result from OnStopRequest.
  virtual void NotifyDownloadEnded(nsresult aStatus);

  // Called as data arrives on the stream and is read into the cache.  Called
  // on the main thread only.
  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset);

  // Called by MediaResource when the principal of the resource has
  // changed. Called on main thread only.
  virtual void NotifyPrincipalChanged();

  // Called by the decode thread to keep track of the number of bytes read
  // from the resource.
  void NotifyBytesConsumed(int64_t aBytes) MOZ_FINAL MOZ_OVERRIDE;

  int64_t GetEndMediaTime() const MOZ_FINAL MOZ_OVERRIDE;

  // Return true if we are currently seeking in the media resource.
  // Call on the main thread only.
  virtual bool IsSeeking() const;

  // Return true if the decoder has reached the end of playback.
  // Call on the main thread only.
  virtual bool IsEnded() const;

  // Set the duration of the media resource in units of seconds.
  // This is called via a channel listener if it can pick up the duration
  // from a content header. Must be called from the main thread only.
  virtual void SetDuration(double aDuration);

  void SetMediaDuration(int64_t aDuration) MOZ_OVERRIDE;
  void UpdateMediaDuration(int64_t aDuration) MOZ_OVERRIDE;

  // Set a flag indicating whether seeking is supported
  virtual void SetMediaSeekable(bool aMediaSeekable) MOZ_OVERRIDE;
  virtual void SetTransportSeekable(bool aTransportSeekable) MOZ_FINAL MOZ_OVERRIDE;
  // Returns true if this media supports seeking. False for example for WebM
  // files without an index and chained ogg files.
  virtual bool IsMediaSeekable() MOZ_FINAL MOZ_OVERRIDE;
  // Returns true if seeking is supported on a transport level (e.g. the server
  // supports range requests, we are playing a file, etc.).
  virtual bool IsTransportSeekable();

  // Return the time ranges that can be seeked into.
  virtual nsresult GetSeekable(TimeRanges* aSeekable);

  // Set the end time of the media resource. When playback reaches
  // this point the media pauses. aTime is in seconds.
  virtual void SetFragmentEndTime(double aTime);

  // Set the end time of the media. aTime is in microseconds.
  void SetMediaEndTime(int64_t aTime) MOZ_FINAL MOZ_OVERRIDE;

  // Invalidate the frame.
  void Invalidate();

  // Suspend any media downloads that are in progress. Called by the
  // media element when it is sent to the bfcache, or when we need
  // to throttle the download. Call on the main thread only. This can
  // be called multiple times, there's an internal "suspend count".
  virtual void Suspend();

  // Resume any media downloads that have been suspended. Called by the
  // media element when it is restored from the bfcache, or when we need
  // to stop throttling the download. Call on the main thread only.
  // The download will only actually resume once as many Resume calls
  // have been made as Suspend calls. When aForceBuffering is true,
  // we force the decoder to go into buffering state before resuming
  // playback.
  virtual void Resume(bool aForceBuffering);

  // Moves any existing channel loads into the background, so that they don't
  // block the load event. This is called when we stop delaying the load
  // event. Any new loads initiated (for example to seek) will also be in the
  // background. Implementations of this must call MoveLoadsToBackground() on
  // their MediaResource.
  virtual void MoveLoadsToBackground();

  // Returns a weak reference to the media decoder owner.
  mozilla::MediaDecoderOwner* GetMediaOwner() const;

  // Returns the current size of the framebuffer used in
  // MozAudioAvailable events.
  uint32_t GetFrameBufferLength() { return mFrameBufferLength; }

  void AudioAvailable(float* aFrameBuffer, uint32_t aFrameBufferLength, float aTime);

  // Called by the state machine to notify the decoder that the duration
  // has changed.
  void DurationChanged();

  bool OnStateMachineThread() const MOZ_OVERRIDE;

  bool OnDecodeThread() const MOZ_OVERRIDE;

  // Returns the monitor for other threads to synchronise access to
  // state.
  ReentrantMonitor& GetReentrantMonitor() MOZ_OVERRIDE;

  // Returns true if the decoder is shut down
  bool IsShutdown() const MOZ_FINAL MOZ_OVERRIDE;

  // Constructs the time ranges representing what segments of the media
  // are buffered and playable.
  virtual nsresult GetBuffered(TimeRanges* aBuffered);

  // Returns the size, in bytes, of the heap memory used by the currently
  // queued decoded video and audio data.
  virtual int64_t VideoQueueMemoryInUse();
  virtual int64_t AudioQueueMemoryInUse();

  VideoFrameContainer* GetVideoFrameContainer() MOZ_FINAL MOZ_OVERRIDE
  {
    return mVideoFrameContainer;
  }
  mozilla::layers::ImageContainer* GetImageContainer() MOZ_OVERRIDE;

  // Sets the length of the framebuffer used in MozAudioAvailable events.
  // The new size must be between 512 and 16384.
  virtual nsresult RequestFrameBufferLength(uint32_t aLength);

  // Return the current state. Can be called on any thread. If called from
  // a non-main thread, the decoder monitor must be held.
  PlayState GetState() {
    return mPlayState;
  }

  // Fire progress events if needed according to the time and byte
  // constraints outlined in the specification. aTimer is true
  // if the method is called as a result of the progress timer rather
  // than the result of downloaded data.
  void Progress(bool aTimer);

  // Fire timeupdate events if needed according to the time constraints
  // outlined in the specification.
  void FireTimeUpdate();

  // Stop updating the bytes downloaded for progress notifications. Called
  // when seeking to prevent wild changes to the progress notification.
  // Must be called with the decoder monitor held.
  virtual void StopProgressUpdates();

  // Allow updating the bytes downloaded for progress notifications. Must
  // be called with the decoder monitor held.
  virtual void StartProgressUpdates();

  // Something has changed that could affect the computed playback rate,
  // so recompute it. The monitor must be held.
  virtual void UpdatePlaybackRate();

  // Used to estimate rates of data passing through the decoder's channel.
  // Records activity stopping on the channel. The monitor must be held.
  virtual void NotifyPlaybackStarted() {
    GetReentrantMonitor().AssertCurrentThreadIn();
    mPlaybackStatistics.Start();
  }

  // Used to estimate rates of data passing through the decoder's channel.
  // Records activity stopping on the channel. The monitor must be held.
  virtual void NotifyPlaybackStopped() {
    GetReentrantMonitor().AssertCurrentThreadIn();
    mPlaybackStatistics.Stop();
  }

  // The actual playback rate computation. The monitor must be held.
  virtual double ComputePlaybackRate(bool* aReliable);

  // Return true when the media is same-origin with the element. The monitor
  // must be held.
  bool IsSameOriginMedia();

  // Returns true if we can play the entire media through without stopping
  // to buffer, given the current download and playback rates.
  bool CanPlayThrough();

  // Make the decoder state machine update the playback position. Called by
  // the reader on the decoder thread (Assertions for this checked by
  // mDecoderStateMachine). This must be called with the decode monitor
  // held.
  void UpdatePlaybackPosition(int64_t aTime) MOZ_FINAL MOZ_OVERRIDE;

  void SetAudioChannelType(AudioChannelType aType) { mAudioChannelType = aType; }
  AudioChannelType GetAudioChannelType() { return mAudioChannelType; }

  // Send a new set of metadata to the state machine, to be dispatched to the
  // main thread to be presented when the |currentTime| of the media is greater
  // or equal to aPublishTime.
  void QueueMetadata(int64_t aPublishTime,
                     int aChannels,
                     int aRate,
                     bool aHasAudio,
                     bool aHasVideo,
                     MetadataTags* aTags);

  /******
   * The following methods must only be called on the main
   * thread.
   ******/

  // Change to a new play state. This updates the mState variable and
  // notifies any thread blocking on this object's monitor of the
  // change. Call on the main thread only.
  void ChangeState(PlayState aState);

  // May be called by the reader to notify this decoder that the metadata from
  // the media file has been read. Call on the decode thread only.
  void OnReadMetadataCompleted() MOZ_OVERRIDE { }

  // Called when the metadata from the media file has been loaded by the
  // state machine. Call on the main thread only.
  void MetadataLoaded(int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags);

  // Called when the first frame has been loaded.
  // Call on the main thread only.
  void FirstFrameLoaded();

  // Returns true if the resource has been loaded. Acquires the monitor.
  // Call from any thread.
  virtual bool IsDataCachedToEndOfResource();

  // Called when the video has completed playing.
  // Call on the main thread only.
  void PlaybackEnded();

  // Seeking has stopped. Inform the element on the main
  // thread.
  void SeekingStopped();

  // Seeking has stopped at the end of the resource. Inform the element on the main
  // thread.
  void SeekingStoppedAtEnd();

  // Seeking has started. Inform the element on the main
  // thread.
  void SeekingStarted();

  // Called when the backend has changed the current playback
  // position. It dispatches a timeupdate event and invalidates the frame.
  // This must be called on the main thread only.
  void PlaybackPositionChanged();

  // Calls mElement->UpdateReadyStateForData, telling it whether we have
  // data for the next frame and if we're buffering. Main thread only.
  void UpdateReadyStateForData();

  // Find the end of the cached data starting at the current decoder
  // position.
  int64_t GetDownloadPosition();

  // Updates the approximate byte offset which playback has reached. This is
  // used to calculate the readyState transitions.
  void UpdatePlaybackOffset(int64_t aOffset);

  // Provide access to the state machine object
  MediaDecoderStateMachine* GetStateMachine() const;

  // Drop reference to state machine.  Only called during shutdown dance.
  virtual void ReleaseStateMachine();

  // Called when a "MozAudioAvailable" event listener is added. This enables
  // the decoder to only dispatch "MozAudioAvailable" events when a
  // handler exists, reducing overhead. Called on the main thread.
  virtual void NotifyAudioAvailableListener();

  // Notifies the element that decoding has failed.
  virtual void DecodeError();

  // Indicate whether the media is same-origin with the element.
  void UpdateSameOriginStatus(bool aSameOrigin);

  MediaDecoderOwner* GetOwner() MOZ_OVERRIDE;

#ifdef MOZ_RAW
  static bool IsRawEnabled();
#endif

#ifdef MOZ_OGG
  static bool IsOggEnabled();
  static bool IsOpusEnabled();
#endif

#ifdef MOZ_WAVE
  static bool IsWaveEnabled();
#endif

#ifdef MOZ_WEBM
  static bool IsWebMEnabled();
#endif

#ifdef MOZ_GSTREAMER
  static bool IsGStreamerEnabled();
#endif

#ifdef MOZ_OMX_DECODER
  static bool IsOmxEnabled();
#endif

#ifdef MOZ_MEDIA_PLUGINS
  static bool IsMediaPluginsEnabled();
#endif

#ifdef MOZ_DASH
  static bool IsDASHEnabled();
#endif

#ifdef MOZ_WMF
  static bool IsWMFEnabled();
#endif

  // Schedules the state machine to run one cycle on the shared state
  // machine thread. Main thread only.
  nsresult ScheduleStateMachineThread();

  struct Statistics {
    // Estimate of the current playback rate (bytes/second).
    double mPlaybackRate;
    // Estimate of the current download rate (bytes/second). This
    // ignores time that the channel was paused by Gecko.
    double mDownloadRate;
    // Total length of media stream in bytes; -1 if not known
    int64_t mTotalBytes;
    // Current position of the download, in bytes. This is the offset of
    // the first uncached byte after the decoder position.
    int64_t mDownloadPosition;
    // Current position of decoding, in bytes (how much of the stream
    // has been consumed)
    int64_t mDecoderPosition;
    // Current position of playback, in bytes
    int64_t mPlaybackPosition;
    // If false, then mDownloadRate cannot be considered a reliable
    // estimate (probably because the download has only been running
    // a short time).
    bool mDownloadRateReliable;
    // If false, then mPlaybackRate cannot be considered a reliable
    // estimate (probably because playback has only been running
    // a short time).
    bool mPlaybackRateReliable;
  };

  // Return statistics. This is used for progress events and other things.
  // This can be called from any thread. It's only a snapshot of the
  // current state, since other threads might be changing the state
  // at any time.
  virtual Statistics GetStatistics();

  // Frame decoding/painting related performance counters.
  // Threadsafe.
  class FrameStatistics {
  public:

    FrameStatistics() :
        mReentrantMonitor("MediaDecoder::FrameStats"),
        mTotalFrameDelay(0.0),
        mParsedFrames(0),
        mDecodedFrames(0),
        mPresentedFrames(0) {}

    // Returns number of frames which have been parsed from the media.
    // Can be called on any thread.
    uint32_t GetParsedFrames() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mParsedFrames;
    }

    // Returns the number of parsed frames which have been decoded.
    // Can be called on any thread.
    uint32_t GetDecodedFrames() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mDecodedFrames;
    }

    // Returns the number of decoded frames which have been sent to the rendering
    // pipeline for painting ("presented").
    // Can be called on any thread.
    uint32_t GetPresentedFrames() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mPresentedFrames;
    }

    double GetTotalFrameDelay() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mTotalFrameDelay;
    }

    // Increments the parsed and decoded frame counters by the passed in counts.
    // Can be called on any thread.
    void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) {
      if (aParsed == 0 && aDecoded == 0)
        return;
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      mParsedFrames += aParsed;
      mDecodedFrames += aDecoded;
    }

    // Increments the presented frame counters.
    // Can be called on any thread.
    void NotifyPresentedFrame() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      ++mPresentedFrames;
    }

    // Tracks the sum of display delay.
    // Can be called on any thread.
    void NotifyFrameDelay(double aFrameDelay) {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      mTotalFrameDelay += aFrameDelay;
    }

  private:

    // ReentrantMonitor to protect access of playback statistics.
    ReentrantMonitor mReentrantMonitor;

    // Sum of displayed frame delays.
    // Access protected by mReentrantMonitor.
    double mTotalFrameDelay;

    // Number of frames parsed and demuxed from media.
    // Access protected by mReentrantMonitor.
    uint32_t mParsedFrames;

    // Number of parsed frames which were actually decoded.
    // Access protected by mReentrantMonitor.
    uint32_t mDecodedFrames;

    // Number of decoded frames which were actually sent down the rendering
    // pipeline to be painted ("presented"). Access protected by mReentrantMonitor.
    uint32_t mPresentedFrames;
  };

  // Return the frame decode/paint related statistics.
  FrameStatistics& GetFrameStatistics() { return mFrameStats; }

  // Increments the parsed and decoded frame counters by the passed in counts.
  // Can be called on any thread.
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) MOZ_OVERRIDE
  {
    GetFrameStatistics().NotifyDecodedFrames(aParsed, aDecoded);
  }

  /******
   * The following members should be accessed with the decoder lock held.
   ******/

  // Current decoding position in the stream. This is where the decoder
  // is up to consuming the stream. This is not adjusted during decoder
  // seek operations, but it's updated at the end when we start playing
  // back again.
  int64_t mDecoderPosition;
  // Current playback position in the stream. This is (approximately)
  // where we're up to playing back the stream. This is not adjusted
  // during decoder seek operations, but it's updated at the end when we
  // start playing back again.
  int64_t mPlaybackPosition;

  // The current playback position of the media resource in units of
  // seconds. This is updated approximately at the framerate of the
  // video (if it is a video) or the callback period of the audio.
  // It is read and written from the main thread only.
  double mCurrentTime;

  // Volume that playback should start at.  0.0 = muted. 1.0 = full
  // volume.  Readable/Writeable from the main thread.
  double mInitialVolume;

  // PlaybackRate and pitch preservation status we should start at.
  // Readable/Writeable from the main thread.
  double mInitialPlaybackRate;
  bool mInitialPreservesPitch;

  // Duration of the media resource. Set to -1 if unknown.
  // Set when the metadata is loaded. Accessed on the main thread
  // only.
  int64_t mDuration;

  // True when playback should start with audio captured (not playing).
  bool mInitialAudioCaptured;

  // True if the resource is seekable at a transport level (server supports byte
  // range requests, local file, etc.).
  bool mTransportSeekable;

  // True if the media is seekable (i.e. supports random access).
  bool mMediaSeekable;

  // True if the media is same-origin with the element. Data can only be
  // passed to MediaStreams when this is true.
  bool mSameOriginMedia;

  /******
   * The following member variables can be accessed from any thread.
   ******/

  // The state machine object for handling the decoding. It is safe to
  // call methods of this object from other threads. Its internal data
  // is synchronised on a monitor. The lifetime of this object is
  // after mPlayState is LOADING and before mPlayState is SHUTDOWN. It
  // is safe to access it during this period.
  nsCOMPtr<MediaDecoderStateMachine> mDecoderStateMachine;

  // Media data resource.
  nsRefPtr<MediaResource> mResource;

  // |ReentrantMonitor| for detecting when the video play state changes. A call
  // to |Wait| on this monitor will block the thread until the next state
  // change.
  // Using a wrapper class to restrict direct access to the |ReentrantMonitor|
  // object. Subclasses may override |MediaDecoder|::|GetReentrantMonitor|
  // e.g. |DASHRepDecoder|::|GetReentrantMonitor| returns the monitor in the
  // main |DASHDecoder| object. In this case, directly accessing the
  // member variable mReentrantMonitor in |DASHRepDecoder| is wrong.
  // The wrapper |RestrictedAccessMonitor| restricts use to the getter
  // function rather than the object itself.
private:
  class RestrictedAccessMonitor
  {
  public:
    RestrictedAccessMonitor(const char* aName) :
      mReentrantMonitor(aName)
    {
      MOZ_COUNT_CTOR(RestrictedAccessMonitor);
    }
    ~RestrictedAccessMonitor()
    {
      MOZ_COUNT_DTOR(RestrictedAccessMonitor);
    }

    // Returns a ref to the reentrant monitor
    ReentrantMonitor& GetReentrantMonitor() {
      return mReentrantMonitor;
    }
  private:
    ReentrantMonitor mReentrantMonitor;
  };

  // The |RestrictedAccessMonitor| member object.
  RestrictedAccessMonitor mReentrantMonitor;

public:
  // Data about MediaStreams that are being fed by this decoder.
  nsTArray<OutputStreamData> mOutputStreams;
  // The SourceMediaStream we are using to feed the mOutputStreams. This stream
  // is never exposed outside the decoder.
  // Only written on the main thread while holding the monitor. Therefore it
  // can be read on any thread while holding the monitor, or on the main thread
  // without holding the monitor.
  nsAutoPtr<DecodedStreamData> mDecodedStream;

  // True if this decoder is in dormant state.
  // Should be true only when PlayState is PLAY_STATE_LOADING.
  bool mIsDormant;

  // True if this decoder is exiting from dormant state.
  // Should be true only when PlayState is PLAY_STATE_LOADING.
  bool mIsExitingDormant;

  // Set to one of the valid play states.
  // This can only be changed on the main thread while holding the decoder
  // monitor. Thus, it can be safely read while holding the decoder monitor
  // OR on the main thread.
  // Any change to the state on the main thread must call NotifyAll on the
  // monitor so the decode thread can wake up.
  PlayState mPlayState;

  // The state to change to after a seek or load operation.
  // This can only be changed on the main thread while holding the decoder
  // monitor. Thus, it can be safely read while holding the decoder monitor
  // OR on the main thread.
  // Any change to the state must call NotifyAll on the monitor.
  // This can only be PLAY_STATE_PAUSED or PLAY_STATE_PLAYING.
  PlayState mNextState;

  // Position to seek to when the seek notification is received by the
  // decode thread.
  // This can only be changed on the main thread while holding the decoder
  // monitor. Thus, it can be safely read while holding the decoder monitor
  // OR on the main thread.
  // If the value is negative then no seek has been requested. When a seek is
  // started this is reset to negative.
  double mRequestedSeekTime;

  // True when we have fully loaded the resource and reported that
  // to the element (i.e. reached NETWORK_LOADED state).
  // Accessed on the main thread only.
  bool mCalledResourceLoaded;

  // True when seeking or otherwise moving the play position around in
  // such a manner that progress event data is inaccurate. This is set
  // during seek and duration operations to prevent the progress indicator
  // from jumping around. Read/Write from any thread. Must have decode monitor
  // locked before accessing.
  bool mIgnoreProgressData;

  // True if the stream is infinite (e.g. a webradio).
  bool mInfiniteStream;

  // True if NotifyDecodedStreamMainThreadStateChanged should retrigger
  // PlaybackEnded when mDecodedStream->mStream finishes.
  bool mTriggerPlaybackEndedWhenSourceStreamFinishes;

protected:

  // Start timer to update download progress information.
  nsresult StartProgress();

  // Stop progress information timer.
  nsresult StopProgress();

  // Ensures our media stream has been pinned.
  void PinForSeek();

  // Ensures our media stream has been unpinned.
  void UnpinForSeek();

  // Timer used for updating progress events
  nsCOMPtr<nsITimer> mProgressTimer;

  // This should only ever be accessed from the main thread.
  // It is set in Init and cleared in Shutdown when the element goes away.
  // The decoder does not add a reference the element.
  MediaDecoderOwner* mOwner;

  // Counters related to decode and presentation of frames.
  FrameStatistics mFrameStats;

  nsRefPtr<VideoFrameContainer> mVideoFrameContainer;

  // Time that the last progress event was fired. Read/Write from the
  // main thread only.
  TimeStamp mProgressTime;

  // Time that data was last read from the media resource. Used for
  // computing if the download has stalled and to rate limit progress events
  // when data is arriving slower than PROGRESS_MS. A value of null indicates
  // that a stall event has already fired and not to fire another one until
  // more data is received. Read/Write from the main thread only.
  TimeStamp mDataTime;

  // Data needed to estimate playback data rate. The timeline used for
  // this estimate is "decode time" (where the "current time" is the
  // time of the last decoded video frame).
  MediaChannelStatistics mPlaybackStatistics;

  // The framebuffer size to use for audioavailable events.
  uint32_t mFrameBufferLength;

  // True when our media stream has been pinned. We pin the stream
  // while seeking.
  bool mPinnedForSeek;

  // True if the decoder is being shutdown. At this point all events that
  // are currently queued need to return immediately to prevent javascript
  // being run that operates on the element and decoder during shutdown.
  // Read/Write from the main thread only.
  bool mShuttingDown;

  // True if the playback is paused because the playback rate member is 0.0.
  bool mPausedForPlaybackRateNull;

  // Be assigned from media element during the initialization and pass to
  // AudioStream Class.
  AudioChannelType mAudioChannelType;
};

} // namespace mozilla

#endif
