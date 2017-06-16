/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/EventStates.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsIImageLoadingContent.h"
#include "nsIRequest.h"
#include "mozilla/ErrorResult.h"
#include "nsIContentPolicy.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/net/ReferrerPolicy.h"

class nsIURI;
class nsIDocument;
class nsPresContext;
class nsIContent;
class imgRequestProxy;

namespace mozilla {
class AsyncEventDispatcher;
} // namespace mozilla

#ifdef LoadImage
// Undefine LoadImage to prevent naming conflict with Windows.
#undef LoadImage
#endif

class nsImageLoadingContent : public nsIImageLoadingContent,
                              public imgIOnloadBlocker
{
  template <typename T> using Maybe = mozilla::Maybe<T>;
  using Nothing = mozilla::Nothing;
  using OnNonvisible = mozilla::OnNonvisible;
  using Visibility = mozilla::Visibility;

  /* METHODS */
public:
  nsImageLoadingContent();
  virtual ~nsImageLoadingContent();

  NS_DECL_IMGINOTIFICATIONOBSERVER
  NS_DECL_NSIIMAGELOADINGCONTENT
  NS_DECL_IMGIONLOADBLOCKER

  // Web IDL binding methods.
  // Note that the XPCOM SetLoadingEnabled, AddObserver, RemoveObserver,
  // ForceImageState methods are OK for Web IDL bindings to use as well,
  // since none of them throw when called via the Web IDL bindings.

  bool LoadingEnabled() const { return mLoadingEnabled; }
  int16_t ImageBlockingStatus() const
  {
    return mImageBlockingStatus;
  }
  already_AddRefed<imgIRequest>
    GetRequest(int32_t aRequestType, mozilla::ErrorResult& aError);
  int32_t
    GetRequestType(imgIRequest* aRequest, mozilla::ErrorResult& aError);
  already_AddRefed<nsIURI> GetCurrentURI(mozilla::ErrorResult& aError);
  void ForceReload(const mozilla::dom::Optional<bool>& aNotify,
                   mozilla::ErrorResult& aError);

  // XPCOM [optional] syntax helper
  nsresult ForceReload(bool aNotify = true) {
    return ForceReload(aNotify, 1);
  }

protected:
  enum ImageLoadType {
    // Most normal image loads
    eImageLoadType_Normal,
    // From a <img srcset> or <picture> context. Affects type given to content
    // policy.
    eImageLoadType_Imageset
  };

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
   * @param aImageLoadType The ImageLoadType for this request
   */
  nsresult LoadImage(const nsAString& aNewURI, bool aForce,
                     bool aNotify, ImageLoadType aImageLoadType);

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
  mozilla::EventStates ImageState() const;

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
   * @param aImageLoadType The ImageLoadType for this request
   * @param aLoadStart If true, dispatch "loadstart" event.
   * @param aDocument Optional parameter giving the document this node is in.
   *        This is purely a performance optimization.
   * @param aLoadFlags Optional parameter specifying load flags to use for
   *        the image load
   */
  nsresult LoadImage(nsIURI* aNewURI, bool aForce, bool aNotify,
                     ImageLoadType aImageLoadType, bool aLoadStart = true,
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
  nsresult UseAsPrimaryRequest(imgRequestProxy* aRequest, bool aNotify,
                               ImageLoadType aImageLoadType);

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

  /**
   * Returns the CORS mode that will be used for all future image loads. The
   * default implementation returns CORS_NONE unconditionally.
   */
  virtual mozilla::CORSMode GetCORSMode();

  virtual mozilla::net::ReferrerPolicy GetImageReferrerPolicy();

  // Subclasses are *required* to call BindToTree/UnbindFromTree.
  void BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                  nsIContent* aBindingParent, bool aCompileEventHandlers);
  void UnbindFromTree(bool aDeep, bool aNullParent);

  nsresult OnLoadComplete(imgIRequest* aRequest, nsresult aStatus);
  void OnUnlockedDraw();
  nsresult OnImageIsAnimated(imgIRequest *aRequest);

  // The nsContentPolicyType we would use for this ImageLoadType
  static nsContentPolicyType PolicyTypeForLoad(ImageLoadType aImageLoadType);

  void AsyncEventRunning(mozilla::AsyncEventDispatcher* aEvent);

  // Get ourselves as an nsIContent*.  Not const because some of the callers
  // want a non-const nsIContent.
  virtual nsIContent* AsContent() = 0;

private:
  /**
   * Struct used to manage the image observers.
   */
  struct ImageObserver {
    explicit ImageObserver(imgINotificationObserver* aObserver);
    ~ImageObserver();

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
   * Method to fire an event once we know what's going on with the image load.
   *
   * @param aEventType "loadstart", "loadend", "load", or "error" depending on
   *                   how things went
   * @param aIsCancelable true if event is cancelable.
   */
  nsresult FireEvent(const nsAString& aEventType, bool aIsCancelable = false);

  /**
   * Method to cancel and null-out pending event if they exist.
   */
  void CancelPendingEvent();

  RefPtr<mozilla::AsyncEventDispatcher> mPendingEvent;

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
   *
   * @param aImageLoadType The ImageLoadType for this request
   */
   RefPtr<imgRequestProxy>& PrepareNextRequest(ImageLoadType aImageLoadType);

  /**
   * Returns a COMPtr reference to the current/pending image requests, cleaning
   * up and canceling anything that was there before. Note that if you just want
   * to get rid of one of the requests, you should call
   * Clear*Request(NS_BINDING_ABORTED) instead.
   *
   * @param aImageLoadType The ImageLoadType for this request
   */
  RefPtr<imgRequestProxy>& PrepareCurrentRequest(ImageLoadType aImageLoadType);
  RefPtr<imgRequestProxy>& PreparePendingRequest(ImageLoadType aImageLoadType);

  /**
   * Switch our pending request to be our current request.
   * mPendingRequest must be non-null!
   */
  void MakePendingRequestCurrent();

  /**
   * Cancels and nulls-out the "current" and "pending" requests if they exist.
   *
   * @param aNonvisibleAction An action to take if the image is no longer
   *                          visible as a result; see |UntrackImage|.
   */
  void ClearCurrentRequest(nsresult aReason,
                           const Maybe<OnNonvisible>& aNonvisibleAction = Nothing());
  void ClearPendingRequest(nsresult aReason,
                           const Maybe<OnNonvisible>& aNonvisibleAction = Nothing());

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
   *
   * @param aFrame If called from FrameCreated the frame passed to FrameCreated.
   *               This is our frame, but at the time of the FrameCreated call
   *               our primary frame pointer hasn't been set yet, so this is
   *               only way to get our frame.
   *
   * @param aNonvisibleAction A requested action if the frame has become
   *                          nonvisible. If Nothing(), no action is
   *                          requested. If DISCARD_IMAGES is specified, the
   *                          frame is requested to ask any images it's
   *                          associated with to discard their surfaces if
   *                          possible.
   */
  void TrackImage(imgIRequest* aImage, nsIFrame* aFrame = nullptr);
  void UntrackImage(imgIRequest* aImage,
                    const Maybe<OnNonvisible>& aNonvisibleAction = Nothing());

  /* MEMBERS */
  RefPtr<imgRequestProxy> mCurrentRequest;
  RefPtr<imgRequestProxy> mPendingRequest;
  uint32_t mCurrentRequestFlags;
  uint32_t mPendingRequestFlags;

  enum {
    // Set if the request needs ResetAnimation called on it.
    REQUEST_NEEDS_ANIMATION_RESET = 0x00000001U,
    // Set if the request is blocking onload.
    REQUEST_BLOCKS_ONLOAD = 0x00000002U,
    // Set if the request is currently tracked with the document.
    REQUEST_IS_TRACKED = 0x00000004U,
    // Set if this is an imageset request, such as from <img srcset> or
    // <picture>
    REQUEST_IS_IMAGESET = 0x00000008U
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
  mozilla::EventStates mForcedImageState;

  mozilla::TimeStamp mMostRecentRequestChange;

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

  /**
   * Flag to indicate whether the channel should be mark as urgent-start.
   * It should be set in *Element and passed to nsContentUtils::LoadImage.
   * True if we want to set nsIClassOfService::UrgentStart to the channel to
   * get the response ASAP for better user responsiveness.
   */
  bool mUseUrgentStartForChannel;
private:
  /* The number of nested AutoStateChangers currently tracking our state. */
  uint8_t mStateChangerDepth;

  // Flags to indicate whether each of the current and pending requests are
  // registered with the refresh driver.
  bool mCurrentRequestRegistered;
  bool mPendingRequestRegistered;

  // TODO:
  // Bug 1353685: Should ServiceWorker call SetBlockedRequest?
  //
  // This member is used in SetBlockedRequest, if it's true, then this call is
  // triggered from LoadImage.
  // If this is false, it means this call is from other places like
  // ServiceWorker, then we will ignore call to SetBlockedRequest for now.
  //
  // Also we use this variable to check if some evil code is reentering LoadImage.
  bool mIsStartingImageLoad;
};

#endif // nsImageLoadingContent_h__
