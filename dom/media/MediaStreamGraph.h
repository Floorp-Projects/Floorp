/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMGRAPH_H_
#define MOZILLA_MEDIASTREAMGRAPH_H_

#include "AudioStream.h"
#include "MainThreadUtils.h"
#include "MediaSegment.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/StateWatching.h"
#include "mozilla/TaskQueue.h"
#include "nsAutoPtr.h"
#include "nsAutoRef.h"
#include "nsIRunnable.h"
#include "nsTArray.h"
#include <speex/speex_resampler.h>

class nsIRunnable;
class nsIGlobalObject;
class nsPIDOMWindowInner;

namespace mozilla {
class AsyncLogger;
class AudioCaptureStream;
};  // namespace mozilla

extern mozilla::AsyncLogger gMSGTraceLogger;

template <>
class nsAutoRefTraits<SpeexResamplerState>
    : public nsPointerRefTraits<SpeexResamplerState> {
 public:
  static void Release(SpeexResamplerState* aState) {
    speex_resampler_destroy(aState);
  }
};

namespace mozilla {

extern LazyLogModule gMediaStreamGraphLog;

namespace dom {
enum class AudioContextOperation;
enum class AudioContextOperationFlags;
}  // namespace dom

inline TrackTicks RateConvertTicksRoundDown(TrackRate aOutRate,
                                            TrackRate aInRate,
                                            TrackTicks aTicks) {
  MOZ_ASSERT(0 < aOutRate && aOutRate <= TRACK_RATE_MAX, "Bad out rate");
  MOZ_ASSERT(0 < aInRate && aInRate <= TRACK_RATE_MAX, "Bad in rate");
  MOZ_ASSERT(0 <= aTicks && aTicks <= TRACK_TICKS_MAX, "Bad ticks");
  return (aTicks * aOutRate) / aInRate;
}
inline TrackTicks RateConvertTicksRoundUp(TrackRate aOutRate, TrackRate aInRate,
                                          TrackTicks aTicks) {
  MOZ_ASSERT(0 < aOutRate && aOutRate <= TRACK_RATE_MAX, "Bad out rate");
  MOZ_ASSERT(0 < aInRate && aInRate <= TRACK_RATE_MAX, "Bad in rate");
  MOZ_ASSERT(0 <= aTicks && aTicks <= TRACK_TICKS_MAX, "Bad ticks");
  return (aTicks * aOutRate + aInRate - 1) / aInRate;
}

/*
 * MediaStreamGraph is a framework for synchronized audio/video processing
 * and playback. It is designed to be used by other browser components such as
 * HTML media elements, media capture APIs, real-time media streaming APIs,
 * multitrack media APIs, and advanced audio APIs.
 *
 * The MediaStreamGraph uses a dedicated thread to process media --- the media
 * graph thread. This ensures that we can process media through the graph
 * without blocking on main-thread activity. The media graph is only modified
 * on the media graph thread, to ensure graph changes can be processed without
 * interfering with media processing. All interaction with the media graph
 * thread is done with message passing.
 *
 * APIs that modify the graph or its properties are described as "control APIs".
 * These APIs are asynchronous; they queue graph changes internally and
 * those changes are processed all-at-once by the MediaStreamGraph. The
 * MediaStreamGraph monitors the main thread event loop via
 * nsIAppShell::RunInStableState to ensure that graph changes from a single
 * event loop task are always processed all together. Control APIs should only
 * be used on the main thread, currently; we may be able to relax that later.
 *
 * To allow precise synchronization of times in the control API, the
 * MediaStreamGraph maintains a "media timeline". Control APIs that take or
 * return times use that timeline. Those times never advance during
 * an event loop task. This time is returned by
 * MediaStreamGraph::GetCurrentTime().
 *
 * Media decoding, audio processing and media playback use thread-safe APIs to
 * the media graph to ensure they can continue while the main thread is blocked.
 *
 * When the graph is changed, we may need to throw out buffered data and
 * reprocess it. This is triggered automatically by the MediaStreamGraph.
 */

class AudioNodeEngine;
class AudioNodeExternalInputStream;
class AudioNodeStream;
class MediaInputPort;
class MediaStream;
class MediaStreamGraph;
class MediaStreamGraphImpl;
class ProcessedMediaStream;
class SourceMediaStream;

class AudioDataListenerInterface {
 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AudioDataListenerInterface() = default;

 public:
  /* These are for cubeb audio input & output streams: */
  /**
   * Output data to speakers, for use as the "far-end" data for echo
   * cancellation.  This is not guaranteed to be in any particular size
   * chunks.
   */
  virtual void NotifyOutputData(MediaStreamGraphImpl* aGraph,
                                AudioDataValue* aBuffer, size_t aFrames,
                                TrackRate aRate, uint32_t aChannels) = 0;
  /**
   * Input data from a microphone (or other audio source.  This is not
   * guaranteed to be in any particular size chunks.
   */
  virtual void NotifyInputData(MediaStreamGraphImpl* aGraph,
                               const AudioDataValue* aBuffer, size_t aFrames,
                               TrackRate aRate, uint32_t aChannels) = 0;

  /**
   * Number of audio input channels.
   */
  virtual uint32_t RequestedInputChannelCount(MediaStreamGraphImpl* aGraph) = 0;

  /**
   * Whether the underlying audio device is used for voice input.
   */
  virtual bool IsVoiceInput(MediaStreamGraphImpl* aGraph) const = 0;
  /**
   * Called when the underlying audio device has changed.
   */
  virtual void DeviceChanged(MediaStreamGraphImpl* aGraph) = 0;

  /**
   * Called when the underlying audio device is being closed.
   */
  virtual void Disconnect(MediaStreamGraphImpl* aGraph) = 0;
};

class AudioDataListener : public AudioDataListenerInterface {
 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AudioDataListener() = default;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioDataListener)
};

/**
 * This is a base class for main-thread listener callbacks.
 * This callback is invoked on the main thread when the main-thread-visible
 * state of a stream has changed.
 *
 * These methods are called with the media graph monitor held, so
 * reentry into general media graph methods is not possible.
 * You should do something non-blocking and non-reentrant (e.g. dispatch an
 * event) and return. NS_DispatchToCurrentThread would be a good choice.
 * The listener is allowed to synchronously remove itself from the stream, but
 * not add or remove any other listeners.
 */
class MainThreadMediaStreamListener {
 public:
  virtual void NotifyMainThreadTrackEnded() = 0;
};

/**
 * Helper struct used to keep track of memory usage by AudioNodes.
 */
struct AudioNodeSizes {
  AudioNodeSizes() : mStream(0), mEngine(0), mNodeType() {}
  size_t mStream;
  size_t mEngine;
  const char* mNodeType;
};

/**
 * Describes how a track should be disabled.
 *
 * ENABLED        Not disabled.
 * SILENCE_BLACK  Audio data is turned into silence, video frames are made
 *                black.
 * SILENCE_FREEZE Audio data is turned into silence, video freezes at
 *                last frame.
 */
enum class DisabledTrackMode { ENABLED, SILENCE_BLACK, SILENCE_FREEZE };

class AudioNodeEngine;
class AudioNodeExternalInputStream;
class AudioNodeStream;
class DirectMediaStreamTrackListener;
class MediaInputPort;
class MediaStreamGraphImpl;
class MediaStreamTrackListener;
class ProcessedMediaStream;
class SourceMediaStream;
class TrackUnionStream;

/**
 * A stream of audio or video data. The media type must be known at construction
 * and cannot change. All streams progress at the same rate --- "real time".
 * Streams cannot seek. The only operation readers can perform on a stream is to
 * read the next data.
 *
 * Consumers of a stream can be reading from it at different offsets, but that
 * should only happen due to the order in which consumers are being run.
 * Those offsets must not diverge in the long term, otherwise we would require
 * unbounded buffering.
 *
 * (DEPRECATED to be removed in bug 1581074)
 * Streams can be in a "blocked" state. While blocked, a stream does not
 * produce data. A stream can be explicitly blocked via the control API,
 * or implicitly blocked by whatever's generating it (e.g. an underrun in the
 * source resource), or implicitly blocked because something consuming it
 * blocks, or implicitly because it has ended.
 *
 * A stream can be in an "ended" state. "Ended" streams are permanently blocked.
 * The "ended" state is terminal.
 *
 * Transitions into and out of the "blocked" and "ended" states are managed
 * by the MediaStreamGraph on the media graph thread.
 *
 * We buffer media data ahead of the consumers' reading offsets. It is possible
 * to have buffered data but still be blocked.
 *
 * Any stream can have its audio or video playing when requested. The media
 * stream graph plays audio by constructing audio output streams as necessary.
 * Video is played through a DirectMediaStreamTrackListener managed by
 * VideoStreamTrack.
 *
 * The data in a stream is managed by mSegment. The segment starts at GraphTime
 * mStartTime and encodes its own StreamTime duration.
 *
 * Streams are explicitly managed. The client creates them via
 * MediaStreamGraph::Create{Source|TrackUnion}Stream, and releases them by
 * calling Destroy() when no longer needed (actual destruction will be
 * deferred). The actual object is owned by the MediaStreamGraph. The basic idea
 * is that main thread objects will keep Streams alive as long as necessary
 * (using the cycle collector to clean up whenever needed).
 *
 * We make them refcounted only so that stream-related messages with
 * MediaStream* pointers can be sent to the main thread safely.
 *
 * The lifetimes of MediaStreams are controlled from the main thread.
 * For MediaStreams exposed to the DOM, the lifetime is controlled by the DOM
 * wrapper; the DOM wrappers own their associated MediaStreams. When a DOM
 * wrapper is destroyed, it sends a Destroy message for the associated
 * MediaStream and clears its reference (the last main-thread reference to
 * the object). When the Destroy message is processed on the graph thread we
 * immediately release the affected objects (disentangling them from other
 * objects as necessary).
 *
 * This could cause problems for media processing if a MediaStream is destroyed
 * while a downstream MediaStream is still using it. Therefore the DOM wrappers
 * must keep upstream MediaStreams alive as long as they could be used in the
 * media graph.
 *
 * At any time, however, a set of MediaStream wrappers could be collected via
 * cycle collection. Destroy messages will be sent for those objects in
 * arbitrary order and the MediaStreamGraph has to be able to handle this.
 */
class MediaStream : public mozilla::LinkedListElement<MediaStream> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaStream)

  MediaStream(TrackRate aSampleRate, MediaSegment::Type aType,
              MediaSegment* aSegment);

  // The sample rate of the graph.
  const TrackRate mSampleRate;
  const MediaSegment::Type mType;

 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~MediaStream();

 public:
  /**
   * Returns the graph that owns this stream.
   */
  MediaStreamGraphImpl* GraphImpl();
  const MediaStreamGraphImpl* GraphImpl() const;
  MediaStreamGraph* Graph();
  const MediaStreamGraph* Graph() const;
  /**
   * Sets the graph that owns this stream.  Should only be called once.
   */
  void SetGraphImpl(MediaStreamGraphImpl* aGraph);
  void SetGraphImpl(MediaStreamGraph* aGraph);

  // Control API.
  // Since a stream can be played multiple ways, we need to combine independent
  // volume settings. The aKey parameter is used to keep volume settings
  // separate. Since the stream is always playing the same contents, only
  // a single audio output stream is used; the volumes are combined.
  // Currently only the first enabled audio track is played.
  // XXX change this so all enabled audio tracks are mixed and played.
  virtual void AddAudioOutput(void* aKey);
  virtual void SetAudioOutputVolume(void* aKey, float aVolume);
  virtual void RemoveAudioOutput(void* aKey);
  // Explicitly suspend. Useful for example if a media element is pausing
  // and we need to stop its stream emitting its buffered data. As soon as the
  // Suspend message reaches the graph, the stream stops processing. It
  // ignores its inputs and produces silence/no video until Resumed. Its
  // current time does not advance.
  virtual void Suspend();
  virtual void Resume();
  // Events will be dispatched by calling methods of aListener.
  virtual void AddListener(MediaStreamTrackListener* aListener);
  virtual void RemoveListener(MediaStreamTrackListener* aListener);

  /**
   * Adds aListener to the source stream of this stream.
   * When the MediaStreamGraph processes the added listener, it will traverse
   * the graph and add it to the track's source stream.
   * Note that the listener will be notified on the MediaStreamGraph thread
   * with whether the installation of it at the source was successful or not.
   */
  virtual void AddDirectListener(DirectMediaStreamTrackListener* aListener);

  /**
   * Removes aListener from the source stream of this stream.
   * Note that the listener has already been removed if the link between the
   * source and this stream has been broken. The caller doesn't have to care
   * about this, removing when the source cannot be found, or when the listener
   * had already been removed does nothing.
   */
  virtual void RemoveDirectListener(DirectMediaStreamTrackListener* aListener);

  // A disabled track has video replaced by black, and audio replaced by
  // silence.
  void SetEnabled(DisabledTrackMode aMode);

  // End event will be notified by calling methods of aListener. It is the
  // responsibility of the caller to remove aListener before it is destroyed.
  void AddMainThreadListener(MainThreadMediaStreamListener* aListener);
  // It's safe to call this even if aListener is not currently a listener;
  // the call will be ignored.
  void RemoveMainThreadListener(MainThreadMediaStreamListener* aListener) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aListener);
    mMainThreadListeners.RemoveElement(aListener);
  }

  /**
   * Ensure a runnable will run on the main thread after running all pending
   * updates that were sent from the graph thread or will be sent before the
   * graph thread receives the next graph update.
   *
   * If the graph has been shut down or destroyed, then the runnable will be
   * dispatched to the event queue immediately.  (There are no pending updates
   * in this situation.)
   *
   * Main thread only.
   */
  void RunAfterPendingUpdates(already_AddRefed<nsIRunnable> aRunnable);

  // Signal that the client is done with this MediaStream. It will be deleted
  // later.
  virtual void Destroy();

  // Returns the main-thread's view of how much data has been processed by
  // this stream.
  StreamTime GetCurrentTime() const {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    return mMainThreadCurrentTime;
  }
  // Return the main thread's view of whether this stream has ended.
  bool IsEnded() const {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    return mMainThreadEnded;
  }

  bool IsDestroyed() const {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    return mMainThreadDestroyed;
  }

  friend class MediaStreamGraphImpl;
  friend class MediaInputPort;
  friend class AudioNodeExternalInputStream;

  virtual SourceMediaStream* AsSourceStream() { return nullptr; }
  virtual ProcessedMediaStream* AsProcessedStream() { return nullptr; }
  virtual AudioNodeStream* AsAudioNodeStream() { return nullptr; }
  virtual TrackUnionStream* AsTrackUnionStream() { return nullptr; }

  // These Impl methods perform the core functionality of the control methods
  // above, on the media graph thread.
  /**
   * Stop all stream activity and disconnect it from all inputs and outputs.
   * This must be idempotent.
   */
  virtual void DestroyImpl();
  StreamTime GetEnd() const;
  void SetAudioOutputVolumeImpl(void* aKey, float aVolume);
  void AddAudioOutputImpl(void* aKey);
  void RemoveAudioOutputImpl(void* aKey);

  /**
   * Removes all direct listeners and signals to them that they have been
   * uninstalled.
   */
  virtual void RemoveAllDirectListenersImpl() {}
  void RemoveAllResourcesAndListenersImpl();

  virtual void AddListenerImpl(
      already_AddRefed<MediaStreamTrackListener> aListener);
  virtual void RemoveListenerImpl(MediaStreamTrackListener* aListener);
  virtual void AddDirectListenerImpl(
      already_AddRefed<DirectMediaStreamTrackListener> aListener);
  virtual void RemoveDirectListenerImpl(
      DirectMediaStreamTrackListener* aListener);
  virtual void SetEnabledImpl(DisabledTrackMode aMode);

  void AddConsumer(MediaInputPort* aPort) { mConsumers.AppendElement(aPort); }
  void RemoveConsumer(MediaInputPort* aPort) {
    mConsumers.RemoveElement(aPort);
  }
  GraphTime StartTime() const { return mStartTime; }
  bool Ended() const { return mEnded; }

  template <class SegmentType>
  SegmentType* GetData() const {
    if (!mSegment) {
      return nullptr;
    }
    if (mSegment->GetType() != SegmentType::StaticType()) {
      return nullptr;
    }
    return static_cast<SegmentType*>(mSegment.get());
  }
  template <>
  MediaSegment* GetData<MediaSegment>() const {
    return mSegment.get();
  }

  double StreamTimeToSeconds(StreamTime aTime) const {
    NS_ASSERTION(0 <= aTime && aTime <= STREAM_TIME_MAX, "Bad time");
    return static_cast<double>(aTime) / mSampleRate;
  }
  int64_t StreamTimeToMicroseconds(StreamTime aTime) const {
    NS_ASSERTION(0 <= aTime && aTime <= STREAM_TIME_MAX, "Bad time");
    return (aTime * 1000000) / mSampleRate;
  }
  StreamTime SecondsToNearestStreamTime(double aSeconds) const {
    NS_ASSERTION(0 <= aSeconds && aSeconds <= TRACK_TICKS_MAX / TRACK_RATE_MAX,
                 "Bad seconds");
    return mSampleRate * aSeconds + 0.5;
  }
  StreamTime MicrosecondsToStreamTimeRoundDown(int64_t aMicroseconds) const {
    return (aMicroseconds * mSampleRate) / 1000000;
  }

  TrackTicks TimeToTicksRoundUp(TrackRate aRate, StreamTime aTime) const {
    return RateConvertTicksRoundUp(aRate, mSampleRate, aTime);
  }
  StreamTime TicksToTimeRoundDown(TrackRate aRate, TrackTicks aTicks) const {
    return RateConvertTicksRoundDown(mSampleRate, aRate, aTicks);
  }
  /**
   * Convert graph time to stream time. aTime must be <= mStateComputedTime
   * to ensure we know exactly how much time this stream will be blocked during
   * the interval.
   */
  StreamTime GraphTimeToStreamTimeWithBlocking(GraphTime aTime) const;
  /**
   * Convert graph time to stream time. This assumes there is no blocking time
   * to take account of, which is always true except between a stream
   * having its blocking time calculated in UpdateGraph and its blocking time
   * taken account of in UpdateCurrentTimeForStreams.
   */
  StreamTime GraphTimeToStreamTime(GraphTime aTime) const;
  /**
   * Convert stream time to graph time. This assumes there is no blocking time
   * to take account of, which is always true except between a stream
   * having its blocking time calculated in UpdateGraph and its blocking time
   * taken account of in UpdateCurrentTimeForStreams.
   */
  GraphTime StreamTimeToGraphTime(StreamTime aTime) const;

  virtual void ApplyTrackDisabling(MediaSegment* aSegment,
                                   MediaSegment* aRawSegment = nullptr);

  // Return true if the main thread needs to observe updates from this stream.
  virtual bool MainThreadNeedsUpdates() const { return true; }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  bool IsSuspended() const { return mSuspendedCount > 0; }
  void IncrementSuspendCount();
  void DecrementSuspendCount();

 protected:
  // Called on graph thread before handing control to the main thread to
  // release streams.
  virtual void NotifyForcedShutdown() {}

  // |AdvanceTimeVaryingValuesToCurrentTime| will be override in
  // SourceMediaStream.
  virtual void AdvanceTimeVaryingValuesToCurrentTime(GraphTime aCurrentTime,
                                                     GraphTime aBlockedTime);

  void NotifyMainThreadListeners() {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");

    for (int32_t i = mMainThreadListeners.Length() - 1; i >= 0; --i) {
      mMainThreadListeners[i]->NotifyMainThreadTrackEnded();
    }
    mMainThreadListeners.Clear();
  }

  bool ShouldNotifyTrackEnded() {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    if (!mMainThreadEnded || mEndedNotificationSent) {
      return false;
    }

    mEndedNotificationSent = true;
    return true;
  }

  // This state is all initialized on the main thread but
  // otherwise modified only on the media graph thread.

  // Buffered data. The start of the buffer corresponds to mStartTime.
  // Conceptually the buffer contains everything this stream has ever played,
  // but we forget some prefix of the buffered data to bound the space usage.
  // Note that this may be null for tracks that never contain data, like
  // non-external AudioNodeTracks.
  const UniquePtr<MediaSegment> mSegment;

  // The time when the buffered data could be considered to have started
  // playing. This increases over time to account for time the stream was
  // blocked before mCurrentTime.
  GraphTime mStartTime;

  // The time until which we last called mSegment->ForgetUpTo().
  StreamTime mForgottenTime;

  // True once we've processed mSegment until the end and no more data will be
  // added. Note that mSegment might still contain data for the current
  // iteration.
  bool mEnded;

  // True after track listeners have been notified that this track has ended.
  bool mNotifiedEnded;

  // Client-set volume of this stream
  struct AudioOutput {
    explicit AudioOutput(void* aKey) : mKey(aKey), mVolume(1.0f) {}
    void* mKey;
    float mVolume;
  };
  nsTArray<AudioOutput> mAudioOutputs;
  nsTArray<RefPtr<MediaStreamTrackListener>> mTrackListeners;
  nsTArray<MainThreadMediaStreamListener*> mMainThreadListeners;
  // This track's associated disabled mode. It can either by disabled by frames
  // being replaced by black, or by retaining the previous frame.
  DisabledTrackMode mDisabledMode;

  // GraphTime at which this stream starts blocking.
  // This is only valid up to mStateComputedTime. The stream is considered to
  // have not been blocked before mCurrentTime (its mStartTime is
  // increased as necessary to account for that time instead).
  GraphTime mStartBlocking;

  // MediaInputPorts to which this is connected
  nsTArray<MediaInputPort*> mConsumers;

  // Where audio output is going. There is one AudioOutputStream per
  // Type::AUDIO MediaStream.
  struct AudioOutputStream {
    // When we started audio playback for this track.
    // Add mStream->GetPosition() to find the current audio playback position.
    GraphTime mAudioPlaybackStartTime;
    // Amount of time that we've wanted to play silence because of the stream
    // blocking.
    MediaTime mBlockedAudioTime;
    // Last tick written to the audio output.
    StreamTime mLastTickWritten;
  };
  UniquePtr<AudioOutputStream> mAudioOutputStream;

  /**
   * Number of outstanding suspend operations on this stream. Stream is
   * suspended when this is > 0.
   */
  int32_t mSuspendedCount;

  // Main-thread views of state
  StreamTime mMainThreadCurrentTime;
  bool mMainThreadEnded;
  bool mEndedNotificationSent;
  bool mMainThreadDestroyed;

  // Our media stream graph.  null if destroyed on the graph thread.
  MediaStreamGraphImpl* mGraph;
};

/**
 * This is a stream into which a decoder can write audio or video.
 *
 * Audio or video can be written on any thread, but you probably want to
 * always write from the same thread to avoid unexpected interleavings.
 *
 * For audio the sample rate of the written data can differ from the sample rate
 * of the graph itself. Use SetAppendDataSourceRate to inform the stream what
 * rate written audio data will be sampled in.
 */
class SourceMediaStream : public MediaStream {
 public:
  SourceMediaStream(MediaSegment::Type aType, TrackRate aSampleRate);

  SourceMediaStream* AsSourceStream() override { return this; }

  // Main thread only

  /**
   * Enable or disable pulling.
   * When pulling is enabled, NotifyPull gets called on the
   * MediaStreamTrackListeners for this track during the MediaStreamGraph
   * control loop. Pulling is initially disabled. Due to unavoidable race
   * conditions, after a call to SetPullingEnabled(false) it is still possible
   * for a NotifyPull to occur.
   */
  void SetPullingEnabled(bool aEnabled);

  // Users of audio inputs go through the stream so it can track when the
  // last stream referencing an input goes away, so it can close the cubeb
  // input. Main thread only.
  nsresult OpenAudioInput(CubebUtils::AudioDeviceID aID,
                          AudioDataListener* aListener);
  // Main thread only.
  void CloseAudioInput(Maybe<CubebUtils::AudioDeviceID>& aID);

  // Main thread only.
  void Destroy() override;
  // MediaStreamGraph thread only
  void DestroyImpl() override;

  // Call these on any thread.
  /**
   * Call all MediaStreamTrackListeners to request new data via the NotifyPull
   * API (if enabled).
   * aDesiredUpToTime (in): end time of new data requested.
   *
   * Returns true if new data is about to be added.
   */
  bool PullNewData(GraphTime aDesiredUpToTime);

  /**
   * Extract any state updates pending in the stream, and apply them.
   */
  void ExtractPendingInput(GraphTime aCurrentTime, GraphTime aDesiredUpToTime);

  /**
   * All data appended with AppendData() from this point on will be resampled
   * from aRate to the graph rate.
   *
   * Resampling for video does not make sense and is forbidden.
   */
  void SetAppendDataSourceRate(TrackRate aRate);

  /**
   * Append media data to this stream. Ownership of aSegment remains with the
   * caller, but aSegment is emptied. Returns 0 if the data was not appended
   * because the stream has ended. Returns the duration of the appended data in
   * the graph's track rate otherwise.
   */
  virtual StreamTime AppendData(MediaSegment* aSegment,
                                MediaSegment* aRawSegment = nullptr);
  /**
   * Indicate that this stream has ended. Do not do any more API calls affecting
   * this stream.
   */
  void End();

  // Overriding allows us to hold the mMutex lock while changing the track
  // enable status
  void SetEnabledImpl(DisabledTrackMode aMode) override;

  // Overriding allows us to ensure mMutex is locked while changing the track
  // enable status
  void ApplyTrackDisabling(MediaSegment* aSegment,
                           MediaSegment* aRawSegment = nullptr) override {
    mMutex.AssertCurrentThreadOwns();
    MediaStream::ApplyTrackDisabling(aSegment, aRawSegment);
  }

  void RemoveAllDirectListenersImpl() override;

  friend class MediaStreamGraphImpl;

 protected:
  enum TrackCommands : uint32_t;

  virtual ~SourceMediaStream();

  /**
   * Data to cater for appending media data to this stream.
   */
  struct TrackData {
    // Sample rate of the input data.
    TrackRate mInputRate;
    // Resampler if the rate of the input track does not match the
    // MediaStreamGraph's.
    nsAutoRef<SpeexResamplerState> mResampler;
    int mResamplerChannelCount;
    // Each time the track updates are flushed to the media graph thread,
    // the segment buffer is emptied.
    UniquePtr<MediaSegment> mData;
    // True once the producer has signaled that no more data is coming.
    bool mEnded;
    // True if the producer of this track is having data pulled by the graph.
    bool mPullingEnabled;
  };

  bool NeedsMixing();

  void ResampleAudioToGraphSampleRate(MediaSegment* aSegment);

  void AddDirectListenerImpl(
      already_AddRefed<DirectMediaStreamTrackListener> aListener) override;
  void RemoveDirectListenerImpl(
      DirectMediaStreamTrackListener* aListener) override;

  /**
   * Notify direct consumers of new data to this stream.
   * The data doesn't have to be resampled (though it may be).  This is called
   * from AppendData on the thread providing the data, and will call
   * the Listeners on this thread.
   */
  void NotifyDirectConsumers(MediaSegment* aSegment);

  virtual void AdvanceTimeVaryingValuesToCurrentTime(
      GraphTime aCurrentTime, GraphTime aBlockedTime) override;

  // Only accessed on the MSG thread.  Used so to ask the MSGImpl to usecount
  // users of a specific input.
  // XXX Should really be a CubebUtils::AudioDeviceID, but they aren't
  // copyable (opaque pointers)
  RefPtr<AudioDataListener> mInputListener;

  // This must be acquired *before* MediaStreamGraphImpl's lock, if they are
  // held together.
  Mutex mMutex;
  // protected by mMutex
  UniquePtr<TrackData> mUpdateTrack;
  nsTArray<RefPtr<DirectMediaStreamTrackListener>> mDirectTrackListeners;
};

/**
 * A ref-counted wrapper of a MediaStream that allows multiple users to share a
 * reference to the same MediaStream with the purpose of being guaranteed that
 * the graph it is in is kept alive.
 *
 * Automatically suspended on creation and destroyed on destruction. Main thread
 * only.
 */
struct SharedDummyStream {
  NS_INLINE_DECL_REFCOUNTING(SharedDummyStream)
  explicit SharedDummyStream(MediaStream* aStream) : mStream(aStream) {
    mStream->Suspend();
  }
  const RefPtr<MediaStream> mStream;

 private:
  ~SharedDummyStream() { mStream->Destroy(); }
};

/**
 * Represents a connection between a ProcessedMediaStream and one of its
 * input streams.
 * We make these refcounted so that stream-related messages with MediaInputPort*
 * pointers can be sent to the main thread safely.
 *
 * When a port's source or destination stream dies, the stream's DestroyImpl
 * calls MediaInputPort::Disconnect to disconnect the port from
 * the source and destination streams.
 *
 * The lifetimes of MediaInputPort are controlled from the main thread.
 * The media graph adds a reference to the port. When a MediaInputPort is no
 * longer needed, main-thread code sends a Destroy message for the port and
 * clears its reference (the last main-thread reference to the object). When
 * the Destroy message is processed on the graph manager thread we disconnect
 * the port and drop the graph's reference, destroying the object.
 */
class MediaInputPort final {
 private:
  // Do not call this constructor directly. Instead call
  // aDest->AllocateInputPort.
  MediaInputPort(MediaStream* aSource, ProcessedMediaStream* aDest,
                 uint16_t aInputNumber, uint16_t aOutputNumber)
      : mSource(aSource),
        mDest(aDest),
        mInputNumber(aInputNumber),
        mOutputNumber(aOutputNumber),
        mGraph(nullptr) {
    MOZ_COUNT_CTOR(MediaInputPort);
  }

  // Private destructor, to discourage deletion outside of Release():
  ~MediaInputPort() { MOZ_COUNT_DTOR(MediaInputPort); }

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaInputPort)

  // Called on graph manager thread
  // Do not call these from outside MediaStreamGraph.cpp!
  void Init();
  // Called during message processing to trigger removal of this stream.
  void Disconnect();

  // Control API
  /**
   * Disconnects and destroys the port. The caller must not reference this
   * object again.
   */
  void Destroy();

  // Any thread
  MediaStream* GetSource() const { return mSource; }
  ProcessedMediaStream* GetDestination() const { return mDest; }

  uint16_t InputNumber() const { return mInputNumber; }
  uint16_t OutputNumber() const { return mOutputNumber; }

  // Call on graph manager thread
  struct InputInterval {
    GraphTime mStart;
    GraphTime mEnd;
    bool mInputIsBlocked;
  };
  // Find the next time interval starting at or after aTime during which
  // aPort->mDest is not blocked and aPort->mSource's blocking status does not
  // change. A null aPort returns a blocked interval starting at aTime.
  static InputInterval GetNextInputInterval(MediaInputPort const* aPort,
                                            GraphTime aTime);

  /**
   * Returns the graph that owns this port.
   */
  MediaStreamGraphImpl* GraphImpl();
  MediaStreamGraph* Graph();

  /**
   * Sets the graph that owns this stream.  Should only be called once.
   */
  void SetGraphImpl(MediaStreamGraphImpl* aGraph);

  /**
   * Notify the port that the source MediaStream has been suspended.
   */
  void Suspended();

  /**
   * Notify the port that the source MediaStream has been resumed.
   */
  void Resumed();

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t amount = 0;

    // Not owned:
    // - mSource
    // - mDest
    // - mGraph
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  friend class MediaStreamGraphImpl;
  friend class MediaStream;
  friend class ProcessedMediaStream;
  // Never modified after Init()
  MediaStream* mSource;
  ProcessedMediaStream* mDest;
  // The input and output numbers are optional, and are currently only used by
  // Web Audio.
  const uint16_t mInputNumber;
  const uint16_t mOutputNumber;

  // Our media stream graph
  MediaStreamGraphImpl* mGraph;
};

/**
 * This stream processes zero or more input streams in parallel to produce
 * its output. The details of how the output is produced are handled by
 * subclasses overriding the ProcessInput method.
 */
class ProcessedMediaStream : public MediaStream {
 public:
  ProcessedMediaStream(TrackRate aSampleRate, MediaSegment::Type aType,
                       MediaSegment* aSegment)
      : MediaStream(aSampleRate, aType, aSegment),
        mAutoend(true),
        mCycleMarker(0) {}

  // Control API.
  /**
   * Allocates a new input port attached to source aStream.
   * This port can be removed by calling MediaInputPort::Destroy().
   */
  already_AddRefed<MediaInputPort> AllocateInputPort(
      MediaStream* aStream, uint16_t aInputNumber = 0,
      uint16_t aOutputNumber = 0);
  /**
   * Queue a message to set the autoend flag on this stream (defaults to
   * true). When this flag is set, and the input stream has ended playout
   * (including if there is no input stream), this stream automatically
   * enters the ended state.
   */
  void QueueSetAutoend(bool aAutoend);

  ProcessedMediaStream* AsProcessedStream() override { return this; }

  friend class MediaStreamGraphImpl;

  // Do not call these from outside MediaStreamGraph.cpp!
  virtual void AddInput(MediaInputPort* aPort);
  virtual void RemoveInput(MediaInputPort* aPort) {
    mInputs.RemoveElement(aPort) || mSuspendedInputs.RemoveElement(aPort);
  }
  bool HasInputPort(MediaInputPort* aPort) const {
    return mInputs.Contains(aPort) || mSuspendedInputs.Contains(aPort);
  }
  uint32_t InputPortCount() const {
    return mInputs.Length() + mSuspendedInputs.Length();
  }
  void InputSuspended(MediaInputPort* aPort);
  void InputResumed(MediaInputPort* aPort);
  void DestroyImpl() override;
  /**
   * This gets called after we've computed the blocking states for all
   * streams (mBlocked is up to date up to mStateComputedTime).
   * Also, we've produced output for all streams up to this one. If this stream
   * is not in a cycle, then all its source streams have produced data.
   * Generate output from aFrom to aTo.
   * This will be called on streams that have ended. Most stream types should
   * just return immediately if they're ended, but some may wish to update
   * internal state (see AudioNodeStream).
   * ProcessInput is allowed to set mEnded only if ALLOW_END is in aFlags. (This
   * flag will be set when aTo >= mStateComputedTime, i.e. when we've produced
   * the last block of data we need to produce.) Otherwise we can get into a
   * situation where we've determined the track should not block before
   * mStateComputedTime, but the track ends before mStateComputedTime, violating
   * the invariant that ended tracks are blocked.
   */
  enum { ALLOW_END = 0x01 };
  virtual void ProcessInput(GraphTime aFrom, GraphTime aTo,
                            uint32_t aFlags) = 0;
  void SetAutoendImpl(bool aAutoend) { mAutoend = aAutoend; }

  // Only valid after MediaStreamGraphImpl::UpdateStreamOrder() has run.
  // A DelayNode is considered to break a cycle and so this will not return
  // true for echo loops, only for muted cycles.
  bool InMutedCycle() const { return mCycleMarker; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = MediaStream::SizeOfExcludingThis(aMallocSizeOf);
    // Not owned:
    // - mInputs elements
    // - mSuspendedInputs elements
    amount += mInputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
    amount += mSuspendedInputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 protected:
  // This state is all accessed only on the media graph thread.

  // The list of all inputs that are not currently suspended.
  nsTArray<MediaInputPort*> mInputs;
  // The list of all inputs that are currently suspended.
  nsTArray<MediaInputPort*> mSuspendedInputs;
  bool mAutoend;
  // After UpdateStreamOrder(), mCycleMarker is either 0 or 1 to indicate
  // whether this stream is in a muted cycle.  During ordering it can contain
  // other marker values - see MediaStreamGraphImpl::UpdateStreamOrder().
  uint32_t mCycleMarker;
};

/**
 * There is a single MediaStreamGraph per window.
 * Additionaly, each OfflineAudioContext object creates its own MediaStreamGraph
 * object too.
 */
class MediaStreamGraph {
 public:
  // We ensure that the graph current time advances in multiples of
  // IdealAudioBlockSize()/AudioStream::PreferredSampleRate(). A stream that
  // never blocks and has the ideal audio rate will produce audio in multiples
  // of the block size.

  // Initializing a graph that outputs audio can take quite long on some
  // platforms. Code that want to output audio at some point can express the
  // fact that they will need an audio stream at some point by passing
  // AUDIO_THREAD_DRIVER when getting an instance of MediaStreamGraph, so that
  // the graph starts with the right driver.
  enum GraphDriverType {
    AUDIO_THREAD_DRIVER,
    SYSTEM_THREAD_DRIVER,
    OFFLINE_THREAD_DRIVER
  };
  // A MediaStreamGraph running an AudioWorklet must always be run from the
  // same thread, in order to run js. To acheive this, create the graph with
  // a SINGLE_THREAD RunType. DIRECT_DRIVER will run the graph directly off
  // the GraphDriver's thread.
  enum GraphRunType {
    DIRECT_DRIVER,
    SINGLE_THREAD,
  };
  static const uint32_t AUDIO_CALLBACK_DRIVER_SHUTDOWN_TIMEOUT = 20 * 1000;
  static const TrackRate REQUEST_DEFAULT_SAMPLE_RATE = 0;

  // Main thread only
  static MediaStreamGraph* GetInstanceIfExists(nsPIDOMWindowInner* aWindow,
                                               TrackRate aSampleRate);
  static MediaStreamGraph* GetInstance(GraphDriverType aGraphDriverRequested,
                                       nsPIDOMWindowInner* aWindow,
                                       TrackRate aSampleRate);
  static MediaStreamGraph* CreateNonRealtimeInstance(
      TrackRate aSampleRate, nsPIDOMWindowInner* aWindowId);

  // Return the correct main thread for this graph. This always returns
  // something that is valid. Thread safe.
  AbstractThread* AbstractMainThread();

  // Idempotent
  static void DestroyNonRealtimeInstance(MediaStreamGraph* aGraph);

  virtual nsresult OpenAudioInput(CubebUtils::AudioDeviceID aID,
                                  AudioDataListener* aListener) = 0;
  virtual void CloseAudioInput(Maybe<CubebUtils::AudioDeviceID>& aID,
                               AudioDataListener* aListener) = 0;
  // Control API.
  /**
   * Create a stream that a media decoder (or some other source of
   * media data, such as a camera) can write to.
   */
  SourceMediaStream* CreateSourceStream(MediaSegment::Type aType);
  /**
   * Create a stream that will forward data from its input stream.
   *
   * A TrackUnionStream can have 0 or 1 input streams. Adding more than that is
   * an error.
   *
   * A TrackUnionStream will end when autoending is enabled (default) and there
   * is no input, or the input's source is ended. If there is no input and
   * autoending is disabled, TrackUnionStream will continue to produce silence
   * for audio or the last video frame for video.
   */
  ProcessedMediaStream* CreateTrackUnionStream(MediaSegment::Type aType);
  /**
   * Create a stream that will mix all its audio inputs.
   */
  AudioCaptureStream* CreateAudioCaptureStream();

  /**
   * Add a new stream to the graph.  Main thread.
   */
  void AddStream(MediaStream* aStream);

  /* From the main thread, ask the MSG to send back an event when the graph
   * thread is running, and audio is being processed. */
  void NotifyWhenGraphStarted(AudioNodeStream* aNodeStream);
  /* From the main thread, suspend, resume or close an AudioContext.
   * aStreams are the streams of all the AudioNodes of the AudioContext that
   * need to be suspended or resumed. This can be empty if this is a second
   * consecutive suspend call and all the nodes are already suspended.
   *
   * This can possibly pause the graph thread, releasing system resources, if
   * all streams have been suspended/closed.
   *
   * When the operation is complete, aPromise is resolved.
   */
  void ApplyAudioContextOperation(MediaStream* aDestinationStream,
                                  const nsTArray<MediaStream*>& aStreams,
                                  dom::AudioContextOperation aState,
                                  void* aPromise,
                                  dom::AudioContextOperationFlags aFlags);

  bool IsNonRealtime() const;
  /**
   * Start processing non-realtime for a specific number of ticks.
   */
  void StartNonRealtimeProcessing(uint32_t aTicksToProcess);

  /**
   * Media graph thread only.
   * Dispatches a runnable that will run on the main thread after all
   * main-thread stream state has been updated, i.e., during stable state.
   *
   * Should only be called during MediaStreamTrackListener callbacks or during
   * ProcessedMediaStream::ProcessInput().
   *
   * Note that if called during shutdown the runnable will be ignored and
   * released on main thread.
   */
  void DispatchToMainThreadStableState(already_AddRefed<nsIRunnable> aRunnable);

  /**
   * Returns graph sample rate in Hz.
   */
  TrackRate GraphRate() const { return mSampleRate; }

  double AudioOutputLatency();

  void RegisterCaptureStreamForWindow(uint64_t aWindowId,
                                      ProcessedMediaStream* aCaptureStream);
  void UnregisterCaptureStreamForWindow(uint64_t aWindowId);
  already_AddRefed<MediaInputPort> ConnectToCaptureStream(
      uint64_t aWindowId, MediaStream* aMediaStream);

  void AssertOnGraphThreadOrNotRunning() const {
    MOZ_ASSERT(OnGraphThreadOrNotRunning());
  }

  /**
   * Returns a watchable of the graph's main-thread observable graph time.
   * Main thread only.
   */
  virtual Watchable<GraphTime>& CurrentTime() = 0;

 protected:
  explicit MediaStreamGraph(TrackRate aSampleRate) : mSampleRate(aSampleRate) {
    MOZ_COUNT_CTOR(MediaStreamGraph);
  }
  virtual ~MediaStreamGraph() { MOZ_COUNT_DTOR(MediaStreamGraph); }

  // Intended only for assertions, either on graph thread or not running (in
  // which case we must be on the main thread).
  virtual bool OnGraphThreadOrNotRunning() const = 0;
  virtual bool OnGraphThread() const = 0;

  // Intended only for internal assertions. Main thread only.
  virtual bool Destroyed() const = 0;

  /**
   * Sample rate at which this graph runs. For real time graphs, this is
   * the rate of the audio mixer. For offline graphs, this is the rate specified
   * at construction.
   */
  const TrackRate mSampleRate;
};

}  // namespace mozilla

#endif /* MOZILLA_MEDIASTREAMGRAPH_H_ */
