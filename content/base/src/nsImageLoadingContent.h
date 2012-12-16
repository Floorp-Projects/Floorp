/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class which implements nsIImageLoadingContent and can be
 * subclassed by various content nodes that want to provide image
 * loading functionality (eg <img>, <object>, etc).
 */

#ifndef nsImageLoadingContent_h__
#define nsImageLoadingContent_h__

#include "imgINotificationObserver.h"
#include "imgIOnloadBlocker.h"
#include "mozilla/CORSMode.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h" // NS_CONTENT_DELETE_LIST_MEMBER
#include "nsEventStates.h"
#include "nsIImageLoadingContent.h"
#include "nsIRequest.h"

class nsIURI;
class nsIDocument;
class imgILoader;
class nsIIOService;

class nsImageLoadingContent : public nsIImageLoadingContent,
                              public imgIOnloadBlocker
{
  /* METHODS */
public:
  nsImageLoadingContent();
  virtual ~nsImageLoadingContent();

  NS_DECL_IMGINOTIFICATIONOBSERVER
  NS_DECL_NSIIMAGELOADINGCONTENT
  NS_DECL_IMGIONLOADBLOCKER

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
                     nsIDocument* aDocument = nullptr,
                     nsLoadFlags aLoadFlags = nsIRequest::LOAD_NORMAL);

  /**
   * helpers to get the document for this content (from the nodeinfo
   * and such).  Not named GetOwnerDoc/GetCurrentDoc to prevent ambiguous
   * method names in subclasses
   *
   * @return the document we belong to
   */
  nsIDocument* GetOurOwnerDoc();
  nsIDocument* GetOurCurrentDoc();

  /**
   * Helper function to get the frame associated with this content. Not named
   * GetPrimaryFrame to prevent ambiguous method names in subclasses.
   *
   * @return The frame which we belong to, or nullptr if it doesn't exist.
   */
  nsIFrame* GetOurPrimaryFrame();

  /**
   * Helper function to get the PresContext associated with this content's
   * frame. Not named GetPresContext to prevent ambiguous method names in
   * subclasses.
   *
   * @return The nsPresContext associated with our frame, or nullptr if either
   *         the frame doesn't exist, or the frame's prescontext doesn't exist.
   */
  nsPresContext* GetFramePresContext();

  /**
   * CancelImageRequests is called by subclasses when they want to
   * cancel all image requests (for example when the subclass is
   * somehow not an image anymore).
   */
  void CancelImageRequests(bool aNotify);

  /**
   * UseAsPrimaryRequest is called by subclasses when they have an existing
   * imgRequestProxy that they want this nsImageLoadingContent to use.  This may
   * effectively be called instead of LoadImage or LoadImageWithChannel.
   * If aNotify is true, this method will notify on state changes.
   */
  nsresult UseAsPrimaryRequest(imgRequestProxy* aRequest, bool aNotify);

  /**
   * Derived classes of nsImageLoadingContent MUST call
   * DestroyImageLoadingContent from their destructor, or earlier.  It
   * does things that cannot be done in ~nsImageLoadingContent because
   * they rely on being able to QueryInterface to other derived classes,
   * which cannot happen once the derived class destructor has started
   * calling the base class destructors.
   */
  void DestroyImageLoadingContent();

  void ClearBrokenState() { mBroken = false; }

  bool LoadingEnabled() { return mLoadingEnabled; }

  // Sets blocking state only if the desired state is different from the
  // current one. See the comment for mBlockingOnload for more information.
  void SetBlockingOnload(bool aBlocking);

  /**
   * Returns the CORS mode that will be used for all future image loads. The
   * default implementation returns CORS_NONE unconditionally.
   */
  virtual mozilla::CORSMode GetCORSMode();

  // Subclasses are *required* to call BindToTree/UnbindFromTree.
  void BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                  nsIContent* aBindingParent, bool aCompileEventHandlers);
  void UnbindFromTree(bool aDeep, bool aNullParent);

  nsresult OnStopRequest(imgIRequest* aRequest, nsresult aStatus);
  nsresult OnImageIsAnimated(imgIRequest *aRequest);

private:
  /**
   * Struct used to manage the image observers.
   */
  struct ImageObserver {
    ImageObserver(imgINotificationObserver* aObserver) :
      mObserver(aObserver),
      mNext(nullptr)
    {
      MOZ_COUNT_CTOR(ImageObserver);
    }
    ~ImageObserver()
    {
      MOZ_COUNT_DTOR(ImageObserver);
      NS_CONTENT_DELETE_LIST_MEMBER(ImageObserver, this, mNext);
    }

    nsCOMPtr<imgINotificationObserver> mObserver;
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
                           int16_t aNewImageStatus);

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
   nsRefPtr<imgRequestProxy>& PrepareNextRequest();

  /**
   * Called when we would normally call PrepareNextRequest(), but the request was
   * blocked.
   */
  void SetBlockedRequest(nsIURI* aURI, int16_t aContentDecision);

  /**
   * Returns a COMPtr reference to the current/pending image requests, cleaning
   * up and canceling anything that was there before. Note that if you just want
   * to get rid of one of the requests, you should call
   * Clear*Request(NS_BINDING_ABORTED) instead, since it passes a more appropriate
   * aReason than Prepare*Request() does (NS_ERROR_IMAGE_SRC_CHANGED).
   */
  nsRefPtr<imgRequestProxy>& PrepareCurrentRequest();
  nsRefPtr<imgRequestProxy>& PreparePendingRequest();

  /**
   * Switch our pending request to be our current request.
   * mPendingRequest must be non-null!
   */
  void MakePendingRequestCurrent();

  /**
   * Cancels and nulls-out the "current" and "pending" requests if they exist.
   */
  void ClearCurrentRequest(nsresult aReason);
  void ClearPendingRequest(nsresult aReason);

  /**
   * Retrieve a pointer to the 'registered with the refresh driver' flag for
   * which a particular image request corresponds.
   *
   * @returns A pointer to the boolean flag for a given image request, or
   *          |nullptr| if the request is not either |mPendingRequest| or
   *          |mCurrentRequest|.
   */
  bool* GetRegisteredFlagForRequest(imgIRequest* aRequest);

  /**
   * Reset animation of the current request if |mNewRequestsWillNeedAnimationReset|
   * was true when the request was prepared.
   */
  void ResetAnimationIfNeeded();

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
  nsRefPtr<imgRequestProxy> mCurrentRequest;
  nsRefPtr<imgRequestProxy> mPendingRequest;
  uint32_t mCurrentRequestFlags;
  uint32_t mPendingRequestFlags;

  enum {
    // Set if the request needs 
    REQUEST_NEEDS_ANIMATION_RESET = 0x00000001U,
    // Set if the request should be tracked.  This is true if the request is
    // not tracked iff this node is not in the document.
    REQUEST_SHOULD_BE_TRACKED = 0x00000002U,
    // Set if the request is blocking onload.
    REQUEST_BLOCKS_ONLOAD = 0x00000004U
  };

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

  int16_t mImageBlockingStatus;
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
  /* The number of nested AutoStateChangers currently tracking our state. */
  uint8_t mStateChangerDepth;

  // Flags to indicate whether each of the current and pending requests are
  // registered with the refresh driver.
  bool mCurrentRequestRegistered;
  bool mPendingRequestRegistered;
};

#endif // nsImageLoadingContent_h__
