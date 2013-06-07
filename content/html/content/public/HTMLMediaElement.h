/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLMediaElement_h
#define mozilla_dom_HTMLMediaElement_h

#include "nsIDOMHTMLMediaElement.h"
#include "nsGenericHTMLElement.h"
#include "MediaDecoderOwner.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsThreadUtils.h"
#include "nsIDOMRange.h"
#include "nsCycleCollectionParticipant.h"
#include "nsILoadGroup.h"
#include "nsIObserver.h"
#include "AudioStream.h"
#include "VideoFrameContainer.h"
#include "mozilla/CORSMode.h"
#include "DOMMediaStream.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsIDOMWakeLock.h"
#include "AudioChannelCommon.h"
#include "DecoderTraits.h"
#include "MediaMetadataManager.h"
#include "AudioChannelAgent.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackList.h"
#include "mozilla/ErrorResult.h"

// Define to output information on decoding and painting framerate
/* #define DEBUG_FRAME_RATE 1 */

typedef uint16_t nsMediaNetworkState;
typedef uint16_t nsMediaReadyState;

namespace mozilla {
class MediaResource;
class MediaDecoder;
}

class nsITimer;
class nsRange;

namespace mozilla {
namespace dom {

class MediaError;

class HTMLMediaElement : public nsGenericHTMLElement,
                         public nsIObserver,
                         public MediaDecoderOwner,
                         public nsIAudioChannelAgentCallback
{
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::VideoFrameContainer VideoFrameContainer;
  typedef mozilla::MediaStream MediaStream;
  typedef mozilla::MediaResource MediaResource;
  typedef mozilla::MediaDecoderOwner MediaDecoderOwner;
  typedef mozilla::MetadataTags MetadataTags;

  CORSMode GetCORSMode() {
    return mCORSMode;
  }

  HTMLMediaElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLMediaElement();

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

  NS_DECL_NSIOBSERVER

  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLMediaElement,
                                           nsGenericHTMLElement)

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) MOZ_OVERRIDE;
  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttr,
                             bool aNotify) MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  virtual void DoneCreatingElement() MOZ_OVERRIDE;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable,
                               int32_t *aTabIndex) MOZ_OVERRIDE;
  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  /**
   * Call this to reevaluate whether we should start/stop due to our owner
   * document being active, inactive, visible or hidden.
   */
  virtual void NotifyOwnerDocumentActivityChanged();

  // Called by the video decoder object, on the main thread,
  // when it has read the metadata containing video dimensions,
  // etc.
  virtual void MetadataLoaded(int aChannels,
                              int aRate,
                              bool aHasAudio,
                              bool aHasVideo,
                              const MetadataTags* aTags) MOZ_FINAL MOZ_OVERRIDE;

  // Called by the video decoder object, on the main thread,
  // when it has read the first frame of the video
  // aResourceFullyLoaded should be true if the resource has been
  // fully loaded and the caller will call ResourceLoaded next.
  virtual void FirstFrameLoaded(bool aResourceFullyLoaded) MOZ_FINAL MOZ_OVERRIDE;

  // Called by the video decoder object, on the main thread,
  // when the resource has completed downloading.
  virtual void ResourceLoaded() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the video decoder object, on the main thread,
  // when the resource has a network error during loading.
  virtual void NetworkError() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the video decoder object, on the main thread, when the
  // resource has a decode error during metadata loading or decoding.
  virtual void DecodeError() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the video decoder object, on the main thread, when the
  // resource load has been cancelled.
  virtual void LoadAborted() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the video decoder object, on the main thread,
  // when the video playback has ended.
  virtual void PlaybackEnded() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the video decoder object, on the main thread,
  // when the resource has started seeking.
  virtual void SeekStarted() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the video decoder object, on the main thread,
  // when the resource has completed seeking.
  virtual void SeekCompleted() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the media stream, on the main thread, when the download
  // has been suspended by the cache or because the element itself
  // asked the decoder to suspend the download.
  virtual void DownloadSuspended() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the media stream, on the main thread, when the download
  // has been resumed by the cache or because the element itself
  // asked the decoder to resumed the download.
  // If aForceNetworkLoading is True, ignore the fact that the download has
  // previously finished. We are downloading the middle of the media after
  // having downloaded the end, we need to notify the element a download in
  // ongoing.
  virtual void DownloadResumed(bool aForceNetworkLoading = false) MOZ_FINAL MOZ_OVERRIDE;

  // Called by the media decoder to indicate that the download has stalled
  // (no data has arrived for a while).
  virtual void DownloadStalled() MOZ_FINAL MOZ_OVERRIDE;

  // Called by the media decoder to indicate whether the media cache has
  // suspended the channel.
  virtual void NotifySuspendedByCache(bool aIsSuspended) MOZ_FINAL MOZ_OVERRIDE;

  // Called when a "MozAudioAvailable" event listener is added. The media
  // element will then notify its decoder that it needs to make a copy of
  // the audio data sent to hardware and dispatch it in "mozaudioavailable"
  // events. This allows us to not perform the copy and thus reduce overhead
  // in the common case where we don't have a "MozAudioAvailable" listener.
  void NotifyAudioAvailableListener();

  // Called by the media decoder and the video frame to get the
  // ImageContainer containing the video data.
  virtual VideoFrameContainer* GetVideoFrameContainer() MOZ_FINAL MOZ_OVERRIDE;
  layers::ImageContainer* GetImageContainer();

  // Called by the video frame to get the print surface, if this is
  // a static document and we're not actually playing video
  gfxASurface* GetPrintSurface() { return mPrintSurface; }

  // Dispatch events
  using nsGenericHTMLElement::DispatchEvent;
  virtual nsresult DispatchEvent(const nsAString& aName) MOZ_FINAL MOZ_OVERRIDE;
  virtual nsresult DispatchAsyncEvent(const nsAString& aName) MOZ_FINAL MOZ_OVERRIDE;
  nsresult DispatchAudioAvailableEvent(float* aFrameBuffer,
                                       uint32_t aFrameBufferLength,
                                       float aTime);

  // Dispatch events that were raised while in the bfcache
  nsresult DispatchPendingMediaEvents();

  // Called by the decoder when some data has been downloaded or
  // buffering/seeking has ended. aNextFrameAvailable is true when
  // the data for the next frame is available. This method will
  // decide whether to set the ready state to HAVE_CURRENT_DATA,
  // HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA.
  virtual void UpdateReadyStateForData(MediaDecoderOwner::NextFrameStatus aNextFrame) MOZ_FINAL MOZ_OVERRIDE;

  // Use this method to change the mReadyState member, so required
  // events can be fired.
  void ChangeReadyState(nsMediaReadyState aState);

  // Return true if we can activate autoplay assuming enough data has arrived.
  bool CanActivateAutoplay();

  // Notify that state has changed that might cause an autoplay element to
  // start playing.
  // If the element is 'autoplay' and is ready to play back (not paused,
  // autoplay pref enabled, etc), it should start playing back.
  void CheckAutoplayDataReady();

  // Check if the media element had crossorigin set when loading started
  bool ShouldCheckAllowOrigin();

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

  // called to notify that the principal of the decoder's media resource has changed.
  virtual void NotifyDecoderPrincipalChanged() MOZ_FINAL MOZ_OVERRIDE;

  // Update the visual size of the media. Called from the decoder on the
  // main thread when/if the size changes.
  void UpdateMediaSize(nsIntSize size);

  // Returns the CanPlayStatus indicating if we can handle the
  // full MIME type including the optional codecs parameter.
  static CanPlayStatus GetCanPlay(const nsAString& aType);

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
   * Called when data has been written to the underlying audio stream.
   */
  virtual void NotifyAudioAvailable(float* aFrameBuffer, uint32_t aFrameBufferLength,
                                    float aTime) MOZ_FINAL MOZ_OVERRIDE;

  virtual bool IsNodeOfType(uint32_t aFlags) const MOZ_OVERRIDE;

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

  nsresult CopyInnerTo(Element* aDest);

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
   * Fires a timeupdate event. If aPeriodic is true, the event will only
   * be fired if we've not fired a timeupdate event (for any reason) in the
   * last 250ms, as required by the spec when the current time is periodically
   * increasing during playback.
   */
  virtual void FireTimeUpdate(bool aPeriodic) MOZ_FINAL MOZ_OVERRIDE;

  MediaStream* GetSrcMediaStream() const
  {
    NS_ASSERTION(mSrcStream, "Don't call this when not playing a stream");
    return mSrcStream->GetStream();
  }

  // WebIDL

  MediaError* GetError() const
  {
    return mError;
  }

  // XPCOM GetSrc() is OK
  void SetSrc(const nsAString& aSrc, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aRv);
  }

  // XPCOM GetCurrentSrc() is OK

  // XPCOM GetCrossorigin() is OK
  void SetCrossOrigin(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::crossorigin, aValue, aRv);
  }

  uint16_t NetworkState() const
  {
    return mNetworkState;
  }

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

  double Duration() const;

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

  void Play(ErrorResult& aRv);

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

  already_AddRefed<DOMMediaStream> GetMozSrcObject() const;

  void SetMozSrcObject(DOMMediaStream& aValue);

  bool MozPreservesPitch() const
  {
    return mPreservesPitch;
  }

  // XPCOM MozPreservesPitch() is OK

  bool MozAutoplayEnabled() const
  {
    return mAutoplayEnabled;
  }

  already_AddRefed<DOMMediaStream> MozCaptureStream(ErrorResult& aRv);

  already_AddRefed<DOMMediaStream> MozCaptureStreamUntilEnded(ErrorResult& aRv);

  bool MozAudioCaptured() const
  {
    return mAudioCaptured;
  }

  uint32_t GetMozChannels(ErrorResult& aRv) const;

  uint32_t GetMozSampleRate(ErrorResult& aRv) const;

  uint32_t GetMozFrameBufferLength(ErrorResult& aRv) const;

  void SetMozFrameBufferLength(uint32_t aValue, ErrorResult& aRv);

  JSObject* MozGetMetadata(JSContext* aCx, ErrorResult& aRv);

  double MozFragmentEnd();

  // XPCOM GetMozAudioChannelType() is OK

  void SetMozAudioChannelType(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::mozaudiochannel, aValue, aRv);
  }

  TextTrackList* TextTracks() const;

  already_AddRefed<TextTrack> AddTextTrack(TextTrackKind aKind,
                                           const nsAString& aLabel,
                                           const nsAString& aLanguage);

  void AddTextTrack(TextTrack* aTextTrack) {
    mTextTracks->AddTextTrack(aTextTrack);
  }

protected:
  class MediaLoadListener;
  class StreamListener;

  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;

  class WakeLockBoolWrapper {
  public:
    WakeLockBoolWrapper(bool val = false)
      : mValue(val), mCanPlay(true), mOuter(nullptr) {}

    ~WakeLockBoolWrapper();

    void SetOuter(HTMLMediaElement* outer) { mOuter = outer; }
    void SetCanPlay(bool aCanPlay);

    operator bool() const { return mValue; }

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

  /**
   * These two methods are called by the WakeLockBoolWrapper when the wakelock
   * has to be created or released.
   */
  virtual void WakeLockCreate();
  virtual void WakeLockRelease();
  nsCOMPtr<nsIDOMMozWakeLock> mWakeLock;

  /**
   * Logs a warning message to the web console to report various failures.
   * aMsg is the localized message identifier, aParams is the parameters to
   * be substituted into the localized message, and aParamCount is the number
   * of parameters in aParams.
   */
  void ReportLoadError(const char* aMsg,
                       const PRUnichar** aParams = nullptr,
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
   * Returns an nsDOMMediaStream containing the played contents of this
   * element. When aFinishWhenEnded is true, when this element ends playback
   * we will finish the stream and not play any more into it.
   * When aFinishWhenEnded is false, ending playback does not finish the stream.
   * The stream will never finish.
   */
  already_AddRefed<DOMMediaStream> CaptureStreamInternal(bool aFinishWhenEnded);

  /**
   * Initialize a decoder as a clone of an existing decoder in another
   * element.
   * mLoadingSrc must already be set.
   */
  nsresult InitializeDecoderAsClone(MediaDecoder* aOriginal);

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
  nsresult FinishDecoderSetup(MediaDecoder* aDecoder,
                              MediaResource* aStream,
                              nsIStreamListener **aListener,
                              MediaDecoder* aCloneDonor);

  /**
   * Call this after setting up mLoadingSrc and mDecoder.
   */
  void AddMediaElementToURITable();
  /**
   * Call this before clearing mLoadingSrc.
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
   * Called when all potential resources are exhausted. Changes network
   * state to NETWORK_NO_SOURCE, and sends error event with code
   * MEDIA_ERR_SRC_NOT_SUPPORTED.
   */
  void NoSupportedMediaSourceError();

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
   * Possible values of the 'preload' attribute.
   */
  enum PreloadAttrValue {
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
   * Dispatches an error event to a child source element.
   */
  void DispatchAsyncSourceError(nsIContent* aSourceElement);

  /**
   * Resets the media element for an error condition as per aErrorCode.
   * aErrorCode must be one of nsIDOMHTMLMediaError codes.
   */
  void Error(uint16_t aErrorCode);

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
   * current mMuted/mVolume state.
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
  virtual HTMLMediaElement* GetMediaElement() MOZ_FINAL MOZ_OVERRIDE
  {
    return this;
  }

  // Return true if decoding should be paused
  virtual bool GetPaused() MOZ_FINAL MOZ_OVERRIDE
  {
    bool isPaused = false;
    GetPaused(&isPaused);
    return isPaused;
  }

  // Check the permissions for audiochannel.
  bool CheckAudioChannelPermissions(const nsAString& aType);

  // This method does the check for muting/unmuting the audio channel.
  nsresult UpdateChannelMuteState(bool aCanPlay);

  // Update the audio channel playing state
  virtual void UpdateAudioChannelPlayingState();

  // The current decoder. Load() has been called on this decoder.
  // At most one of mDecoder and mSrcStream can be non-null.
  nsRefPtr<MediaDecoder> mDecoder;

  // A reference to the VideoFrameContainer which contains the current frame
  // of video to display.
  nsRefPtr<VideoFrameContainer> mVideoFrameContainer;

  // Holds a reference to the DOM wrapper for the MediaStream that has been
  // set in the src attribute.
  nsRefPtr<DOMMediaStream> mSrcAttrStream;

  // Holds a reference to the DOM wrapper for the MediaStream that we're
  // actually playing.
  // At most one of mDecoder and mSrcStream can be non-null.
  nsRefPtr<DOMMediaStream> mSrcStream;

  // Holds references to the DOM wrappers for the MediaStreams that we're
  // writing to.
  struct OutputMediaStream {
    nsRefPtr<DOMMediaStream> mStream;
    bool mFinishWhenEnded;
  };
  nsTArray<OutputMediaStream> mOutputStreams;

  // Holds a reference to the MediaStreamListener attached to mSrcStream.
  nsRefPtr<StreamListener> mSrcStreamListener;

  // Holds a reference to the first channel we open to the media resource.
  // Once the decoder is created, control over the channel passes to the
  // decoder, and we null out this reference. We must store this in case
  // we need to cancel the channel before control of it passes to the decoder.
  nsCOMPtr<nsIChannel> mChannel;

  // Error attribute
  nsRefPtr<MediaError> mError;

  // The current media load ID. This is incremented every time we start a
  // new load. Async events note the ID when they're first sent, and only fire
  // if the ID is unchanged when they come to fire.
  uint32_t mCurrentLoadID;

  // Points to the child source elements, used to iterate through the children
  // when selecting a resource to load.
  nsRefPtr<nsRange> mSourcePointer;

  // Points to the document whose load we're blocking. This is the document
  // we're bound to when loading starts.
  nsCOMPtr<nsIDocument> mLoadBlockedDoc;

  // Contains names of events that have been raised while in the bfcache.
  // These events get re-dispatched when the bfcache is exited.
  nsTArray<nsString> mPendingEvents;

  // Media loading flags. See:
  //   http://www.whatwg.org/specs/web-apps/current-work/#video)
  nsMediaNetworkState mNetworkState;
  nsMediaReadyState mReadyState;

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

  // Current number of audio channels.
  uint32_t mChannels;

  // Current audio sample rate.
  uint32_t mRate;

  // Helper function to iterate over a hash table
  // and convert it to a JSObject.
  static PLDHashOperator BuildObjectFromTags(nsCStringHashKey::KeyType aKey,
                                             nsCString aValue,
                                             void* aUserArg);
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

  // Size of the media. Updated by the decoder on the main thread if
  // it changes. Defaults to a width and height of -1 if not set.
  // We keep this separate from the intrinsic size stored in the
  // VideoFrameContainer so that it doesn't change unexpectedly under us
  // due to decoder activity.
  nsIntSize mMediaSize;

  // Time that the last timeupdate event was fired. Read/Write from the
  // main thread only.
  TimeStamp mTimeUpdateTime;

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

  nsRefPtr<gfxASurface> mPrintSurface;

  // Reference to the source element last returned by GetNextSource().
  // This is the child source element which we're trying to load from.
  nsCOMPtr<nsIContent> mSourceLoadCandidate;

  // An audio stream for writing audio directly from JS.
  nsAutoPtr<AudioStream> mAudioStream;

  // Range of time played.
  nsRefPtr<TimeRanges> mPlayed;

  // Stores the time at the start of the current 'played' range.
  double mCurrentPlayRangeStart;

  // True if MozAudioAvailable events can be safely dispatched, based on
  // a media and element same-origin check.
  bool mAllowAudioData;

  // If true then we have begun downloading the media content.
  // Set to false when completed, or not yet started.
  bool mBegun;

  // True when the decoder has loaded enough data to display the
  // first frame of the content.
  bool mLoadedFirstFrame;

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
    MUTED_BY_AUDIO_CHANNEL         = 0x04
  };

  uint32_t mMuted;

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

  // True if we've reported a "waiting" event since the last
  // readyState change to HAVE_CURRENT_DATA.
  bool mWaitingFired;

  // True if we're running the "load()" method.
  bool mIsRunningLoadMethod;

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

  // True if a same-origin check has been done for the media element and resource.
  bool mMediaSecurityVerified;

  // The CORS mode when loading the media element
  CORSMode mCORSMode;

  // True if the media has an audio track
  bool mHasAudio;

  // True if the media's channel's download has been suspended.
  bool mDownloadSuspendedByCache;

  // Audio Channel Type.
  AudioChannelType mAudioChannelType;

  // Is this media element playing?
  bool mPlayingThroughTheAudioChannel;

  // Has this element been in a document?
  bool mWasInDocument;

  // An agent used to join audio channel service.
  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;

  // List of our attached text track objects.
  nsRefPtr<TextTrackList> mTextTracks;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMediaElement_h
