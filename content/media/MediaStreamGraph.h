/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMGRAPH_H_
#define MOZILLA_MEDIASTREAMGRAPH_H_

#include "mozilla/Mutex.h"
#include "nsAudioStream.h"
#include "nsTArray.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "StreamBuffer.h"
#include "TimeVarying.h"
#include "VideoFrameContainer.h"
#include "VideoSegment.h"

class nsDOMMediaStream;

namespace mozilla {

/**
 * Microseconds relative to the start of the graph timeline.
 */
typedef PRInt64 GraphTime;
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
 *
 * Streams that use different sampling rates complicate things a lot. We
 * considered forcing all streams to have the same audio sample rate, resampling
 * at inputs and outputs only, but that would create situations where a stream
 * is resampled from X to Y and then back to X unnecessarily. It seems easier
 * to just live with streams having different sample rates. We do require that
 * the sample rate for a stream be constant for the life of a stream.
 *
 * XXX does not yet support blockInput/blockOutput functionality.
 */

class MediaStreamGraph;

/**
 * This is a base class for listener callbacks. Override methods to be
 * notified of audio or video data or changes in stream state.
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
   * Notify that the stream output is advancing.
   */
  virtual void NotifyOutput(MediaStreamGraph* aGraph) {}

  /**
   * Notify that the stream finished.
   */
  virtual void NotifyFinished(MediaStreamGraph* aGraph) {}

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
                                        PRUint32 aTrackEvents,
                                        const MediaSegment& aQueuedMedia) {}
};

class MediaStreamGraphImpl;
class SourceMediaStream;

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
 */
class MediaStream {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaStream)

  MediaStream(nsDOMMediaStream* aWrapper)
    : mBufferStartTime(0)
    , mExplicitBlockerCount(0)
    , mBlocked(false)
    , mGraphUpdateIndices(0)
    , mFinished(false)
    , mNotifiedFinished(false)
    , mAudioPlaybackStartTime(0)
    , mBlockedAudioTime(0)
    , mMessageAffectedTime(0)
    , mWrapper(aWrapper)
    , mMainThreadCurrentTime(0)
    , mMainThreadFinished(false)
  {
    for (PRUint32 i = 0; i < ArrayLength(mFirstActiveTracks); ++i) {
      mFirstActiveTracks[i] = TRACK_NONE;
    }
  }
  virtual ~MediaStream() {}

  /**
   * Returns the graph that owns this stream.
   */
  MediaStreamGraphImpl* GraphImpl();

  // Control API.
  // Since a stream can be played multiple ways, we need to combine independent
  // volume settings. The aKey parameter is used to keep volume settings
  // separate. Since the stream is always playing the same contents, only
  // a single audio output stream is used; the volumes are combined.
  // Currently only the first enabled audio track is played.
  // XXX change this so all enabled audio tracks are mixed and played.
  void AddAudioOutput(void* aKey);
  void SetAudioOutputVolume(void* aKey, float aVolume);
  void RemoveAudioOutput(void* aKey);
  // Since a stream can be played multiple ways, we need to be able to
  // play to multiple VideoFrameContainers.
  // Only the first enabled video track is played.
  void AddVideoOutput(VideoFrameContainer* aContainer);
  void RemoveVideoOutput(VideoFrameContainer* aContainer);
  // Explicitly block. Useful for example if a media element is pausing
  // and we need to stop its stream emitting its buffered data.
  void ChangeExplicitBlockerCount(PRInt32 aDelta);
  // Events will be dispatched by calling methods of aListener.
  void AddListener(MediaStreamListener* aListener);
  void RemoveListener(MediaStreamListener* aListener);
  // Signal that the client is done with this MediaStream. It will be deleted later.
  void Destroy();
  // Returns the main-thread's view of how much data has been processed by
  // this stream.
  StreamTime GetCurrentTime() { return mMainThreadCurrentTime; }
  // Return the main thread's view of whether this stream has finished.
  bool IsFinished() { return mMainThreadFinished; }

  friend class MediaStreamGraphImpl;

  virtual SourceMediaStream* AsSourceStream() { return nsnull; }

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
  void ChangeExplicitBlockerCountImpl(StreamTime aTime, PRInt32 aDelta)
  {
    mExplicitBlockerCount.SetAtAndAfter(aTime, mExplicitBlockerCount.GetAt(aTime) + aDelta);
  }
  void AddListenerImpl(already_AddRefed<MediaStreamListener> aListener);
  void RemoveListenerImpl(MediaStreamListener* aListener)
  {
    mListeners.RemoveElement(aListener);
  }

#ifdef DEBUG
  const StreamBuffer& GetStreamBuffer() { return mBuffer; }
#endif

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
  TimeVarying<GraphTime,PRUint32> mExplicitBlockerCount;
  nsTArray<nsRefPtr<MediaStreamListener> > mListeners;

  // Precomputed blocking status (over GraphTime).
  // This is only valid between the graph's mCurrentTime and
  // mBlockingDecisionsMadeUntilTime. The stream is considered to have
  // not been blocked before mCurrentTime (its mBufferStartTime is increased
  // as necessary to account for that time instead) --- this avoids us having to
  // record the entire history of the stream's blocking-ness in mBlocked.
  TimeVarying<GraphTime,bool> mBlocked;
  // Maps graph time to the graph update that affected this stream at that time
  TimeVarying<GraphTime,PRInt64> mGraphUpdateIndices;

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

  // Where audio output is going
  nsRefPtr<nsAudioStream> mAudioOutput;
  // When we started audio playback for this stream.
  // Add mAudioOutput->GetPosition() to find the current audio playback position.
  GraphTime mAudioPlaybackStartTime;
  // Amount of time that we've wanted to play silence because of the stream
  // blocking.
  MediaTime mBlockedAudioTime;

  // For each track type, this is the first active track found for that type.
  // The first active track is the track that started earliest; if multiple
  // tracks start at the same time, the one with the lowest ID.
  TrackID mFirstActiveTracks[MediaSegment::TYPE_COUNT];

  // Temporary data used by MediaStreamGraph on the graph thread
  // The earliest time for which we would like to change this stream's output.
  GraphTime mMessageAffectedTime;

  // This state is only used on the main thread.
  nsDOMMediaStream* mWrapper;
  // Main-thread views of state
  StreamTime mMainThreadCurrentTime;
  bool mMainThreadFinished;
};

/**
 * This is a stream into which a decoder can write audio and video.
 *
 * Audio and video can be written on any thread, but you probably want to
 * always write from the same thread to avoid unexpected interleavings.
 */
class SourceMediaStream : public MediaStream {
public:
  SourceMediaStream(nsDOMMediaStream* aWrapper) :
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
   */
  void AppendToTrack(TrackID aID, MediaSegment* aSegment);
  /**
   * Returns true if the buffer currently has enough data.
   */
  bool HaveEnoughBuffered(TrackID aID);
  /**
   * Ensures that aSignalRunnable will be dispatched to aSignalThread
   * when we don't have enough buffered data in the track (which could be
   * immediately).
   */
  void DispatchWhenNotEnoughBuffered(TrackID aID,
      nsIThread* aSignalThread, nsIRunnable* aSignalRunnable);
  /**
   * Indicate that a track has ended. Do not do any more API calls
   * affecting this track.
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
  void Finish();

  // XXX need a Reset API

  friend class MediaStreamGraph;
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
    PRUint32 mCommands;
    // Each time the track updates are flushed to the media graph thread,
    // the segment buffer is emptied.
    nsAutoPtr<MediaSegment> mData;
    nsTArray<ThreadAndRunnable> mDispatchWhenNotEnough;
    bool mHaveEnough;
  };

protected:
  TrackData* FindDataForTrack(TrackID aID)
  {
    for (PRUint32 i = 0; i < mUpdateTracks.Length(); ++i) {
      if (mUpdateTracks[i].mID == aID) {
        return &mUpdateTracks[i];
      }
    }
    NS_ERROR("Bad track ID!");
    return nsnull;
  }

  // Media stream graph thread only
  MediaStreamListener::Consumption mLastConsumptionState;

  // This must be acquired *before* MediaStreamGraphImpl's lock, if they are
  // held together.
  Mutex mMutex;
  // protected by mMutex
  StreamTime mUpdateKnownTracksTime;
  nsTArray<TrackData> mUpdateTracks;
  bool mPullEnabled;
  bool mUpdateFinished;
  bool mDestroyed;
};

/**
 * Initially, at least, we will have a singleton MediaStreamGraph per
 * process.
 */
class MediaStreamGraph {
public:
  // Main thread only
  static MediaStreamGraph* GetInstance();
  // Control API.
  /**
   * Create a stream that a media decoder (or some other source of
   * media data, such as a camera) can write to.
   */
  SourceMediaStream* CreateInputStream(nsDOMMediaStream* aWrapper);
  /**
   * Returns the number of graph updates sent. This can be used to track
   * whether a given update has been processed by the graph thread and reflected
   * in main-thread stream state.
   */
  PRInt64 GetCurrentGraphUpdateIndex() { return mGraphUpdatesSent; }

  /**
   * Media graph thread only.
   * Dispatches a runnable that will run on the main thread after all
   * main-thread stream state has been next updated.
   * Should only be called during MediaStreamListener callbacks.
   */
  void DispatchToMainThreadAfterStreamStateUpdate(nsIRunnable* aRunnable)
  {
    mPendingUpdateRunnables.AppendElement(aRunnable);
  }

protected:
  MediaStreamGraph()
    : mGraphUpdatesSent(1)
  {
    MOZ_COUNT_CTOR(MediaStreamGraph);
  }
  ~MediaStreamGraph()
  {
    MOZ_COUNT_DTOR(MediaStreamGraph);
  }

  // Media graph thread only
  nsTArray<nsCOMPtr<nsIRunnable> > mPendingUpdateRunnables;

  // Main thread only
  // The number of updates we have sent to the media graph thread. We start
  // this at 1 just to ensure that 0 is usable as a special value.
  PRInt64 mGraphUpdatesSent;
};

}

#endif /* MOZILLA_MEDIASTREAMGRAPH_H_ */
