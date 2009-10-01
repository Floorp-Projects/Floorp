/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLMediaElement.h"
#include "nsGenericHTMLElement.h"
#include "nsMediaDecoder.h"
#include "nsIChannel.h"
#include "nsThreadUtils.h"
#include "nsIDOMRange.h"
#include "nsCycleCollectionParticipant.h"
#include "nsILoadGroup.h"

// Define to output information on decoding and painting framerate
/* #define DEBUG_FRAME_RATE 1 */

typedef PRUint16 nsMediaNetworkState;
typedef PRUint16 nsMediaReadyState;

class nsHTMLMediaElement : public nsGenericHTMLElement
{
public:
  nsHTMLMediaElement(nsINodeInfo *aNodeInfo, PRBool aFromParser = PR_FALSE);
  virtual ~nsHTMLMediaElement();

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
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLMediaElement,
                                           nsGenericHTMLElement)

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr, 
                             PRBool aNotify);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);

  virtual PRBool IsDoneAddingChildren();
  virtual nsresult DoneAddingChildren(PRBool aHaveNotified);

  /**
   * Call this to reevaluate whether we should start/stop due to our owner
   * document being active or inactive.
   */
  void NotifyOwnerDocumentActivityChanged();

  // Called by the video decoder object, on the main thread,
  // when it has read the metadata containing video dimensions,
  // etc.
  void MetadataLoaded();

  // Called by the video decoder object, on the main thread,
  // when it has read the first frame of the video
  // aResourceFullyLoaded should be true if the resource has been
  // fully loaded and the caller will call ResourceLoaded next.
  void FirstFrameLoaded(PRBool aResourceFullyLoaded);

  // Called by the video decoder object, on the main thread,
  // when the resource has completed downloading.
  void ResourceLoaded();

  // Called by the video decoder object, on the main thread,
  // when the resource has a network error during loading.
  void NetworkError();

  // Called by the video decoder object, on the main thread, when the
  // resource has a decode error during metadata loading or decoding.
  void DecodeError();

  // Called by the video decoder object, on the main thread,
  // when the video playback has ended.
  void PlaybackEnded();

  // Called by the video decoder object, on the main thread,
  // when the resource has started seeking.
  void SeekStarted();

  // Called by the video decoder object, on the main thread,
  // when the resource has completed seeking.
  void SeekCompleted();

  // Called by the media stream, on the main thread, when the download
  // has been suspended by the cache or because the element itself
  // asked the decoder to suspend the download.
  void DownloadSuspended();

  // Called by the media stream, on the main thread, when the download
  // has been resumed by the cache or because the element itself
  // asked the decoder to resumed the download.
  void DownloadResumed();

  // Called by the media decoder to indicate that the download has stalled
  // (no data has arrived for a while).
  void DownloadStalled();

  // Draw the latest video data. See nsMediaDecoder for 
  // details.
  void Paint(gfxContext* aContext,
             gfxPattern::GraphicsFilter aFilter,
             const gfxRect& aRect);

  // Dispatch events
  nsresult DispatchSimpleEvent(const nsAString& aName);
  nsresult DispatchProgressEvent(const nsAString& aName);
  nsresult DispatchAsyncSimpleEvent(const nsAString& aName);
  nsresult DispatchAsyncProgressEvent(const nsAString& aName);

  // Called by the decoder when some data has been downloaded or
  // buffering/seeking has ended. aNextFrameAvailable is true when
  // the data for the next frame is available. This method will
  // decide whether to set the ready state to HAVE_CURRENT_DATA,
  // HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA.
  enum NextFrameStatus {
    // The next frame of audio/video is available
    NEXT_FRAME_AVAILABLE,
    // The next frame of audio/video is unavailable because the decoder
    // is paused while it buffers up data
    NEXT_FRAME_UNAVAILABLE_BUFFERING,
    // The next frame of audio/video is unavailable for some other reasons
    NEXT_FRAME_UNAVAILABLE
  };
  void UpdateReadyStateForData(NextFrameStatus aNextFrame);

  // Use this method to change the mReadyState member, so required
  // events can be fired.
  void ChangeReadyState(nsMediaReadyState aState);

  // Return true if we can activate autoplay assuming enough data has arrived.
  PRBool CanActivateAutoplay();

  // Notify that enough data has arrived to start autoplaying.
  // If the element is 'autoplay' and is ready to play back (not paused,
  // autoplay pref enabled, etc), it should start playing back.
  void NotifyAutoplayDataReady();

  // Gets the pref media.enforce_same_site_origin, which determines
  // if we should check Access Controls, or allow cross domain loads.
  PRBool ShouldCheckAllowOrigin();

  // Is the media element potentially playing as defined by the HTML 5 specification.
  // http://www.whatwg.org/specs/web-apps/current-work/#potentially-playing
  PRBool IsPotentiallyPlaying() const;

  // Has playback ended as defined by the HTML 5 specification.
  // http://www.whatwg.org/specs/web-apps/current-work/#ended
  PRBool IsPlaybackEnded() const;

  // principal of the currently playing stream
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal();

  // Update the visual size of the media. Called from the decoder on the
  // main thread when/if the size changes.
  void UpdateMediaSize(nsIntSize size);

  // Returns true if we can handle this MIME type.
  // If it returns true, then it also returns a null-terminated list
  // of supported codecs in *aSupportedCodecs. This
  // list should not be freed, it is static data.
  static PRBool CanHandleMediaType(const char* aMIMEType,
                                   const char*** aSupportedCodecs);

  // Returns true if we should handle this MIME type when it appears
  // as an <object> or as a toplevel page. If, in practice, our support
  // for the type is more limited than appears in the wild, we should return
  // false here even if CanHandleMediaType would return true.
  static PRBool ShouldHandleMediaType(const char* aMIMEType);

  /**
   * Initialize data for available media types
   */
  static void InitMediaTypes();
  /**
   * Shutdown data for available media types
   */
  static void ShutdownMediaTypes();

  /**
   * Called when a child source element is added to this media element. This
   * may queue a load() task if appropriate.
   */
  void NotifyAddedSource();

  /**
   * Called when there's been an error fetching the resource. This decides
   * whether it's appropriate to fire an error event.
   */
  void NotifyLoadError();

  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;

  /**
   * Returns the current load ID. Asynchronous events store the ID that was
   * current when they were enqueued, and if it has changed when they come to
   * fire, they consider themselves cancelled, and don't fire.
   */
  PRUint32 GetCurrentLoadID() { return mCurrentLoadID; }

  /**
   * Returns the load group for this media element's owner document.
   * XXX XBL2 issue.
   */
  already_AddRefed<nsILoadGroup> GetDocumentLoadGroup();

  /** 
   * Returns PR_TRUE if the media has played or completed a seek.
   * Used by video frame to determine whether to paint the poster.
   */
  PRBool GetPlayedOrSeeked() { return mHasPlayedOrSeeked; }

protected:
  class MediaLoadListener;
  class LoadNextSourceEvent;
  class SelectResourceEvent;

  /**
   * Changes mHasPlayedOrSeeked to aValue. If mHasPlayedOrSeeked changes
   * we'll force a reflow so that the video frame gets reflowed to reflect
   * the poster hiding or showing immediately.
   */
  void SetPlayedOrSeeked(PRBool aValue);

  /**
   * Create a decoder for the given aMIMEType. Returns null if we
   * were unable to create the decoder.
   */
  already_AddRefed<nsMediaDecoder> CreateDecoder(const nsACString& aMIMEType);

  /**
   * Initialize a decoder as a clone of an existing decoder in another
   * element.
   */
  nsresult InitializeDecoderAsClone(nsMediaDecoder* aOriginal); 

  /**
   * Initialize a decoder to load the given channel. The decoder's stream
   * listener is returned via aListener.
   */
  nsresult InitializeDecoderForChannel(nsIChannel *aChannel,
                                       nsIStreamListener **aListener);

  /**
   * Finish setting up the decoder after Load() has been called on it.
   */
  nsresult FinishDecoderSetup(nsMediaDecoder* aDecoder);

  /**
   * Execute the initial steps of the load algorithm that ensure existing
   * loads are aborted, the element is emptied, and a new load ID is
   * created.
   */
  void AbortExistingLoads();

  /**
   * Create a URI for the given aURISpec string.
   */
  nsresult NewURIFromString(const nsAutoString& aURISpec, nsIURI** aURI);

  /**
   * Called when all potential resources are exhausted. Changes network
   * state to NETWORK_NO_SOURCE, and sends error event with code
   * MEDIA_ERR_SRC_NOT_SUPPORTED.
   */
  void NoSupportedMediaSourceError();

  /**
   * Attempts to load resources from the <source> children. This is a
   * substep of the media selection algorith. Do not call this directly,
   * call QueueLoadFromSourceTask() instead.
   */
  void LoadFromSourceChildren();

  /**
   * Sends an async event to call LoadFromSourceChildren().
   */
  void QueueLoadFromSourceTask();
 
  /**
   * Media selection algorithm.
   */
  void SelectResource();

  /**
   * Sends an async event to call SelectResource().
   */
  void QueueSelectResourceTask();

  /**
   * The resource-fetch algorithm step of the load algorithm.
   */
  nsresult LoadResource(nsIURI* aURI);

  /**
   * Selects the next <source> child from which to load a resource. Called
   * during the media selection algorithm.
   */
  already_AddRefed<nsIURI> GetNextSource();

  /**
   * Changes mDelayingLoadEvent, and will call BlockOnLoad()/UnblockOnLoad()
   * on the owning document, so it can delay the load event firing.
   */
  void ChangeDelayLoadStatus(PRBool aDelay);

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
                             PRUint32 aFlags);

  /**
   * Call this to reevaluate whether we should be holding a self-reference.
   */
  void AddRemoveSelfReference();

  /**
   * Alias for Release(), but using stdcall calling convention so on
   * platforms where Release has a strange calling convention (Windows)
   * we can still get a method pointer to this method.
   */
  void DoRelease() { Release(); }

  nsRefPtr<nsMediaDecoder> mDecoder;

  // Holds a reference to the first channel we open to the media resource.
  // Once the decoder is created, control over the channel passes to the
  // decoder, and we null out this reference. We must store this in case
  // we need to cancel the channel before control of it passes to the decoder.
  nsCOMPtr<nsIChannel> mChannel;

  // Error attribute
  nsCOMPtr<nsIDOMHTMLMediaError> mError;

  // The current media load ID. This is incremented every time we start a
  // new load. Async events note the ID when they're first sent, and only fire
  // if the ID is unchanged when they come to fire.
  PRUint32 mCurrentLoadID;

  // Points to the child source elements, used to iterate through the children
  // when selecting a resource to load.
  nsCOMPtr<nsIDOMRange> mSourcePointer;

  // Points to the document whose load we're blocking. This is the document
  // we're bound to when loading starts.
  nsCOMPtr<nsIDocument> mLoadBlockedDoc;

  // Media loading flags. See: 
  //   http://www.whatwg.org/specs/web-apps/current-work/#video)
  nsMediaNetworkState mNetworkState;
  nsMediaReadyState mReadyState;

  enum LoadAlgorithmState {
    // Not waiting for any src/<source>.
    NOT_WAITING,
    // No src or <source> children, load is waiting at load algorithm step 1.
    WAITING_FOR_SRC_OR_SOURCE, 
    // No src at load time, and all <source> children don't resolve or
    // give network errors during fetch, waiting for more <source> children
    // to be added.
    WAITING_FOR_SOURCE 
  };
  
  // When the load algorithm is waiting for more src/<source>, this denotes
  // what type of waiting we're doing.
  LoadAlgorithmState mLoadWaitStatus;

  // Current audio volume
  float mVolume;

  // Size of the media. Updated by the decoder on the main thread if
  // it changes. Defaults to a width and height of -1 if not set.
  nsIntSize mMediaSize;

  // If true then we have begun downloading the media content.
  // Set to false when completed, or not yet started.
  PRPackedBool mBegun;

  // True when the decoder has loaded enough data to display the
  // first frame of the content.
  PRPackedBool mLoadedFirstFrame;

  // Indicates whether current playback is a result of user action
  // (ie. calling of the Play method), or automatic playback due to
  // the 'autoplay' attribute being set. A true value indicates the 
  // latter case.
  // The 'autoplay' HTML attribute indicates that the video should
  // start playing when loaded. The 'autoplay' attribute of the object
  // is a mirror of the HTML attribute. These are different from this
  // 'mAutoplaying' flag, which indicates whether the current playback
  // is a result of the autoplay attribute.
  PRPackedBool mAutoplaying;

  // Indicates whether |autoplay| will actually autoplay based on the pref 
  // media.autoplay.enabled
  PRPackedBool mAutoplayEnabled;

  // Playback of the video is paused either due to calling the
  // 'Pause' method, or playback not yet having started.
  PRPackedBool mPaused;

  // True if the sound is muted
  PRPackedBool mMuted;

  // Flag to indicate if the child elements (eg. <source/>) have been
  // parsed.
  PRPackedBool mIsDoneAddingChildren;

  // If TRUE then the media element was actively playing before the currently
  // in progress seeking. If FALSE then the media element is either not seeking
  // or was not actively playing before the current seek. Used to decide whether
  // to raise the 'waiting' event as per 4.7.1.8 in HTML 5 specification.
  PRPackedBool mPlayingBeforeSeek;

  // PR_TRUE iff this element is paused because the document is inactive
  PRPackedBool mPausedForInactiveDocument;
  
  // PR_TRUE if we've reported a "waiting" event since the last
  // readyState change to HAVE_CURRENT_DATA.
  PRPackedBool mWaitingFired;

  // PR_TRUE if we're in BindToTree().
  PRPackedBool mIsBindingToTree;

  // PR_TRUE if we're running the "load()" method.
  PRPackedBool mIsRunningLoadMethod;

  // PR_TRUE if we're loading exclusively from the src attribute's resource.
  PRPackedBool mIsLoadingFromSrcAttribute;

  // PR_TRUE if we're delaying the "load" event. They are delayed until either
  // an error occurs, or the first frame is loaded.
  PRPackedBool mDelayingLoadEvent;

  // PR_TRUE when we've got a task queued to call SelectResource(),
  // or while we're running SelectResource().
  PRPackedBool mIsRunningSelectResource;

  // PR_TRUE if we suspended the decoder because we were paused,
  // autobuffer and autoplay were not set, and we loaded the first frame.
  PRPackedBool mSuspendedAfterFirstFrame;

  // PR_TRUE if we are allowed to suspend the decoder because we were paused,
  // autobuffer and autoplay were not set, and we loaded the first frame.
  PRPackedBool mAllowSuspendAfterFirstFrame;

  // PR_TRUE if we've played or completed a seek. We use this to determine
  // when the poster frame should be shown.
  PRPackedBool mHasPlayedOrSeeked;

  // PR_TRUE if we've added a reference to ourselves to keep the element
  // alive while no-one is referencing it but the element may still fire
  // events of its own accord.
  PRPackedBool mHasSelfReference;
};
