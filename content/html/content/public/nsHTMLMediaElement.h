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

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);

  virtual PRBool IsDoneAddingChildren();
  virtual nsresult DoneAddingChildren(PRBool aHaveNotified);
  virtual void DestroyContent();

  // Called by the video decoder object, on the main thread,
  // when it has read the metadata containing video dimensions,
  // etc.
  void MetadataLoaded();

  // Called by the video decoder object, on the main thread,
  // when it has read the first frame of the video
  void FirstFrameLoaded();

  // Called by the video decoder object, on the main thread,
  // when the resource has completed downloading.
  void ResourceLoaded();

  // Called by the video decoder object, on the main thread,
  // when the resource has a network error during loading.
  void NetworkError();

  // Called by the video decoder object, on the main thread,
  // when the video playback has ended.
  void PlaybackEnded();

  // Called by the video decoder object, on the main thread,
  // when the resource has started seeking.
  void SeekStarted();

  // Called by the video decoder object, on the main thread,
  // when the resource has completed seeking.
  void SeekCompleted();

  // Draw the latest video data. See nsMediaDecoder for 
  // details.
  void Paint(gfxContext* aContext, const gfxRect& aRect);

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
  void UpdateReadyStateForData(PRBool aNextFrameAvailable);

  // Use this method to change the mReadyState member, so required
  // events can be fired.
  void ChangeReadyState(nsMediaReadyState aState);

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
  nsIPrincipal* GetCurrentPrincipal();

  // Update the visual size of the media. Called from the decoder on the
  // main thread when/if the size changes.
  void UpdateMediaSize(nsIntSize size);

  // Handle moving into and out of the bfcache by pausing and playing
  // as needed.
  void Freeze();
  void Thaw();

  // Returns true if we can handle this MIME type in a <video> or <audio>
  // element.
  // If it returns true, then it also returns a null-terminated list
  // of supported codecs in *aSupportedCodecs, and a null-terminated list
  // of codecs that *may* be supported in *aMaybeSupportedCodecs. These
  // lists should not be freed, they area static data.
  static PRBool CanHandleMediaType(const char* aMIMEType,
                                   const char*** aSupportedCodecs,
                                   const char*** aMaybeSupportedCodecs);

  /**
   * Initialize data for available media types
   */
  static void InitMediaTypes();
  /**
   * Shutdown data for available media types
   */
  static void ShutdownMediaTypes();

protected:
  class nsMediaLoadListener;

  /**
   * Figure out which resource to load (either the 'src' attribute or a
   * <source> child) and return the associated URI.
   */
  nsresult PickMediaElement(nsIURI** aURI);
  /**
   * Create a decoder for the given aMIMEType. Returns false if we
   * were unable to create the decoder.
   */
  PRBool CreateDecoder(const nsACString& aMIMEType);
  /**
   * Initialize a decoder to load the given channel. The decoder's stream
   * listener is returned via aListener.
   */
  nsresult InitializeDecoderForChannel(nsIChannel *aChannel,
                                       nsIStreamListener **aListener);
  /**
   * Execute the initial steps of the load algorithm that ensure existing
   * loads are aborted and the element is emptied.  Returns true if aborted,
   * false if emptied.
   */
  PRBool AbortExistingLoads();
  /**
   * Create a URI for the given aURISpec string.
   */
  nsresult NewURIFromString(const nsAutoString& aURISpec, nsIURI** aURI);

  /**
   * Does step 12 of the media load() algorithm, sends error/emptied events to
   * to the media element, and reset network/begun state.
   */
  void NoSupportedMediaError();

  nsRefPtr<nsMediaDecoder> mDecoder;

  nsCOMPtr<nsIChannel> mChannel;

  // Error attribute
  nsCOMPtr<nsIDOMHTMLMediaError> mError;

  // Media loading flags. See: 
  //   http://www.whatwg.org/specs/web-apps/current-work/#video)
  nsMediaNetworkState mNetworkState;
  nsMediaReadyState mReadyState;

  // Value of the volume before it was muted
  float mMutedVolume;

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

  // PR_TRUE if the video was paused before Freeze was called. This is checked
  // to ensure that the playstate doesn't change when the user goes Forward/Back
  // from the bfcache.
  PRPackedBool mPausedBeforeFreeze;

  // True if playback was requested before a decoder was available to begin
  // playback with.
  PRPackedBool mPlayRequested;
};
