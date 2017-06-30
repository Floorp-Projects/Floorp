/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLMediaElement_h
#define mozilla_dom_HTMLMediaElement_h

#include "nsAutoPtr.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsGenericHTMLElement.h"
#include "MediaDecoderOwner.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "mozilla/CORSMode.h"
#include "DecoderTraits.h"
#include "nsIAudioChannelAgent.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/TextTrackManager.h"
#include "mozilla/WeakPtr.h"
#include "MediaDecoder.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/StateWatching.h"
#include "nsGkAtoms.h"
#include "PrincipalChangeObserver.h"
#include "nsStubMutationObserver.h"

// X.h on Linux #defines CurrentTime as 0L, so we have to #undef it here.
#ifdef CurrentTime
#undef CurrentTime
#endif

#include "mozilla/dom/HTMLMediaElementBinding.h"

// Define to output information on decoding and painting framerate
/* #define DEBUG_FRAME_RATE 1 */

typedef uint16_t nsMediaNetworkState;
typedef uint16_t nsMediaReadyState;
typedef uint32_t SuspendTypes;
typedef uint32_t AudibleChangedReasons;
typedef uint8_t AudibleState;

namespace mozilla {
class AbstractThread;
class ChannelMediaDecoder;
class DecoderDoctorDiagnostics;
class DOMMediaStream;
class ErrorResult;
class MediaResource;
class MediaDecoder;
class VideoFrameContainer;
namespace dom {
class MediaKeys;
class TextTrack;
class TimeRanges;
class WakeLock;
class MediaTrack;
class MediaStreamTrack;
class VideoStreamTrack;
} // namespace dom
} // namespace mozilla

class nsIChannel;
class nsIHttpChannel;
class nsILoadGroup;
class nsIRunnable;
class nsITimer;
class nsRange;

namespace mozilla {
namespace dom {

// Number of milliseconds between timeupdate events as defined by spec
#define TIMEUPDATE_MS 250

class MediaError;
class MediaSource;
class Promise;
class TextTrackList;
class AudioTrackList;
class VideoTrackList;

class HTMLMediaElement : public nsGenericHTMLElement,
                         public nsIDOMHTMLMediaElement,
                         public MediaDecoderOwner,
                         public PrincipalChangeObserver<DOMMediaStream>,
                         public SupportsWeakPtr<HTMLMediaElement>,
                         public nsStubMutationObserver
{
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::VideoFrameContainer VideoFrameContainer;
  typedef mozilla::MediaStream MediaStream;
  typedef mozilla::MediaResource MediaResource;
  typedef mozilla::MediaDecoderOwner MediaDecoderOwner;
  typedef mozilla::MetadataTags MetadataTags;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(HTMLMediaElement)
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  CORSMode GetCORSMode() {
    return mCORSMode;
  }

  explicit HTMLMediaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  void ReportCanPlayTelemetry();

  /**
   * This is used when the browser is constructing a video element to play
   * a channel that we've already started loading. The src attribute and
   * <source> children are ignored.
   * @param aChannel the channel to use
   * @param aListener returns a stream listener that should receive
   * notifications for the stream
   */
  nsresult LoadWithChannel(nsIChannel *aChannel, nsIStreamListener **aListener);

  // nsIDOMHTMLMediaElement
  NS_DECL_NSIDOMHTMLMEDIAELEMENT

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLMediaElement,
                                           nsGenericHTMLElement)

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) override;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual void DoneCreatingElement() override;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable,
                               int32_t *aTabIndex) override;
  virtual int32_t TabIndexDefault() override;

  /**
   * Call this to reevaluate whether we should start/stop due to our owner
   * document being active, inactive, visible or hidden.
   */
  virtual void NotifyOwnerDocumentActivityChanged();

  // Called by the video decoder object, on the main thread,
  // when it has read the metadata containing video dimensions,
  // etc.
  virtual void MetadataLoaded(const MediaInfo* aInfo,
                              nsAutoPtr<const MetadataTags> aTags) final override;

  // Called by the decoder object, on the main thread,
  // when it has read the first frame of the video or audio.
  virtual void FirstFrameLoaded() final override;

  // Called by the video decoder object, on the main thread,
  // when the resource has a network error during loading.
  virtual void NetworkError() final override;

  // Called by the video decoder object, on the main thread, when the
  // resource has a decode error during metadata loading or decoding.
  virtual void DecodeError(const MediaResult& aError) final override;

  // Called by the decoder object, on the main thread, when the
  // resource has a decode issue during metadata loading or decoding, but can
  // continue decoding.
  virtual void DecodeWarning(const MediaResult& aError) final override;

  // Return true if error attribute is not null.
  virtual bool HasError() const final override;

  // Called by the video decoder object, on the main thread, when the
  // resource load has been cancelled.
  virtual void LoadAborted() final override;

  // Called by the video decoder object, on the main thread,
  // when the video playback has ended.
  virtual void PlaybackEnded() final override;

  // Called by the video decoder object, on the main thread,
  // when the resource has started seeking.
  virtual void SeekStarted() final override;

  // Called by the video decoder object, on the main thread,
  // when the resource has completed seeking.
  virtual void SeekCompleted() final override;

  // Called by the media stream, on the main thread, when the download
  // has been suspended by the cache or because the element itself
  // asked the decoder to suspend the download.
  virtual void DownloadSuspended() final override;

  // Called by the media stream, on the main thread, when the download
  // has been resumed by the cache or because the element itself
  // asked the decoder to resumed the download.
  // If aForceNetworkLoading is True, ignore the fact that the download has
  // previously finished. We are downloading the middle of the media after
  // having downloaded the end, we need to notify the element a download in
  // ongoing.
  virtual void DownloadResumed(bool aForceNetworkLoading = false) final override;

  // Called to indicate the download is progressing.
  virtual void DownloadProgressed() final override;

  // Called by the media decoder to indicate whether the media cache has
  // suspended the channel.
  virtual void NotifySuspendedByCache(bool aIsSuspended) final override;

  virtual bool IsActive() const final override;

  virtual bool IsHidden() const final override;

  // Called by the media decoder and the video frame to get the
  // ImageContainer containing the video data.
  virtual VideoFrameContainer* GetVideoFrameContainer() final override;
  layers::ImageContainer* GetImageContainer();

  // From PrincipalChangeObserver<DOMMediaStream>.
  void PrincipalChanged(DOMMediaStream* aStream) override;

  void UpdateSrcStreamVideoPrincipal(const PrincipalHandle& aPrincipalHandle);

  // Called after the MediaStream we're playing rendered a frame to aContainer
  // with a different principalHandle than the previous frame.
  void PrincipalHandleChangedForVideoFrameContainer(VideoFrameContainer* aContainer,
                                                    const PrincipalHandle& aNewPrincipalHandle);

  // Dispatch events
  virtual nsresult DispatchAsyncEvent(const nsAString& aName) final override;

  // Triggers a recomputation of readyState.
  void UpdateReadyState() override { UpdateReadyStateInternal(); }

  // Dispatch events that were raised while in the bfcache
  nsresult DispatchPendingMediaEvents();

  // Return true if we can activate autoplay assuming enough data has arrived.
  bool CanActivateAutoplay();

  // Notify that state has changed that might cause an autoplay element to
  // start playing.
  // If the element is 'autoplay' and is ready to play back (not paused,
  // autoplay pref enabled, etc), it should start playing back.
  void CheckAutoplayDataReady();

  // Check if the media element had crossorigin set when loading started
  bool ShouldCheckAllowOrigin();

  // Returns true if the currently loaded resource is CORS same-origin with
  // respect to the document.
  bool IsCORSSameOrigin();

  // Is the media element potentially playing as defined by the HTML 5 specification.
  // http://www.whatwg.org/specs/web-apps/current-work/#potentially-playing
  bool IsPotentiallyPlaying() const;

  // Has playback ended as defined by the HTML 5 specification.
  // http://www.whatwg.org/specs/web-apps/current-work/#ended
  bool IsPlaybackEnded() const;

  // principal of the currently playing resource. Anything accessing the contents
  // of this element must have a principal that subsumes this principal.
  // Returns null if nothing is playing.
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal();

  // Principal of the currently playing video resource. Anything accessing the
  // image container of this element must have a principal that subsumes this
  // principal. If there are no live video tracks but content has been rendered
  // to the image container, we return the last video principal we had. Should
  // the image container be empty with no live video tracks, we return nullptr.
  already_AddRefed<nsIPrincipal> GetCurrentVideoPrincipal();

  // called to notify that the principal of the decoder's media resource has changed.
  void NotifyDecoderPrincipalChanged() final override;

  void GetEMEInfo(nsString& aEMEInfo);

  // An interface for observing principal changes on the media elements
  // MediaDecoder. This will also be notified if the active CORSMode changes.
  class DecoderPrincipalChangeObserver
  {
  public:
    virtual void NotifyDecoderPrincipalChanged() = 0;
  };

  /**
   * Add a DecoderPrincipalChangeObserver to this media element.
   *
   * Ownership of the DecoderPrincipalChangeObserver remains with the caller,
   * and it's the caller's responsibility to remove the observer before it dies.
   */
  void AddDecoderPrincipalChangeObserver(DecoderPrincipalChangeObserver* aObserver);

  /**
   * Remove an added DecoderPrincipalChangeObserver from this media element.
   *
   * Returns true if it was successfully removed.
   */
  bool RemoveDecoderPrincipalChangeObserver(DecoderPrincipalChangeObserver* aObserver);

  class StreamCaptureTrackSource;
  class DecoderCaptureTrackSource;
  class CaptureStreamTrackSourceGetter;

  // Update the visual size of the media. Called from the decoder on the
  // main thread when/if the size changes.
  void UpdateMediaSize(const nsIntSize& aSize);
  // Like UpdateMediaSize, but only updates the size if no size has yet
  // been set.
  void UpdateInitialMediaSize(const nsIntSize& aSize);

  // Returns the CanPlayStatus indicating if we can handle the
  // full MIME type including the optional codecs parameter.
  static CanPlayStatus GetCanPlay(const nsAString& aType,
                                  DecoderDoctorDiagnostics* aDiagnostics);

  /**
   * Called when a child source element is added to this media element. This
   * may queue a task to run the select resource algorithm if appropriate.
   */
  void NotifyAddedSource();

  /**
   * Called when there's been an error fetching the resource. This decides
   * whether it's appropriate to fire an error event.
   */
  void NotifyLoadError();

  /**
   * Called by one of our associated MediaTrackLists (audio/video) when an
   * AudioTrack is enabled or a VideoTrack is selected.
   */
  void NotifyMediaTrackEnabled(MediaTrack* aTrack);

  /**
   * Called by one of our associated MediaTrackLists (audio/video) when an
   * AudioTrack is disabled or a VideoTrack is unselected.
   */
  void NotifyMediaTrackDisabled(MediaTrack* aTrack);

  /**
   * Called when tracks become available to the source media stream.
   */
  void NotifyMediaStreamTracksAvailable(DOMMediaStream* aStream);

  /**
   * Called when a captured MediaStreamTrack is stopped so we can clean up its
   * MediaInputPort.
   */
  void NotifyOutputTrackStopped(DOMMediaStream* aOwningStream,
                                TrackID aDestinationTrackID);

  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  /**
   * Returns the current load ID. Asynchronous events store the ID that was
   * current when they were enqueued, and if it has changed when they come to
   * fire, they consider themselves cancelled, and don't fire.
   */
  uint32_t GetCurrentLoadID() { return mCurrentLoadID; }

  /**
   * Returns the load group for this media element's owner document.
   * XXX XBL2 issue.
   */
  already_AddRefed<nsILoadGroup> GetDocumentLoadGroup();

  /**
   * Returns true if the media has played or completed a seek.
   * Used by video frame to determine whether to paint the poster.
   */
  bool GetPlayedOrSeeked() const { return mHasPlayedOrSeeked; }

  nsresult CopyInnerTo(Element* aDest, bool aPreallocateChildren);

  /**
   * Sets the Accept header on the HTTP channel to the required
   * video or audio MIME types.
   */
  virtual nsresult SetAcceptHeader(nsIHttpChannel* aChannel) = 0;

  /**
   * Sets the required request headers on the HTTP channel for
   * video or audio requests.
   */
  void SetRequestHeaders(nsIHttpChannel* aChannel);

  /**
   * Asynchronously awaits a stable state, whereupon aRunnable runs on the main
   * thread. This adds an event which run aRunnable to the appshell's list of
   * sections synchronous the next time control returns to the event loop.
   */
  void RunInStableState(nsIRunnable* aRunnable);

  /**
   * Fires a timeupdate event. If aPeriodic is true, the event will only
   * be fired if we've not fired a timeupdate event (for any reason) in the
   * last 250ms, as required by the spec when the current time is periodically
   * increasing during playback.
   */
  virtual void FireTimeUpdate(bool aPeriodic) final override;

  /**
   * This will return null if mSrcStream is null, or if mSrcStream is not
   * null but its GetPlaybackStream() returns null --- which can happen during
   * cycle collection unlinking!
   */
  MediaStream* GetSrcMediaStream() const;

  // WebIDL

  MediaError* GetError() const;

  // XPCOM GetSrc() is OK
  void SetSrc(const nsAString& aSrc, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aRv);
  }

  // XPCOM GetCurrentSrc() is OK

  void GetCrossOrigin(nsAString& aResult)
  {
    // Null for both missing and invalid defaults is ok, since we
    // always parse to an enum value, so we don't need an invalid
    // default, and we _want_ the missing default to be null.
    GetEnumAttr(nsGkAtoms::crossorigin, nullptr, aResult);
  }
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError)
  {
    SetOrRemoveNullableStringAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
  }

  uint16_t NetworkState() const
  {
    return mNetworkState;
  }

  void NotifyXPCOMShutdown() final override;

  // Called by media decoder when the audible state changed or when input is
  // a media stream.
  virtual void SetAudibleState(bool aAudible) final override;

  // Notify agent when the MediaElement changes its audible state.
  void NotifyAudioPlaybackChanged(AudibleChangedReasons aReason);

  // XPCOM GetPreload() is OK
  void SetPreload(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::preload, aValue, aRv);
  }

  already_AddRefed<TimeRanges> Buffered() const;

  // XPCOM Load() is OK

  // XPCOM CanPlayType() is OK

  uint16_t ReadyState() const
  {
    return mReadyState;
  }

  bool Seeking() const;

  double CurrentTime() const;

  void SetCurrentTime(double aCurrentTime, ErrorResult& aRv);

  void FastSeek(double aTime, ErrorResult& aRv);

  already_AddRefed<Promise> SeekToNextFrame(ErrorResult& aRv);

  double Duration() const;

  bool HasAudio() const
  {
    return mMediaInfo.HasAudio();
  }

  bool HasVideo() const
  {
    return mMediaInfo.HasVideo();
  }

  bool IsEncrypted() const
  {
    return mIsEncrypted;
  }

  bool Paused() const
  {
    return mPaused;
  }

  double DefaultPlaybackRate() const
  {
    return mDefaultPlaybackRate;
  }

  void SetDefaultPlaybackRate(double aDefaultPlaybackRate, ErrorResult& aRv);

  double PlaybackRate() const
  {
    return mPlaybackRate;
  }

  void SetPlaybackRate(double aPlaybackRate, ErrorResult& aRv);

  already_AddRefed<TimeRanges> Played();

  already_AddRefed<TimeRanges> Seekable() const;

  bool Ended();

  bool Autoplay() const
  {
    return GetBoolAttr(nsGkAtoms::autoplay);
  }

  void SetAutoplay(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::autoplay, aValue, aRv);
  }

  bool Loop() const
  {
    return GetBoolAttr(nsGkAtoms::loop);
  }

  void SetLoop(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::loop, aValue, aRv);
  }

  already_AddRefed<Promise> Play(ErrorResult& aRv);

  void Pause(ErrorResult& aRv);

  bool Controls() const
  {
    return GetBoolAttr(nsGkAtoms::controls);
  }

  void SetControls(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::controls, aValue, aRv);
  }

  double Volume() const
  {
    return mVolume;
  }

  void SetVolume(double aVolume, ErrorResult& aRv);

  bool Muted() const
  {
    return mMuted & MUTED_BY_CONTENT;
  }

  // XPCOM SetMuted() is OK

  bool DefaultMuted() const
  {
    return GetBoolAttr(nsGkAtoms::muted);
  }

  void SetDefaultMuted(bool aMuted, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::muted, aMuted, aRv);
  }

  bool MozAllowCasting() const
  {
    return mAllowCasting;
  }

  void SetMozAllowCasting(bool aShow)
  {
    mAllowCasting = aShow;
  }

  bool MozIsCasting() const
  {
    return mIsCasting;
  }

  void SetMozIsCasting(bool aShow)
  {
    mIsCasting = aShow;
  }

  already_AddRefed<MediaSource> GetMozMediaSourceObject() const;
  // Returns a string describing the state of the media player internal
  // data. Used for debugging purposes.
  void GetMozDebugReaderData(nsAString& aString);

  // Returns a promise which will be resolved after collecting debugging
  // data from decoder/reader/MDSM. Used for debugging purposes.
  already_AddRefed<Promise> MozRequestDebugInfo(ErrorResult& aRv);

  void MozDumpDebugInfo();

  // For use by mochitests. Enabling pref "media.test.video-suspend"
  void SetVisible(bool aVisible);

  // For use by mochitests. Enabling pref "media.test.video-suspend"
  bool HasSuspendTaint() const;

  // Synchronously, return the next video frame and mark the element unable to
  // participate in decode suspending.
  //
  // This function is synchronous for cases where decoding has been suspended
  // and JS needs a frame to use in, eg., nsLayoutUtils::SurfaceFromElement()
  // via drawImage().
  already_AddRefed<layers::Image> GetCurrentImage();

  already_AddRefed<DOMMediaStream> GetSrcObject() const;
  void SetSrcObject(DOMMediaStream& aValue);
  void SetSrcObject(DOMMediaStream* aValue);

  // TODO: remove prefixed versions soon (1183495).
  already_AddRefed<DOMMediaStream> GetMozSrcObject() const;
  void SetMozSrcObject(DOMMediaStream& aValue);
  void SetMozSrcObject(DOMMediaStream* aValue);

  bool MozPreservesPitch() const
  {
    return mPreservesPitch;
  }

  // XPCOM MozPreservesPitch() is OK

  MediaKeys* GetMediaKeys() const;

  already_AddRefed<Promise> SetMediaKeys(MediaKeys* mediaKeys,
                                         ErrorResult& aRv);

  mozilla::dom::EventHandlerNonNull* GetOnencrypted();
  void SetOnencrypted(mozilla::dom::EventHandlerNonNull* aCallback);

  mozilla::dom::EventHandlerNonNull* GetOnwaitingforkey();
  void SetOnwaitingforkey(mozilla::dom::EventHandlerNonNull* aCallback);

  void DispatchEncrypted(const nsTArray<uint8_t>& aInitData,
                         const nsAString& aInitDataType) override;

  bool IsEventAttributeName(nsIAtom* aName) override;

  // Returns the principal of the "top level" document; the origin displayed
  // in the URL bar of the browser window.
  already_AddRefed<nsIPrincipal> GetTopLevelPrincipal();

  bool ContainsRestrictedContent();

  void CannotDecryptWaitingForKey();

  bool MozAutoplayEnabled() const
  {
    return mAutoplayEnabled;
  }

  already_AddRefed<DOMMediaStream> CaptureAudio(ErrorResult& aRv,
                                                MediaStreamGraph* aGraph);

  already_AddRefed<DOMMediaStream> MozCaptureStream(ErrorResult& aRv);

  already_AddRefed<DOMMediaStream> MozCaptureStreamUntilEnded(ErrorResult& aRv);

  bool MozAudioCaptured() const
  {
    return mAudioCaptured;
  }

  void MozGetMetadata(JSContext* aCx, JS::MutableHandle<JSObject*> aResult,
                      ErrorResult& aRv);

  double MozFragmentEnd();

  AudioTrackList* AudioTracks();

  VideoTrackList* VideoTracks();

  TextTrackList* GetTextTracks();

  already_AddRefed<TextTrack> AddTextTrack(TextTrackKind aKind,
                                           const nsAString& aLabel,
                                           const nsAString& aLanguage);

  void AddTextTrack(TextTrack* aTextTrack) {
    GetOrCreateTextTrackManager()->AddTextTrack(aTextTrack);
  }

  void RemoveTextTrack(TextTrack* aTextTrack, bool aPendingListOnly = false) {
    if (mTextTrackManager) {
      mTextTrackManager->RemoveTextTrack(aTextTrack, aPendingListOnly);
    }
  }

  void NotifyCueAdded(TextTrackCue& aCue) {
    if (mTextTrackManager) {
      mTextTrackManager->NotifyCueAdded(aCue);
    }
  }
  void NotifyCueRemoved(TextTrackCue& aCue) {
    if (mTextTrackManager) {
      mTextTrackManager->NotifyCueRemoved(aCue);
    }
  }
  void NotifyCueUpdated(TextTrackCue *aCue) {
    if (mTextTrackManager) {
      mTextTrackManager->NotifyCueUpdated(aCue);
    }
  }

  void NotifyCueDisplayStatesChanged();

  bool GetHasUserInteraction()
  {
    return mHasUserInteraction;
  }

  // A method to check whether we are currently playing.
  bool IsCurrentlyPlaying() const;

  // Returns true if the media element is being destroyed. Used in
  // dormancy checks to prevent dormant processing for an element
  // that will soon be gone.
  bool IsBeingDestroyed();

  // These are used for testing only
  float ComputedVolume() const;
  bool ComputedMuted() const;
  nsSuspendedTypes ComputedSuspended() const;

  void SetMediaInfo(const MediaInfo& aInfo);

  virtual AbstractThread* AbstractMainThread() const final override;

  // Telemetry: to record the usage of a {visible / invisible} video element as
  // the source of {drawImage(), createPattern(), createImageBitmap() and
  // captureStream()} APIs.
  enum class CallerAPI {
    DRAW_IMAGE,
    CREATE_PATTERN,
    CREATE_IMAGEBITMAP,
    CAPTURE_STREAM,
  };
  void MarkAsContentSource(CallerAPI aAPI);

  nsIDocument* GetDocument() const override;

  void ConstructMediaTracks(const MediaInfo* aInfo) override;

  void RemoveMediaTracks() override;

  already_AddRefed<GMPCrashHelper> CreateGMPCrashHelper() override;

  // The promise resolving/rejection is queued as a "micro-task" which will be
  // handled immediately after the current JS task and before any pending JS
  // tasks.
  // At the time we are going to resolve/reject a promise, the "seeking" event
  // task should already be queued but might yet be processed, so we queue one
  // more task to file the promise resolving/rejection micro-tasks
  // asynchronously to make sure that the micro-tasks are processed after the
  // "seeking" event task.
  void AsyncResolveSeekDOMPromiseIfExists() override;
  void AsyncRejectSeekDOMPromiseIfExists() override;

protected:
  virtual ~HTMLMediaElement();

  class AudioChannelAgentCallback;
  class ChannelLoader;
  class ErrorSink;
  class MediaLoadListener;
  class MediaStreamTracksAvailableCallback;
  class MediaStreamTrackListener;
  class StreamListener;
  class StreamSizeListener;
  class ShutdownObserver;

  MediaDecoderOwner::NextFrameStatus NextFrameStatus();

  void SetDecoder(MediaDecoder* aDecoder) {
    MOZ_ASSERT(aDecoder); // Use ShutdownDecoder() to clear.
    if (mDecoder) {
      ShutdownDecoder();
    }
    mDecoder = aDecoder;
  }

  class WakeLockBoolWrapper {
  public:
    explicit WakeLockBoolWrapper(bool val = false)
      : mValue(val), mCanPlay(true), mOuter(nullptr) {}

    ~WakeLockBoolWrapper();

    void SetOuter(HTMLMediaElement* outer) { mOuter = outer; }
    void SetCanPlay(bool aCanPlay);

    MOZ_IMPLICIT operator bool() const { return mValue; }

    WakeLockBoolWrapper& operator=(bool val);

    bool operator !() const { return !mValue; }

    static void TimerCallback(nsITimer* aTimer, void* aClosure);

  private:
    void UpdateWakeLock();

    bool mValue;
    bool mCanPlay;
    HTMLMediaElement* mOuter;
    nsCOMPtr<nsITimer> mTimer;
  };

  // Holds references to the DOM wrappers for the MediaStreams that we're
  // writing to.
  struct OutputMediaStream {
    OutputMediaStream();
    ~OutputMediaStream();

    RefPtr<DOMMediaStream> mStream;
    bool mFinishWhenEnded;
    bool mCapturingAudioOnly;
    bool mCapturingDecoder;
    bool mCapturingMediaStream;

    // The following members are keeping state for a captured MediaStream.
    TrackID mNextAvailableTrackID;
    nsTArray<Pair<nsString, RefPtr<MediaInputPort>>> mTrackPorts;
  };

  already_AddRefed<Promise> PlayInternal(ErrorResult& aRv);

  /** Use this method to change the mReadyState member, so required
   * events can be fired.
   */
  void ChangeReadyState(nsMediaReadyState aState);

  /**
   * Use this method to change the mNetworkState member, so required
   * actions will be taken during the transition.
   */
  void ChangeNetworkState(nsMediaNetworkState aState);

  /**
   * These two methods are called by the WakeLockBoolWrapper when the wakelock
   * has to be created or released.
   */
  virtual void WakeLockCreate();
  virtual void WakeLockRelease();
  RefPtr<WakeLock> mWakeLock;

  /**
   * Logs a warning message to the web console to report various failures.
   * aMsg is the localized message identifier, aParams is the parameters to
   * be substituted into the localized message, and aParamCount is the number
   * of parameters in aParams.
   */
  void ReportLoadError(const char* aMsg,
                       const char16_t** aParams = nullptr,
                       uint32_t aParamCount = 0);

  /**
   * Changes mHasPlayedOrSeeked to aValue. If mHasPlayedOrSeeked changes
   * we'll force a reflow so that the video frame gets reflowed to reflect
   * the poster hiding or showing immediately.
   */
  void SetPlayedOrSeeked(bool aValue);

  /**
   * Initialize the media element for playback of aStream
   */
  void SetupSrcMediaStreamPlayback(DOMMediaStream* aStream);
  /**
   * Stop playback on mSrcStream.
   */
  void EndSrcMediaStreamPlayback();
  /**
   * Ensure we're playing mSrcStream if and only if we're not paused.
   */
  enum { REMOVING_SRC_STREAM = 0x1 };
  void UpdateSrcMediaStreamPlaying(uint32_t aFlags = 0);

  /**
   * Called by our DOMMediaStream::TrackListener when a new MediaStreamTrack has
   * been added to the playback stream of |mSrcStream|.
   */
  void NotifyMediaStreamTrackAdded(const RefPtr<MediaStreamTrack>& aTrack);

  /**
   * Called by our DOMMediaStream::TrackListener when a MediaStreamTrack in
   * |mSrcStream|'s playback stream has ended.
   */
  void NotifyMediaStreamTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack);

  /**
   * Enables or disables all tracks forwarded from mSrcStream to all
   * OutputMediaStreams. We do this for muting the tracks when pausing,
   * and unmuting when playing the media element again.
   *
   * If mSrcStream is unset, this does nothing.
   */
  void SetCapturedOutputStreamsEnabled(bool aEnabled);

  /**
   * Create a new MediaStreamTrack for aTrack and add it to the DOMMediaStream
   * in aOutputStream. This automatically sets the output track to enabled or
   * disabled depending on our current playing state.
   */
  void AddCaptureMediaTrackToOutputStream(MediaTrack* aTrack,
                                          OutputMediaStream& aOutputStream,
                                          bool aAsyncAddtrack = true);

  /**
   * Returns an DOMMediaStream containing the played contents of this
   * element. When aFinishWhenEnded is true, when this element ends playback
   * we will finish the stream and not play any more into it.
   * When aFinishWhenEnded is false, ending playback does not finish the stream.
   * The stream will never finish.
   *
   * When aCaptureAudio is true, we stop playout of audio and instead route it
   * to the DOMMediaStream. Volume and mute state will be applied to the audio
   * reaching the stream. No video tracks will be captured in this case.
   */
  already_AddRefed<DOMMediaStream> CaptureStreamInternal(bool aFinishWhenEnded,
                                                         bool aCaptureAudio,
                                                         MediaStreamGraph* aGraph);

  /**
   * Initialize a decoder as a clone of an existing decoder in another
   * element.
   * mLoadingSrc must already be set.
   */
  nsresult InitializeDecoderAsClone(ChannelMediaDecoder* aOriginal);

  /**
   * Initialize a decoder to load the given channel. The decoder's stream
   * listener is returned via aListener.
   * mLoadingSrc must already be set.
   */
  nsresult InitializeDecoderForChannel(nsIChannel *aChannel,
                                       nsIStreamListener **aListener);

  /**
   * Finish setting up the decoder after Load() has been called on it.
   * Called by InitializeDecoderForChannel/InitializeDecoderAsClone.
   */
  nsresult FinishDecoderSetup(MediaDecoder* aDecoder);

  /**
   * Call this after setting up mLoadingSrc and mDecoder.
   */
  void AddMediaElementToURITable();
  /**
   * Call this before modifying mLoadingSrc.
   */
  void RemoveMediaElementFromURITable();
  /**
   * Call this to find a media element with the same NodePrincipal and mLoadingSrc
   * set to aURI, and with a decoder on which Load() has been called.
   */
  HTMLMediaElement* LookupMediaElementURITable(nsIURI* aURI);

  /**
   * Shutdown and clear mDecoder and maintain associated invariants.
   */
  void ShutdownDecoder();
  /**
   * Execute the initial steps of the load algorithm that ensure existing
   * loads are aborted, the element is emptied, and a new load ID is
   * created.
   */
  void AbortExistingLoads();

  /**
   * This is the dedicated media source failure steps.
   * Called when all potential resources are exhausted. Changes network
   * state to NETWORK_NO_SOURCE, and sends error event with code
   * MEDIA_ERR_SRC_NOT_SUPPORTED.
   */
  void NoSupportedMediaSourceError(const nsACString& aErrorDetails = nsCString());

  /**
   * Per spec, Failed with elements: Queue a task, using the DOM manipulation
   * task source, to fire a simple event named error at the candidate element.
   * So dispatch |QueueLoadFromSourceTask| to main thread to make sure the task
   * will be executed later than loadstart event.
   */
  void DealWithFailedElement(nsIContent* aSourceElement);

  /**
   * Attempts to load resources from the <source> children. This is a
   * substep of the resource selection algorithm. Do not call this directly,
   * call QueueLoadFromSourceTask() instead.
   */
  void LoadFromSourceChildren();

  /**
   * Asynchronously awaits a stable state, and then causes
   * LoadFromSourceChildren() to be called on the main threads' event loop.
   */
  void QueueLoadFromSourceTask();

  /**
   * Runs the media resource selection algorithm.
   */
  void SelectResource();

  /**
   * A wrapper function that allows us to cleanly reset flags after a call
   * to SelectResource()
   */
  void SelectResourceWrapper();

  /**
   * Asynchronously awaits a stable state, and then causes SelectResource()
   * to be run on the main thread's event loop.
   */
  void QueueSelectResourceTask();

  /**
   * When loading a new source on an existing media element, make sure to reset
   * everything that is accessible using the media element API.
   */
  void ResetState();

  /**
   * The resource-fetch algorithm step of the load algorithm.
   */
  nsresult LoadResource();

  /**
   * Selects the next <source> child from which to load a resource. Called
   * during the resource selection algorithm. Stores the return value in
   * mSourceLoadCandidate before returning.
   */
  nsIContent* GetNextSource();

  /**
   * Changes mDelayingLoadEvent, and will call BlockOnLoad()/UnblockOnLoad()
   * on the owning document, so it can delay the load event firing.
   */
  void ChangeDelayLoadStatus(bool aDelay);

  /**
   * If we suspended downloading after the first frame, unsuspend now.
   */
  void StopSuspendingAfterFirstFrame();

  /**
   * Called when our channel is redirected to another channel.
   * Updates our mChannel reference to aNewChannel.
   */
  nsresult OnChannelRedirect(nsIChannel *aChannel,
                             nsIChannel *aNewChannel,
                             uint32_t aFlags);

  /**
   * Call this to reevaluate whether we should be holding a self-reference.
   */
  void AddRemoveSelfReference();

  /**
   * Called asynchronously to release a self-reference to this element.
   */
  void DoRemoveSelfReference();

  /**
   * Called when "xpcom-shutdown" event is received.
   */
  void NotifyShutdownEvent();

  /**
   * Possible values of the 'preload' attribute.
   */
  enum PreloadAttrValue : uint8_t {
    PRELOAD_ATTR_EMPTY,    // set to ""
    PRELOAD_ATTR_NONE,     // set to "none"
    PRELOAD_ATTR_METADATA, // set to "metadata"
    PRELOAD_ATTR_AUTO      // set to "auto"
  };

  /**
   * The preloading action to perform. These dictate how we react to the
   * preload attribute. See mPreloadAction.
   */
  enum PreloadAction {
    PRELOAD_UNDEFINED = 0, // not determined - used only for initialization
    PRELOAD_NONE = 1,      // do not preload
    PRELOAD_METADATA = 2,  // preload only the metadata (and first frame)
    PRELOAD_ENOUGH = 3     // preload enough data to allow uninterrupted
                           // playback
  };

  /**
   * The guts of Load(). Load() acts as a wrapper around this which sets
   * mIsDoingExplicitLoad to true so that when script calls 'load()'
   * preload-none will be automatically upgraded to preload-metadata.
   */
  void DoLoad();

  /**
   * Suspends the load of mLoadingSrc, so that it can be resumed later
   * by ResumeLoad(). This is called when we have a media with a 'preload'
   * attribute value of 'none', during the resource selection algorithm.
   */
  void SuspendLoad();

  /**
   * Resumes a previously suspended load (suspended by SuspendLoad(uri)).
   * Will continue running the resource selection algorithm.
   * Sets mPreloadAction to aAction.
   */
  void ResumeLoad(PreloadAction aAction);

  /**
   * Handle a change to the preload attribute. Should be called whenever the
   * value (or presence) of the preload attribute changes. The change in
   * attribute value may cause a change in the mPreloadAction of this
   * element. If there is a change then this method will initiate any
   * behaviour that is necessary to implement the action.
   */
  void UpdatePreloadAction();

  /**
   * Fire progress events if needed according to the time and byte constraints
   * outlined in the specification. aHaveNewProgress is true if progress has
   * just been detected.  Otherwise the method is called as a result of the
   * progress timer.
   */
  void CheckProgress(bool aHaveNewProgress);
  static void ProgressTimerCallback(nsITimer* aTimer, void* aClosure);
  /**
   * Start timer to update download progress.
   */
  void StartProgressTimer();
  /**
   * Start sending progress and/or stalled events.
   */
  void StartProgress();
  /**
   * Stop progress information timer and events.
   */
  void StopProgress();

  /**
   * Dispatches an error event to a child source element.
   */
  void DispatchAsyncSourceError(nsIContent* aSourceElement);

  /**
   * Resets the media element for an error condition as per aErrorCode.
   * aErrorCode must be one of nsIDOMHTMLMediaError codes.
   */
  void Error(uint16_t aErrorCode, const nsACString& aErrorDetails = nsCString());

  /**
   * Returns the URL spec of the currentSrc.
   **/
  void GetCurrentSpec(nsCString& aString);

  /**
   * Process any media fragment entries in the URI
   */
  void ProcessMediaFragmentURI();

  /**
   * Mute or unmute the audio and change the value that the |muted| map.
   */
  void SetMutedInternal(uint32_t aMuted);
  /**
   * Update the volume of the output audio stream to match the element's
   * current mMuted/mVolume/mAudioChannelFaded state.
   */
  void SetVolumeInternal();

  /**
   * Suspend (if aPauseForInactiveDocument) or resume element playback and
   * resource download.  If aSuspendEvents is true, event delivery is
   * suspended (and events queued) until the element is resumed.
   */
  void SuspendOrResumeElement(bool aPauseElement, bool aSuspendEvents);

  // Get the HTMLMediaElement object if the decoder is being used from an
  // HTML media element, and null otherwise.
  virtual HTMLMediaElement* GetMediaElement() final override
  {
    return this;
  }

  // Return true if decoding should be paused
  virtual bool GetPaused() final override
  {
    bool isPaused = false;
    GetPaused(&isPaused);
    return isPaused;
  }

  /**
   * Video has been playing while hidden and, if feature was enabled, would
   * trigger suspending decoder.
   * Used to track hidden-video-decode-suspend telemetry.
   */
  static void VideoDecodeSuspendTimerCallback(nsITimer* aTimer, void* aClosure);
  /**
   * Video is now both: playing and hidden.
   * Used to track hidden-video telemetry.
   */
  void HiddenVideoStart();
  /**
   * Video is not playing anymore and/or has become visible.
   * Used to track hidden-video telemetry.
   */
  void HiddenVideoStop();

  void ReportEMETelemetry();

  void ReportTelemetry();

  // Seeks to aTime seconds. aSeekType can be Exact to seek to exactly the
  // seek target, or PrevSyncPoint if a quicker but less precise seek is
  // desired, and we'll seek to the sync point (keyframe and/or start of the
  // next block of audio samples) preceeding seek target.
  already_AddRefed<Promise> Seek(double aTime, SeekTarget::Type aSeekType, ErrorResult& aRv);

  // Update the audio channel playing state
  void UpdateAudioChannelPlayingState(bool aForcePlaying = false);

  // Adds to the element's list of pending text tracks each text track
  // in the element's list of text tracks whose text track mode is not disabled
  // and whose text track readiness state is loading.
  void PopulatePendingTextTrackList();

  // Gets a reference to the MediaElement's TextTrackManager. If the
  // MediaElement doesn't yet have one then it will create it.
  TextTrackManager* GetOrCreateTextTrackManager();

  // Recomputes ready state and fires events as necessary based on current state.
  void UpdateReadyStateInternal();

  // Determine if the element should be paused because of suspend conditions.
  bool ShouldElementBePaused();

  // Create or destroy the captured stream.
  void AudioCaptureStreamChange(bool aCapture);

  // A method to check whether the media element is allowed to start playback.
  bool IsAllowedToPlay();

  // If the network state is empty and then we would trigger DoLoad().
  void MaybeDoLoad();

  // Anything we need to check after played success and not related with spec.
  void UpdateCustomPolicyAfterPlayed();

  // True if this element can be captured, false otherwise.
  bool CanBeCaptured(bool aCaptureAudio);

  class nsAsyncEventRunner;
  class nsNotifyAboutPlayingRunner;
  class nsResolveOrRejectPendingPlayPromisesRunner;
  using nsGenericHTMLElement::DispatchEvent;
  // For nsAsyncEventRunner.
  nsresult DispatchEvent(const nsAString& aName);

  // Open unsupported types media with the external app when the media element
  // triggers play() after loaded fail. eg. preload the data before start play.
  void OpenUnsupportedMediaWithExternalAppIfNeeded() const;

  // This method moves the mPendingPlayPromises into a temperate object. So the
  // mPendingPlayPromises is cleared after this method call.
  nsTArray<RefPtr<Promise>> TakePendingPlayPromises();

  // This method snapshots the mPendingPlayPromises by TakePendingPlayPromises()
  // and queues a task to resolve them.
  void AsyncResolvePendingPlayPromises();

  // This method snapshots the mPendingPlayPromises by TakePendingPlayPromises()
  // and queues a task to reject them.
  void AsyncRejectPendingPlayPromises(nsresult aError);

  // This method snapshots the mPendingPlayPromises by TakePendingPlayPromises()
  // and queues a task to resolve them also to dispatch a "playing" event.
  void NotifyAboutPlaying();

  already_AddRefed<Promise> CreateDOMPromise(ErrorResult& aRv) const;

  // Pass information for deciding the video decode mode to decoder.
  void NotifyDecoderActivityChanges() const;

  // Mark the decoder owned by the element as tainted so that the
  // suspend-video-decoder is disabled.
  void MarkAsTainted();

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                bool aNotify) override;
  virtual nsresult OnAttrSetButNotChanged(int32_t aNamespaceID, nsIAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify) override;

  // The current decoder. Load() has been called on this decoder.
  // At most one of mDecoder and mSrcStream can be non-null.
  RefPtr<MediaDecoder> mDecoder;

  // The DocGroup-specific AbstractThread::MainThread() of this HTML element.
  RefPtr<AbstractThread> mAbstractMainThread;

  // Observers listening to changes to the mDecoder principal.
  // Used by streams captured from this element.
  nsTArray<DecoderPrincipalChangeObserver*> mDecoderPrincipalChangeObservers;

  // State-watching manager.
  WatchManager<HTMLMediaElement> mWatchManager;

  // A reference to the VideoFrameContainer which contains the current frame
  // of video to display.
  RefPtr<VideoFrameContainer> mVideoFrameContainer;

  // Holds a reference to the DOM wrapper for the MediaStream that has been
  // set in the src attribute.
  RefPtr<DOMMediaStream> mSrcAttrStream;

  // Holds a reference to the DOM wrapper for the MediaStream that we're
  // actually playing.
  // At most one of mDecoder and mSrcStream can be non-null.
  RefPtr<DOMMediaStream> mSrcStream;

  // True once mSrcStream's initial set of tracks are known.
  bool mSrcStreamTracksAvailable;

  // If non-negative, the time we should return for currentTime while playing
  // mSrcStream.
  double mSrcStreamPausedCurrentTime;

  // Holds a reference to the stream connecting this stream to the capture sink.
  RefPtr<MediaInputPort> mCaptureStreamPort;

  // Holds references to the DOM wrappers for the MediaStreams that we're
  // writing to.
  nsTArray<OutputMediaStream> mOutputStreams;

  // Holds a reference to the MediaStreamListener attached to mSrcStream's
  // playback stream.
  RefPtr<StreamListener> mMediaStreamListener;
  // Holds a reference to the size-getting MediaStreamListener attached to
  // mSrcStream.
  RefPtr<StreamSizeListener> mMediaStreamSizeListener;
  // The selected video stream track which contained mMediaStreamSizeListener.
  RefPtr<VideoStreamTrack> mSelectedVideoStreamTrack;

  const RefPtr<ShutdownObserver> mShutdownObserver;

  // Holds a reference to the MediaSource, if any, referenced by the src
  // attribute on the media element.
  RefPtr<MediaSource> mSrcMediaSource;

  // Holds a reference to the MediaSource supplying data for playback.  This
  // may either match mSrcMediaSource or come from Source element children.
  // This is set when and only when mLoadingSrc corresponds to an object url
  // that resolved to a MediaSource.
  RefPtr<MediaSource> mMediaSource;

  RefPtr<ChannelLoader> mChannelLoader;

  // The current media load ID. This is incremented every time we start a
  // new load. Async events note the ID when they're first sent, and only fire
  // if the ID is unchanged when they come to fire.
  uint32_t mCurrentLoadID;

  // Points to the child source elements, used to iterate through the children
  // when selecting a resource to load.  This is the index of the child element
  // that is the current 'candidate' in:
  // https://html.spec.whatwg.org/multipage/media.html#concept-media-load-algorithm
  uint32_t mSourcePointer;

  // Points to the document whose load we're blocking. This is the document
  // we're bound to when loading starts.
  nsCOMPtr<nsIDocument> mLoadBlockedDoc;

  // Contains names of events that have been raised while in the bfcache.
  // These events get re-dispatched when the bfcache is exited.
  nsTArray<nsString> mPendingEvents;

  // Media loading flags. See:
  //   http://www.whatwg.org/specs/web-apps/current-work/#video)
  nsMediaNetworkState mNetworkState;
  Watchable<nsMediaReadyState> mReadyState;

  enum LoadAlgorithmState {
    // No load algorithm instance is waiting for a source to be added to the
    // media in order to continue loading.
    NOT_WAITING,
    // We've run the load algorithm, and we tried all source children of the
    // media element, and failed to load any successfully. We're waiting for
    // another source element to be added to the media element, and will try
    // to load any such element when its added.
    WAITING_FOR_SOURCE
  };

  // Denotes the waiting state of a load algorithm instance. When the load
  // algorithm is waiting for a source element child to be added, this is set
  // to WAITING_FOR_SOURCE, otherwise it's NOT_WAITING.
  LoadAlgorithmState mLoadWaitStatus;

  // Current audio volume
  double mVolume;

  nsAutoPtr<const MetadataTags> mTags;

  // URI of the resource we're attempting to load. This stores the value we
  // return in the currentSrc attribute. Use GetCurrentSrc() to access the
  // currentSrc attribute.
  // This is always the original URL we're trying to load --- before
  // redirects etc.
  nsCOMPtr<nsIURI> mLoadingSrc;

  // Stores the current preload action for this element. Initially set to
  // PRELOAD_UNDEFINED, its value is changed by calling
  // UpdatePreloadAction().
  PreloadAction mPreloadAction;

  // Time that the last timeupdate event was fired. Read/Write from the
  // main thread only.
  TimeStamp mTimeUpdateTime;

  // Time that the last progress event was fired. Read/Write from the
  // main thread only.
  TimeStamp mProgressTime;

  // Time that data was last read from the media resource. Used for
  // computing if the download has stalled and to rate limit progress events
  // when data is arriving slower than PROGRESS_MS.
  // Read/Write from the main thread only.
  TimeStamp mDataTime;

  // Media 'currentTime' value when the last timeupdate event occurred.
  // Read/Write from the main thread only.
  double mLastCurrentTime;

  // Logical start time of the media resource in seconds as obtained
  // from any media fragments. A negative value indicates that no
  // fragment time has been set. Read/Write from the main thread only.
  double mFragmentStart;

  // Logical end time of the media resource in seconds as obtained
  // from any media fragments. A negative value indicates that no
  // fragment time has been set. Read/Write from the main thread only.
  double mFragmentEnd;

  // The defaultPlaybackRate attribute gives the desired speed at which the
  // media resource is to play, as a multiple of its intrinsic speed.
  double mDefaultPlaybackRate;

  // The playbackRate attribute gives the speed at which the media resource
  // plays, as a multiple of its intrinsic speed. If it is not equal to the
  // defaultPlaybackRate, then the implication is that the user is using a
  // feature such as fast forward or slow motion playback.
  double mPlaybackRate;

  // True if pitch correction is applied when playbackRate is set to a
  // non-intrinsic value.
  bool mPreservesPitch;

  // Reference to the source element last returned by GetNextSource().
  // This is the child source element which we're trying to load from.
  nsCOMPtr<nsIContent> mSourceLoadCandidate;

  // Range of time played.
  RefPtr<TimeRanges> mPlayed;

  // Timer used for updating progress events.
  nsCOMPtr<nsITimer> mProgressTimer;

  // Timer used to simulate video-suspend.
  nsCOMPtr<nsITimer> mVideoDecodeSuspendTimer;

  // Encrypted Media Extension media keys.
  RefPtr<MediaKeys> mMediaKeys;

  // Stores the time at the start of the current 'played' range.
  double mCurrentPlayRangeStart;

  // If true then we have begun downloading the media content.
  // Set to false when completed, or not yet started.
  bool mBegun;

  // True if loadeddata has been fired.
  bool mLoadedDataFired;

  // Indicates whether current playback is a result of user action
  // (ie. calling of the Play method), or automatic playback due to
  // the 'autoplay' attribute being set. A true value indicates the
  // latter case.
  // The 'autoplay' HTML attribute indicates that the video should
  // start playing when loaded. The 'autoplay' attribute of the object
  // is a mirror of the HTML attribute. These are different from this
  // 'mAutoplaying' flag, which indicates whether the current playback
  // is a result of the autoplay attribute.
  bool mAutoplaying;

  // Indicates whether |autoplay| will actually autoplay based on the pref
  // media.autoplay.enabled
  bool mAutoplayEnabled;

  // Playback of the video is paused either due to calling the
  // 'Pause' method, or playback not yet having started.
  WakeLockBoolWrapper mPaused;

  enum MutedReasons {
    MUTED_BY_CONTENT               = 0x01,
    MUTED_BY_INVALID_PLAYBACK_RATE = 0x02,
    MUTED_BY_AUDIO_CHANNEL         = 0x04,
    MUTED_BY_AUDIO_TRACK           = 0x08
  };

  uint32_t mMuted;

  // True if the media statistics are currently being shown by the builtin
  // video controls
  bool mStatsShowing;

  // The following two fields are here for the private storage of the builtin
  // video controls, and control 'casting' of the video to external devices
  // (TVs, projectors etc.)
  // True if casting is currently allowed
  bool mAllowCasting;
  // True if currently casting this video
  bool mIsCasting;

  // True if the sound is being captured.
  bool mAudioCaptured;

  // If TRUE then the media element was actively playing before the currently
  // in progress seeking. If FALSE then the media element is either not seeking
  // or was not actively playing before the current seek. Used to decide whether
  // to raise the 'waiting' event as per 4.7.1.8 in HTML 5 specification.
  bool mPlayingBeforeSeek;

  // True iff this element is paused because the document is inactive or has
  // been suspended by the audio channel service.
  bool mPausedForInactiveDocumentOrChannel;

  // True iff event delivery is suspended (mPausedForInactiveDocumentOrChannel must also be true).
  bool mEventDeliveryPaused;

  // True if we're running the "load()" method.
  bool mIsRunningLoadMethod;

  // True if we're running or waiting to run queued tasks due to an explicit
  // call to "load()".
  bool mIsDoingExplicitLoad;

  // True if we're loading the resource from the child source elements.
  bool mIsLoadingFromSourceChildren;

  // True if we're delaying the "load" event. They are delayed until either
  // an error occurs, or the first frame is loaded.
  bool mDelayingLoadEvent;

  // True when we've got a task queued to call SelectResource(),
  // or while we're running SelectResource().
  bool mIsRunningSelectResource;

  // True when we already have select resource call queued
  bool mHaveQueuedSelectResource;

  // True if we suspended the decoder because we were paused,
  // preloading metadata is enabled, autoplay was not enabled, and we loaded
  // the first frame.
  bool mSuspendedAfterFirstFrame;

  // True if we are allowed to suspend the decoder because we were paused,
  // preloading metdata was enabled, autoplay was not enabled, and we loaded
  // the first frame.
  bool mAllowSuspendAfterFirstFrame;

  // True if we've played or completed a seek. We use this to determine
  // when the poster frame should be shown.
  bool mHasPlayedOrSeeked;

  // True if we've added a reference to ourselves to keep the element
  // alive while no-one is referencing it but the element may still fire
  // events of its own accord.
  bool mHasSelfReference;

  // True if we've received a notification that the engine is shutting
  // down.
  bool mShuttingDown;

  // True if we've suspended a load in the resource selection algorithm
  // due to loading a preload:none media. When true, the resource we'll
  // load when the user initiates either playback or an explicit load is
  // stored in mPreloadURI.
  bool mSuspendedForPreloadNone;

  // True if we've connected mSrcStream to the media element output.
  bool mSrcStreamIsPlaying;

  // True if a same-origin check has been done for the media element and resource.
  bool mMediaSecurityVerified;

  // True if we should set nsIClassOfService::UrgentStart to the channel to
  // get the response ASAP for better user responsiveness.
  bool mUseUrgentStartForChannel = false;

  // The CORS mode when loading the media element
  CORSMode mCORSMode;

  // Info about the played media.
  MediaInfo mMediaInfo;

  // True if the media has encryption information.
  bool mIsEncrypted;

  enum WaitingForKeyState {
    NOT_WAITING_FOR_KEY = 0,
    WAITING_FOR_KEY = 1,
    WAITING_FOR_KEY_DISPATCHED = 2
  };

  // True when the CDM cannot decrypt the current block due to lacking a key.
  // Note: the "waitingforkey" event is not dispatched until all decoded data
  // has been rendered.
  WaitingForKeyState mWaitingForKey;

  // Listens for waitingForKey events from the owned decoder.
  MediaEventListener mWaitingForKeyListener;

  // Init Data that needs to be sent in 'encrypted' events in MetadataLoaded().
  EncryptionInfo mPendingEncryptedInitData;

  // True if the media's channel's download has been suspended.
  Watchable<bool> mDownloadSuspendedByCache;

  // Audio Channel.
  AudioChannel mAudioChannel;

  // Disable the video playback by track selection. This flag might not be
  // enough if we ever expand the ability of supporting multi-tracks video
  // playback.
  bool mDisableVideo;

  RefPtr<TextTrackManager> mTextTrackManager;

  RefPtr<AudioTrackList> mAudioTrackList;

  RefPtr<VideoTrackList> mVideoTrackList;

  nsAutoPtr<MediaStreamTrackListener> mMediaStreamTrackListener;

  // The principal guarding mVideoFrameContainer access when playing a
  // MediaStream.
  nsCOMPtr<nsIPrincipal> mSrcStreamVideoPrincipal;

  // True if UnbindFromTree() is called on the element.
  // Note this flag is false when the element is in a phase after creation and
  // before attaching to the DOM tree.
  bool mUnboundFromTree = false;

public:
  // Helper class to measure times for MSE telemetry stats
  class TimeDurationAccumulator
  {
  public:
    TimeDurationAccumulator()
      : mCount(0)
    {}
    void Start()
    {
      if (IsStarted()) {
        return;
      }
      mStartTime = TimeStamp::Now();
    }
    void Pause()
    {
      if (!IsStarted()) {
        return;
      }
      mSum += (TimeStamp::Now() - mStartTime);
      mCount++;
      mStartTime = TimeStamp();
    }
    bool IsStarted() const
    {
      return !mStartTime.IsNull();
    }
    double Total() const
    {
      if (!IsStarted()) {
        return mSum.ToSeconds();
      }
      // Add current running time until now, but keep it running.
      return (mSum + (TimeStamp::Now() - mStartTime)).ToSeconds();
    }
    uint32_t Count() const
    {
      if (!IsStarted()) {
        return mCount;
      }
      // Count current run in this report, without increasing the stored count.
      return mCount + 1;
    }
  private:
    TimeStamp mStartTime;
    TimeDuration mSum;
    uint32_t mCount;
  };
private:
  /**
   * This function is called by AfterSetAttr and OnAttrSetButNotChanged.
   * It will not be called if the value is being unset.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aNotify Whether we plan to notify document observers.
   */
  void AfterMaybeChangeAttr(int32_t aNamespaceID, nsIAtom* aName, bool aNotify);

  // Total time a video has spent playing.
  TimeDurationAccumulator mPlayTime;

  // Total time a video has spent playing while hidden.
  TimeDurationAccumulator mHiddenPlayTime;

  // Total time a video has (or would have) spent in video-decode-suspend mode.
  TimeDurationAccumulator mVideoDecodeSuspendTime;

  // Indicates if user has interacted with the element.
  // Used to block autoplay when disabled.
  bool mHasUserInteraction;

  // True if the first frame has been successfully loaded.
  bool mFirstFrameLoaded;

  // Media elements also have a default playback start position, which must
  // initially be set to zero seconds. This time is used to allow the element to
  // be seeked even before the media is loaded.
  double mDefaultPlaybackStartPosition;

  // True if the audio track is not silent.
  bool mIsAudioTrackAudible;

  // True if media element has been marked as 'tainted' and can't
  // participate in video decoder suspending.
  bool mHasSuspendTaint;

  // True if audio tracks and video tracks are constructed and added into the
  // track list, false if all tracks are removed from the track list.
  bool mMediaTracksConstructed;

  Visibility mVisibilityState;

  UniquePtr<ErrorSink> mErrorSink;

  // This wrapper will handle all audio channel related stuffs, eg. the operations
  // of tab audio indicator, Fennec's media control.
  // Note: mAudioChannelWrapper might be null after GC happened.
  RefPtr<AudioChannelAgentCallback> mAudioChannelWrapper;

  // A list of pending play promises. The elements are pushed during the play()
  // method call and are resolved/rejected during further playback steps.
  nsTArray<RefPtr<Promise>> mPendingPlayPromises;

  // A list of already-dispatched but not yet run
  // nsResolveOrRejectPendingPlayPromisesRunners.
  // Runners whose Run() method is called remove themselves from this list.
  // We keep track of these because the load algorithm resolves/rejects all
  // already-dispatched pending play promises.
  nsTArray<nsResolveOrRejectPendingPlayPromisesRunner*> mPendingPlayPromisesRunners;

  // A pending seek promise which is created at Seek() method call and is
  // resolved/rejected at AsyncResolveSeekDOMPromiseIfExists()/
  // AsyncRejectSeekDOMPromiseIfExists() methods.
  RefPtr<dom::Promise> mSeekDOMPromise;
};

// Check if the context is chrome or has the debugger permission
bool HasDebuggerPrivilege(JSContext* aCx, JSObject* aObj);

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMediaElement_h
