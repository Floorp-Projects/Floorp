/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMGRAPH_H_
#define MOZILLA_MEDIASTREAMGRAPH_H_

#include "mozilla/Mutex.h"
#include "mozilla/LinkedList.h"
#include "AudioStream.h"
#include "nsTArray.h"
#include "nsIRunnable.h"
#include "StreamBuffer.h"
#include "TimeVarying.h"
#include "VideoFrameContainer.h"
#include "VideoSegment.h"
#include "MainThreadUtils.h"

class nsIRunnable;

namespace mozilla {

class DOMMediaStream;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaStreamGraphLog;
#endif

/**
 * Microseconds relative to the start of the graph timeline.
 */
typedef int64_t GraphTime;
const GraphTime GRAPH_TIME_MAX = MEDIA_TIME_MAX;

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

class MediaStreamGraph;

/**
 * This is a base class for media graph thread listener callbacks.
 * Override methods to be notified of audio or video data or changes in stream
 * state.
 *
 * This can be used by stream recorders or network connections that receive
 * stream input. It could also be used for debugging.
 *
 * All notification methods are called from the media graph thread. Overriders
 * of these methods are responsible for all synchronization. Beware!
 * These methods are called without the media graph monitor held, so
 * reentry into media graph methods is possible, although very much discouraged!
 * You should do something non-blocking and non-reentrant (e.g. dispatch an
 * event to some thread) and return.
 * The listener is not allowed to add/remove any listeners from the stream.
 *
 * When a listener is first attached, we guarantee to send a NotifyBlockingChanged
 * callback to notify of the initial blocking state. Also, if a listener is
 * attached to a stream that has already finished, we'll call NotifyFinished.
 */
class MediaStreamListener {
public:
  virtual ~MediaStreamListener() {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaStreamListener)

  enum Consumption {
    CONSUMED,
    NOT_CONSUMED
  };
  /**
   * Notify that the stream is hooked up and we'd like to start or stop receiving
   * data on it. Only fires on SourceMediaStreams.
   * The initial state is assumed to be NOT_CONSUMED.
   */
  virtual void NotifyConsumptionChanged(MediaStreamGraph* aGraph, Consumption aConsuming) {}

  /**
   * When a SourceMediaStream has pulling enabled, and the MediaStreamGraph
   * control loop is ready to pull, this gets called. A NotifyPull implementation
   * is allowed to call the SourceMediaStream methods that alter track
   * data. It is not allowed to make other MediaStream API calls, including
   * calls to add or remove MediaStreamListeners. It is not allowed to block
   * for any length of time.
   * aDesiredTime is the stream time we would like to get data up to. Data
   * beyond this point will not be played until NotifyPull runs again, so there's
   * not much point in providing it. Note that if the stream is blocked for
   * some reason, then data before aDesiredTime may not be played immediately.
   */
  virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime) {}

  enum Blocking {
    BLOCKED,
    UNBLOCKED
  };
  /**
   * Notify that the blocking status of the stream changed. The initial state
   * is assumed to be BLOCKED.
   */
  virtual void NotifyBlockingChanged(MediaStreamGraph* aGraph, Blocking aBlocked) {}

  /**
   * Notify that the stream has data in each track
   * for the stream's current time. Once this state becomes true, it will
   * always be true since we block stream time from progressing to times where
   * there isn't data in each track.
   */
  virtual void NotifyHasCurrentData(MediaStreamGraph* aGraph) {}

  /**
   * Notify that the stream output is advancing.
   */
  virtual void NotifyOutput(MediaStreamGraph* aGraph) {}

  /**
   * Notify that the stream finished.
   */
  virtual void NotifyFinished(MediaStreamGraph* aGraph) {}

  /**
   * Notify that your listener has been removed, either due to RemoveListener(),
   * or due to the stream being destroyed.  You will get no further notifications.
   */
  virtual void NotifyRemoved(MediaStreamGraph* aGraph) {}

  enum {
    TRACK_EVENT_CREATED = 0x01,
    TRACK_EVENT_ENDED = 0x02
  };
  /**
   * Notify that changes to one of the stream tracks have been queued.
   * aTrackEvents can be any combination of TRACK_EVENT_CREATED and
   * TRACK_EVENT_ENDED. aQueuedMedia is the data being added to the track
   * at aTrackOffset (relative to the start of the stream).
   */
  virtual void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                        TrackRate aTrackRate,
                                        TrackTicks aTrackOffset,
                                        uint32_t aTrackEvents,
                                        const MediaSegment& aQueuedMedia) {}
};

/**
 * This is a base class for media graph thread listener direct callbacks
 * from within AppendToTrack().  Note that your regular listener will
 * still get NotifyQueuedTrackChanges() callbacks from the MSG thread, so
 * you must be careful to ignore them if AddDirectListener was successful.
 */
class MediaStreamDirectListener : public MediaStreamListener
{
public:
  virtual ~MediaStreamDirectListener() {}

  /*
   * This will be called on any MediaStreamDirectListener added to a
   * a SourceMediaStream when AppendToTrack() is called.  The MediaSegment
   * will be the RawSegment (unresampled) if available in AppendToTrack().
   * Note that NotifyQueuedTrackChanges() calls will also still occur.
   */
  virtual void NotifyRealtimeData(MediaStreamGraph* aGraph, TrackID aID,
                                  TrackRate aTrackRate,
                                  TrackTicks aTrackOffset,
                                  uint32_t aTrackEvents,
                                  const MediaSegment& aMedia) {}
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
  virtual void NotifyMainThreadStateChanged() = 0;
};

class MediaStreamGraphImpl;
class SourceMediaStream;
class ProcessedMediaStream;
class MediaInputPort;
class AudioNodeEngine;
class AudioNodeExternalInputStream;
class AudioNodeStream;
struct AudioChunk;

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
 * Video is played by setting video frames into an VideoFrameContainer at the right
 * time. To ensure video plays in sync with audio, make sure that the same
 * stream is playing both the audio and video.
 *
 * The data in a stream is managed by StreamBuffer. It consists of a set of
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
 * the object). When the Destroy message is processed on the graph
 * manager thread we immediately release the affected objects (disentangling them
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
class MediaStream : public mozilla::LinkedListElement<MediaStream> {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaStream)

  MediaStream(DOMMediaStream* aWrapper);
  virtual ~MediaStream()
  {
    MOZ_COUNT_DTOR(MediaStream);
    NS_ASSERTION(mMainThreadDestroyed, "Should have been destroyed already");
    NS_ASSERTION(mMainThreadListeners.IsEmpty(),
                 "All main thread listeners should have been removed");
  }

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
  // play to multiple VideoFrameContainers.
  // Only the first enabled video track is played.
  virtual void AddVideoOutput(VideoFrameContainer* aContainer);
  virtual void RemoveVideoOutput(VideoFrameContainer* aContainer);
  // Explicitly block. Useful for example if a media element is pausing
  // and we need to stop its stream emitting its buffered data.
  virtual void ChangeExplicitBlockerCount(int32_t aDelta);
  // Events will be dispatched by calling methods of aListener.
  virtual void AddListener(MediaStreamListener* aListener);
  virtual void RemoveListener(MediaStreamListener* aListener);
  // A disabled track has video replaced by black, and audio replaced by
  // silence.
  void SetTrackEnabled(TrackID aTrackID, bool aEnabled);
  // Events will be dispatched by calling methods of aListener. It is the
  // responsibility of the caller to remove aListener before it is destroyed.
  void AddMainThreadListener(MainThreadMediaStreamListener* aListener)
  {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    mMainThreadListeners.AppendElement(aListener);
  }
  // It's safe to call this even if aListener is not currently a listener;
  // the call will be ignored.
  void RemoveMainThreadListener(MainThreadMediaStreamListener* aListener)
  {
    NS_ASSERTION(NS_IsMainThread(), "Call only on main thread");
    mMainThreadListeners.RemoveElement(aListener);
  }
  /**
   * Ensure a runnable will run on the main thread after running all pending
   * updates that were sent from the graph thread or will be sent before the
   * graph thread receives the next graph update.
   *
   * If the graph has been shutdown or destroyed, or if it is non-realtime
   * and has not started, then the runnable will be run
   * synchronously/immediately.  (There are no pending updates in these
   * situations.)
   *
   * Main thread only.
   */
  void RunAfterPendingUpdates(nsRefPtr<nsIRunnable> aRunnable);

  // Signal that the client is done with this MediaStream. It will be deleted later.
  virtual void Destroy();
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

  // media graph thread only
  void Init();
  // These Impl methods perform the core functionality of the control methods
  // above, on the media graph thread.
  /**
   * Stop all stream activity and disconnect it from all inputs and outputs.
   * This must be idempotent.
   */
  virtual void DestroyImpl();
  StreamTime GetBufferEnd() { return mBuffer.GetEnd(); }
#ifdef DEBUG
  void DumpTrackInfo() { return mBuffer.DumpTrackInfo(); }
#endif
  void SetAudioOutputVolumeImpl(void* aKey, float aVolume);
  void AddAudioOutputImpl(void* aKey)
  {
    mAudioOutputs.AppendElement(AudioOutput(aKey));
  }
  void RemoveAudioOutputImpl(void* aKey);
  void AddVideoOutputImpl(already_AddRefed<VideoFrameContainer> aContainer)
  {
    *mVideoOutputs.AppendElement() = aContainer;
  }
  void RemoveVideoOutputImpl(VideoFrameContainer* aContainer)
  {
    mVideoOutputs.RemoveElement(aContainer);
  }
  void ChangeExplicitBlockerCountImpl(GraphTime aTime, int32_t aDelta)
  {
    mExplicitBlockerCount.SetAtAndAfter(aTime, mExplicitBlockerCount.GetAt(aTime) + aDelta);
  }
  void AddListenerImpl(already_AddRefed<MediaStreamListener> aListener);
  void RemoveListenerImpl(MediaStreamListener* aListener);
  void RemoveAllListenersImpl();
  void SetTrackEnabledImpl(TrackID aTrackID, bool aEnabled);
  /**
   * Returns true when this stream requires the contents of its inputs even if
   * its own outputs are not being consumed. This is used to signal inputs to
   * this stream that they are being consumed; when they're not being consumed,
   * we make some optimizations.
   */
  virtual bool IsIntrinsicallyConsumed() const
  {
    return !mAudioOutputs.IsEmpty() || !mVideoOutputs.IsEmpty();
  }

  void AddConsumer(MediaInputPort* aPort)
  {
    mConsumers.AppendElement(aPort);
  }
  void RemoveConsumer(MediaInputPort* aPort)
  {
    mConsumers.RemoveElement(aPort);
  }
  const StreamBuffer& GetStreamBuffer() { return mBuffer; }
  GraphTime GetStreamBufferStartTime() { return mBufferStartTime; }
  /**
   * Convert graph time to stream time. aTime must be <= mStateComputedTime
   * to ensure we know exactly how much time this stream will be blocked during
   * the interval.
   */
  StreamTime GraphTimeToStreamTime(GraphTime aTime);
  /**
   * Convert graph time to stream time. aTime can be > mStateComputedTime,
   * in which case we optimistically assume the stream will not be blocked
   * after mStateComputedTime.
   */
  StreamTime GraphTimeToStreamTimeOptimistic(GraphTime aTime);
  /**
   * Convert stream time to graph time. The result can be > mStateComputedTime,
   * in which case we did the conversion optimistically assuming the stream
   * will not be blocked after mStateComputedTime.
   */
  GraphTime StreamTimeToGraphTime(StreamTime aTime);
  bool IsFinishedOnGraphThread() { return mFinished; }
  void FinishOnGraphThread();
  /**
   * Identify which graph update index we are currently processing.
   */
  int64_t GetProcessingGraphUpdateIndex();

  bool HasCurrentData() { return mHasCurrentData; }

  StreamBuffer::Track* EnsureTrack(TrackID aTrack, TrackRate aSampleRate);

  void ApplyTrackDisabling(TrackID aTrackID, MediaSegment* aSegment, MediaSegment* aRawSegment = nullptr);

  DOMMediaStream* GetWrapper()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only use DOMMediaStream on main thread");
    return mWrapper;
  }

  // Return true if the main thread needs to observe updates from this stream.
  virtual bool MainThreadNeedsUpdates() const
  {
    return true;
  }

protected:
  virtual void AdvanceTimeVaryingValuesToCurrentTime(GraphTime aCurrentTime, GraphTime aBlockedTime)
  {
    mBufferStartTime += aBlockedTime;
    mGraphUpdateIndices.InsertTimeAtStart(aBlockedTime);
    mGraphUpdateIndices.AdvanceCurrentTime(aCurrentTime);
    mExplicitBlockerCount.AdvanceCurrentTime(aCurrentTime);

    mBuffer.ForgetUpTo(aCurrentTime - mBufferStartTime);
  }

  // This state is all initialized on the main thread but
  // otherwise modified only on the media graph thread.

  // Buffered data. The start of the buffer corresponds to mBufferStartTime.
  // Conceptually the buffer contains everything this stream has ever played,
  // but we forget some prefix of the buffered data to bound the space usage.
  StreamBuffer mBuffer;
  // The time when the buffered data could be considered to have started playing.
  // This increases over time to account for time the stream was blocked before
  // mCurrentTime.
  GraphTime mBufferStartTime;

  // Client-set volume of this stream
  struct AudioOutput {
    AudioOutput(void* aKey) : mKey(aKey), mVolume(1.0f) {}
    void* mKey;
    float mVolume;
  };
  nsTArray<AudioOutput> mAudioOutputs;
  nsTArray<nsRefPtr<VideoFrameContainer> > mVideoOutputs;
  // We record the last played video frame to avoid redundant setting
  // of the current video frame.
  VideoFrame mLastPlayedVideoFrame;
  // The number of times this stream has been explicitly blocked by the control
  // API, minus the number of times it has been explicitly unblocked.
  TimeVarying<GraphTime,uint32_t,0> mExplicitBlockerCount;
  nsTArray<nsRefPtr<MediaStreamListener> > mListeners;
  nsTArray<MainThreadMediaStreamListener*> mMainThreadListeners;
  nsTArray<TrackID> mDisabledTrackIDs;

  // Precomputed blocking status (over GraphTime).
  // This is only valid between the graph's mCurrentTime and
  // mStateComputedTime. The stream is considered to have
  // not been blocked before mCurrentTime (its mBufferStartTime is increased
  // as necessary to account for that time instead) --- this avoids us having to
  // record the entire history of the stream's blocking-ness in mBlocked.
  TimeVarying<GraphTime,bool,5> mBlocked;
  // Maps graph time to the graph update that affected this stream at that time
  TimeVarying<GraphTime,int64_t,0> mGraphUpdateIndices;

  // MediaInputPorts to which this is connected
  nsTArray<MediaInputPort*> mConsumers;

  // Where audio output is going. There is one AudioOutputStream per
  // audio track.
  struct AudioOutputStream {
    // When we started audio playback for this track.
    // Add mStream->GetPosition() to find the current audio playback position.
    GraphTime mAudioPlaybackStartTime;
    // Amount of time that we've wanted to play silence because of the stream
    // blocking.
    MediaTime mBlockedAudioTime;
    nsAutoPtr<AudioStream> mStream;
    TrackID mTrackID;
  };
  nsTArray<AudioOutputStream> mAudioOutputStreams;

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

  // Temporary data for ordering streams by dependency graph
  bool mHasBeenOrdered;
  bool mIsOnOrderingStack;
  // True if the stream is being consumed (i.e. has track data being played,
  // or is feeding into some stream that is being consumed).
  bool mIsConsumed;
  // Temporary data for computing blocking status of streams
  // True if we've added this stream to the set of streams we're computing
  // blocking for.
  bool mInBlockingSet;
  // True if this stream should be blocked in this phase.
  bool mBlockInThisPhase;

  // This state is only used on the main thread.
  DOMMediaStream* mWrapper;
  // Main-thread views of state
  StreamTime mMainThreadCurrentTime;
  bool mMainThreadFinished;
  bool mMainThreadDestroyed;

  // Our media stream graph
  MediaStreamGraphImpl* mGraph;
};

/**
 * This is a stream into which a decoder can write audio and video.
 *
 * Audio and video can be written on any thread, but you probably want to
 * always write from the same thread to avoid unexpected interleavings.
 */
class SourceMediaStream : public MediaStream {
public:
  SourceMediaStream(DOMMediaStream* aWrapper) :
    MediaStream(aWrapper),
    mLastConsumptionState(MediaStreamListener::NOT_CONSUMED),
    mMutex("mozilla::media::SourceMediaStream"),
    mUpdateKnownTracksTime(0),
    mPullEnabled(false),
    mUpdateFinished(false), mDestroyed(false)
  {}

  virtual SourceMediaStream* AsSourceStream() { return this; }

  // Media graph thread only
  virtual void DestroyImpl();

  // Call these on any thread.
  /**
   * Enable or disable pulling. When pulling is enabled, NotifyPull
   * gets called on MediaStreamListeners for this stream during the
   * MediaStreamGraph control loop. Pulling is initially disabled.
   * Due to unavoidable race conditions, after a call to SetPullEnabled(false)
   * it is still possible for a NotifyPull to occur.
   */
  void SetPullEnabled(bool aEnabled);

  void AddDirectListener(MediaStreamDirectListener* aListener);
  void RemoveDirectListener(MediaStreamDirectListener* aListener);

  /**
   * Add a new track to the stream starting at the given base time (which
   * must be greater than or equal to the last time passed to
   * AdvanceKnownTracksTime). Takes ownership of aSegment. aSegment should
   * contain data starting after aStart.
   */
  void AddTrack(TrackID aID, TrackRate aRate, TrackTicks aStart,
                MediaSegment* aSegment);
  /**
   * Append media data to a track. Ownership of aSegment remains with the caller,
   * but aSegment is emptied.
   * Returns false if the data was not appended because no such track exists
   * or the stream was already finished.
   */
  bool AppendToTrack(TrackID aID, MediaSegment* aSegment, MediaSegment *aRawSegment = nullptr);
  /**
   * Returns true if the buffer currently has enough data.
   * Returns false if there isn't enough data or if no such track exists.
   */
  bool HaveEnoughBuffered(TrackID aID);
  /**
   * Ensures that aSignalRunnable will be dispatched to aSignalThread
   * when we don't have enough buffered data in the track (which could be
   * immediately). Will dispatch the runnable immediately if the track
   * does not exist.
   */
  void DispatchWhenNotEnoughBuffered(TrackID aID,
      nsIThread* aSignalThread, nsIRunnable* aSignalRunnable);
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
   * when all tracks have ended and when latest time sent to
   * AdvanceKnownTracksTime() has been reached.
   */
  void FinishWithLockHeld();
  void Finish()
    {
      MutexAutoLock lock(mMutex);
      FinishWithLockHeld();
    }

  // Overriding allows us to hold the mMutex lock while changing the track enable status
  void SetTrackEnabledImpl(TrackID aTrackID, bool aEnabled) {
    MutexAutoLock lock(mMutex);
    MediaStream::SetTrackEnabledImpl(aTrackID, aEnabled);
  }

  /**
   * End all tracks and Finish() this stream.  Used to voluntarily revoke access
   * to a LocalMediaStream.
   */
  void EndAllTrackAndFinish();

  /**
   * Note: Only call from Media Graph thread (eg NotifyPull)
   *
   * Returns amount of time (data) that is currently buffered in the track,
   * assuming playout via PlayAudio or via a TrackUnion - note that
   * NotifyQueuedTrackChanges() on a SourceMediaStream will occur without
   * any "extra" buffering, but NotifyQueued TrackChanges() on a TrackUnion
   * will be buffered.
   */
  TrackTicks GetBufferedTicks(TrackID aID);

  // XXX need a Reset API

  friend class MediaStreamGraphImpl;

  struct ThreadAndRunnable {
    void Init(nsIThread* aThread, nsIRunnable* aRunnable)
    {
      mThread = aThread;
      mRunnable = aRunnable;
    }

    nsCOMPtr<nsIThread> mThread;
    nsCOMPtr<nsIRunnable> mRunnable;
  };
  enum TrackCommands {
    TRACK_CREATE = MediaStreamListener::TRACK_EVENT_CREATED,
    TRACK_END = MediaStreamListener::TRACK_EVENT_ENDED
  };
  /**
   * Data for each track that hasn't ended.
   */
  struct TrackData {
    TrackID mID;
    TrackRate mRate;
    TrackTicks mStart;
    // Each time the track updates are flushed to the media graph thread,
    // this is cleared.
    uint32_t mCommands;
    // Each time the track updates are flushed to the media graph thread,
    // the segment buffer is emptied.
    nsAutoPtr<MediaSegment> mData;
    nsTArray<ThreadAndRunnable> mDispatchWhenNotEnough;
    bool mHaveEnough;
  };

protected:
  TrackData* FindDataForTrack(TrackID aID)
  {
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

  // Media stream graph thread only
  MediaStreamListener::Consumption mLastConsumptionState;

  // This must be acquired *before* MediaStreamGraphImpl's lock, if they are
  // held together.
  Mutex mMutex;
  // protected by mMutex
  StreamTime mUpdateKnownTracksTime;
  nsTArray<TrackData> mUpdateTracks;
  nsTArray<nsRefPtr<MediaStreamDirectListener> > mDirectListeners;
  bool mPullEnabled;
  bool mUpdateFinished;
  bool mDestroyed;
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
class MediaInputPort {
  // Do not call this constructor directly. Instead call aDest->AllocateInputPort.
  MediaInputPort(MediaStream* aSource, ProcessedMediaStream* aDest,
                 uint32_t aFlags, uint16_t aInputNumber,
                 uint16_t aOutputNumber)
    : mSource(aSource)
    , mDest(aDest)
    , mFlags(aFlags)
    , mInputNumber(aInputNumber)
    , mOutputNumber(aOutputNumber)
    , mGraph(nullptr)
  {
    MOZ_COUNT_CTOR(MediaInputPort);
  }

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaInputPort)

  /**
   * The FLAG_BLOCK_INPUT and FLAG_BLOCK_OUTPUT flags can be used to control
   * exactly how the blocking statuses of the input and output streams affect
   * each other.
   */
  enum {
    // When set, blocking on the output stream forces blocking on the input
    // stream.
    FLAG_BLOCK_INPUT = 0x01,
    // When set, blocking on the input stream forces blocking on the output
    // stream.
    FLAG_BLOCK_OUTPUT = 0x02
  };
  ~MediaInputPort()
  {
    MOZ_COUNT_DTOR(MediaInputPort);
  }

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
  ProcessedMediaStream* GetDestination() { return mDest; }

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

protected:
  friend class MediaStreamGraphImpl;
  friend class MediaStream;
  friend class ProcessedMediaStream;
  // Never modified after Init()
  MediaStream* mSource;
  ProcessedMediaStream* mDest;
  uint32_t mFlags;
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
 * subclasses overriding the ProduceOutput method.
 */
class ProcessedMediaStream : public MediaStream {
public:
  ProcessedMediaStream(DOMMediaStream* aWrapper)
    : MediaStream(aWrapper), mAutofinish(false), mInCycle(false)
  {}

  // Control API.
  /**
   * Allocates a new input port attached to source aStream.
   * This stream can be removed by calling MediaInputPort::Remove().
   */
  already_AddRefed<MediaInputPort> AllocateInputPort(MediaStream* aStream,
                                                     uint32_t aFlags = 0,
                                                     uint16_t aInputNumber = 0,
                                                     uint16_t aOutputNumber = 0);
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

  virtual ProcessedMediaStream* AsProcessedStream() { return this; }

  friend class MediaStreamGraphImpl;

  // Do not call these from outside MediaStreamGraph.cpp!
  virtual void AddInput(MediaInputPort* aPort);
  virtual void RemoveInput(MediaInputPort* aPort)
  {
    mInputs.RemoveElement(aPort);
  }
  bool HasInputPort(MediaInputPort* aPort)
  {
    return mInputs.Contains(aPort);
  }
  virtual void DestroyImpl();
  /**
   * This gets called after we've computed the blocking states for all
   * streams (mBlocked is up to date up to mStateComputedTime).
   * Also, we've produced output for all streams up to this one. If this stream
   * is not in a cycle, then all its source streams have produced data.
   * Generate output up to mStateComputedTime.
   * This is called only on streams that have not finished.
   */
  virtual void ProduceOutput(GraphTime aFrom, GraphTime aTo) = 0;
  void SetAutofinishImpl(bool aAutofinish) { mAutofinish = aAutofinish; }

  /**
   * Forward SetTrackEnabled() to the input MediaStream(s) and translate the ID
   */
  virtual void ForwardTrackEnabled(TrackID aOutputID, bool aEnabled) {};

  bool InCycle() const { return mInCycle; }


protected:
  // This state is all accessed only on the media graph thread.

  // The list of all inputs that are currently enabled or waiting to be enabled.
  nsTArray<MediaInputPort*> mInputs;
  bool mAutofinish;
  // True if and only if this stream is in a cycle.
  // Updated by MediaStreamGraphImpl::UpdateStreamOrder.
  bool mInCycle;
};

// Returns ideal audio rate for processing
inline TrackRate IdealAudioRate() { return AudioStream::PreferredSampleRate(); }

/**
 * Initially, at least, we will have a singleton MediaStreamGraph per
 * process.  Each OfflineAudioContext object creates its own MediaStreamGraph
 * object too.
 */
class MediaStreamGraph {
public:
  // We ensure that the graph current time advances in multiples of
  // IdealAudioBlockSize()/IdealAudioRate(). A stream that never blocks
  // and has a track with the ideal audio rate will produce audio in
  // multiples of the block size.

  // Main thread only
  static MediaStreamGraph* GetInstance();
  static MediaStreamGraph* CreateNonRealtimeInstance();
  // Idempotent
  static void DestroyNonRealtimeInstance(MediaStreamGraph* aGraph);

  // Control API.
  /**
   * Create a stream that a media decoder (or some other source of
   * media data, such as a camera) can write to.
   */
  SourceMediaStream* CreateSourceStream(DOMMediaStream* aWrapper);
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
  ProcessedMediaStream* CreateTrackUnionStream(DOMMediaStream* aWrapper);
  // Internal AudioNodeStreams can only pass their output to another
  // AudioNode, whereas external AudioNodeStreams can pass their output
  // to an nsAudioStream for playback.
  enum AudioNodeStreamKind { SOURCE_STREAM, INTERNAL_STREAM, EXTERNAL_STREAM };
  /**
   * Create a stream that will process audio for an AudioNode.
   * Takes ownership of aEngine.  aSampleRate is the sampling rate used
   * for the stream.  If 0 is passed, the sampling rate of the engine's
   * node will get used.
   */
  AudioNodeStream* CreateAudioNodeStream(AudioNodeEngine* aEngine,
                                         AudioNodeStreamKind aKind,
                                         TrackRate aSampleRate = 0);

  AudioNodeExternalInputStream*
  CreateAudioNodeExternalInputStream(AudioNodeEngine* aEngine,
                                     TrackRate aSampleRate = 0);

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
   * ProcessedMediaStream::ProduceOutput().
   */
  void DispatchToMainThreadAfterStreamStateUpdate(already_AddRefed<nsIRunnable> aRunnable)
  {
    *mPendingUpdateRunnables.AppendElement() = aRunnable;
  }

protected:
  MediaStreamGraph()
    : mNextGraphUpdateIndex(1)
  {
    MOZ_COUNT_CTOR(MediaStreamGraph);
  }
  virtual ~MediaStreamGraph()
  {
    MOZ_COUNT_DTOR(MediaStreamGraph);
  }

  // Media graph thread only
  nsTArray<nsCOMPtr<nsIRunnable> > mPendingUpdateRunnables;

  // Main thread only
  // The number of updates we have sent to the media graph thread + 1.
  // We start this at 1 just to ensure that 0 is usable as a special value.
  int64_t mNextGraphUpdateIndex;
};

}

#endif /* MOZILLA_MEDIASTREAMGRAPH_H_ */
