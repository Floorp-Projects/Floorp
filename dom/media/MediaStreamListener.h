/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMLISTENER_h_
#define MOZILLA_MEDIASTREAMLISTENER_h_

#include "StreamTracks.h"

namespace mozilla {

class AudioSegment;
class MediaStream;
class MediaStreamGraph;
class VideoSegment;

enum MediaStreamGraphEvent : uint32_t {
  EVENT_FINISHED,
  EVENT_REMOVED,
  EVENT_HAS_DIRECT_LISTENERS, // transition from no direct listeners
  EVENT_HAS_NO_DIRECT_LISTENERS,  // transition to no direct listeners
};

// maskable flags, not a simple enumerated value
enum TrackEventCommand : uint32_t {
  TRACK_EVENT_NONE = 0x00,
  TRACK_EVENT_CREATED = 0x01,
  TRACK_EVENT_ENDED = 0x02,
  TRACK_EVENT_UNUSED = ~(TRACK_EVENT_ENDED | TRACK_EVENT_CREATED),
};

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
protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~MediaStreamListener() {}

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaStreamListener)

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
   * Notify that the stream output is advancing. aCurrentTime is the graph's
   * current time. MediaStream::GraphTimeToStreamTime can be used to get the
   * stream time.
   */
  virtual void NotifyOutput(MediaStreamGraph* aGraph, GraphTime aCurrentTime) {}

  /**
   * Notify that an event has occurred on the Stream
   */
  virtual void NotifyEvent(MediaStreamGraph* aGraph, MediaStreamGraphEvent aEvent) {}

  /**
   * Notify that changes to one of the stream tracks have been queued.
   * aTrackEvents can be any combination of TRACK_EVENT_CREATED and
   * TRACK_EVENT_ENDED. aQueuedMedia is the data being added to the track
   * at aTrackOffset (relative to the start of the stream).
   * aInputStream and aInputTrackID will be set if the changes originated
   * from an input stream's track. In practice they will only be used for
   * ProcessedMediaStreams.
   */
  virtual void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                        StreamTime aTrackOffset,
                                        TrackEventCommand aTrackEvents,
                                        const MediaSegment& aQueuedMedia,
                                        MediaStream* aInputStream = nullptr,
                                        TrackID aInputTrackID = TRACK_INVALID) {}

  /**
   * Notify queued audio data. Only audio data need to be queued. The video data
   * will be notified by MediaStreamVideoSink::SetCurrentFrame.
   */
  virtual void NotifyQueuedAudioData(MediaStreamGraph* aGraph, TrackID aID,
                                     StreamTime aTrackOffset,
                                     const AudioSegment& aQueuedMedia,
                                     MediaStream* aInputStream = nullptr,
                                     TrackID aInputTrackID = TRACK_INVALID) {}

  /**
   * Notify that all new tracks this iteration have been created.
   * This is to ensure that tracks added atomically to MediaStreamGraph
   * are also notified of atomically to MediaStreamListeners.
   */
  virtual void NotifyFinishedTrackCreation(MediaStreamGraph* aGraph) {}
};

/**
 * This is a base class for media graph thread listener callbacks locked to
 * specific tracks. Override methods to be notified of audio or video data or
 * changes in track state.
 *
 * All notification methods are called from the media graph thread. Overriders
 * of these methods are responsible for all synchronization. Beware!
 * These methods are called without the media graph monitor held, so
 * reentry into media graph methods is possible, although very much discouraged!
 * You should do something non-blocking and non-reentrant (e.g. dispatch an
 * event to some thread) and return.
 * The listener is not allowed to add/remove any listeners from the parent
 * stream.
 *
 * If a listener is attached to a track that has already ended, we guarantee
 * to call NotifyEnded.
 */
class MediaStreamTrackListener
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaStreamTrackListener)

public:
  virtual void NotifyQueuedChanges(MediaStreamGraph* aGraph,
                                   StreamTime aTrackOffset,
                                   const MediaSegment& aQueuedMedia) {}

  virtual void NotifyPrincipalHandleChanged(MediaStreamGraph* aGraph,
                                            const PrincipalHandle& aNewPrincipalHandle) {}

  virtual void NotifyEnded() {}

  virtual void NotifyRemoved() {}

protected:
  virtual ~MediaStreamTrackListener() {}
};


/**
 * This is a base class for media graph thread listener direct callbacks
 * from within AppendToTrack(). Note that your regular listener will
 * still get NotifyQueuedTrackChanges() callbacks from the MSG thread, so
 * you must be careful to ignore them if AddDirectListener was successful.
 */
class DirectMediaStreamListener : public MediaStreamListener
{
public:
  virtual ~DirectMediaStreamListener() {}

  /*
   * This will be called on any DirectMediaStreamListener added to a
   * a SourceMediaStream when AppendToTrack() is called.  The MediaSegment
   * will be the RawSegment (unresampled) if available in AppendToTrack().
   * Note that NotifyQueuedTrackChanges() calls will also still occur.
   */
  virtual void NotifyRealtimeData(MediaStreamGraph* aGraph, TrackID aID,
                                  StreamTime aTrackOffset,
                                  uint32_t aTrackEvents,
                                  const MediaSegment& aMedia) {}
};

/**
 * This is a base class for media graph thread listener direct callbacks from
 * within AppendToTrack(). It is bound to a certain track and can only be
 * installed on audio tracks. Once added to a track on any stream in the graph,
 * the graph will try to install it at that track's source of media data.
 *
 * This works for TrackUnionStreams, which will forward the listener to the
 * track's input track if it exists, or wait for it to be created before
 * forwarding if it doesn't.
 * Once it reaches a SourceMediaStream, it can be successfully installed.
 * Other types of streams will fail installation since they are not supported.
 *
 * Note that this listener and others for the same track will still get
 * NotifyQueuedChanges() callbacks from the MSG tread, so you must be careful
 * to ignore them if this listener was successfully installed.
 */
class DirectMediaStreamTrackListener : public MediaStreamTrackListener
{
  friend class SourceMediaStream;
  friend class TrackUnionStream;

public:
  /*
   * This will be called on any DirectMediaStreamTrackListener added to a
   * SourceMediaStream when AppendToTrack() is called for the listener's bound
   * track, using the thread of the AppendToTrack() caller. The MediaSegment
   * will be the RawSegment (unresampled) if available in AppendToTrack().
   * If the track is enabled at the source but has been disabled in one of the
   * streams in between the source and where it was originally added, aMedia
   * will be a disabled version of the one passed to AppendToTrack() as well.
   * Note that NotifyQueuedTrackChanges() calls will also still occur.
   */
  virtual void NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                                       StreamTime aTrackOffset,
                                       const MediaSegment& aMedia) {}

  /**
   * When a direct listener is processed for installation by the
   * MediaStreamGraph it will be notified with whether the installation was
   * successful or not. The results of this installation are the following:
   * TRACK_NOT_FOUND_AT_SOURCE
   *    We found the source stream of media data for this track, but the track
   *    didn't exist. This should only happen if you try to install the listener
   *    directly to a SourceMediaStream that doesn't contain the given TrackID.
   * STREAM_NOT_SUPPORTED
   *    While looking for the data source of this track, we found a MediaStream
   *    that is not a SourceMediaStream or a TrackUnionStream.
   * SUCCESS
   *    Installation was successful and this listener will start receiving
   *    NotifyRealtimeData on the next AppendToTrack().
   */
  enum class InstallationResult {
    TRACK_NOT_FOUND_AT_SOURCE,
    TRACK_TYPE_NOT_SUPPORTED,
    STREAM_NOT_SUPPORTED,
    SUCCESS
  };
  virtual void NotifyDirectListenerInstalled(InstallationResult aResult) {}
  virtual void NotifyDirectListenerUninstalled() {}

protected:
  virtual ~DirectMediaStreamTrackListener() {}

  void MirrorAndDisableSegment(AudioSegment& aFrom, AudioSegment& aTo);
  void MirrorAndDisableSegment(VideoSegment& aFrom, VideoSegment& aTo);
  void NotifyRealtimeTrackDataAndApplyTrackDisabling(MediaStreamGraph* aGraph,
                                                     StreamTime aTrackOffset,
                                                     MediaSegment& aMedia);

  void IncreaseDisabled()
  {
    ++mDisabledCount;
  }
  void DecreaseDisabled()
  {
    --mDisabledCount;
    MOZ_ASSERT(mDisabledCount >= 0, "Double decrease");
  }

  // Matches the number of disabled streams to which this listener is attached.
  // The number of streams are those between the stream the listener was added
  // and the SourceMediaStream that is the input of the data.
  Atomic<int32_t> mDisabledCount;

  nsAutoPtr<MediaSegment> mMedia;
};

} // namespace mozilla

#endif // MOZILLA_MEDIASTREAMLISTENER_h_
