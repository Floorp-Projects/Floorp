/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaDecoder_h_)
#define MediaDecoder_h_

#include "mozilla/Atomics.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/StateWatching.h"

#include "mozilla/dom/AudioChannelBinding.h"

#include "necko-config.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsITimer.h"

#include "AbstractMediaDecoder.h"
#include "DecoderDoctorDiagnostics.h"
#include "MediaDecoderOwner.h"
#include "MediaEventSource.h"
#include "MediaMetadataManager.h"
#include "MediaResource.h"
#include "MediaResourceCallback.h"
#include "MediaStatistics.h"
#include "MediaStreamGraph.h"
#include "TimeUnits.h"
#include "SeekTarget.h"

class nsIStreamListener;
class nsIPrincipal;

namespace mozilla {

namespace dom {
class Promise;
}

class VideoFrameContainer;
class MediaDecoderStateMachine;

enum class MediaEventType : int8_t;

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaDecoder::GetCurrentTime implementation.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

class MediaDecoder : public AbstractMediaDecoder
{
public:
  // Used to register with MediaResource to receive notifications which will
  // be forwarded to MediaDecoder.
  class ResourceCallback : public MediaResourceCallback {
    // Throttle calls to MediaDecoder::NotifyDataArrived()
    // to be at most once per 500ms.
    static const uint32_t sDelay = 500;

  public:
    // Start to receive notifications from ResourceCallback.
    void Connect(MediaDecoder* aDecoder);
    // Called upon shutdown to stop receiving notifications.
    void Disconnect();

  private:
    /* MediaResourceCallback functions */
    MediaDecoderOwner* GetMediaOwner() const override;
    void SetInfinite(bool aInfinite) override;
    void NotifyNetworkError() override;
    void NotifyDecodeError() override;
    void NotifyDataArrived() override;
    void NotifyBytesDownloaded() override;
    void NotifyDataEnded(nsresult aStatus) override;
    void NotifyPrincipalChanged() override;
    void NotifySuspendedStatusChanged() override;
    void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) override;

    static void TimerCallback(nsITimer* aTimer, void* aClosure);

    // The decoder to send notifications. Main-thread only.
    MediaDecoder* mDecoder = nullptr;
    nsCOMPtr<nsITimer> mTimer;
    bool mTimerArmed = false;
  };

  typedef MozPromise<bool /* aIgnored */, bool /* aIgnored */, /* IsExclusive = */ true> SeekPromise;

  NS_DECL_THREADSAFE_ISUPPORTS

  // Enumeration for the valid play states (see mPlayState)
  enum PlayState {
    PLAY_STATE_START,
    PLAY_STATE_LOADING,
    PLAY_STATE_PAUSED,
    PLAY_STATE_PLAYING,
    PLAY_STATE_ENDED,
    PLAY_STATE_SHUTDOWN
  };

  // Must be called exactly once, on the main thread, during startup.
  static void InitStatics();

  explicit MediaDecoder(MediaDecoderOwner* aOwner);

  // Return a callback object used to register with MediaResource to receive
  // notifications.
  MediaResourceCallback* GetResourceCallback() const;

  // Create a new decoder of the same type as this one.
  // Subclasses must implement this.
  virtual MediaDecoder* Clone(MediaDecoderOwner* aOwner) = 0;
  // Create a new state machine to run this decoder.
  // Subclasses must implement this.
  virtual MediaDecoderStateMachine* CreateStateMachine() = 0;

  // Cleanup internal data structures. Must be called on the main
  // thread by the owning object before that object disposes of this object.
  virtual void Shutdown();

  // Start downloading the media. Decode the downloaded data up to the
  // point of the first frame of data.
  // This is called at most once per decoder, after Init().
  virtual nsresult Load(nsIStreamListener** aListener);

  // Called in |Load| to open mResource.
  nsresult OpenResource(nsIStreamListener** aStreamListener);

  // Called if the media file encounters a network error.
  void NetworkError();

  // Get the current MediaResource being used. Its URI will be returned
  // by currentSrc. Returns what was passed to Load(), if Load() has been called.
  // Note: The MediaResource is refcounted, but it outlives the MediaDecoder,
  // so it's OK to use the reference returned by this function without
  // refcounting, *unless* you need to store and use the reference after the
  // MediaDecoder has been destroyed. You might need to do this if you're
  // wrapping the MediaResource in some kind of byte stream interface to be
  // passed to a platform decoder.
  MediaResource* GetResource() const final override
  {
    return mResource;
  }
  void SetResource(MediaResource* aResource)
  {
    MOZ_ASSERT(NS_IsMainThread());
    mResource = aResource;
  }

  // Return the principal of the current URI being played or downloaded.
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();

  // Return the time position in the video stream being
  // played measured in seconds.
  virtual double GetCurrentTime();

  // Seek to the time position in (seconds) from the start of the video.
  // If aDoFastSeek is true, we'll seek to the sync point/keyframe preceeding
  // the seek target.
  virtual nsresult Seek(double aTime, SeekTarget::Type aSeekType,
                        dom::Promise* aPromise = nullptr);

  // Initialize state machine and schedule it.
  nsresult InitializeStateMachine();

  // Start playback of a video. 'Load' must have previously been
  // called.
  virtual nsresult Play();

  // Notify activity of the decoder owner is changed.
  virtual void NotifyOwnerActivityChanged(bool aIsVisible);

  // Pause video playback.
  virtual void Pause();
  // Adjust the speed of the playback, optionally with pitch correction,
  virtual void SetVolume(double aVolume);

  virtual void SetPlaybackRate(double aPlaybackRate);
  void SetPreservesPitch(bool aPreservesPitch);

  // Directs the decoder to not preroll extra samples until the media is
  // played. This reduces the memory overhead of media elements that may
  // not be played. Note that seeking also doesn't cause us start prerolling.
  void SetMinimizePrerollUntilPlaybackStarts();

  // All MediaStream-related data is protected by mReentrantMonitor.
  // We have at most one DecodedStreamData per MediaDecoder. Its stream
  // is used as the input for each ProcessedMediaStream created by calls to
  // captureStream(UntilEnded). Seeking creates a new source stream, as does
  // replaying after the input as ended. In the latter case, the new source is
  // not connected to streams created by captureStreamUntilEnded.

  // Add an output stream. All decoder output will be sent to the stream.
  // The stream is initially blocked. The decoder is responsible for unblocking
  // it while it is playing back.
  virtual void AddOutputStream(ProcessedMediaStream* aStream, bool aFinishWhenEnded);
  // Remove an output stream added with AddOutputStream.
  virtual void RemoveOutputStream(MediaStream* aStream);

  // Return the duration of the video in seconds.
  virtual double GetDuration();

  // Return true if the stream is infinite (see SetInfinite).
  bool IsInfinite() const;

  // Called by MediaResource when some data has been received.
  // Call on the main thread only.
  virtual void NotifyBytesDownloaded();

  // Called as data arrives on the stream and is read into the cache.  Called
  // on the main thread only.
  void NotifyDataArrived();

  // Return true if we are currently seeking in the media resource.
  // Call on the main thread only.
  bool IsSeeking() const;

  // Return true if the decoder has reached the end of playback.
  bool IsEnded() const;

  // Return true if the MediaDecoderOwner's error attribute is not null.
  // Must be called before Shutdown().
  bool OwnerHasError() const;

  already_AddRefed<GMPCrashHelper> GetCrashHelper() override;

protected:
  // Updates the media duration. This is called while the media is being
  // played, calls before the media has reached loaded metadata are ignored.
  // The duration is assumed to be an estimate, and so a degree of
  // instability is expected; if the incoming duration is not significantly
  // different from the existing duration, the change request is ignored.
  // If the incoming duration is significantly different, the duration is
  // changed, this causes a durationchanged event to fire to the media
  // element.
  void UpdateEstimatedMediaDuration(int64_t aDuration) override;

public:
  // Returns true if this media supports random seeking. False for example with
  // chained ogg files.
  bool IsMediaSeekable();
  // Returns true if seeking is supported on a transport level (e.g. the server
  // supports range requests, we are playing a file, etc.).
  bool IsTransportSeekable();

  // Return the time ranges that can be seeked into.
  virtual media::TimeIntervals GetSeekable();

  // Set the end time of the media resource. When playback reaches
  // this point the media pauses. aTime is in seconds.
  virtual void SetFragmentEndTime(double aTime);

  // Invalidate the frame.
  void Invalidate();
  void InvalidateWithFlags(uint32_t aFlags);

  // Suspend any media downloads that are in progress. Called by the
  // media element when it is sent to the bfcache, or when we need
  // to throttle the download. Call on the main thread only. This can
  // be called multiple times, there's an internal "suspend count".
  virtual void Suspend();

  // Resume any media downloads that have been suspended. Called by the
  // media element when it is restored from the bfcache, or when we need
  // to stop throttling the download. Call on the main thread only.
  // The download will only actually resume once as many Resume calls
  // have been made as Suspend calls.
  virtual void Resume();

  // Moves any existing channel loads into or out of background. Background
  // loads don't block the load event. This is called when we stop or restart
  // delaying the load event. This also determines whether any new loads
  // initiated (for example to seek) will be in the background.  This calls
  // SetLoadInBackground() on mResource.
  void SetLoadInBackground(bool aLoadInBackground);

  MediaDecoderStateMachine* GetStateMachine() const;
  void SetStateMachine(MediaDecoderStateMachine* aStateMachine);

  // Constructs the time ranges representing what segments of the media
  // are buffered and playable.
  virtual media::TimeIntervals GetBuffered();

  // Returns the size, in bytes, of the heap memory used by the currently
  // queued decoded video and audio data.
  size_t SizeOfVideoQueue();
  size_t SizeOfAudioQueue();

  // Helper struct for accumulating resource sizes that need to be measured
  // asynchronously. Once all references are dropped the callback will be
  // invoked.
  struct ResourceSizes
  {
    typedef MozPromise<size_t, size_t, true> SizeOfPromise;
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ResourceSizes)
    explicit ResourceSizes(MallocSizeOf aMallocSizeOf)
      : mMallocSizeOf(aMallocSizeOf)
      , mByteSize(0)
      , mCallback()
    {
    }

    mozilla::MallocSizeOf mMallocSizeOf;
    mozilla::Atomic<size_t> mByteSize;

    RefPtr<SizeOfPromise> Promise()
    {
      return mCallback.Ensure(__func__);
    }

private:
    ~ResourceSizes()
    {
      mCallback.ResolveIfExists(mByteSize, __func__);
    }

    MozPromiseHolder<SizeOfPromise> mCallback;
  };

  virtual void AddSizeOfResources(ResourceSizes* aSizes);

  VideoFrameContainer* GetVideoFrameContainer() final override
  {
    return mVideoFrameContainer;
  }
  layers::ImageContainer* GetImageContainer() override;

  // Fire timeupdate events if needed according to the time constraints
  // outlined in the specification.
  void FireTimeUpdate();

  // Something has changed that could affect the computed playback rate,
  // so recompute it. The monitor must be held.
  virtual void UpdatePlaybackRate();

  // The actual playback rate computation. The monitor must be held.
  void ComputePlaybackRate();

  // Returns true if we can play the entire media through without stopping
  // to buffer, given the current download and playback rates.
  virtual bool CanPlayThrough();

  void SetAudioChannel(dom::AudioChannel aChannel) { mAudioChannel = aChannel; }
  dom::AudioChannel GetAudioChannel() { return mAudioChannel; }

  // Called from HTMLMediaElement when owner document activity changes
  virtual void SetElementVisibility(bool aIsVisible);

  // Force override the visible state to hidden.
  // Called from HTMLMediaElement when testing of video decode suspend from mochitests.
  void SetForcedHidden(bool aForcedHidden);

  /******
   * The following methods must only be called on the main
   * thread.
   ******/

  // Change to a new play state. This updates the mState variable and
  // notifies any thread blocking on this object's monitor of the
  // change. Call on the main thread only.
  virtual void ChangeState(PlayState aState);

  // Called from MetadataLoaded(). Creates audio tracks and adds them to its
  // owner's audio track list, and implies to video tracks respectively.
  // Call on the main thread only.
  void ConstructMediaTracks();

  // Removes all audio tracks and video tracks that are previously added into
  // the track list. Call on the main thread only.
  void RemoveMediaTracks();

  // Called when the video has completed playing.
  // Call on the main thread only.
  void PlaybackEnded();

  void OnSeekRejected();
  void OnSeekResolved();

  void SeekingChanged()
  {
    // Stop updating the bytes downloaded for progress notifications when
    // seeking to prevent wild changes to the progress notification.
    MOZ_ASSERT(NS_IsMainThread());
    mIgnoreProgressData = mLogicallySeeking;
  }

  // Seeking has started. Inform the element on the main thread.
  void SeekingStarted();

  void UpdateLogicalPositionInternal();
  void UpdateLogicalPosition()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!IsShutdown());
    // Per spec, offical position remains stable during pause and seek.
    if (mPlayState == PLAY_STATE_PAUSED || IsSeeking()) {
      return;
    }
    UpdateLogicalPositionInternal();
  }

  // Find the end of the cached data starting at the current decoder
  // position.
  int64_t GetDownloadPosition();

  // Notifies the element that decoding has failed.
  void DecodeError(const MediaResult& aError);

  // Indicate whether the media is same-origin with the element.
  void UpdateSameOriginStatus(bool aSameOrigin);

  MediaDecoderOwner* GetOwner() const override;

  typedef MozPromise<RefPtr<CDMProxy>, bool /* aIgnored */, /* IsExclusive = */ true> CDMProxyPromise;

  // Resolved when a CDMProxy is available and the capabilities are known or
  // rejected when this decoder is about to shut down.
  RefPtr<CDMProxyPromise> RequestCDMProxy() const;

  void SetCDMProxy(CDMProxy* aProxy);

  void EnsureTelemetryReported();

  static bool IsOggEnabled();
  static bool IsOpusEnabled();
  static bool IsWaveEnabled();
  static bool IsWebMEnabled();

#ifdef MOZ_ANDROID_OMX
  static bool IsAndroidMediaPluginEnabled();
#endif

#ifdef MOZ_WMF
  static bool IsWMFEnabled();
#endif

  // Return statistics. This is used for progress events and other things.
  // This can be called from any thread. It's only a snapshot of the
  // current state, since other threads might be changing the state
  // at any time.
  MediaStatistics GetStatistics();

  // Return the frame decode/paint related statistics.
  FrameStatistics& GetFrameStatistics() { return *mFrameStats; }

  // Increments the parsed and decoded frame counters by the passed in counts.
  // Can be called on any thread.
  virtual void NotifyDecodedFrames(const FrameStatisticsData& aStats) override
  {
    GetFrameStatistics().NotifyDecodedFrames(aStats);
  }

  void UpdateReadyState()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!IsShutdown());
    mOwner->UpdateReadyState();
  }

  virtual MediaDecoderOwner::NextFrameStatus NextFrameStatus() { return mNextFrameStatus; }
  virtual MediaDecoderOwner::NextFrameStatus NextFrameBufferedStatus();

  // Returns a string describing the state of the media player internal
  // data. Used for debugging purposes.
  virtual void GetMozDebugReaderData(nsAString& aString) {}

  virtual void DumpDebugInfo();

protected:
  virtual ~MediaDecoder();

  // Called when the first audio and/or video from the media file has been loaded
  // by the state machine. Call on the main thread only.
  virtual void FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo,
                                MediaDecoderEventVisibility aEventVisibility);

  void SetStateMachineParameters();

  bool IsShutdown() const;

  // Called by the state machine to notify the decoder that the duration
  // has changed.
  void DurationChanged();

  // State-watching manager.
  WatchManager<MediaDecoder> mWatchManager;

  // Used by the ogg decoder to watch mStateMachineIsShutdown.
  virtual void ShutdownBitChanged() {}

  double ExplicitDuration() { return mExplicitDuration.Ref().ref(); }

  void SetExplicitDuration(double aValue)
  {
    MOZ_ASSERT(!IsShutdown());
    mExplicitDuration.Set(Some(aValue));

    // We Invoke DurationChanged explicitly, rather than using a watcher, so
    // that it takes effect immediately, rather than at the end of the current task.
    DurationChanged();
  }

  /******
   * The following members should be accessed with the decoder lock held.
   ******/

  // The logical playback position of the media resource in units of
  // seconds. This corresponds to the "official position" in HTML5. Note that
  // we need to store this as a double, rather than an int64_t (like
  // mCurrentPosition), so that |v.currentTime = foo; v.currentTime == foo|
  // returns true without being affected by rounding errors.
  double mLogicalPosition;

  // The current playback position of the underlying playback infrastructure.
  // This corresponds to the "current position" in HTML5.
  // We allow omx subclasses to substitute an alternative current position for
  // usage with the audio offload player.
  virtual int64_t CurrentPosition() { return mCurrentPosition; }

  // Official duration of the media resource as observed by script.
  double mDuration;

  /******
   * The following member variables can be accessed from any thread.
   ******/

  // Media data resource.
  RefPtr<MediaResource> mResource;

  // Amount of buffered data ahead of current time required to consider that
  // the next frame is available.
  // An arbitrary value of 250ms is used.
  static const int DEFAULT_NEXT_FRAME_AVAILABLE_BUFFERED = 250000;

private:
  // Called when the metadata from the media file has been loaded by the
  // state machine. Call on the main thread only.
  void MetadataLoaded(nsAutoPtr<MediaInfo> aInfo,
                      nsAutoPtr<MetadataTags> aTags,
                      MediaDecoderEventVisibility aEventVisibility);

  MediaEventSource<void>*
  DataArrivedEvent() override { return &mDataArrivedEvent; }

  MediaEventSource<RefPtr<layers::KnowsCompositor>>*
  CompositorUpdatedEvent() override { return &mCompositorUpdatedEvent; }

  void OnPlaybackEvent(MediaEventType aEvent);
  void OnPlaybackErrorEvent(const MediaResult& aError);

  void OnDecoderDoctorEvent(DecoderDoctorEvent aEvent);

  void OnMediaNotSeekable()
  {
    mMediaSeekable = false;
  }

  void FinishShutdown();

  void ConnectMirrors(MediaDecoderStateMachine* aObject);
  void DisconnectMirrors();

  MediaEventProducer<void> mDataArrivedEvent;
  MediaEventProducer<RefPtr<layers::KnowsCompositor>> mCompositorUpdatedEvent;

  // The state machine object for handling the decoding. It is safe to
  // call methods of this object from other threads. Its internal data
  // is synchronised on a monitor. The lifetime of this object is
  // after mPlayState is LOADING and before mPlayState is SHUTDOWN. It
  // is safe to access it during this period.
  //
  // Explicitly prievate to force access via accessors.
  RefPtr<MediaDecoderStateMachine> mDecoderStateMachine;

  RefPtr<ResourceCallback> mResourceCallback;

  MozPromiseHolder<CDMProxyPromise> mCDMProxyPromiseHolder;
  RefPtr<CDMProxyPromise> mCDMProxyPromise;

protected:
  // The promise resolving/rejection is queued as a "micro-task" which will be
  // handled immediately after the current JS task and before any pending JS
  // tasks.
  // At the time we are going to resolve/reject a promise, the "seeking" event
  // task should already be queued but might yet be processed, so we queue one
  // more task to file the promise resolving/rejection micro-tasks
  // asynchronously to make sure that the micro-tasks are processed after the
  // "seeking" event task.
  void AsyncResolveSeekDOMPromiseIfExists();
  void AsyncRejectSeekDOMPromiseIfExists();
  void DiscardOngoingSeekIfExists();
  virtual void CallSeek(const SeekTarget& aTarget, dom::Promise* aPromise);

  MozPromiseRequestHolder<SeekPromise> mSeekRequest;
  RefPtr<dom::Promise> mSeekDOMPromise;

  // True when seeking or otherwise moving the play position around in
  // such a manner that progress event data is inaccurate. This is set
  // during seek and duration operations to prevent the progress indicator
  // from jumping around. Read/Write on the main thread only.
  bool mIgnoreProgressData;

  // True if the stream is infinite (e.g. a webradio).
  bool mInfiniteStream;

  // Ensures our media stream has been pinned.
  void PinForSeek();

  // Ensures our media stream has been unpinned.
  void UnpinForSeek();

  const char* PlayStateStr();

  void OnMetadataUpdate(TimedMetadata&& aMetadata);

  // This should only ever be accessed from the main thread.
  // It is set in the constructor and cleared in Shutdown when the element goes
  // away. The decoder does not add a reference the element.
  MediaDecoderOwner* mOwner;

  // Counters related to decode and presentation of frames.
  const RefPtr<FrameStatistics> mFrameStats;

  RefPtr<VideoFrameContainer> mVideoFrameContainer;

  // Data needed to estimate playback data rate. The timeline used for
  // this estimate is "decode time" (where the "current time" is the
  // time of the last decoded video frame).
  RefPtr<MediaChannelStatistics> mPlaybackStatistics;

  // True when our media stream has been pinned. We pin the stream
  // while seeking.
  bool mPinnedForSeek;

  // Be assigned from media element during the initialization and pass to
  // AudioStream Class.
  dom::AudioChannel mAudioChannel;

  // True if the decoder has been directed to minimize its preroll before
  // playback starts. After the first time playback starts, we don't attempt
  // to minimize preroll, as we assume the user is likely to keep playing,
  // or play the media again.
  bool mMinimizePreroll;

  // True if audio tracks and video tracks are constructed and added into the
  // track list, false if all tracks are removed from the track list.
  bool mMediaTracksConstructed;

  // True if we've already fired metadataloaded.
  bool mFiredMetadataLoaded;

  // True if the media is seekable (i.e. supports random access).
  bool mMediaSeekable = true;

  // True if the media is only seekable within its buffered ranges
  // like WebMs with no cues.
  bool mMediaSeekableOnlyInBufferedRanges = false;

  // Stores media info, including info of audio tracks and video tracks, should
  // only be accessed from main thread.
  nsAutoPtr<MediaInfo> mInfo;

  // Tracks the visiblity status from HTMLMediaElement
  bool mElementVisible;

  // If true, forces the decoder to be considered hidden.
  bool mForcedHidden;

  // A listener to receive metadata updates from MDSM.
  MediaEventListener mTimedMetadataListener;

  MediaEventListener mMetadataLoadedListener;
  MediaEventListener mFirstFrameLoadedListener;

  MediaEventListener mOnPlaybackEvent;
  MediaEventListener mOnPlaybackErrorEvent;
  MediaEventListener mOnDecoderDoctorEvent;
  MediaEventListener mOnMediaNotSeekable;

protected:
  // Whether the state machine is shut down.
  Mirror<bool> mStateMachineIsShutdown;

  // Buffered range, mirrored from the reader.
  Mirror<media::TimeIntervals> mBuffered;

  // NextFrameStatus, mirrored from the state machine.
  Mirror<MediaDecoderOwner::NextFrameStatus> mNextFrameStatus;

  // NB: Don't use mCurrentPosition directly, but rather CurrentPosition().
  Mirror<int64_t> mCurrentPosition;

  // Duration of the media resource according to the state machine.
  Mirror<media::NullableTimeUnit> mStateMachineDuration;

  // Current playback position in the stream. This is (approximately)
  // where we're up to playing back the stream. This is not adjusted
  // during decoder seek operations, but it's updated at the end when we
  // start playing back again.
  Mirror<int64_t> mPlaybackPosition;

  // Used to distinguish whether the audio is producing sound.
  Mirror<bool> mIsAudioDataAudible;

  // Volume of playback.  0.0 = muted. 1.0 = full volume.
  Canonical<double> mVolume;

  // PlaybackRate and pitch preservation status we should start at.
  double mPlaybackRate = 1;

  Canonical<bool> mPreservesPitch;

  // Media duration according to the demuxer's current estimate.
  // Note that it's quite bizarre for this to live on the main thread - it would
  // make much more sense for this to be owned by the demuxer's task queue. But
  // currently this is only every changed in NotifyDataArrived, which runs on
  // the main thread. That will need to be cleaned up at some point.
  Canonical<media::NullableTimeUnit> mEstimatedDuration;

  // Media duration set explicitly by JS. At present, this is only ever present
  // for MSE.
  Canonical<Maybe<double>> mExplicitDuration;

  // Set to one of the valid play states.
  // This can only be changed on the main thread while holding the decoder
  // monitor. Thus, it can be safely read while holding the decoder monitor
  // OR on the main thread.
  Canonical<PlayState> mPlayState;

  // This can only be changed on the main thread while holding the decoder
  // monitor. Thus, it can be safely read while holding the decoder monitor
  // OR on the main thread.
  Canonical<PlayState> mNextState;

  // True if the decoder is seeking.
  Canonical<bool> mLogicallySeeking;

  // True if the media is same-origin with the element. Data can only be
  // passed to MediaStreams when this is true.
  Canonical<bool> mSameOriginMedia;

  // An identifier for the principal of the media. Used to track when
  // main-thread induced principal changes get reflected on MSG thread.
  Canonical<PrincipalHandle> mMediaPrincipalHandle;

  // Estimate of the current playback rate (bytes/second).
  Canonical<double> mPlaybackBytesPerSecond;

  // True if mPlaybackBytesPerSecond is a reliable estimate.
  Canonical<bool> mPlaybackRateReliable;

  // Current decoding position in the stream. This is where the decoder
  // is up to consuming the stream. This is not adjusted during decoder
  // seek operations, but it's updated at the end when we start playing
  // back again.
  Canonical<int64_t> mDecoderPosition;

  // True if the decoder is visible.
  Canonical<bool> mIsVisible;

public:
  AbstractCanonical<media::NullableTimeUnit>* CanonicalDurationOrNull() override;
  AbstractCanonical<double>* CanonicalVolume() {
    return &mVolume;
  }
  AbstractCanonical<bool>* CanonicalPreservesPitch() {
    return &mPreservesPitch;
  }
  AbstractCanonical<media::NullableTimeUnit>* CanonicalEstimatedDuration() {
    return &mEstimatedDuration;
  }
  AbstractCanonical<Maybe<double>>* CanonicalExplicitDuration() {
    return &mExplicitDuration;
  }
  AbstractCanonical<PlayState>* CanonicalPlayState() {
    return &mPlayState;
  }
  AbstractCanonical<PlayState>* CanonicalNextPlayState() {
    return &mNextState;
  }
  AbstractCanonical<bool>* CanonicalLogicallySeeking() {
    return &mLogicallySeeking;
  }
  AbstractCanonical<bool>* CanonicalSameOriginMedia() {
    return &mSameOriginMedia;
  }
  AbstractCanonical<PrincipalHandle>* CanonicalMediaPrincipalHandle() {
    return &mMediaPrincipalHandle;
  }
  AbstractCanonical<double>* CanonicalPlaybackBytesPerSecond() {
    return &mPlaybackBytesPerSecond;
  }
  AbstractCanonical<bool>* CanonicalPlaybackRateReliable() {
    return &mPlaybackRateReliable;
  }
  AbstractCanonical<int64_t>* CanonicalDecoderPosition() {
    return &mDecoderPosition;
  }
  AbstractCanonical<bool>* CanonicalIsVisible() {
    return &mIsVisible;
  }

private:
  // Notify owner when the audible state changed
  void NotifyAudibleStateChanged();

  /* Functions called by ResourceCallback */

  // A media stream is assumed to be infinite if the metadata doesn't
  // contain the duration, and range requests are not supported, and
  // no headers give a hint of a possible duration (Content-Length,
  // Content-Duration, and variants), and we cannot seek in the media
  // stream to determine the duration.
  //
  // When the media stream ends, we can know the duration, thus the stream is
  // no longer considered to be infinite.
  void SetInfinite(bool aInfinite);

  // Called by MediaResource when the principal of the resource has
  // changed. Called on main thread only.
  void NotifyPrincipalChanged();

  // Called by MediaResource when the "cache suspended" status changes.
  // If MediaResource::IsSuspendedByCache returns true, then the decoder
  // should stop buffering or otherwise waiting for download progress and
  // start consuming data, if possible, because the cache is full.
  void NotifySuspendedStatusChanged();

  // Called by the MediaResource to keep track of the number of bytes read
  // from the resource. Called on the main by an event runner dispatched
  // by the MediaResource read functions.
  void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset);

  // Called by nsChannelToPipeListener or MediaResource when the
  // download has ended. Called on the main thread only. aStatus is
  // the result from OnStopRequest.
  void NotifyDownloadEnded(nsresult aStatus);

  bool mTelemetryReported;
};

} // namespace mozilla

#endif
