/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDOMMEDIASTREAM_H_
#define NSDOMMEDIASTREAM_H_

#include "ImageContainer.h"

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "StreamTracks.h"
#include "nsIDOMWindow.h"
#include "nsIPrincipal.h"
#include "MediaTrackConstraints.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "PrincipalChangeObserver.h"

// X11 has a #define for CurrentTime. Unbelievable :-(.
// See dom/media/webaudio/AudioContext.h for more fun!
#ifdef CurrentTime
#undef CurrentTime
#endif

namespace mozilla {

class AbstractThread;
class DOMHwMediaStream;
class DOMLocalMediaStream;
class DOMMediaStream;
class MediaStream;
class MediaInputPort;
class DirectMediaStreamListener;
class MediaStreamGraph;
class ProcessedMediaStream;

enum class BlockingMode;

namespace dom {
class AudioNode;
class HTMLCanvasElement;
class MediaStreamTrack;
class MediaStreamTrackSource;
class AudioStreamTrack;
class VideoStreamTrack;
class AudioTrack;
class VideoTrack;
class AudioTrackList;
class VideoTrackList;
class MediaTrackListListener;
} // namespace dom

namespace layers {
class ImageContainer;
class OverlayImage;
} // namespace layers

namespace media {
template<typename V, typename E> class Pledge;
} // namespace media

#define NS_DOMMEDIASTREAM_IID \
{ 0x8cb65468, 0x66c0, 0x444e, \
  { 0x89, 0x9f, 0x89, 0x1d, 0x9e, 0xd2, 0xbe, 0x7c } }

class OnTracksAvailableCallback {
public:
  virtual ~OnTracksAvailableCallback() {}
  virtual void NotifyTracksAvailable(DOMMediaStream* aStream) = 0;
};

/**
 * Interface through which a DOMMediaStream can query its producer for a
 * MediaStreamTrackSource. This will be used whenever a track occurs in the
 * DOMMediaStream's owned stream that has not yet been created on the main
 * thread (see DOMMediaStream::CreateOwnDOMTrack).
 */
class MediaStreamTrackSourceGetter : public nsISupports
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(MediaStreamTrackSourceGetter)

public:
  MediaStreamTrackSourceGetter()
  {
  }

  virtual already_AddRefed<dom::MediaStreamTrackSource>
  GetMediaStreamTrackSource(TrackID aInputTrackID) = 0;

protected:
  virtual ~MediaStreamTrackSourceGetter()
  {
  }
};

/**
 * DOM wrapper for MediaStreams.
 *
 * To account for track operations such as clone(), addTrack() and
 * removeTrack(), a DOMMediaStream wraps three internal (and chained)
 * MediaStreams:
 *   1. mInputStream
 *      - Controlled by the owner/source of the DOMMediaStream.
 *        It's a stream of the type indicated by
 *      - DOMMediaStream::CreateSourceStream/CreateTrackUnionStream. A source
 *        typically creates its DOMMediaStream, creates the MediaStreamTracks
 *        owned by said stream, then gets the internal input stream to which it
 *        feeds data for the previously created tracks.
 *      - When necessary it can create tracks on the internal stream only and
 *        their corresponding MediaStreamTracks will be asynchronously created.
 *   2. mOwnedStream
 *      - A TrackUnionStream containing tracks owned by this stream.
 *      - The internal model of a MediaStreamTrack consists of its owning
 *        DOMMediaStream and the TrackID of the corresponding internal track in
 *        the owning DOMMediaStream's mOwnedStream.
 *      - The owned stream is different from the input stream since a cloned
 *        DOMMediaStream is also the owner of its (cloned) MediaStreamTracks.
 *      - Stopping an original track shall not stop its clone. This is
 *        solved by stopping it at the owned stream, while the clone's owned
 *        stream gets data directly from the original input stream.
 *      - A DOMMediaStream (original or clone) gets all tracks dynamically
 *        added by the source automatically forwarded by having a TRACK_ANY
 *        MediaInputPort set up from the owning DOMMediaStream's input stream
 *        to this DOMMediaStream's owned stream.
 *   3. mPlaybackStream
 *      - A TrackUnionStream containing the tracks corresponding to the
 *        MediaStreamTracks currently in this DOMMediaStream (per getTracks()).
 *      - Similarly as for mOwnedStream, there's a TRACK_ANY MediaInputPort set
 *        up from the owned stream to the playback stream to allow tracks
 *        dynamically added by the source to be automatically forwarded to any
 *        audio or video sinks.
 *      - MediaStreamTracks added by addTrack() are set up with a MediaInputPort
 *        locked to their internal TrackID, from their owning DOMMediaStream's
 *        owned stream to this playback stream.
 *
 *
 * A graphical representation of how tracks are connected in various cases as
 * follows:
 *
 *                     addTrack()ed case:
 * DOMStream A
 *           Input        Owned          Playback
 *            t1 ---------> t1 ------------> t1     <- MediaStreamTrack X
 *                                                     (pointing to t1 in A)
 *                                 --------> t2     <- MediaStreamTrack Y
 *                                /                    (pointing to t1 in B)
 * DOMStream B                   /
 *           Input        Owned /        Playback
 *            t1 ---------> t1 ------------> t1     <- MediaStreamTrack Y
 *                                                     (pointing to t1 in B)
 *
 *                     removeTrack()ed case:
 * DOMStream A
 *           Input        Owned          Playback
 *            t1 ---------> t1                      <- No tracks
 *
 *
 *                     clone()d case:
 * DOMStream A
 *           Input        Owned          Playback
 *            t1 ---------> t1 ------------> t1     <- MediaStreamTrack X
 *               \                                     (pointing to t1 in A)
 *                -----
 * DOMStream B         \
 *           Input      \ Owned          Playback
 *                       -> t1 ------------> t1     <- MediaStreamTrack Y
 *                                                     (pointing to t1 in B)
 *
 *
 *            addTrack()ed, removeTrack()ed and clone()d case:
 *
 *  Here we have done the following:
 *    var A = someStreamWithTwoTracks;
 *    var B = someStreamWithOneTrack;
 *    var X = A.getTracks()[0];
 *    var Y = A.getTracks()[1];
 *    var Z = B.getTracks()[0];
 *    A.addTrack(Z);
 *    A.removeTrack(X);
 *    B.removeTrack(Z);
 *    var A' = A.clone();
 *
 * DOMStream A
 *           Input        Owned          Playback
 *            t1 ---------> t1                      <- MediaStreamTrack X (removed)
 *                                                     (pointing to t1 in A)
 *            t2 ---------> t2 ------------> t2     <- MediaStreamTrack Y
 *             \                                       (pointing to t2 in A)
 *              \                    ------> t3     <- MediaStreamTrack Z
 *               \                  /                  (pointing to t1 in B)
 * DOMStream B    \                /
 *           Input \      Owned   /      Playback
 *            t1 ---^-----> t1 ---                  <- MediaStreamTrack Z (removed)
 *              \    \                                 (pointing to t1 in B)
 *               \    \
 * DOMStream A'   \    \
 *           Input \    \ Owned          Playback
 *                  \    -> t1 ------------> t1     <- MediaStreamTrack Y'
 *                   \                                 (pointing to t1 in A')
 *                    ----> t2 ------------> t2     <- MediaStreamTrack Z'
 *                                                     (pointing to t2 in A')
 */
class DOMMediaStream : public DOMEventTargetHelper,
                       public dom::PrincipalChangeObserver<dom::MediaStreamTrack>
{
  friend class DOMLocalMediaStream;
  friend class dom::MediaStreamTrack;
  typedef dom::MediaStreamTrack MediaStreamTrack;
  typedef dom::AudioStreamTrack AudioStreamTrack;
  typedef dom::VideoStreamTrack VideoStreamTrack;
  typedef dom::MediaStreamTrackSource MediaStreamTrackSource;
  typedef dom::AudioTrack AudioTrack;
  typedef dom::VideoTrack VideoTrack;
  typedef dom::AudioTrackList AudioTrackList;
  typedef dom::VideoTrackList VideoTrackList;

public:
  typedef dom::MediaTrackConstraints MediaTrackConstraints;

  class TrackListener {
  public:
    virtual ~TrackListener() {}

    /**
     * Called when the DOMMediaStream has a live track added, either by
     * script (addTrack()) or the source creating one.
     */
    virtual void
    NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) {};

    /**
     * Called when the DOMMediaStream removes a live track from playback, either
     * by script (removeTrack(), track.stop()) or the source ending it.
     */
    virtual void
    NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack) {};

    /**
     * Called when the DOMMediaStream has become active.
     */
    virtual void
    NotifyActive() {};

    /**
     * Called when the DOMMediaStream has become inactive.
     */
    virtual void
    NotifyInactive() {};
  };

  /**
   * TrackPort is a representation of a MediaStreamTrack-MediaInputPort pair
   * that make up a link between the Owned stream and the Playback stream.
   *
   * Semantically, the track is the identifier/key and the port the value of this
   * connection.
   *
   * The input port can be shared between several TrackPorts. This is the case
   * for DOMMediaStream's mPlaybackPort which forwards all tracks in its
   * mOwnedStream automatically.
   *
   * If the MediaStreamTrack is owned by another DOMMediaStream (called A) than
   * the one owning the TrackPort (called B), the input port (locked to the
   * MediaStreamTrack's TrackID) connects A's mOwnedStream to B's mPlaybackStream.
   *
   * A TrackPort may never leave the DOMMediaStream it was created in. Internal
   * use only.
   */
  class TrackPort
  {
  public:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TrackPort)
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(TrackPort)

    /**
     * Indicates MediaInputPort ownership to the TrackPort.
     *
     * OWNED    - Owned by the TrackPort itself. TrackPort must destroy the
     *            input port when it's destructed.
     * EXTERNAL - Owned by another entity. It's the caller's responsibility to
     *            ensure the the MediaInputPort outlives the TrackPort.
     */
    enum class InputPortOwnership {
      OWNED = 1,
      EXTERNAL
    };

    TrackPort(MediaInputPort* aInputPort,
              MediaStreamTrack* aTrack,
              const InputPortOwnership aOwnership);

  protected:
    virtual ~TrackPort();

  public:
    void DestroyInputPort();

    /**
     * Returns the source stream of the input port.
     */
    MediaStream* GetSource() const;

    /**
     * Returns the track ID this track is locked to in the source stream of the
     * input port.
     */
    TrackID GetSourceTrackId() const;

    MediaInputPort* GetInputPort() const { return mInputPort; }
    MediaStreamTrack* GetTrack() const { return mTrack; }

    /**
     * Blocks aTrackId from going into mInputPort unless the port has been
     * destroyed. Returns a pledge that gets resolved when the MediaStreamGraph
     * has applied the block in the playback stream.
     */
    already_AddRefed<media::Pledge<bool, nsresult>>
    BlockSourceTrackId(TrackID aTrackId, BlockingMode aBlockingMode);

  private:
    RefPtr<MediaInputPort> mInputPort;
    RefPtr<MediaStreamTrack> mTrack;

    // Defines if we've been given ownership of the input port or if it's owned
    // externally. The owner is responsible for destroying the port.
    const InputPortOwnership mOwnership;
  };

  DOMMediaStream(nsPIDOMWindowInner* aWindow,
                 MediaStreamTrackSourceGetter* aTrackSourceGetter);

  NS_DECL_ISUPPORTS_INHERITED
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMMediaStream,
                                           DOMEventTargetHelper)
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOMMEDIASTREAM_IID)

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mWindow;
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL

  static already_AddRefed<DOMMediaStream>
  Constructor(const dom::GlobalObject& aGlobal,
              ErrorResult& aRv);

  static already_AddRefed<DOMMediaStream>
  Constructor(const dom::GlobalObject& aGlobal,
              const DOMMediaStream& aStream,
              ErrorResult& aRv);

  static already_AddRefed<DOMMediaStream>
  Constructor(const dom::GlobalObject& aGlobal,
              const dom::Sequence<OwningNonNull<MediaStreamTrack>>& aTracks,
              ErrorResult& aRv);

  double CurrentTime();

  void GetId(nsAString& aID) const;

  void GetAudioTracks(nsTArray<RefPtr<AudioStreamTrack> >& aTracks) const;
  void GetVideoTracks(nsTArray<RefPtr<VideoStreamTrack> >& aTracks) const;
  void GetTracks(nsTArray<RefPtr<MediaStreamTrack> >& aTracks) const;
  MediaStreamTrack* GetTrackById(const nsAString& aId) const;
  void AddTrack(MediaStreamTrack& aTrack);
  void RemoveTrack(MediaStreamTrack& aTrack);

  /** Identical to CloneInternal(TrackForwardingOption::EXPLICIT) */
  already_AddRefed<DOMMediaStream> Clone();

  bool Active() const;

  IMPL_EVENT_HANDLER(addtrack)

  // NON-WebIDL

  /**
   * Option to provide to CloneInternal() of which tracks should be forwarded
   * from the source stream (`this`) to the returned stream clone.
   *
   * CURRENT forwards the tracks currently in the source stream's track set.
   * ALL     forwards like EXPLICIT plus any and all future tracks originating
   *         from the same input stream as the source DOMMediaStream (`this`).
   */
  enum class TrackForwardingOption {
    CURRENT,
    ALL
  };
  already_AddRefed<DOMMediaStream> CloneInternal(TrackForwardingOption aForwarding);

  MediaStreamTrack* GetOwnedTrackById(const nsAString& aId);

  /**
   * Returns true if this DOMMediaStream has aTrack in its mPlaybackStream.
   */
  bool HasTrack(const MediaStreamTrack& aTrack) const;

  /**
   * Returns true if this DOMMediaStream owns aTrack.
   */
  bool OwnsTrack(const MediaStreamTrack& aTrack) const;

  /**
   * Returns the corresponding MediaStreamTrack if it's in our mOwnedStream.
   * aInputTrackID should match the track's TrackID in its input stream,
   * and aTrackID the TrackID in mOwnedStream.
   *
   * When aTrackID is not supplied or set to TRACK_ANY, we return the first
   * MediaStreamTrack that matches the given input track. Note that there may
   * be multiple MediaStreamTracks matching the same input track, but that they
   * in that case all share the same MediaStreamTrackSource.
   */
  MediaStreamTrack* FindOwnedDOMTrack(MediaStream* aInputStream,
                                      TrackID aInputTrackID,
                                      TrackID aTrackID = TRACK_ANY) const;

  /**
   * Returns the TrackPort connecting aTrack's input stream to mOwnedStream,
   * or nullptr if aTrack is not owned by this DOMMediaStream.
   */
  TrackPort* FindOwnedTrackPort(const MediaStreamTrack& aTrack) const;

  /**
   * Returns the corresponding MediaStreamTrack if it's in our mPlaybackStream.
   * aInputTrackID should match the track's TrackID in its owned stream.
   */
  MediaStreamTrack* FindPlaybackDOMTrack(MediaStream* aInputStream,
                                         TrackID aInputTrackID) const;

  /**
   * Returns the TrackPort connecting mOwnedStream to mPlaybackStream for aTrack.
   */
  TrackPort* FindPlaybackTrackPort(const MediaStreamTrack& aTrack) const;

  MediaStream* GetInputStream() const { return mInputStream; }
  ProcessedMediaStream* GetOwnedStream() const { return mOwnedStream; }
  ProcessedMediaStream* GetPlaybackStream() const { return mPlaybackStream; }

  /**
   * Allows a video element to identify this stream as a camera stream, which
   * needs special treatment.
   */
  virtual MediaStream* GetCameraStream() const { return nullptr; }

  /**
   * Allows users to get access to media data without going through graph
   * queuing. Returns a bool to let us know if direct data will be delivered.
   */
  bool AddDirectListener(DirectMediaStreamListener *aListener);
  void RemoveDirectListener(DirectMediaStreamListener *aListener);

  virtual DOMLocalMediaStream* AsDOMLocalMediaStream() { return nullptr; }
  virtual DOMHwMediaStream* AsDOMHwMediaStream() { return nullptr; }

  /**
   * Legacy method that returns true when the playback stream has finished.
   */
  bool IsFinished() const;

  /**
   * Becomes inactive only when the playback stream has finished.
   */
  void SetInactiveOnFinish();

  /**
   * Returns a principal indicating who may access this stream. The stream contents
   * can only be accessed by principals subsuming this principal.
   */
  nsIPrincipal* GetPrincipal() { return mPrincipal; }

  /**
   * Returns a principal indicating who may access video data of this stream.
   * The video principal will be a combination of all live video tracks.
   */
  nsIPrincipal* GetVideoPrincipal() { return mVideoPrincipal; }

  // From PrincipalChangeObserver<MediaStreamTrack>.
  void PrincipalChanged(MediaStreamTrack* aTrack) override;

  /**
   * Add a PrincipalChangeObserver to this stream.
   *
   * Returns true if it was successfully added.
   *
   * Ownership of the PrincipalChangeObserver remains with the caller, and it's
   * the caller's responsibility to remove the observer before it dies.
   */
  bool AddPrincipalChangeObserver(dom::PrincipalChangeObserver<DOMMediaStream>* aObserver);

  /**
   * Remove an added PrincipalChangeObserver from this stream.
   *
   * Returns true if it was successfully removed.
   */
  bool RemovePrincipalChangeObserver(dom::PrincipalChangeObserver<DOMMediaStream>* aObserver);

  // Webrtc allows the remote side to name a stream whatever it wants, and we
  // need to surface this to content.
  void AssignId(const nsAString& aID) { mID = aID; }

  /**
   * Create a DOMMediaStream whose underlying input stream is a SourceMediaStream.
   */
  static already_AddRefed<DOMMediaStream> CreateSourceStreamAsInput(nsPIDOMWindowInner* aWindow,
                                                                    MediaStreamGraph* aGraph,
                                                                    MediaStreamTrackSourceGetter* aTrackSourceGetter = nullptr);

  /**
   * Create a DOMMediaStream whose underlying input stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMMediaStream> CreateTrackUnionStreamAsInput(nsPIDOMWindowInner* aWindow,
                                                                        MediaStreamGraph* aGraph,
                                                                        MediaStreamTrackSourceGetter* aTrackSourceGetter = nullptr);

  /**
   * Create an DOMMediaStream whose underlying input stream is an
   * AudioCaptureStream.
   */
  static already_AddRefed<DOMMediaStream>
  CreateAudioCaptureStreamAsInput(nsPIDOMWindowInner* aWindow,
                                  nsIPrincipal* aPrincipal,
                                  MediaStreamGraph* aGraph);

  void SetLogicalStreamStartTime(StreamTime aTime)
  {
    mLogicalStreamStartTime = aTime;
  }

  /**
   * Adds a MediaStreamTrack to mTracks and raises "addtrack".
   *
   * Note that "addtrack" is raised synchronously and only has an effect if
   * this MediaStream is already exposed to script. For spec compliance this is
   * to be called from an async task.
   */
  void AddTrackInternal(MediaStreamTrack* aTrack);

  /**
   * Called for each track in our owned stream to indicate to JS that we
   * are carrying that track.
   *
   * Pre-creates a MediaStreamTrack and returns it.
   * It is up to the caller to make sure it is added through AddTrackInternal.
   */
  already_AddRefed<MediaStreamTrack> CreateDOMTrack(TrackID aTrackID,
                                                    MediaSegment::Type aType,
                                                    MediaStreamTrackSource* aSource,
                                                    const MediaTrackConstraints& aConstraints = MediaTrackConstraints());

  /**
   * Creates a MediaStreamTrack cloned from aTrack, adds it to mTracks and
   * returns it.
   * aCloneTrackID is the TrackID the new track will get in mOwnedStream.
   */
  already_AddRefed<MediaStreamTrack> CloneDOMTrack(MediaStreamTrack& aTrack,
                                                   TrackID aCloneTrackID);

  // When the initial set of tracks has been added, run
  // aCallback->NotifyTracksAvailable.
  // It is allowed to do anything, including run script.
  // aCallback may run immediately during this call if tracks are already
  // available!
  // We only care about track additions, we'll fire the notification even if
  // some of the tracks have been removed.
  // Takes ownership of aCallback.
  void OnTracksAvailable(OnTracksAvailableCallback* aCallback);

  /**
   * Add an nsISupports object that this stream will keep alive as long as
   * the stream itself is alive.
   */
  void AddConsumerToKeepAlive(nsISupports* aConsumer)
  {
    mConsumersToKeepAlive.AppendElement(aConsumer);
  }

  // Registers a track listener to this MediaStream, for listening to changes
  // to our track set. The caller must call UnregisterTrackListener before
  // being destroyed, so we don't hold on to a dead pointer. Main thread only.
  void RegisterTrackListener(TrackListener* aListener);

  // Unregisters a track listener from this MediaStream. The caller must call
  // UnregisterTrackListener before being destroyed, so we don't hold on to
  // a dead pointer. Main thread only.
  void UnregisterTrackListener(TrackListener* aListener);

protected:
  virtual ~DOMMediaStream();

  void Destroy();
  void InitSourceStream(MediaStreamGraph* aGraph);
  void InitTrackUnionStream(MediaStreamGraph* aGraph);
  void InitAudioCaptureStream(nsIPrincipal* aPrincipal, MediaStreamGraph* aGraph);

  // Sets up aStream as mInputStream. A producer may append data to a
  // SourceMediaStream input stream, or connect another stream to a
  // TrackUnionStream input stream.
  void InitInputStreamCommon(MediaStream* aStream, MediaStreamGraph* aGraph);

  // Sets up a new TrackUnionStream as mOwnedStream and connects it to
  // mInputStream with a TRACK_ANY MediaInputPort if available.
  // If this DOMMediaStream should have an input stream (producing data),
  // it has to be initiated before the owned stream.
  void InitOwnedStreamCommon(MediaStreamGraph* aGraph);

  // Sets up a new TrackUnionStream as mPlaybackStream and connects it to
  // mOwnedStream with a TRACK_ANY MediaInputPort if available.
  // If this DOMMediaStream should have an owned stream (producer or clone),
  // it has to be initiated before the playback stream.
  void InitPlaybackStreamCommon(MediaStreamGraph* aGraph);

  void CheckTracksAvailable();

  // Called when MediaStreamGraph has finished an iteration where tracks were
  // created.
  void NotifyTracksCreated();

  // Called when our playback stream has finished in the MediaStreamGraph.
  void NotifyFinished();

  // Dispatches NotifyActive() to all registered track listeners.
  void NotifyActive();

  // Dispatches NotifyInactive() to all registered track listeners.
  void NotifyInactive();

  // Dispatches NotifyTrackAdded() to all registered track listeners.
  void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack);

  // Dispatches NotifyTrackRemoved() to all registered track listeners.
  void NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack);

  // Dispatches "addtrack" or "removetrack".
  nsresult DispatchTrackEvent(const nsAString& aName,
                              const RefPtr<MediaStreamTrack>& aTrack);

  class OwnedStreamListener;
  friend class OwnedStreamListener;

  class PlaybackStreamListener;
  friend class PlaybackStreamListener;

  class PlaybackTrackListener;
  friend class PlaybackTrackListener;

  /**
   * Block a track in our playback stream. Calls NotifyPlaybackTrackBlocked()
   * after the MediaStreamGraph has applied the block and the track is no longer
   * live.
   */
  void BlockPlaybackTrack(TrackPort* aTrack);

  /**
   * Called on main thread after MediaStreamGraph has applied a track block in
   * our playback stream.
   */
  void NotifyPlaybackTrackBlocked();

  // Recomputes the current principal of this stream based on the set of tracks
  // it currently contains. PrincipalChangeObservers will be notified only if
  // the principal changes.
  void RecomputePrincipal();

  // StreamTime at which the currentTime attribute would return 0.
  StreamTime mLogicalStreamStartTime;

  // We need this to track our parent object.
  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  // MediaStreams are owned by the graph, but we tell them when to die,
  // and they won't die until we let them.

  // This stream contains tracks used as input by us. Cloning happens from this
  // stream. Tracks may exist in these stream but not in |mOwnedStream| if they
  // have been stopped.
  MediaStream* mInputStream;

  // This stream contains tracks owned by us (if we were created directly from
  // source, or cloned from some other stream). Tracks map to |mOwnedTracks|.
  ProcessedMediaStream* mOwnedStream;

  // This stream contains tracks currently played by us, despite of owner.
  // Tracks map to |mTracks|.
  ProcessedMediaStream* mPlaybackStream;

  // This port connects mInputStream to mOwnedStream. All tracks forwarded.
  RefPtr<MediaInputPort> mOwnedPort;

  // This port connects mOwnedStream to mPlaybackStream. All tracks not
  // explicitly blocked due to removal are forwarded.
  RefPtr<MediaInputPort> mPlaybackPort;

  // MediaStreamTracks corresponding to tracks in our mOwnedStream.
  AutoTArray<RefPtr<TrackPort>, 2> mOwnedTracks;

  // MediaStreamTracks corresponding to tracks in our mPlaybackStream.
  AutoTArray<RefPtr<TrackPort>, 2> mTracks;

  // Number of MediaStreamTracks that have been removed on main thread but are
  // waiting to be removed on MediaStreamGraph thread.
  size_t mTracksPendingRemoval;

  // The interface through which we can query the stream producer for
  // track sources.
  RefPtr<MediaStreamTrackSourceGetter> mTrackSourceGetter;

  // Listener tracking changes to mOwnedStream. We use this to notify the
  // MediaStreamTracks we own about state changes.
  RefPtr<OwnedStreamListener> mOwnedListener;

  // Listener tracking changes to mPlaybackStream. This drives state changes
  // in this DOMMediaStream and notifications to mTrackListeners.
  RefPtr<PlaybackStreamListener> mPlaybackListener;

  // Listener tracking when live MediaStreamTracks in mTracks end.
  RefPtr<PlaybackTrackListener> mPlaybackTrackListener;

  nsTArray<nsAutoPtr<OnTracksAvailableCallback> > mRunOnTracksAvailable;

  // Set to true after MediaStreamGraph has created tracks for mPlaybackStream.
  bool mTracksCreated;

  nsString mID;

  // Keep these alive while the stream is alive.
  nsTArray<nsCOMPtr<nsISupports>> mConsumersToKeepAlive;

  bool mNotifiedOfMediaStreamGraphShutdown;

  // The track listeners subscribe to changes in this stream's track set.
  nsTArray<TrackListener*> mTrackListeners;

  // True if this stream has live tracks.
  bool mActive;

  // True if this stream only sets mActive to false when its playback stream
  // finishes. This is a hack to maintain legacy functionality for playing a
  // HTMLMediaElement::MozCaptureStream(). See bug 1302379.
  bool mSetInactiveOnFinish;

private:
  void NotifyPrincipalChanged();
  // Principal identifying who may access the collected contents of this stream.
  // If null, this stream can be used by anyone because it has no content yet.
  nsCOMPtr<nsIPrincipal> mPrincipal;
  // Video principal is used by video element as access is requested to its
  // image data.
  nsCOMPtr<nsIPrincipal> mVideoPrincipal;
  nsTArray<dom::PrincipalChangeObserver<DOMMediaStream>*> mPrincipalChangeObservers;
  CORSMode mCORSMode;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMMediaStream,
                              NS_DOMMEDIASTREAM_IID)

#define NS_DOMLOCALMEDIASTREAM_IID \
{ 0xb1437260, 0xec61, 0x4dfa, \
  { 0x92, 0x54, 0x04, 0x44, 0xe2, 0xb5, 0x94, 0x9c } }

class DOMLocalMediaStream : public DOMMediaStream
{
public:
  explicit DOMLocalMediaStream(nsPIDOMWindowInner* aWindow,
                               MediaStreamTrackSourceGetter* aTrackSourceGetter)
    : DOMMediaStream(aWindow, aTrackSourceGetter) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOMLOCALMEDIASTREAM_IID)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void Stop();

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<DOMLocalMediaStream>
  CreateSourceStreamAsInput(nsPIDOMWindowInner* aWindow,
                            MediaStreamGraph* aGraph,
                            MediaStreamTrackSourceGetter* aTrackSourceGetter = nullptr);

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMLocalMediaStream>
  CreateTrackUnionStreamAsInput(nsPIDOMWindowInner* aWindow,
                                MediaStreamGraph* aGraph,
                                MediaStreamTrackSourceGetter* aTrackSourceGetter = nullptr);

protected:
  virtual ~DOMLocalMediaStream();

  void StopImpl();
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMLocalMediaStream,
                              NS_DOMLOCALMEDIASTREAM_IID)

class DOMAudioNodeMediaStream : public DOMMediaStream
{
  typedef dom::AudioNode AudioNode;
public:
  DOMAudioNodeMediaStream(nsPIDOMWindowInner* aWindow, AudioNode* aNode);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)

  /**
   * Create a DOMAudioNodeMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMAudioNodeMediaStream>
  CreateTrackUnionStreamAsInput(nsPIDOMWindowInner* aWindow,
                                AudioNode* aNode,
                                MediaStreamGraph* aGraph);

protected:
  ~DOMAudioNodeMediaStream();

private:
  // If this object wraps a stream owned by an AudioNode, we need to ensure that
  // the node isn't cycle-collected too early.
  RefPtr<AudioNode> mStreamNode;
};

class DOMHwMediaStream : public DOMLocalMediaStream
{
  typedef mozilla::gfx::IntSize IntSize;
  typedef layers::OverlayImage OverlayImage;
#ifdef MOZ_WIDGET_GONK
  typedef layers::OverlayImage::Data Data;
#endif

public:
  explicit DOMHwMediaStream(nsPIDOMWindowInner* aWindow);

  static already_AddRefed<DOMHwMediaStream> CreateHwStream(nsPIDOMWindowInner* aWindow,
                                                           OverlayImage* aImage = nullptr);
  virtual DOMHwMediaStream* AsDOMHwMediaStream() override { return this; }
  int32_t RequestOverlayId();
  void SetOverlayId(int32_t aOverlayId);
  void SetImageSize(uint32_t width, uint32_t height);
  void SetOverlayImage(OverlayImage* aImage);

protected:
  ~DOMHwMediaStream();

private:
  void Init(MediaStream* aStream, OverlayImage* aImage);

#ifdef MOZ_WIDGET_GONK
  const int DEFAULT_IMAGE_ID = 0x01;
  const int DEFAULT_IMAGE_WIDTH = 400;
  const int DEFAULT_IMAGE_HEIGHT = 300;
  RefPtr<OverlayImage> mOverlayImage;
  PrincipalHandle mPrincipalHandle;
#endif
};

} // namespace mozilla

#endif /* NSDOMMEDIASTREAM_H_ */
