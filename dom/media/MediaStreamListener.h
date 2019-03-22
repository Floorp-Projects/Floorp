/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMLISTENER_h_
#define MOZILLA_MEDIASTREAMLISTENER_h_

#include "StreamTracks.h"

#include "MediaStreamGraph.h"

namespace mozilla {

class AudioSegment;
class MediaStream;
class MediaStreamGraph;
class MediaStreamVideoSink;
class VideoSegment;

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
class MediaStreamTrackListener {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaStreamTrackListener)

 public:
  /**
   * When a SourceMediaStream has pulling enabled, and the MediaStreamGraph
   * control loop is ready to pull, this gets called for each track in the
   * SourceMediaStream that is lacking data for the current iteration.
   * A NotifyPull implementation is allowed to call the SourceMediaStream
   * methods that alter track data.
   *
   * It is not allowed to make other MediaStream API calls, including
   * calls to add or remove MediaStreamTrackListeners. It is not allowed to
   * block for any length of time.
   *
   * aEndOfAppendedData is the duration of the data that has already been
   * appended to this track, in stream time.
   *
   * aDesiredTime is the stream time we should append data up to. Data
   * beyond this point will not be played until NotifyPull runs again, so
   * there's not much point in providing it. Note that if the stream is blocked
   * for some reason, then data before aDesiredTime may not be played
   * immediately.
   */
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          StreamTime aEndOfAppendedData,
                          StreamTime aDesiredTime) {}

  virtual void NotifyQueuedChanges(MediaStreamGraph* aGraph,
                                   StreamTime aTrackOffset,
                                   const MediaSegment& aQueuedMedia) {}

  virtual void NotifyPrincipalHandleChanged(
      MediaStreamGraph* aGraph, const PrincipalHandle& aNewPrincipalHandle) {}

  /**
   * Notify that the enabled state for the track this listener is attached to
   * has changed.
   *
   * The enabled state here is referring to whether audio should be audible
   * (enabled) or silent (not enabled); or whether video should be displayed as
   * is (enabled), or black (not enabled).
   */
  virtual void NotifyEnabledStateChanged(bool aEnabled) {}

  /**
   * Notify that the stream output is advancing. aCurrentTrackTime is the number
   * of samples that has been played out for this track in stream time.
   */
  virtual void NotifyOutput(MediaStreamGraph* aGraph,
                            StreamTime aCurrentTrackTime) {}

  /**
   * Notify that this track has been ended and all data has been played out.
   */
  virtual void NotifyEnded() {}

  /**
   * Notify that this track listener has been removed from the graph, either
   * after shutdown or RemoveTrackListener.
   */
  virtual void NotifyRemoved() {}

 protected:
  virtual ~MediaStreamTrackListener() {}
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
class DirectMediaStreamTrackListener : public MediaStreamTrackListener {
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
   * ALREADY_EXIST
   *    This DirectMediaStreamTrackListener already exists in the
   *    SourceMediaStream.
   * SUCCESS
   *    Installation was successful and this listener will start receiving
   *    NotifyRealtimeData on the next AppendToTrack().
   */
  enum class InstallationResult {
    TRACK_NOT_FOUND_AT_SOURCE,
    TRACK_TYPE_NOT_SUPPORTED,
    STREAM_NOT_SUPPORTED,
    ALREADY_EXISTS,
    SUCCESS
  };
  virtual void NotifyDirectListenerInstalled(InstallationResult aResult) {}
  virtual void NotifyDirectListenerUninstalled() {}

 protected:
  virtual ~DirectMediaStreamTrackListener() {}

  void MirrorAndDisableSegment(AudioSegment& aFrom, AudioSegment& aTo);
  void MirrorAndDisableSegment(VideoSegment& aFrom, VideoSegment& aTo,
                               DisabledTrackMode aMode);
  void NotifyRealtimeTrackDataAndApplyTrackDisabling(MediaStreamGraph* aGraph,
                                                     StreamTime aTrackOffset,
                                                     MediaSegment& aMedia);

  void IncreaseDisabled(DisabledTrackMode aMode);
  void DecreaseDisabled(DisabledTrackMode aMode);

  // Matches the number of disabled streams to which this listener is attached.
  // The number of streams are those between the stream where the listener was
  // added and the SourceMediaStream that is the source of the data reaching
  // this listener.
  Atomic<int32_t> mDisabledFreezeCount;
  Atomic<int32_t> mDisabledBlackCount;
};

}  // namespace mozilla

#endif  // MOZILLA_MEDIASTREAMLISTENER_h_
