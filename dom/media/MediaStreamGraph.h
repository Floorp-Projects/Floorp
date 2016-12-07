/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMGRAPH_H_
#define MOZILLA_MEDIASTREAMGRAPH_H_

#include "mozilla/LinkedList.h"
#include "mozilla/Mutex.h"
#include "mozilla/TaskQueue.h"

#include "mozilla/dom/AudioChannelBinding.h"

#include "AudioStream.h"
#include "nsTArray.h"
#include "nsIRunnable.h"
#include "VideoSegment.h"
#include "StreamTracks.h"
#include "MainThreadUtils.h"
#include "StreamTracks.h"
#include "nsAutoPtr.h"
#include "nsAutoRef.h"
#include <speex/speex_resampler.h>

class nsIRunnable;

template <>
class nsAutoRefTraits<SpeexResamplerState> : public nsPointerRefTraits<SpeexResamplerState>
{
  public:
  static void Release(SpeexResamplerState* aState) { speex_resampler_destroy(aState); }
};

namespace mozilla {

extern LazyLogModule gMediaStreamGraphLog;

namespace dom {
  enum class AudioContextOperation;
}

namespace media {
  template<typename V, typename E> class Pledge;
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
 * MediaStreamGraph monitors the main thread event loop via nsIAppShell::RunInStableState
 * to ensure that graph changes from a single event loop task are always
 * processed all together. Control APIs should only be used on the main thread,
 * currently; we may be able to relax that later.
 *
 * To allow precise synchronization of times in the control API, the
 * MediaStreamGraph maintains a "media timeline". Control APIs that take or
 * return times use that timeline. Those times never advance during
 * an event loop task. This time is returned by MediaStreamGraph::GetCurrentTime().
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
  virtual ~AudioDataListenerInterface() {}

public:
  /* These are for cubeb audio input & output streams: */
  /**
   * Output data to speakers, for use as the "far-end" data for echo
   * cancellation.  This is not guaranteed to be in any particular size
   * chunks.
   */
  virtual void NotifyOutputData(MediaStreamGraph* aGraph,
                                AudioDataValue* aBuffer, size_t aFrames,
                                TrackRate aRate, uint32_t aChannels) = 0;
  /**
   * Input data from a microphone (or other audio source.  This is not
   * guaranteed to be in any particular size chunks.
   */
  virtual void NotifyInputData(MediaStreamGraph* aGraph,
                               const AudioDataValue* aBuffer, size_t aFrames,
                               TrackRate aRate, uint32_t aChannels) = 0;

  /**
   * Called when the underlying audio device has changed.
   */
  virtual void DeviceChanged() = 0;
};

class AudioDataListener : public AudioDataListenerInterface {
protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AudioDataListener() {}

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
 * event) and return. DispatchFromMainThreadAfterNextStreamStateUpdate
 * would be a good choice.
 * The listener is allowed to synchronously remove itself from the stream, but
 * not add or remove any other listeners.
 */
class MainThreadMediaStreamListener {
public:
  virtual void NotifyMainThreadStreamFinished() = 0;
};

/**
 * Helper struct used to keep track of memory usage by AudioNodes.
 */
struct AudioNodeSizes
{
  AudioNodeSizes() : mStream(0), mEngine(0), mNodeType() {}
  size_t mStream;
  size_t mEngine;
  const char* mNodeType;
};

class AudioNodeEngine;
class AudioNodeExternalInputStream;
class AudioNodeStream;
class AudioSegment;
class DirectMediaStreamListener;
class DirectMediaStreamTrackListener;
class MediaInputPort;
class MediaStreamGraphImpl;
class MediaStreamListener;
class MediaStreamTrackListener;
class MediaStreamVideoSink;
class ProcessedMediaStream;
class SourceMediaStream;
class TrackUnionStream;

enum MediaStreamGraphEvent : uint32_t;
enum TrackEventCommand : uint32_t;

/**
 * Helper struct for binding a track listener to a specific TrackID.
 */
template<typename Listener>
struct TrackBound
{
  RefPtr<Listener> mListener;
  TrackID mTrackID;
};

/**
 * Describes how a track should be disabled.
 *
 * ENABLED        Not disabled.
 * SILENCE_BLACK  Audio data is turned into silence, video frames are made black.
 * SILENCE_FREEZE Audio data is turned into silence, video freezes at last frame.
 */
enum class DisabledTrackMode
{
  ENABLED, SILENCE_BLACK, SILENCE_FREEZE
};
struct DisabledTrack {
  DisabledTrack(TrackID aTrackID, DisabledTrackMode aMode)
    : mTrackID(aTrackID), mMode(aMode) {}
  TrackID mTrackID;
  DisabledTrackMode mMode;
};

/**
 * A stream of synchronized audio and video data. All (not blocked) streams
 * progress at the same rate --- "real time". Streams cannot seek. The only
 * operation readers can perform on a stream is to read the next data.
 *
 * Consumers of a stream can be reading from it at different offsets, but that
 * should only happen due to the order in which consumers are being run.
 * Those offsets must not diverge in the long term, otherwise we would require
 * unbounded buffering.
 *
 * Streams can be in a "blocked" state. While blocked, a stream does not
 * produce data. A stream can be explicitly blocked via the control API,
 * or implicitly blocked by whatever's generating it (e.g. an underrun in the
 * source resource), or implicitly blocked because something consuming it
 * blocks, or implicitly because it has finished.
 *
 * A stream can be in a "finished" state. "Finished" streams are permanently
 * blocked.
 *
 * Transitions into and out of the "blocked" and "finished" states are managed
 * by the MediaStreamGraph on the media graph thread.
 *
 * We buffer media data ahead of the consumers' reading offsets. It is possible
 * to have buffered data but still be blocked.
 *
 * Any stream can have its audio and video playing when requested. The media
 * stream graph plays audio by constructing audio output streams as necessary.
 * Video is played by setting video frames into an MediaStreamVideoSink at the right
 * time. To ensure video plays in sync with audio, make sure that the same
 * stream is playing both the audio and video.
 *
 * The data in a stream is managed by StreamTracks. It consists of a set of
 * tracks of various types that can start and end over time.
 *
 * Streams are explicitly managed. The client creates them via
 * MediaStreamGraph::CreateInput/ProcessedMediaStream, and releases them by calling
 * Destroy() when no longer needed (actual destruction will be deferred).
 * The actual object is owned by the MediaStreamGraph. The basic idea is that
 * main thread objects will keep Streams alive as long as necessary (using the
 * cycle collector to clean up whenever needed).
 *
 * We make them refcounted only so that stream-related messages with MediaStream*
 * pointers can be sent to the main thread safely.
 *
 * The lifetimes of MediaStreams are controlled from the main thread.
 * For MediaStreams exposed to the DOM, the lifetime is controlled by the DOM
 * wrapper; the DOM wrappers own their associated MediaStreams. When a DOM
 * wrapper is destroyed, it sends a Destroy message for the associated
 * MediaStream and clears its reference (the last main-thread reference to
 * the object). When the Destroy message is processed on the graph manager
 * thread we immediately release the affected objects (disentangling them
 * from other objects as necessary).
 *
 * This could cause problems for media processing if a MediaStream is
 * destroyed while a downstream MediaStream is still using it. Therefore
 * the DOM wrappers must keep upstream MediaStreams alive as long as they
 * could be being used in the media graph.
 *
 * At any time, however, a set of MediaStream wrappers could be
 * collected via cycle collection. Destroy messages will be sent
 * for those objects in arbitrary order and the MediaStreamGraph has to be able
 * to handle this.
 */

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaStream::GetCurrentTime.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

class MediaStream : public mozilla::LinkedListElement<MediaStream>
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaStream)

  MediaStream();

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~MediaStream();

public:
  /**
   * Returns the graph that owns this stream.
   */
  MediaStreamGraphImpl* GraphImpl();
  MediaStreamGraph* Graph();
  /**
   * Sets the graph that owns this stream.  Should only be called once.
   */
  void SetGraphImpl(MediaStreamGraphImpl* aGraph);
  void SetGraphImpl(MediaStreamGraph* aGraph);

  /**
   * Returns sample rate of the graph.
   */
  TrackRate GraphRate() { return mTracks.GraphRate(); }

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
  // Since a stream can be played multiple ways, we need to be able to
  // play to multiple MediaStreamVideoSinks.
  // Only the first enabled video track is played.
  virtual void AddVideoOutput(MediaStreamVideoSink* aSink,
                              TrackID aID = TRACK_ANY);
  virtual void RemoveVideoOutput(MediaStreamVideoSink* aSink,
                                 TrackID aID = TRACK_ANY);
  // Explicitly suspend. Useful for example if a media element is pausing
  // and we need to stop its stream emitting its buffered data. As soon as the
  // Suspend message reaches the graph, the stream stops processing. It
  // ignores its inputs and produces silence/no video until Resumed. Its
  // current time does not advance.
  virtual void Suspend();
  virtual void Resume();
  // Events will be dispatched by calling methods of aListener.
  virtual void AddListener(MediaStreamListener* aListener);
  virtual void RemoveListener(MediaStreamListener* aListener);
  virtual void AddTrackListener(MediaStreamTrackListener* aListener,
                                TrackID aTrackID);
  virtual void RemoveTrackListener(MediaStreamTrackListener* aListener,
                                   TrackID aTrackID);

  /**
   * Adds aListener to the source stream of track aTrackID in this stream.
   * When the MediaStreamGraph processes the added listener, it will traverse
   * the graph and add it to the track's source stream (remapping the TrackID
   * along the way).
   * Note that the listener will be notified on the MediaStreamGraph thread
   * with whether the installation of it at the source was successful or not.
   */
  virtual void AddDirectTrackListener(DirectMediaStreamTrackListener* aListener,
                                      TrackID aTrackID);

  /**
   * Removes aListener from the source stream of track aTrackID in this stream.
   * Note that the listener has already been removed if the link between the
   * source of track aTrackID and this stream has been broken (and made track
   * aTrackID end). The caller doesn't have to care about this, removing when
   * the source cannot be found, or when the listener had already been removed
   * does nothing.
   */
  virtual void RemoveDirectTrackListener(DirectMediaStreamTrackListener* aListener,
                                         TrackID aTrackID);

  // A disabled track has video replaced by black, and audio replaced by
  // silence.
  void SetTrackEnabled(TrackID aTrackID, DisabledTrackMode aMode);

  // Finish event will be notified by calling methods of aListener. It is the
  // responsibility of the caller to remove aListener before it is destroyed.
  void AddMainThreadListener(MainThreadMediaStreamListener* aListener);
  // It's safe to call this even if aListener is not currently a listener;
  // the call will be ignored.
  void RemoveMainThreadListener(MainThreadMediaStreamListener* aListener)
  {
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
   * dispatched to the event queue immediately.  If the graph is non-realtime
   * and has not started, then the runnable will be run
   * synchronously/immediately.  (There are no pending updates in these
   * situations.)
   *
   * Main thread only.
   */
  void RunAfterPendingUpdates(already_AddRefed<nsIRunnable> aRunnable);

  // Signal that the client is done with this MediaStream. It will be deleted
  // later. Do not mix usage of Destroy() with RegisterUser()/UnregisterUser().
  // That will cause the MediaStream to be destroyed twice, which will cause
  // some assertions to fail.
  virtual void Destroy();
  // Signal that a client is using this MediaStream. Useful to not have to
  // explicitly manage ownership (responsibility to Destroy()) when there are
  // multiple clients using a MediaStream.
  void RegisterUser();
  // Signal that a client no longer needs this MediaStream. When the number of
  // clients using this MediaStream reaches 0, it will be destroyed.
  void UnregisterUser();

  // Returns the main-thread's view of how much data has been processed by
  // this stream.
  StreamTime GetCurrentTime()
  {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    return mMainThreadCurrentTime;
  }
  // Return the main thread's view of whether this stream has finished.
  bool IsFinished()
  {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    return mMainThreadFinished;
  }

  bool IsDestroyed()
  {
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
  StreamTime GetTracksEnd() { return mTracks.GetEnd(); }
#ifdef DEBUG
  void DumpTrackInfo() { return mTracks.DumpTrackInfo(); }
#endif
  void SetAudioOutputVolumeImpl(void* aKey, float aVolume);
  void AddAudioOutputImpl(void* aKey);
  // Returns true if this stream has an audio output.
  bool HasAudioOutput()
  {
    return !mAudioOutputs.IsEmpty();
  }
  void RemoveAudioOutputImpl(void* aKey);
  void AddVideoOutputImpl(already_AddRefed<MediaStreamVideoSink> aSink,
                          TrackID aID);
  void RemoveVideoOutputImpl(MediaStreamVideoSink* aSink, TrackID aID);
  void AddListenerImpl(already_AddRefed<MediaStreamListener> aListener);
  void RemoveListenerImpl(MediaStreamListener* aListener);
  void RemoveAllListenersImpl();
  virtual void AddTrackListenerImpl(already_AddRefed<MediaStreamTrackListener> aListener,
                                    TrackID aTrackID);
  virtual void RemoveTrackListenerImpl(MediaStreamTrackListener* aListener,
                                       TrackID aTrackID);
  virtual void AddDirectTrackListenerImpl(already_AddRefed<DirectMediaStreamTrackListener> aListener,
                                          TrackID aTrackID);
  virtual void RemoveDirectTrackListenerImpl(DirectMediaStreamTrackListener* aListener,
                                             TrackID aTrackID);
  virtual void SetTrackEnabledImpl(TrackID aTrackID, DisabledTrackMode aMode);
  DisabledTrackMode GetDisabledTrackMode(TrackID aTrackID);

  void AddConsumer(MediaInputPort* aPort)
  {
    mConsumers.AppendElement(aPort);
  }
  void RemoveConsumer(MediaInputPort* aPort)
  {
    mConsumers.RemoveElement(aPort);
  }
  uint32_t ConsumerCount()
  {
    return mConsumers.Length();
  }
  StreamTracks& GetStreamTracks() { return mTracks; }
  GraphTime GetStreamTracksStartTime() { return mTracksStartTime; }

  double StreamTimeToSeconds(StreamTime aTime)
  {
    NS_ASSERTION(0 <= aTime && aTime <= STREAM_TIME_MAX, "Bad time");
    return static_cast<double>(aTime)/mTracks.GraphRate();
  }
  int64_t StreamTimeToMicroseconds(StreamTime aTime)
  {
    NS_ASSERTION(0 <= aTime && aTime <= STREAM_TIME_MAX, "Bad time");
    return (aTime*1000000)/mTracks.GraphRate();
  }
  StreamTime SecondsToNearestStreamTime(double aSeconds)
  {
    NS_ASSERTION(0 <= aSeconds && aSeconds <= TRACK_TICKS_MAX/TRACK_RATE_MAX,
                 "Bad seconds");
    return mTracks.GraphRate() * aSeconds + 0.5;
  }
  StreamTime MicrosecondsToStreamTimeRoundDown(int64_t aMicroseconds) {
    return (aMicroseconds*mTracks.GraphRate())/1000000;
  }

  TrackTicks TimeToTicksRoundUp(TrackRate aRate, StreamTime aTime)
  {
    return RateConvertTicksRoundUp(aRate, mTracks.GraphRate(), aTime);
  }
  StreamTime TicksToTimeRoundDown(TrackRate aRate, TrackTicks aTicks)
  {
    return RateConvertTicksRoundDown(mTracks.GraphRate(), aRate, aTicks);
  }
  /**
   * Convert graph time to stream time. aTime must be <= mStateComputedTime
   * to ensure we know exactly how much time this stream will be blocked during
   * the interval.
   */
  StreamTime GraphTimeToStreamTimeWithBlocking(GraphTime aTime);
  /**
   * Convert graph time to stream time. This assumes there is no blocking time
   * to take account of, which is always true except between a stream
   * having its blocking time calculated in UpdateGraph and its blocking time
   * taken account of in UpdateCurrentTimeForStreams.
   */
  StreamTime GraphTimeToStreamTime(GraphTime aTime);
  /**
   * Convert stream time to graph time. This assumes there is no blocking time
   * to take account of, which is always true except between a stream
   * having its blocking time calculated in UpdateGraph and its blocking time
   * taken account of in UpdateCurrentTimeForStreams.
   */
  GraphTime StreamTimeToGraphTime(StreamTime aTime);

  bool IsFinishedOnGraphThread() { return mFinished; }
  void FinishOnGraphThread();

  bool HasCurrentData() { return mHasCurrentData; }

  /**
   * Find track by track id.
   */
  StreamTracks::Track* FindTrack(TrackID aID);

  StreamTracks::Track* EnsureTrack(TrackID aTrack);

  virtual void ApplyTrackDisabling(TrackID aTrackID, MediaSegment* aSegment, MediaSegment* aRawSegment = nullptr);

  // Return true if the main thread needs to observe updates from this stream.
  virtual bool MainThreadNeedsUpdates() const
  {
    return true;
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  void SetAudioChannelType(dom::AudioChannel aType) { mAudioChannelType = aType; }
  dom::AudioChannel AudioChannelType() const { return mAudioChannelType; }

  bool IsSuspended() { return mSuspendedCount > 0; }
  void IncrementSuspendCount();
  void DecrementSuspendCount();

protected:
  // |AdvanceTimeVaryingValuesToCurrentTime| will be override in SourceMediaStream.
  virtual void AdvanceTimeVaryingValuesToCurrentTime(GraphTime aCurrentTime,
                                                     GraphTime aBlockedTime)
  {
    mTracksStartTime += aBlockedTime;
    mTracks.ForgetUpTo(aCurrentTime - mTracksStartTime);
  }

  void NotifyMainThreadListeners()
  {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");

    for (int32_t i = mMainThreadListeners.Length() - 1; i >= 0; --i) {
      mMainThreadListeners[i]->NotifyMainThreadStreamFinished();
    }
    mMainThreadListeners.Clear();
  }

  bool ShouldNotifyStreamFinished()
  {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    if (!mMainThreadFinished || mFinishedNotificationSent) {
      return false;
    }

    mFinishedNotificationSent = true;
    return true;
  }

  // This state is all initialized on the main thread but
  // otherwise modified only on the media graph thread.

  // Buffered data. The start of the buffer corresponds to mTracksStartTime.
  // Conceptually the buffer contains everything this stream has ever played,
  // but we forget some prefix of the buffered data to bound the space usage.
  StreamTracks mTracks;
  // The time when the buffered data could be considered to have started playing.
  // This increases over time to account for time the stream was blocked before
  // mCurrentTime.
  GraphTime mTracksStartTime;

  // Client-set volume of this stream
  struct AudioOutput {
    explicit AudioOutput(void* aKey) : mKey(aKey), mVolume(1.0f) {}
    void* mKey;
    float mVolume;
  };
  nsTArray<AudioOutput> mAudioOutputs;
  nsTArray<TrackBound<MediaStreamVideoSink>> mVideoOutputs;
  // We record the last played video frame to avoid playing the frame again
  // with a different frame id.
  VideoFrame mLastPlayedVideoFrame;
  nsTArray<RefPtr<MediaStreamListener> > mListeners;
  nsTArray<TrackBound<MediaStreamTrackListener>> mTrackListeners;
  nsTArray<MainThreadMediaStreamListener*> mMainThreadListeners;
  // List of disabled TrackIDs and their associated disabled mode.
  // They can either by disabled by frames being replaced by black, or by
  // retaining the previous frame.
  nsTArray<DisabledTrack> mDisabledTracks;

  // GraphTime at which this stream starts blocking.
  // This is only valid up to mStateComputedTime. The stream is considered to
  // have not been blocked before mCurrentTime (its mTracksStartTime is increased
  // as necessary to account for that time instead).
  GraphTime mStartBlocking;

  // MediaInputPorts to which this is connected
  nsTArray<MediaInputPort*> mConsumers;

  // Where audio output is going. There is one AudioOutputStream per
  // audio track.
  struct AudioOutputStream
  {
    // When we started audio playback for this track.
    // Add mStream->GetPosition() to find the current audio playback position.
    GraphTime mAudioPlaybackStartTime;
    // Amount of time that we've wanted to play silence because of the stream
    // blocking.
    MediaTime mBlockedAudioTime;
    // Last tick written to the audio output.
    StreamTime mLastTickWritten;
    TrackID mTrackID;
  };
  nsTArray<AudioOutputStream> mAudioOutputStreams;

  /**
   * Number of outstanding suspend operations on this stream. Stream is
   * suspended when this is > 0.
   */
  int32_t mSuspendedCount;

  /**
   * When true, this means the stream will be finished once all
   * buffered data has been consumed.
   */
  bool mFinished;
  /**
   * When true, mFinished is true and we've played all the data in this stream
   * and fired NotifyFinished notifications.
   */
  bool mNotifiedFinished;
  /**
   * When true, the last NotifyBlockingChanged delivered to the listeners
   * indicated that the stream is blocked.
   */
  bool mNotifiedBlocked;
  /**
   * True if some data can be present by this stream if/when it's unblocked.
   * Set by the stream itself on the MediaStreamGraph thread. Only changes
   * from false to true once a stream has data, since we won't
   * unblock it until there's more data.
   */
  bool mHasCurrentData;
  /**
   * True if mHasCurrentData is true and we've notified listeners.
   */
  bool mNotifiedHasCurrentData;

  // Main-thread views of state
  StreamTime mMainThreadCurrentTime;
  bool mMainThreadFinished;
  bool mFinishedNotificationSent;
  bool mMainThreadDestroyed;
  int mNrOfMainThreadUsers;

  // Our media stream graph.  null if destroyed on the graph thread.
  MediaStreamGraphImpl* mGraph;

  dom::AudioChannel mAudioChannelType;
};

/**
 * This is a stream into which a decoder can write audio and video.
 *
 * Audio and video can be written on any thread, but you probably want to
 * always write from the same thread to avoid unexpected interleavings.
 */
class SourceMediaStream : public MediaStream
{
public:
  explicit SourceMediaStream();

  SourceMediaStream* AsSourceStream() override { return this; }

  // Media graph thread only

  // Users of audio inputs go through the stream so it can track when the
  // last stream referencing an input goes away, so it can close the cubeb
  // input.  Also note: callable on any thread (though it bounces through
  // MainThread to set the command if needed).
  nsresult OpenAudioInput(int aID,
                          AudioDataListener *aListener);
  // Note: also implied when Destroy() happens
  void CloseAudioInput();

  void DestroyImpl() override;

  // Call these on any thread.
  /**
   * Enable or disable pulling. When pulling is enabled, NotifyPull
   * gets called on MediaStreamListeners for this stream during the
   * MediaStreamGraph control loop. Pulling is initially disabled.
   * Due to unavoidable race conditions, after a call to SetPullEnabled(false)
   * it is still possible for a NotifyPull to occur.
   */
  void SetPullEnabled(bool aEnabled);

  /**
   * These add/remove DirectListeners, which allow bypassing the graph and any
   * synchronization delays for e.g. PeerConnection, which wants the data ASAP
   * and lets the far-end handle sync and playout timing.
   */
  void NotifyListenersEventImpl(MediaStreamGraphEvent aEvent);
  void NotifyListenersEvent(MediaStreamGraphEvent aEvent);
  void AddDirectListener(DirectMediaStreamListener* aListener);
  void RemoveDirectListener(DirectMediaStreamListener* aListener);

  enum {
    ADDTRACK_QUEUED    = 0x01 // Queue track add until FinishAddTracks()
  };
  /**
   * Add a new track to the stream starting at the given base time (which
   * must be greater than or equal to the last time passed to
   * AdvanceKnownTracksTime). Takes ownership of aSegment. aSegment should
   * contain data starting after aStart.
   */
  void AddTrack(TrackID aID, StreamTime aStart, MediaSegment* aSegment,
                uint32_t aFlags = 0)
  {
    AddTrackInternal(aID, GraphRate(), aStart, aSegment, aFlags);
  }

  /**
   * Like AddTrack, but resamples audio from aRate to the graph rate.
   */
  void AddAudioTrack(TrackID aID, TrackRate aRate, StreamTime aStart,
                     AudioSegment* aSegment, uint32_t aFlags = 0);

  /**
   * Call after a series of AddTrack or AddAudioTrack calls to implement
   * any pending track adds.
   */
  void FinishAddTracks();

  /**
   * Append media data to a track. Ownership of aSegment remains with the caller,
   * but aSegment is emptied.
   * Returns false if the data was not appended because no such track exists
   * or the stream was already finished.
   */
  bool AppendToTrack(TrackID aID, MediaSegment* aSegment, MediaSegment *aRawSegment = nullptr);
  /**
   * Get the stream time of the end of the data that has been appended so far.
   * Can be called from any thread but won't be useful if it can race with
   * an AppendToTrack call, so should probably just be called from the thread
   * that also calls AppendToTrack.
   */
  StreamTime GetEndOfAppendedData(TrackID aID);
  /**
   * Indicate that a track has ended. Do not do any more API calls
   * affecting this track.
   * Ignored if the track does not exist.
   */
  void EndTrack(TrackID aID);
  /**
   * Indicate that no tracks will be added starting before time aKnownTime.
   * aKnownTime must be >= its value at the last call to AdvanceKnownTracksTime.
   */
  void AdvanceKnownTracksTime(StreamTime aKnownTime);
  /**
   * Indicate that this stream should enter the "finished" state. All tracks
   * must have been ended via EndTrack. The finish time of the stream is
   * when all tracks have ended.
   */
  void FinishWithLockHeld();
  void Finish()
  {
    MutexAutoLock lock(mMutex);
    FinishWithLockHeld();
  }

  // Overriding allows us to hold the mMutex lock while changing the track enable status
  void SetTrackEnabledImpl(TrackID aTrackID, DisabledTrackMode aMode) override;

  // Overriding allows us to ensure mMutex is locked while changing the track enable status
  void
  ApplyTrackDisabling(TrackID aTrackID, MediaSegment* aSegment,
                      MediaSegment* aRawSegment = nullptr) override {
    mMutex.AssertCurrentThreadOwns();
    MediaStream::ApplyTrackDisabling(aTrackID, aSegment, aRawSegment);
  }

  /**
   * End all tracks and Finish() this stream.  Used to voluntarily revoke access
   * to a LocalMediaStream.
   */
  void EndAllTrackAndFinish();

  void RegisterForAudioMixing();

  /**
   * Returns true if this SourceMediaStream contains at least one audio track
   * that is in pending state.
   * This is thread safe, and takes the SourceMediaStream mutex.
   */
  bool HasPendingAudioTrack();

  TimeStamp GetStreamTracksStrartTimeStamp() {
    MutexAutoLock lock(mMutex);
    return mStreamTracksStartTimeStamp;
  }

  // XXX need a Reset API

  friend class MediaStreamGraphImpl;

protected:
  enum TrackCommands : uint32_t;

  virtual ~SourceMediaStream();

  /**
   * Data for each track that hasn't ended.
   */
  struct TrackData {
    TrackID mID;
    // Sample rate of the input data.
    TrackRate mInputRate;
    // Resampler if the rate of the input track does not match the
    // MediaStreamGraph's.
    nsAutoRef<SpeexResamplerState> mResampler;
    int mResamplerChannelCount;
    StreamTime mStart;
    // End-time of data already flushed to the track (excluding mData)
    StreamTime mEndOfFlushedData;
    // Each time the track updates are flushed to the media graph thread,
    // the segment buffer is emptied.
    nsAutoPtr<MediaSegment> mData;
    // Each time the track updates are flushed to the media graph thread,
    // this is cleared.
    uint32_t mCommands;
  };

  bool NeedsMixing();

  void ResampleAudioToGraphSampleRate(TrackData* aTrackData, MediaSegment* aSegment);

  void AddDirectTrackListenerImpl(already_AddRefed<DirectMediaStreamTrackListener> aListener,
                                  TrackID aTrackID) override;
  void RemoveDirectTrackListenerImpl(DirectMediaStreamTrackListener* aListener,
                                     TrackID aTrackID) override;

  void AddTrackInternal(TrackID aID, TrackRate aRate,
                        StreamTime aStart, MediaSegment* aSegment,
                        uint32_t aFlags);

  TrackData* FindDataForTrack(TrackID aID)
  {
    mMutex.AssertCurrentThreadOwns();
    for (uint32_t i = 0; i < mUpdateTracks.Length(); ++i) {
      if (mUpdateTracks[i].mID == aID) {
        return &mUpdateTracks[i];
      }
    }
    return nullptr;
  }

  /**
   * Notify direct consumers of new data to one of the stream tracks.
   * The data doesn't have to be resampled (though it may be).  This is called
   * from AppendToTrack on the thread providing the data, and will call
   * the Listeners on this thread.
   */
  void NotifyDirectConsumers(TrackData *aTrack,
                             MediaSegment *aSegment);

  virtual void
  AdvanceTimeVaryingValuesToCurrentTime(GraphTime aCurrentTime,
                                        GraphTime aBlockedTime) override;
  void SetStreamTracksStartTimeStamp(const TimeStamp& aTimeStamp)
  {
    MutexAutoLock lock(mMutex);
    mStreamTracksStartTimeStamp = aTimeStamp;
  }

  // Only accessed on the MSG thread.  Used so to ask the MSGImpl to usecount
  // users of a specific input.
  // XXX Should really be a CubebUtils::AudioDeviceID, but they aren't
  // copyable (opaque pointers)
  RefPtr<AudioDataListener> mInputListener;

  // This must be acquired *before* MediaStreamGraphImpl's lock, if they are
  // held together.
  Mutex mMutex;
  // protected by mMutex
  StreamTime mUpdateKnownTracksTime;
  // This time stamp will be updated in adding and blocked SourceMediaStream,
  // |AddStreamGraphThread| and |AdvanceTimeVaryingValuesToCurrentTime| in
  // particularly.
  TimeStamp mStreamTracksStartTimeStamp;
  nsTArray<TrackData> mUpdateTracks;
  nsTArray<TrackData> mPendingTracks;
  nsTArray<RefPtr<DirectMediaStreamListener>> mDirectListeners;
  nsTArray<TrackBound<DirectMediaStreamTrackListener>> mDirectTrackListeners;
  bool mPullEnabled;
  bool mUpdateFinished;
  bool mNeedsMixing;
};

/**
 * The blocking mode decides how a track should be blocked in a MediaInputPort.
 */
enum class BlockingMode
{
  /**
   * BlockingMode CREATION blocks the source track from being created
   * in the destination. It'll end if it already exists.
   */
  CREATION,
  /**
   * BlockingMode END_EXISTING allows a track to be created in the destination
   * but will end it before any data has been passed through.
   */
  END_EXISTING,
};

/**
 * Represents a connection between a ProcessedMediaStream and one of its
 * input streams.
 * We make these refcounted so that stream-related messages with MediaInputPort*
 * pointers can be sent to the main thread safely.
 *
 * A port can be locked to a specific track in the source stream, in which case
 * only this track will be forwarded to the destination stream. TRACK_ANY
 * can used to signal that all tracks shall be forwarded.
 *
 * When a port is locked to a specific track in the source stream, it may also
 * indicate a TrackID to map this source track to in the destination stream
 * by setting aDestTrack to an explicit ID. When we do this, we must know
 * that this TrackID in the destination stream is available. We assert during
 * processing that the ID is available and that there are no generic input
 * ports already attached to the destination stream.
 * Note that this is currently only handled by TrackUnionStreams.
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
class MediaInputPort final
{
private:
  // Do not call this constructor directly. Instead call aDest->AllocateInputPort.
  MediaInputPort(MediaStream* aSource, TrackID& aSourceTrack,
                 ProcessedMediaStream* aDest, TrackID& aDestTrack,
                 uint16_t aInputNumber, uint16_t aOutputNumber)
    : mSource(aSource)
    , mSourceTrack(aSourceTrack)
    , mDest(aDest)
    , mDestTrack(aDestTrack)
    , mInputNumber(aInputNumber)
    , mOutputNumber(aOutputNumber)
    , mGraph(nullptr)
  {
    MOZ_COUNT_CTOR(MediaInputPort);
  }

  // Private destructor, to discourage deletion outside of Release():
  ~MediaInputPort()
  {
    MOZ_COUNT_DTOR(MediaInputPort);
  }

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
  MediaStream* GetSource() { return mSource; }
  TrackID GetSourceTrackId() { return mSourceTrack; }
  ProcessedMediaStream* GetDestination() { return mDest; }
  TrackID GetDestinationTrackId() { return mDestTrack; }

  /**
   * Block aTrackId in the source stream from being passed through the port.
   * Consumers will interpret this track as ended.
   * Returns a pledge that resolves on the main thread after the track block has
   * been applied by the MSG.
   */
  already_AddRefed<media::Pledge<bool, nsresult>> BlockSourceTrackId(TrackID aTrackId,
                                                                     BlockingMode aBlockingMode);
private:
  void BlockSourceTrackIdImpl(TrackID aTrackId, BlockingMode aBlockingMode);

public:
  // Returns true if aTrackId has not been blocked for any reason and this port
  // has not been locked to another track.
  bool PassTrackThrough(TrackID aTrackId) {
    bool blocked = false;
    for (auto pair : mBlockedTracks) {
      if (pair.first() == aTrackId &&
          (pair.second() == BlockingMode::CREATION ||
           pair.second() == BlockingMode::END_EXISTING)) {
        blocked = true;
        break;
      }
    }
    return !blocked && (mSourceTrack == TRACK_ANY || mSourceTrack == aTrackId);
  }

  // Returns true if aTrackId has not been blocked for track creation and this
  // port has not been locked to another track.
  bool AllowCreationOf(TrackID aTrackId) {
    bool blocked = false;
    for (auto pair : mBlockedTracks) {
      if (pair.first() == aTrackId &&
          pair.second() == BlockingMode::CREATION) {
        blocked = true;
        break;
      }
    }
    return !blocked && (mSourceTrack == TRACK_ANY || mSourceTrack == aTrackId);
  }

  uint16_t InputNumber() const { return mInputNumber; }
  uint16_t OutputNumber() const { return mOutputNumber; }

  // Call on graph manager thread
  struct InputInterval {
    GraphTime mStart;
    GraphTime mEnd;
    bool mInputIsBlocked;
  };
  // Find the next time interval starting at or after aTime during which
  // mDest is not blocked and mSource's blocking status does not change.
  InputInterval GetNextInputInterval(GraphTime aTime);

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

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t amount = 0;

    // Not owned:
    // - mSource
    // - mDest
    // - mGraph
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  friend class MediaStreamGraphImpl;
  friend class MediaStream;
  friend class ProcessedMediaStream;
  // Never modified after Init()
  MediaStream* mSource;
  TrackID mSourceTrack;
  ProcessedMediaStream* mDest;
  TrackID mDestTrack;
  // The input and output numbers are optional, and are currently only used by
  // Web Audio.
  const uint16_t mInputNumber;
  const uint16_t mOutputNumber;

  typedef Pair<TrackID, BlockingMode> BlockedTrack;
  nsTArray<BlockedTrack> mBlockedTracks;

  // Our media stream graph
  MediaStreamGraphImpl* mGraph;
};

/**
 * This stream processes zero or more input streams in parallel to produce
 * its output. The details of how the output is produced are handled by
 * subclasses overriding the ProcessInput method.
 */
class ProcessedMediaStream : public MediaStream
{
public:
  explicit ProcessedMediaStream()
    : MediaStream(), mAutofinish(false), mCycleMarker(0)
  {}

  // Control API.
  /**
   * Allocates a new input port attached to source aStream.
   * This stream can be removed by calling MediaInputPort::Remove().
   *
   * The input port is tied to aTrackID in the source stream.
   * aTrackID can be set to TRACK_ANY to automatically forward all tracks from
   * aStream.
   *
   * If aTrackID is an explicit ID, aDestTrackID can also be made explicit
   * to ensure that the track is assigned this ID in the destination stream.
   * To avoid intermittent TrackID collisions the destination stream may not
   * have any existing generic input ports (with TRACK_ANY source track) when
   * you allocate an input port with a destination TrackID.
   *
   * To end a track in the destination stream forwarded with TRACK_ANY,
   * it can be blocked in the input port through MediaInputPort::BlockTrackId().
   *
   * Tracks in aBlockedTracks will be blocked in the input port initially. This
   * ensures that they don't get created by the MSG-thread before we can
   * BlockTrackId() on the main thread.
   */
  already_AddRefed<MediaInputPort>
  AllocateInputPort(MediaStream* aStream,
                    TrackID aTrackID = TRACK_ANY,
                    TrackID aDestTrackID = TRACK_ANY,
                    uint16_t aInputNumber = 0,
                    uint16_t aOutputNumber = 0,
                    nsTArray<TrackID>* aBlockedTracks = nullptr);
  /**
   * Force this stream into the finished state.
   */
  void Finish();
  /**
   * Set the autofinish flag on this stream (defaults to false). When this flag
   * is set, and all input streams are in the finished state (including if there
   * are no input streams), this stream automatically enters the finished state.
   */
  void SetAutofinish(bool aAutofinish);

  ProcessedMediaStream* AsProcessedStream() override { return this; }

  friend class MediaStreamGraphImpl;

  // Do not call these from outside MediaStreamGraph.cpp!
  virtual void AddInput(MediaInputPort* aPort);
  virtual void RemoveInput(MediaInputPort* aPort)
  {
    mInputs.RemoveElement(aPort) || mSuspendedInputs.RemoveElement(aPort);
  }
  bool HasInputPort(MediaInputPort* aPort)
  {
    return mInputs.Contains(aPort) || mSuspendedInputs.Contains(aPort);
  }
  uint32_t InputPortCount()
  {
    return mInputs.Length() + mSuspendedInputs.Length();
  }
  void InputSuspended(MediaInputPort* aPort);
  void InputResumed(MediaInputPort* aPort);
  virtual MediaStream* GetInputStreamFor(TrackID aTrackID) { return nullptr; }
  virtual TrackID GetInputTrackIDFor(TrackID aTrackID) { return TRACK_NONE; }
  void DestroyImpl() override;
  /**
   * This gets called after we've computed the blocking states for all
   * streams (mBlocked is up to date up to mStateComputedTime).
   * Also, we've produced output for all streams up to this one. If this stream
   * is not in a cycle, then all its source streams have produced data.
   * Generate output from aFrom to aTo.
   * This will be called on streams that have finished. Most stream types should
   * just return immediately if IsFinishedOnGraphThread(), but some may wish to
   * update internal state (see AudioNodeStream).
   * ProcessInput is allowed to call FinishOnGraphThread only if ALLOW_FINISH
   * is in aFlags. (This flag will be set when aTo >= mStateComputedTime, i.e.
   * when we've producing the last block of data we need to produce.) Otherwise
   * we can get into a situation where we've determined the stream should not
   * block before mStateComputedTime, but the stream finishes before
   * mStateComputedTime, violating the invariant that finished streams are blocked.
   */
  enum {
    ALLOW_FINISH = 0x01
  };
  virtual void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) = 0;
  void SetAutofinishImpl(bool aAutofinish) { mAutofinish = aAutofinish; }

  // Only valid after MediaStreamGraphImpl::UpdateStreamOrder() has run.
  // A DelayNode is considered to break a cycle and so this will not return
  // true for echo loops, only for muted cycles.
  bool InMutedCycle() const { return mCycleMarker; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = MediaStream::SizeOfExcludingThis(aMallocSizeOf);
    // Not owned:
    // - mInputs elements
    // - mSuspendedInputs elements
    amount += mInputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
    amount += mSuspendedInputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  // This state is all accessed only on the media graph thread.

  // The list of all inputs that are not currently suspended.
  nsTArray<MediaInputPort*> mInputs;
  // The list of all inputs that are currently suspended.
  nsTArray<MediaInputPort*> mSuspendedInputs;
  bool mAutofinish;
  // After UpdateStreamOrder(), mCycleMarker is either 0 or 1 to indicate
  // whether this stream is in a muted cycle.  During ordering it can contain
  // other marker values - see MediaStreamGraphImpl::UpdateStreamOrder().
  uint32_t mCycleMarker;
};

/**
 * There can be multiple MediaStreamGraph per process: one per AudioChannel.
 * Additionaly, each OfflineAudioContext object creates its own MediaStreamGraph
 * object too..
 */
class MediaStreamGraph
{
public:

  // We ensure that the graph current time advances in multiples of
  // IdealAudioBlockSize()/AudioStream::PreferredSampleRate(). A stream that
  // never blocks and has a track with the ideal audio rate will produce audio
  // in multiples of the block size.

  // Initializing an graph that outputs audio can be quite long on some
  // platforms. Code that want to output audio at some point can express the
  // fact that they will need an audio stream at some point by passing
  // AUDIO_THREAD_DRIVER when getting an instance of MediaStreamGraph, so that
  // the graph starts with the right driver.
  enum GraphDriverType {
    AUDIO_THREAD_DRIVER,
    SYSTEM_THREAD_DRIVER,
    OFFLINE_THREAD_DRIVER
  };
  static const uint32_t AUDIO_CALLBACK_DRIVER_SHUTDOWN_TIMEOUT = 20*1000;

  // Main thread only
  static MediaStreamGraph* GetInstance(GraphDriverType aGraphDriverRequested,
                                       dom::AudioChannel aChannel);
  static MediaStreamGraph* CreateNonRealtimeInstance(TrackRate aSampleRate);
  // Idempotent
  static void DestroyNonRealtimeInstance(MediaStreamGraph* aGraph);

  virtual nsresult OpenAudioInput(int aID,
                                  AudioDataListener *aListener) {
    return NS_ERROR_FAILURE;
  }
  virtual void CloseAudioInput(AudioDataListener *aListener) {}

  // Control API.
  /**
   * Create a stream that a media decoder (or some other source of
   * media data, such as a camera) can write to.
   */
  SourceMediaStream* CreateSourceStream();
  /**
   * Create a stream that will form the union of the tracks of its input
   * streams.
   * A TrackUnionStream contains all the tracks of all its input streams.
   * Adding a new input stream makes that stream's tracks immediately appear as new
   * tracks starting at the time the input stream was added.
   * Removing an input stream makes the output tracks corresponding to the
   * removed tracks immediately end.
   * For each added track, the track ID of the output track is the track ID
   * of the input track or one plus the maximum ID of all previously added
   * tracks, whichever is greater.
   * TODO at some point we will probably need to add API to select
   * particular tracks of each input stream.
   */
  ProcessedMediaStream* CreateTrackUnionStream();
  /**
   * Create a stream that will mix all its audio input.
   */
  ProcessedMediaStream* CreateAudioCaptureStream(TrackID aTrackId);

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
                                  void* aPromise);

  bool IsNonRealtime() const;
  /**
   * Start processing non-realtime for a specific number of ticks.
   */
  void StartNonRealtimeProcessing(uint32_t aTicksToProcess);

  /**
   * Media graph thread only.
   * Dispatches a runnable that will run on the main thread after all
   * main-thread stream state has been next updated.
   * Should only be called during MediaStreamListener callbacks or during
   * ProcessedMediaStream::ProcessInput().
   */
  virtual void DispatchToMainThreadAfterStreamStateUpdate(already_AddRefed<nsIRunnable> aRunnable)
  {
    AssertOnGraphThreadOrNotRunning();
    *mPendingUpdateRunnables.AppendElement() = aRunnable;
  }

  /**
   * Returns graph sample rate in Hz.
   */
  TrackRate GraphRate() const { return mSampleRate; }

  void RegisterCaptureStreamForWindow(uint64_t aWindowId,
                                      ProcessedMediaStream* aCaptureStream);
  void UnregisterCaptureStreamForWindow(uint64_t aWindowId);
  already_AddRefed<MediaInputPort> ConnectToCaptureStream(
    uint64_t aWindowId, MediaStream* aMediaStream);

  /**
   * Data going to the speakers from the GraphDriver's DataCallback
   * to notify any listeners (for echo cancellation).
   */
  void NotifyOutputData(AudioDataValue* aBuffer, size_t aFrames,
                        TrackRate aRate, uint32_t aChannels);

  void AssertOnGraphThreadOrNotRunning() const;

protected:
  explicit MediaStreamGraph(TrackRate aSampleRate)
    : mSampleRate(aSampleRate)
  {
    MOZ_COUNT_CTOR(MediaStreamGraph);
  }
  virtual ~MediaStreamGraph()
  {
    MOZ_COUNT_DTOR(MediaStreamGraph);
  }

  // Media graph thread only
  nsTArray<nsCOMPtr<nsIRunnable> > mPendingUpdateRunnables;

  /**
   * Sample rate at which this graph runs. For real time graphs, this is
   * the rate of the audio mixer. For offline graphs, this is the rate specified
   * at construction.
   */
  TrackRate mSampleRate;

  /**
   * CloseAudioInput is async, so hold a reference here.
   */
  nsTArray<RefPtr<AudioDataListener>> mAudioInputs;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASTREAMGRAPH_H_ */
