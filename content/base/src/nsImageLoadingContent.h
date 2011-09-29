/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * A base class which implements nsIImageLoadingContent and can be
 * subclassed by various content nodes that want to provide image
 * loading functionality (eg <img>, <object>, etc).
 */

#ifndef nsImageLoadingContent_h__
#define nsImageLoadingContent_h__

#include "nsIImageLoadingContent.h"
#include "nsINode.h"
#include "imgIRequest.h"
#include "prtypes.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h" // NS_CONTENT_DELETE_LIST_MEMBER
#include "nsString.h"
#include "nsEventStates.h"

class nsIURI;
class nsIDocument;
class imgILoader;
class nsIIOService;

class nsImageLoadingContent : public nsIImageLoadingContent
{
  /* METHODS */
public:
  nsImageLoadingContent();
  virtual ~nsImageLoadingContent();

  NS_DECL_IMGICONTAINEROBSERVER
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_NSIIMAGELOADINGCONTENT

  enum CORSMode {
    /**
     * The default of not using CORS to validate cross-origin loads.
     */
    CORS_NONE,

    /**
     * Validate cross-site loads using CORS, but do not send any credentials
     * (cookies, HTTP auth logins, etc) along with the request.
     */
    CORS_ANONYMOUS,

    /**
     * Validate cross-site loads using CORS, and send credentials such as cookies
     * and HTTP auth logins along with the request.
     */
    CORS_USE_CREDENTIALS
  };

protected:
  /**
   * LoadImage is called by subclasses when the appropriate
   * attributes (eg 'src' for <img> tags) change.  The string passed
   * in is the new uri string; this consolidates the code for getting
   * the charset, constructing URI objects, and any other incidentals
   * into this superclass.   
   *
   * @param aNewURI the URI spec to be loaded (may be a relative URI)
   * @param aForce If true, make sure to load the URI.  If false, only
   *        load if the URI is different from the currently loaded URI.
   * @param aNotify If true, nsIDocumentObserver state change notifications
   *                will be sent as needed.
   */
  nsresult LoadImage(const nsAString& aNewURI, bool aForce,
                     bool aNotify);

  /**
   * ImageState is called by subclasses that are computing their content state.
   * The return value will have the NS_EVENT_STATE_BROKEN,
   * NS_EVENT_STATE_USERDISABLED, and NS_EVENT_STATE_SUPPRESSED bits set as
   * needed.  Note that this state assumes that this node is "trying" to be an
   * image (so for example complete lack of attempt to load an image will lead
   * to NS_EVENT_STATE_BROKEN being set).  Subclasses that are not "trying" to
   * be an image (eg an HTML <input> of type other than "image") should just
   * not call this method when computing their intrinsic state.
   */
  nsEventStates ImageState() const;

  /**
   * LoadImage is called by subclasses when the appropriate
   * attributes (eg 'src' for <img> tags) change. If callers have an
   * URI object already available, they should use this method.
   *
   * @param aNewURI the URI to be loaded
   * @param aForce If true, make sure to load the URI.  If false, only
   *        load if the URI is different from the currently loaded URI.
   * @param aNotify If true, nsIDocumentObserver state change notifications
   *                will be sent as needed.
   * @param aDocument Optional parameter giving the document this node is in.
   *        This is purely a performance optimization.
   * @param aLoadFlags Optional parameter specifying load flags to use for
   *        the image load
   */
  nsresult LoadImage(nsIURI* aNewURI, bool aForce, bool aNotify,
                     nsIDocument* aDocument = nsnull,
                     nsLoadFlags aLoadFlags = nsIRequest::LOAD_NORMAL);

  /**
   * helper to get the document for this content (from the nodeinfo
   * and such).  Not named GetDocument to prevent ambiguous method
   * names in subclasses
   *
   * @return the document we belong to
   */
  nsIDocument* GetOurDocument();

  /**
   * CancelImageRequests is called by subclasses when they want to
   * cancel all image requests (for example when the subclass is
   * somehow not an image anymore).
   */
  void CancelImageRequests(bool aNotify);

  /**
   * UseAsPrimaryRequest is called by subclasses when they have an existing
   * imgIRequest that they want this nsImageLoadingContent to use.  This may
   * effectively be called instead of LoadImage or LoadImageWithChannel.
   * If aNotify is true, this method will notify on state changes.
   */
  nsresult UseAsPrimaryRequest(imgIRequest* aRequest, bool aNotify);

  /**
   * Derived classes of nsImageLoadingContent MUST call
   * DestroyImageLoadingContent from their destructor, or earlier.  It
   * does things that cannot be done in ~nsImageLoadingContent because
   * they rely on being able to QueryInterface to other derived classes,
   * which cannot happen once the derived class destructor has started
   * calling the base class destructors.
   */
  void DestroyImageLoadingContent();

  void ClearBrokenState() { mBroken = PR_FALSE; }

  bool LoadingEnabled() { return mLoadingEnabled; }

  // Sets blocking state only if the desired state is different from the
  // current one. See the comment for mBlockingOnload for more information.
  void SetBlockingOnload(bool aBlocking);

  /**
   * Returns the CORS mode that will be used for all future image loads. The
   * default implementation returns CORS_NONE unconditionally.
   */
  virtual CORSMode GetCORSMode();

private:
  /**
   * Struct used to manage the image observers.
   */
  struct ImageObserver {
    ImageObserver(imgIDecoderObserver* aObserver) :
      mObserver(aObserver),
      mNext(nsnull)
    {
      MOZ_COUNT_CTOR(ImageObserver);
    }
    ~ImageObserver()
    {
      MOZ_COUNT_DTOR(ImageObserver);
      NS_CONTENT_DELETE_LIST_MEMBER(ImageObserver, this, mNext);
    }

    nsCOMPtr<imgIDecoderObserver> mObserver;
    ImageObserver* mNext;
  };

  /**
   * Struct to report state changes
   */
  struct AutoStateChanger {
    AutoStateChanger(nsImageLoadingContent* aImageContent,
                     bool aNotify) :
      mImageContent(aImageContent),
      mNotify(aNotify)
    {
      mImageContent->mStateChangerDepth++;
    }
    ~AutoStateChanger()
    {
      mImageContent->mStateChangerDepth--;
      mImageContent->UpdateImageState(mNotify);
    }

    nsImageLoadingContent* mImageContent;
    bool mNotify;
  };

  friend struct AutoStateChanger;

  /**
   * UpdateImageState recomputes the current state of this image loading
   * content and updates what ImageState() returns accordingly.  It will also
   * fire a ContentStatesChanged() notification as needed if aNotify is true.
   */
  void UpdateImageState(bool aNotify);

  /**
   * CancelImageRequests can be called when we want to cancel the
   * image requests, generally due to our src changing and us wanting
   * to start a new load.  The "current" request will be canceled only
   * if it has not progressed far enough to know the image size yet
   * unless aEvenIfSizeAvailable is true.
   *
   * @param aReason the reason the requests are being canceled
   * @param aEvenIfSizeAvailable cancels the current load even if its size is
   *                             available
   * @param aNewImageStatus the nsIContentPolicy status of the new image load
   */
  void CancelImageRequests(nsresult aReason, bool aEvenIfSizeAvailable,
                           PRInt16 aNewImageStatus);

  /**
   * Method to fire an event once we know what's going on with the image load.
   *
   * @param aEventType "load" or "error" depending on how things went
   */
  nsresult FireEvent(const nsAString& aEventType);
protected:
  /**
   * Method to create an nsIURI object from the given string (will
   * handle getting the right charset, base, etc).  You MUST pass in a
   * non-null document to this function.
   *
   * @param aSpec the string spec (from an HTML attribute, eg)
   * @param aDocument the document we belong to
   * @return the URI we want to be loading
   */
  nsresult StringToURI(const nsAString& aSpec, nsIDocument* aDocument,
                       nsIURI** aURI);

  void CreateStaticImageClone(nsImageLoadingContent* aDest) const;

  /**
   * Prepare and returns a reference to the "next request". If there's already
   * a _usable_ current request (one with SIZE_AVAILABLE), this request is
   * "pending" until it becomes usable. Otherwise, this becomes the current
   * request.
   */
   nsCOMPtr<imgIRequest>& PrepareNextRequest();

  /**
   * Called when we would normally call PrepareNextRequest(), but the request was
   * blocked.
   */
  void SetBlockedRequest(nsIURI* aURI, PRInt16 aContentDecision);

  /**
   * Returns a COMPtr reference to the current/pending image requests, cleaning
   * up and canceling anything that was there before. Note that if you just want
   * to get rid of one of the requests, you should call
   * Clear*Request(NS_BINDING_ABORTED) instead, since it passes a more appropriate
   * aReason than Prepare*Request() does (NS_ERROR_IMAGE_SRC_CHANGED).
   */
  nsCOMPtr<imgIRequest>& PrepareCurrentRequest();
  nsCOMPtr<imgIRequest>& PreparePendingRequest();

  /**
   * Cancels and nulls-out the "current" and "pending" requests if they exist.
   */
  void ClearCurrentRequest(nsresult aReason);
  void ClearPendingRequest(nsresult aReason);

  /**
   * Static helper method to tell us if we have the size of a request. The
   * image may be null.
   */
  static bool HaveSize(imgIRequest *aImage);

  /**
   * Adds/Removes a given imgIRequest from our document's tracker.
   *
   * No-op if aImage is null.
   */
  nsresult TrackImage(imgIRequest* aImage);
  nsresult UntrackImage(imgIRequest* aImage);

  /* MEMBERS */
  nsCOMPtr<imgIRequest> mCurrentRequest;
  nsCOMPtr<imgIRequest> mPendingRequest;

  // If the image was blocked or if there was an error loading, it's nice to
  // still keep track of what the URI was despite not having an imgIRequest.
  // We only maintain this in those situations (in the common case, this is
  // always null).
  nsCOMPtr<nsIURI>      mCurrentURI;

private:
  /**
   * Typically we will have only one observer (our frame in the screen
   * prescontext), so we want to only make space for one and to
   * heap-allocate anything past that (saves memory and malloc churn
   * in the common case).  The storage is a linked list, we just
   * happen to actually hold the first observer instead of a pointer
   * to it.
   */
  ImageObserver mObserverList;

  /**
   * When mIsImageStateForced is true, this holds the ImageState that we'll
   * return in ImageState().
   */
  nsEventStates mForcedImageState;

  PRInt16 mImageBlockingStatus;
  bool mLoadingEnabled : 1;

  /**
   * When true, we return mForcedImageState from ImageState().
   */
  bool mIsImageStateForced : 1;

  /**
   * The state we had the last time we checked whether we needed to notify the
   * document of a state change.  These are maintained by UpdateImageState.
   */
  bool mLoading : 1;
  bool mBroken : 1;
  bool mUserDisabled : 1;
  bool mSuppressed : 1;

  /**
   * Whether we're currently blocking document load.
   */
  bool mBlockingOnload : 1;

protected:
  /**
   * A hack to get animations to reset, see bug 594771. On requests
   * that originate from setting .src, we mark them for needing their animation
   * reset when they are ready. mNewRequestsWillNeedAnimationReset is set to
   * true while preparing such requests (as a hack around needing to change an
   * interface), and the other two booleans store which of the current
   * and pending requests are of the sort that need their animation restarted.
   */
  bool mNewRequestsWillNeedAnimationReset : 1;

private:
  bool mPendingRequestNeedsResetAnimation : 1;
  bool mCurrentRequestNeedsResetAnimation : 1;

  /* The number of nested AutoStateChangers currently tracking our state. */
  PRUint8 mStateChangerDepth;
};

#endif // nsImageLoadingContent_h__
