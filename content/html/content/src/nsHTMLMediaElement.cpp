/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
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
#include "nsIDOMHTMLSourceElement.h"
#include "nsHTMLMediaElement.h"
#include "nsGenericHTMLElement.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"
#include "nsNodeInfoManager.h"
#include "plbase64.h"
#include "nsNetUtil.h"
#include "prmem.h"
#include "nsNetUtil.h"
#include "nsXPCOMStrings.h"
#include "prlock.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsIRenderingContext.h"
#include "nsITimer.h"

#include "nsEventDispatcher.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMProgressEvent.h"
#include "nsHTMLMediaError.h"
#include "nsICategoryManager.h"
#include "nsCommaSeparatedTokenizer.h"
#include "nsMediaStream.h"

#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsCycleCollectionParticipant.h"

#ifdef MOZ_OGG
#include "nsOggDecoder.h"
#endif
#ifdef MOZ_WAVE
#include "nsWaveDecoder.h"
#endif


class nsMediaEvent : public nsRunnable
{
public:

  nsMediaEvent(nsHTMLMediaElement* aElement) :
    mElement(aElement),
    mLoadID(mElement->GetCurrentLoadID()) {}
  ~nsMediaEvent() {}

  NS_IMETHOD Run() = 0;

protected:
  PRBool IsCancelled() {
    return mElement->GetCurrentLoadID() != mLoadID;
  }

  nsCOMPtr<nsHTMLMediaElement> mElement;
  PRUint32 mLoadID;
};


class nsAsyncEventRunner : public nsMediaEvent
{
private:
  nsString mName;
  PRPackedBool mProgress;
  
public:
  nsAsyncEventRunner(const nsAString& aName, nsHTMLMediaElement* aElement, PRBool aProgress) : 
    nsMediaEvent(aElement), mName(aName), mProgress(aProgress)
  {
  }
  
  NS_IMETHOD Run() {
    // Silently cancel if our load has been cancelled.
    if (IsCancelled())
      return NS_OK;
    return mProgress ?
      mElement->DispatchProgressEvent(mName) :
      mElement->DispatchSimpleEvent(mName);
  }
};

class nsHTMLMediaElement::LoadNextSourceEvent : public nsMediaEvent {
public:
  LoadNextSourceEvent(nsHTMLMediaElement *aElement)
    : nsMediaEvent(aElement) {}
  NS_IMETHOD Run() {
    if (!IsCancelled())
      mElement->LoadFromSourceChildren();
    return NS_OK;
  }
};

class nsHTMLMediaElement::SelectResourceEvent : public nsMediaEvent {
public:
  SelectResourceEvent(nsHTMLMediaElement *aElement)
    : nsMediaEvent(aElement) {}
  NS_IMETHOD Run() {
    if (!IsCancelled()) {
      NS_ASSERTION(mElement->mIsRunningSelectResource,
                   "Should have flagged that we're running SelectResource()");
      mElement->SelectResource();
      mElement->mIsRunningSelectResource = PR_FALSE;
    }
    return NS_OK;
  }
};

void nsHTMLMediaElement::QueueSelectResourceTask()
{
  // Don't allow multiple async select resource calls to be queued.
  if (mIsRunningSelectResource)
    return;
  mIsRunningSelectResource = PR_TRUE;
  ChangeDelayLoadStatus(PR_TRUE);
  nsCOMPtr<nsIRunnable> event = new SelectResourceEvent(this);
  NS_DispatchToMainThread(event);
}

void nsHTMLMediaElement::QueueLoadFromSourceTask()
{
  ChangeDelayLoadStatus(PR_TRUE);
  nsCOMPtr<nsIRunnable> event = new LoadNextSourceEvent(this);
  NS_DispatchToMainThread(event);
}

/**
 * There is a reference cycle involving this class: MediaLoadListener
 * holds a reference to the nsHTMLMediaElement, which holds a reference
 * to an nsIChannel, which holds a reference to this listener.
 * We break the reference cycle in OnStartRequest by clearing mElement.
 */
class nsHTMLMediaElement::MediaLoadListener : public nsIStreamListener,
                                              public nsIChannelEventSink,
                                              public nsIInterfaceRequestor
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

public:
  MediaLoadListener(nsHTMLMediaElement* aElement)
    : mElement(aElement)
  {
    NS_ABORT_IF_FALSE(mElement, "Must pass an element to call back");
  }

private:
  nsRefPtr<nsHTMLMediaElement> mElement;
  nsCOMPtr<nsIStreamListener> mNextListener;
};

NS_IMPL_ISUPPORTS4(nsHTMLMediaElement::MediaLoadListener, nsIRequestObserver,
                   nsIStreamListener, nsIChannelEventSink,
                   nsIInterfaceRequestor)

NS_IMETHODIMP nsHTMLMediaElement::MediaLoadListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  // The element is only needed until we've had a chance to call
  // InitializeDecoderForChannel. So make sure mElement is cleared here.
  nsRefPtr<nsHTMLMediaElement> element;
  element.swap(mElement);

  // Don't continue to load if the request failed or has been canceled.
  nsresult rv;
  nsresult status;
  rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_FAILED(status)) {
    if (element)
      element->NotifyLoadError();
    return status;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel &&
      element &&
      NS_SUCCEEDED(rv = element->InitializeDecoderForChannel(channel, getter_AddRefs(mNextListener))) &&
      mNextListener) {
    rv = mNextListener->OnStartRequest(aRequest, aContext);
  } else {
    // If InitializeDecoderForChannel() returned an error, fire a network
    // error.
    if (NS_FAILED(rv) && !mNextListener && element) {
      // Load failed, attempt to load the next candidate resource. If there
      // are none, this will trigger a MEDIA_ERR_SRC_NOT_SUPPORTED error.
      element->NotifyLoadError();
    }
    // If InitializeDecoderForChannel did not return a listener (but may
    // have otherwise succeeded), we abort the connection since we aren't
    // interested in keeping the channel alive ourselves.
    rv = NS_BINDING_ABORTED;
  }

  return rv;
}

NS_IMETHODIMP nsHTMLMediaElement::MediaLoadListener::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                                                                     nsresult aStatus)
{
  if (mNextListener) {
    return mNextListener->OnStopRequest(aRequest, aContext, aStatus);
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::MediaLoadListener::OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                                                                       nsIInputStream* aStream, PRUint32 aOffset,
                                                                       PRUint32 aCount)
{
  if (!mNextListener) {
    NS_ERROR("Must have a chained listener; OnStartRequest should have canceled this request");
    return NS_BINDING_ABORTED;
  }
  return mNextListener->OnDataAvailable(aRequest, aContext, aStream, aOffset, aCount);
}

NS_IMETHODIMP nsHTMLMediaElement::MediaLoadListener::OnChannelRedirect(nsIChannel* aOldChannel,
                                                                       nsIChannel* aNewChannel,
                                                                       PRUint32 aFlags)
{
  if (mElement)
    mElement->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
  nsCOMPtr<nsIChannelEventSink> sink = do_QueryInterface(mNextListener);
  if (sink)
    return sink->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::MediaLoadListener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

NS_IMPL_ADDREF_INHERITED(nsHTMLMediaElement, nsGenericHTMLElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLMediaElement, nsGenericHTMLElement)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLMediaElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLMediaElement, nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSourcePointer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLoadBlockedDoc)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLMediaElement, nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSourcePointer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsHTMLMediaElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

// nsIDOMHTMLMediaElement
NS_IMPL_URI_ATTR(nsHTMLMediaElement, Src, src)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Controls, controls)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Autoplay, autoplay)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Autobuffer, autobuffer)

/* readonly attribute nsIDOMHTMLMediaElement mozAutoplayEnabled; */
NS_IMETHODIMP nsHTMLMediaElement::GetMozAutoplayEnabled(PRBool *aAutoplayEnabled)
{
  *aAutoplayEnabled = mAutoplayEnabled;

  return NS_OK;
}

/* readonly attribute nsIDOMHTMLMediaError error; */
NS_IMETHODIMP nsHTMLMediaElement::GetError(nsIDOMHTMLMediaError * *aError)
{
  NS_IF_ADDREF(*aError = mError);

  return NS_OK;
}

/* readonly attribute boolean ended; */
NS_IMETHODIMP nsHTMLMediaElement::GetEnded(PRBool *aEnded)
{
  *aEnded = mDecoder ? mDecoder->IsEnded() : PR_FALSE;

  return NS_OK;
}

/* readonly attribute DOMString currentSrc; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentSrc(nsAString & aCurrentSrc)
{
  nsCAutoString src;
  
  if (mDecoder) {
    nsMediaStream* stream = mDecoder->GetCurrentStream();
    if (stream) {
      stream->URI()->GetSpec(src);
    }
  }

  aCurrentSrc = NS_ConvertUTF8toUTF16(src);

  return NS_OK;
}

/* readonly attribute unsigned short networkState; */
NS_IMETHODIMP nsHTMLMediaElement::GetNetworkState(PRUint16 *aNetworkState)
{
  *aNetworkState = mNetworkState;

  return NS_OK;
}

nsresult
nsHTMLMediaElement::OnChannelRedirect(nsIChannel *aChannel,
                                      nsIChannel *aNewChannel,
                                      PRUint32 aFlags)
{
  NS_ASSERTION(aChannel == mChannel, "Channels should match!");
  mChannel = aNewChannel;
  return NS_OK;
}

void nsHTMLMediaElement::AbortExistingLoads()
{
  // Abort any already-running instance of the resource selection algorithm.
  mLoadWaitStatus = NOT_WAITING;

  // Set a new load ID. This will cause events which were enqueued
  // with a different load ID to silently be cancelled.
  mCurrentLoadID++;

  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nsnull;
  }

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING ||
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_IDLE)
  {
    mError = new nsHTMLMediaError(nsIDOMHTMLMediaError::MEDIA_ERR_ABORTED);
    DispatchProgressEvent(NS_LITERAL_STRING("abort"));
  }

  mError = nsnull;
  mLoadedFirstFrame = PR_FALSE;
  mAutoplaying = PR_TRUE;
  mIsLoadingFromSrcAttribute = PR_FALSE;
  mSuspendedAfterFirstFrame = PR_FALSE;
  mAllowSuspendAfterFirstFrame = PR_TRUE;

  // TODO: The playback rate must be set to the default playback rate.

  if (mNetworkState != nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING);
    mPaused = PR_TRUE;

    // TODO: The current playback position must be set to 0.
    DispatchSimpleEvent(NS_LITERAL_STRING("emptied"));
  }

  mIsRunningSelectResource = PR_FALSE;
}

void nsHTMLMediaElement::NoSupportedMediaSourceError()
{
  mError = new nsHTMLMediaError(nsIDOMHTMLMediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
  DispatchAsyncProgressEvent(NS_LITERAL_STRING("error"));
  ChangeDelayLoadStatus(PR_FALSE);
}

/* void load (); */
NS_IMETHODIMP nsHTMLMediaElement::Load()
{
  if (mIsRunningLoadMethod)
    return NS_OK;
  SetPlayedOrSeeked(PR_FALSE);
  mIsRunningLoadMethod = PR_TRUE;
  AbortExistingLoads();
  QueueSelectResourceTask();
  mIsRunningLoadMethod = PR_FALSE;
  return NS_OK;
}

static PRBool HasSourceChildren(nsIContent *aElement)
{
  PRUint32 count = aElement->GetChildCount();
  for (PRUint32 i = 0; i < count; ++i) {
    nsIContent* child = aElement->GetChildAt(i);
    NS_ASSERTION(child, "GetChildCount lied!");
    if (child &&
        child->Tag() == nsGkAtoms::source &&
        child->IsHTML())
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

// Returns true if aElement has a src attribute, or a <source> child.
static PRBool HasPotentialResource(nsIContent *aElement) 
{
  nsAutoString src;
  if (aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src))
    return PR_TRUE;
  return HasSourceChildren(aElement);
}

void nsHTMLMediaElement::SelectResource()
{
  if (!HasPotentialResource(this)) {
    // While the media element has neither a src attribute nor any source
    // element children, wait. (This steps might wait forever.) 
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
    mLoadWaitStatus = WAITING_FOR_SRC_OR_SOURCE;
    ChangeDelayLoadStatus(PR_FALSE);
    return;
  }

  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  DispatchAsyncProgressEvent(NS_LITERAL_STRING("loadstart"));

  nsAutoString src;
  nsCOMPtr<nsIURI> uri;

  // If we have a 'src' attribute, use that exclusively.
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    nsresult rv = NewURIFromString(src, getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      mIsLoadingFromSrcAttribute = PR_TRUE;
      rv = LoadResource(uri);
      if (NS_SUCCEEDED(rv))
        return;
    }
    NoSupportedMediaSourceError();
  } else {
    // Otherwise, the source elements will be used.
    LoadFromSourceChildren();
  }
}

void nsHTMLMediaElement::NotifyLoadError()
{
  if (mIsLoadingFromSrcAttribute) {
    NoSupportedMediaSourceError();
  } else {
    QueueLoadFromSourceTask();
  }
}

void nsHTMLMediaElement::LoadFromSourceChildren()
{
  NS_ASSERTION(!IsInDoc() || mDelayingLoadEvent,
               "Should delay load event while loading in document");
  while (PR_TRUE) {
    nsresult rv;
    nsCOMPtr<nsIURI> uri = GetNextSource();
    if (!uri) {
      // Exhausted candidates, wait for more candidates to be appended to
      // the media element.
      mLoadWaitStatus = WAITING_FOR_SOURCE;
      NoSupportedMediaSourceError();
      return;
    }

    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  
    rv = LoadResource(uri);
    if (NS_SUCCEEDED(rv))
      return;

    // If we fail to load, loop back and try loading the next resource.
  }
  NS_NOTREACHED("Execution should not reach here!");
}

nsresult nsHTMLMediaElement::LoadResource(nsIURI* aURI)
{
  NS_ASSERTION(!IsInDoc() || mDelayingLoadEvent,
               "Should delay load event while loading in document");
  nsresult rv;

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }

  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_MEDIA,
                                 aURI,
                                 NodePrincipal(),
                                 this,
                                 EmptyCString(), // mime type
                                 nsnull, // extra
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv,rv);
  if (NS_CP_REJECTED(shouldLoad)) return NS_ERROR_FAILURE;

  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
  rv = NS_NewChannel(getter_AddRefs(mChannel),
                     aURI,
                     nsnull,
                     loadGroup,
                     nsnull,
                     nsIRequest::LOAD_NORMAL);
  NS_ENSURE_SUCCESS(rv,rv);

  // The listener holds a strong reference to us.  This creates a reference
  // cycle which is manually broken in the listener's OnStartRequest method
  // after it is finished with the element.
  nsRefPtr<MediaLoadListener> loadListener = new MediaLoadListener(this);
  if (!loadListener) return NS_ERROR_OUT_OF_MEMORY;
  
  mChannel->SetNotificationCallbacks(loadListener);

  nsCOMPtr<nsIStreamListener> listener;
  if (ShouldCheckAllowOrigin()) {
    listener = new nsCrossSiteListenerProxy(loadListener,
                                            NodePrincipal(),
                                            mChannel, 
                                            PR_FALSE,
                                            &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!listener) return NS_ERROR_OUT_OF_MEMORY;
  } else {
    rv = nsContentUtils::GetSecurityManager()->
           CheckLoadURIWithPrincipal(NodePrincipal(),
                                     aURI,
                                     nsIScriptSecurityManager::STANDARD);
    NS_ENSURE_SUCCESS(rv,rv);
    listener = loadListener;
  }

  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
  if (hc) {
    // Use a byte range request from the start of the resource.
    // This enables us to detect if the stream supports byte range
    // requests, and therefore seeking, early.
    hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"),
                         NS_LITERAL_CSTRING("bytes=0-"),
                         PR_FALSE);
  }

  rv = mChannel->AsyncOpen(listener, nsnull);
  if (NS_FAILED(rv)) {
    // OnStartRequest is guaranteed to be called if the open succeeds.  If
    // the open failed, the listener's OnStartRequest will never be called,
    // so we need to break the element->channel->listener->element reference
    // cycle here.  The channel holds the only reference to the listener,
    // and is useless now anyway, so drop our reference to it to allow it to
    // be destroyed.
    mChannel = nsnull;
    return rv;
  }

  // Else the channel must be open and starting to download. If it encounters
  // a non-catestrophic failure, it will set a new task to continue loading
  // another candidate.
  return NS_OK;
}

nsresult nsHTMLMediaElement::LoadWithChannel(nsIChannel *aChannel,
                                             nsIStreamListener **aListener)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aListener);

  *aListener = nsnull;

  AbortExistingLoads();

  ChangeDelayLoadStatus(PR_TRUE);

  nsresult rv = InitializeDecoderForChannel(aChannel, aListener);
  if (NS_FAILED(rv)) {
    ChangeDelayLoadStatus(PR_FALSE);
    return rv;
  }

  DispatchAsyncProgressEvent(NS_LITERAL_STRING("loadstart"));

  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::MozLoadFrom(nsIDOMHTMLMediaElement* aOther)
{
  NS_ENSURE_ARG_POINTER(aOther);

  AbortExistingLoads();

  nsCOMPtr<nsIContent> content = do_QueryInterface(aOther);
  nsHTMLMediaElement* other = static_cast<nsHTMLMediaElement*>(content.get());
  if (!other || !other->mDecoder)
    return NS_OK;

  ChangeDelayLoadStatus(PR_TRUE);

  nsresult rv = InitializeDecoderAsClone(other->mDecoder);
  if (NS_FAILED(rv)) {
    ChangeDelayLoadStatus(PR_FALSE);
    return rv;
  }

  DispatchAsyncProgressEvent(NS_LITERAL_STRING("loadstart"));

  return NS_OK;
}

/* readonly attribute unsigned short readyState; */
NS_IMETHODIMP nsHTMLMediaElement::GetReadyState(PRUint16 *aReadyState)
{
  *aReadyState = mReadyState;

  return NS_OK;
}

/* readonly attribute boolean seeking; */
NS_IMETHODIMP nsHTMLMediaElement::GetSeeking(PRBool *aSeeking)
{
  *aSeeking = mDecoder && mDecoder->IsSeeking();

  return NS_OK;
}

/* attribute float currentTime; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentTime(float *aCurrentTime)
{
  *aCurrentTime = mDecoder ? mDecoder->GetCurrentTime() : 0.0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetCurrentTime(float aCurrentTime)
{
  StopSuspendingAfterFirstFrame();

  if (!mDecoder)
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_NOTHING) 
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // Detect for a NaN and invalid values.
  if (aCurrentTime != aCurrentTime)
    return NS_ERROR_FAILURE;

  // Clamp the time to [0, duration] as required by the spec
  float clampedTime = PR_MAX(0, aCurrentTime);
  float duration = mDecoder->GetDuration();
  if (duration >= 0) {
    clampedTime = PR_MIN(clampedTime, duration);
  }

  mPlayingBeforeSeek = IsPotentiallyPlaying();
  // The media backend is responsible for dispatching the timeupdate
  // event if it changes the playback position as a result of the seek.
  nsresult rv = mDecoder->Seek(clampedTime);
  return rv;
}

/* readonly attribute float duration; */
NS_IMETHODIMP nsHTMLMediaElement::GetDuration(float *aDuration)
{
  *aDuration =  mDecoder ? mDecoder->GetDuration() : 0.0;
  return NS_OK;
}

/* readonly attribute boolean paused; */
NS_IMETHODIMP nsHTMLMediaElement::GetPaused(PRBool *aPaused)
{
  *aPaused = mPaused;

  return NS_OK;
}

/* void pause (); */
NS_IMETHODIMP nsHTMLMediaElement::Pause()
{
  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    nsresult rv = Load();
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (mDecoder) {
    mDecoder->Pause();
  }

  PRBool oldPaused = mPaused;
  mPaused = PR_TRUE;
  mAutoplaying = PR_FALSE;
  
  if (!oldPaused) {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("timeupdate"));
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("pause"));
  }

  return NS_OK;
}

/* attribute float volume; */
NS_IMETHODIMP nsHTMLMediaElement::GetVolume(float *aVolume)
{
  *aVolume = mVolume;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetVolume(float aVolume)
{
  if (aVolume < 0.0f || aVolume > 1.0f)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  if (aVolume == mVolume)
    return NS_OK;

  mVolume = aVolume;

  if (mDecoder && !mMuted)
    mDecoder->SetVolume(mVolume);

  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("volumechange"));

  return NS_OK;
}

/* attribute boolean muted; */
NS_IMETHODIMP nsHTMLMediaElement::GetMuted(PRBool *aMuted)
{
  *aMuted = mMuted;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetMuted(PRBool aMuted)
{
  if (aMuted == mMuted)
    return NS_OK;

  mMuted = aMuted;

  if (mDecoder) {
    mDecoder->SetVolume(mMuted ? 0.0 : mVolume);
  }

  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("volumechange"));

  return NS_OK;
}

nsHTMLMediaElement::nsHTMLMediaElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
  : nsGenericHTMLElement(aNodeInfo),
    mCurrentLoadID(0),
    mNetworkState(nsIDOMHTMLMediaElement::NETWORK_EMPTY),
    mReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING),
    mLoadWaitStatus(NOT_WAITING),
    mVolume(1.0),
    mMediaSize(-1,-1),
    mBegun(PR_FALSE),
    mLoadedFirstFrame(PR_FALSE),
    mAutoplaying(PR_TRUE),
    mAutoplayEnabled(PR_TRUE),
    mPaused(PR_TRUE),
    mMuted(PR_FALSE),
    mIsDoneAddingChildren(!aFromParser),
    mPlayingBeforeSeek(PR_FALSE),
    mPausedBeforeFreeze(PR_FALSE),
    mWaitingFired(PR_FALSE),
    mIsBindingToTree(PR_FALSE),
    mIsRunningLoadMethod(PR_FALSE),
    mIsLoadingFromSrcAttribute(PR_FALSE),
    mDelayingLoadEvent(PR_FALSE),
    mIsRunningSelectResource(PR_FALSE),
    mSuspendedAfterFirstFrame(PR_FALSE),
    mAllowSuspendAfterFirstFrame(PR_TRUE),
    mHasPlayedOrSeeked(PR_FALSE)
{
  RegisterFreezableElement();
}

nsHTMLMediaElement::~nsHTMLMediaElement()
{
  UnregisterFreezableElement();
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nsnull;
  }
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }
}

void nsHTMLMediaElement::StopSuspendingAfterFirstFrame()
{
  mAllowSuspendAfterFirstFrame = PR_FALSE;
  if (!mSuspendedAfterFirstFrame)
    return;
  mSuspendedAfterFirstFrame = PR_FALSE;
  if (mDecoder) {
    mDecoder->Resume();
  }
}

void nsHTMLMediaElement::SetPlayedOrSeeked(PRBool aValue)
{
  if (aValue == mHasPlayedOrSeeked)
    return;

  mHasPlayedOrSeeked = aValue;

  // Force a reflow so that the poster frame hides or shows immediately.
  nsIDocument *doc = GetDocument();
  if (!doc) return;
  nsIPresShell *presShell = doc->GetPrimaryShell();  
  if (!presShell) return;
  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);
  if (!frame) return;
  presShell->FrameNeedsReflow(frame,
                              nsIPresShell::eTreeChange,
                              NS_FRAME_IS_DIRTY);
}

NS_IMETHODIMP nsHTMLMediaElement::Play()
{
  StopSuspendingAfterFirstFrame();
  SetPlayedOrSeeked(PR_TRUE);

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    nsresult rv = Load();
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (mDecoder) {
    if (mDecoder->IsEnded()) {
      SetCurrentTime(0);
    }
    nsresult rv = mDecoder->Play();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // TODO: If the playback has ended, then the user agent must set 
  // seek to the effective start.
  // TODO: The playback rate must be set to the default playback rate.
  if (mPaused) {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("play"));
    switch (mReadyState) {
    case nsIDOMHTMLMediaElement::HAVE_METADATA:
    case nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA:
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("waiting"));
      break;
    case nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA:
    case nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA:
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("playing"));
      break;
    }
  }

  mPaused = PR_FALSE;
  mAutoplaying = PR_FALSE;

  return NS_OK;
}

PRBool nsHTMLMediaElement::ParseAttribute(PRInt32 aNamespaceID,
                                          nsIAtom* aAttribute,
                                          const nsAString& aValue,
                                          nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::src) {
      static const char* kWhitespace = " \n\r\t\b";
      aResult.SetTo(nsContentUtils::TrimCharsInSet(kWhitespace, aValue));
      return PR_TRUE;
    }
    else if (aAttribute == nsGkAtoms::loopstart
            || aAttribute == nsGkAtoms::loopend
            || aAttribute == nsGkAtoms::start
            || aAttribute == nsGkAtoms::end) {
      return aResult.ParseFloatValue(aValue);
    }
    else if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return PR_TRUE;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsresult nsHTMLMediaElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                     nsIAtom* aPrefix, const nsAString& aValue,
                                     PRBool aNotify)
{
  nsresult rv = 
    nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                    aNotify);
  if (aNotify && aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::src) {
      if (mLoadWaitStatus == WAITING_FOR_SRC_OR_SOURCE) {
        // A previous load algorithm instance is waiting on a src
        // addition, resume the load. It is waiting at "step 1 of the load
        // algorithm".
        mLoadWaitStatus = NOT_WAITING;
        QueueSelectResourceTask();
      }
    } else if (aName == nsGkAtoms::autoplay) {
      StopSuspendingAfterFirstFrame();
      if (mReadyState == nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) {
        NotifyAutoplayDataReady();
      }
    } else if (aName == nsGkAtoms::autobuffer) {
      StopSuspendingAfterFirstFrame();
    }
  }

  return rv;
}

static PRBool IsAutoplayEnabled()
{
  return nsContentUtils::GetBoolPref("media.autoplay.enabled");
}

nsresult nsHTMLMediaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                        nsIContent* aBindingParent,
                                        PRBool aCompileEventHandlers)
{
  mIsBindingToTree = PR_TRUE;
  mAutoplayEnabled = IsAutoplayEnabled();
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, 
                                                 aParent, 
                                                 aBindingParent, 
                                                 aCompileEventHandlers);
  if (NS_SUCCEEDED(rv) &&
      mIsDoneAddingChildren &&
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY)
  {
    QueueSelectResourceTask();
  }

  mIsBindingToTree = PR_FALSE;

  return rv;
}

void nsHTMLMediaElement::UnbindFromTree(PRBool aDeep,
                                        PRBool aNullParent)
{
  if (!mPaused && mNetworkState != nsIDOMHTMLMediaElement::NETWORK_EMPTY)
    Pause();
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

#ifdef MOZ_OGG
// See http://www.rfc-editor.org/rfc/rfc5334.txt for the definitions
// of Ogg media types and codec types
static const char gOggTypes[][16] = {
  "video/ogg",
  "audio/ogg",
  "application/ogg"
};

static const char* gOggCodecs[] = {
  "vorbis",
  "theora",
  nsnull
};

static PRBool IsOggEnabled()
{
  return nsContentUtils::GetBoolPref("media.ogg.enabled");
}

static PRBool IsOggType(const nsACString& aType)
{
  if (!IsOggEnabled())
    return PR_FALSE;
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gOggTypes); ++i) {
    if (aType.EqualsASCII(gOggTypes[i]))
      return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

#ifdef MOZ_WAVE
// See http://www.rfc-editor.org/rfc/rfc2361.txt for the definitions
// of WAVE media types and codec types. However, the audio/vnd.wave
// MIME type described there is not used.
static const char gWaveTypes[][16] = {
  "audio/x-wav",
  "audio/wav",
  "audio/wave",
  "audio/x-pn-wav"
};

static const char* gWaveCodecs[] = {
  "1", // Microsoft PCM Format
  nsnull
};

static PRBool IsWaveEnabled()
{
  return nsContentUtils::GetBoolPref("media.wave.enabled");
}

static PRBool IsWaveType(const nsACString& aType)
{
  if (!IsWaveEnabled())
    return PR_FALSE;
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gWaveTypes); ++i) {
    if (aType.EqualsASCII(gWaveTypes[i]))
      return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

/* static */
PRBool nsHTMLMediaElement::CanHandleMediaType(const char* aMIMEType,
                                              const char*** aCodecList)
{
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType))) {
    *aCodecList = gOggCodecs;
    return PR_TRUE;
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    *aCodecList = gWaveCodecs;
    return PR_TRUE;
  }
#endif
  return PR_FALSE;
}

/* static */
PRBool nsHTMLMediaElement::ShouldHandleMediaType(const char* aMIMEType)
{
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType)))
    return PR_TRUE;
#endif
  // We should not return true for Wave types, since there are some
  // Wave codecs actually in use in the wild that we don't support, and
  // we should allow those to be handled by plugins or helper apps.
  // Furthermore people can play Wave files on most platforms by other
  // means.
  return PR_FALSE;
}

static PRBool
CodecListContains(const char** aCodecs, const nsAString& aCodec)
{
  for (PRInt32 i = 0; aCodecs[i]; ++i) {
    if (aCodec.EqualsASCII(aCodecs[i]))
      return PR_TRUE;
  }
  return PR_FALSE;
}

enum CanPlayStatus {
  CANPLAY_NO,
  CANPLAY_MAYBE,
  CANPLAY_YES
};

static CanPlayStatus GetCanPlay(const nsAString& aType)
{
  nsContentTypeParser parser(aType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv))
    return CANPLAY_NO;

  NS_ConvertUTF16toUTF8 mimeTypeUTF8(mimeType);
  const char** supportedCodecs;
  if (!nsHTMLMediaElement::CanHandleMediaType(mimeTypeUTF8.get(),
                                              &supportedCodecs))
    return CANPLAY_NO;

  nsAutoString codecs;
  rv = parser.GetParameter("codecs", codecs);
  if (NS_FAILED(rv))
    // Parameter not found or whatever
    return CANPLAY_MAYBE;

  CanPlayStatus result = CANPLAY_YES;
  // See http://www.rfc-editor.org/rfc/rfc4281.txt for the description
  // of the 'codecs' parameter
  nsCommaSeparatedTokenizer tokenizer(codecs);
  PRBool expectMoreTokens = PR_FALSE;
  while (tokenizer.hasMoreTokens()) {
    const nsSubstring& token = tokenizer.nextToken();

    if (!CodecListContains(supportedCodecs, token)) {
      // Totally unsupported codec
      return CANPLAY_NO;
    }
    expectMoreTokens = tokenizer.lastTokenEndedWithComma();
  }
  if (expectMoreTokens) {
    // Last codec name was empty
    return CANPLAY_NO;
  }
  return result;
}

NS_IMETHODIMP
nsHTMLMediaElement::CanPlayType(const nsAString& aType, nsAString& aResult)
{
  switch (GetCanPlay(aType)) {
  case CANPLAY_NO: aResult.AssignLiteral(""); break;
  case CANPLAY_YES: aResult.AssignLiteral("probably"); break;
  default:
  case CANPLAY_MAYBE: aResult.AssignLiteral("maybe"); break;
  }
  return NS_OK;
}

/* static */
void nsHTMLMediaElement::InitMediaTypes()
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
#ifdef MOZ_OGG
    if (IsOggEnabled()) {
      for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gOggTypes); i++) {
        catMan->AddCategoryEntry("Gecko-Content-Viewers", gOggTypes[i],
                                 "@mozilla.org/content/document-loader-factory;1",
                                 PR_FALSE, PR_TRUE, nsnull);
      }
    }
#endif
  }
}

/* static */
void nsHTMLMediaElement::ShutdownMediaTypes()
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
#ifdef MOZ_OGG
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gOggTypes); i++) {
      catMan->DeleteCategoryEntry("Gecko-Content-Viewers", gOggTypes[i], PR_FALSE);
    }
#endif
#ifdef MOZ_WAVE
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gWaveTypes); i++) {
      catMan->DeleteCategoryEntry("Gecko-Content-Viewers", gWaveTypes[i], PR_FALSE);
    }
#endif
  }
}

PRBool nsHTMLMediaElement::CreateDecoder(const nsACString& aType)
{
#ifdef MOZ_OGG
  if (IsOggType(aType)) {
    mDecoder = new nsOggDecoder();
    if (mDecoder && !mDecoder->Init(this)) {
      mDecoder = nsnull;
    }
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    mDecoder = new nsWaveDecoder();
    if (mDecoder && !mDecoder->Init(this)) {
      mDecoder = nsnull;
    }
  }
#endif
  return mDecoder != nsnull;
}

nsresult nsHTMLMediaElement::InitializeDecoderAsClone(nsMediaDecoder* aOriginal)
{
  nsMediaStream* originalStream = aOriginal->GetCurrentStream();
  if (!originalStream)
    return NS_ERROR_FAILURE;
  mDecoder = aOriginal->Clone();
  if (!mDecoder)
    return NS_ERROR_FAILURE;

  if (!mDecoder->Init(this)) {
    mDecoder = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsMediaStream* stream = originalStream->CloneData(mDecoder);
  if (!stream) {
    mDecoder = nsnull;
    return NS_ERROR_FAILURE;
  }

  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;

  nsresult rv = mDecoder->Load(stream, nsnull);
  if (NS_FAILED(rv)) {
    mDecoder = nsnull;
    return rv;
  }

  return FinishDecoderSetup();
}

nsresult nsHTMLMediaElement::InitializeDecoderForChannel(nsIChannel *aChannel,
                                                         nsIStreamListener **aListener)
{
  nsCAutoString mimeType;
  aChannel->GetContentType(mimeType);

  if (!CreateDecoder(mimeType))
    return NS_ERROR_FAILURE;

  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;

  nsMediaStream* stream = nsMediaStream::Create(mDecoder, aChannel);
  if (!stream)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = mDecoder->Load(stream, aListener);
  if (NS_FAILED(rv)) {
    mDecoder = nsnull;
    return rv;
  }

  // Decoder successfully created, the decoder now owns the nsMediaStream
  // which owns the channel.
  mChannel = nsnull;

  return FinishDecoderSetup();
}

nsresult nsHTMLMediaElement::FinishDecoderSetup()
{
  nsresult rv = NS_OK;

  mDecoder->SetVolume(mMuted ? 0.0 : mVolume);

  if (!mPaused) {
    SetPlayedOrSeeked(PR_TRUE);
    rv = mDecoder->Play();
  }

  mBegun = PR_TRUE;
  return rv;
}

nsresult nsHTMLMediaElement::NewURIFromString(const nsAutoString& aURISpec, nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  *aURI = nsnull;

  nsCOMPtr<nsIDocument> doc = GetOwnerDoc();
  if (!doc) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsresult rv = nsContentUtils::NewURIWithDocumentCharset(aURI,
                                                          aURISpec,
                                                          doc,
                                                          baseURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool equal;
  if (aURISpec.IsEmpty() &&
      doc->GetDocumentURI() &&
      NS_SUCCEEDED(doc->GetDocumentURI()->Equals(*aURI, &equal)) &&
      equal) {
    // It's not possible for a media resource to be embedded in the current
    // document we extracted aURISpec from, so there's no point returning
    // the current document URI just to let the caller attempt and fail to
    // decode it.
    NS_RELEASE(*aURI);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  return NS_OK;
}

void nsHTMLMediaElement::MetadataLoaded()
{
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("durationchange"));
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("loadedmetadata"));
}

void nsHTMLMediaElement::FirstFrameLoaded(PRBool aResourceFullyLoaded)
{
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
  ChangeDelayLoadStatus(PR_FALSE);

  NS_ASSERTION(!mSuspendedAfterFirstFrame, "Should not have already suspended");

  if (mDecoder && mAllowSuspendAfterFirstFrame && mPaused &&
      !aResourceFullyLoaded &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::autobuffer)) {
    mSuspendedAfterFirstFrame = PR_TRUE;
    mDecoder->Suspend();
  }
}

void nsHTMLMediaElement::ResourceLoaded()
{
  mBegun = PR_FALSE;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADED;
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
  DispatchAsyncProgressEvent(NS_LITERAL_STRING("load"));
}

void nsHTMLMediaElement::NetworkError()
{
  mError = new nsHTMLMediaError(nsIDOMHTMLMediaError::MEDIA_ERR_NETWORK);
  mBegun = PR_FALSE;
  DispatchAsyncProgressEvent(NS_LITERAL_STRING("error"));
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("emptied"));
  ChangeDelayLoadStatus(PR_FALSE);
}

void nsHTMLMediaElement::DecodeError()
{
  mError = new nsHTMLMediaError(nsIDOMHTMLMediaError::MEDIA_ERR_DECODE);
  mBegun = PR_FALSE;
  DispatchAsyncProgressEvent(NS_LITERAL_STRING("error"));
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("emptied"));
  ChangeDelayLoadStatus(PR_FALSE);
}

void nsHTMLMediaElement::PlaybackEnded()
{
  NS_ASSERTION(mDecoder->IsEnded(), "Decoder fired ended, but not in ended state");
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("ended"));
}

void nsHTMLMediaElement::SeekStarted()
{
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("seeking"));
}

void nsHTMLMediaElement::SeekCompleted()
{
  mPlayingBeforeSeek = PR_FALSE;
  SetPlayedOrSeeked(PR_TRUE);
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("seeked"));
}

void nsHTMLMediaElement::DownloadSuspended()
{
  if (mBegun) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("suspend"));
  }
}

void nsHTMLMediaElement::DownloadResumed()
{
  if (mBegun) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  }
}

void nsHTMLMediaElement::DownloadStalled()
{
  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING) {
    DispatchAsyncProgressEvent(NS_LITERAL_STRING("stalled"));
  }
}

PRBool nsHTMLMediaElement::ShouldCheckAllowOrigin()
{
  return nsContentUtils::GetBoolPref("media.enforce_same_site_origin",
                                     PR_TRUE);
}

// Number of bytes to add to the download size when we're computing
// when the download will finish --- a safety margin in case bandwidth
// or other conditions are worse than expected
static const PRInt32 gDownloadSizeSafetyMargin = 1000000;

void nsHTMLMediaElement::UpdateReadyStateForData(NextFrameStatus aNextFrame)
{
  if (mReadyState < nsIDOMHTMLMediaElement::HAVE_METADATA) {
    // aNextFrame might have a next frame because the decoder can advance
    // on its own thread before ResourceLoaded or MetadataLoaded gets
    // a chance to run.
    // The arrival of more data can't change us out of this readyState.
    return;
  }

  nsMediaDecoder::Statistics stats = mDecoder->GetStatistics();

  if (aNextFrame != NEXT_FRAME_AVAILABLE) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
    if (!mWaitingFired && aNextFrame == NEXT_FRAME_UNAVAILABLE_BUFFERING) {
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("waiting"));
      mWaitingFired = PR_TRUE;
    }
    return;
  }

  // Now see if we should set HAVE_ENOUGH_DATA.
  // If it's something we don't know the size of, then we can't
  // make a real estimate, so we go straight to HAVE_ENOUGH_DATA once
  // we've downloaded enough data that our download rate is considered
  // reliable. We have to move to HAVE_ENOUGH_DATA at some point or
  // autoplay elements for live streams will never play.
  if (stats.mTotalBytes < 0 ? stats.mDownloadRateReliable :
                              stats.mTotalBytes == stats.mDownloadPosition) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
    return;
  }

  if (stats.mDownloadRateReliable && stats.mPlaybackRateReliable) {
    PRInt64 bytesToDownload = stats.mTotalBytes - stats.mDownloadPosition;
    PRInt64 bytesToPlayback = stats.mTotalBytes - stats.mPlaybackPosition;
    double timeToDownload =
      (bytesToDownload + gDownloadSizeSafetyMargin)/stats.mDownloadRate;
    double timeToPlay = bytesToPlayback/stats.mPlaybackRate;
    if (timeToDownload <= timeToPlay) {
      ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
      return;
    }
  }

  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA);
}

#ifdef DEBUG
static const char* gReadyStateToString[] = {
  "HAVE_NOTHING",
  "HAVE_METADATA",
  "HAVE_CURRENT_DATA",
  "HAVE_FUTURE_DATA",
  "HAVE_ENOUGH_DATA"
};
#endif

void nsHTMLMediaElement::ChangeReadyState(nsMediaReadyState aState)
{
  nsMediaReadyState oldState = mReadyState;
  mReadyState = aState;

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY || 
      oldState == mReadyState) {
    return;
  }

  LOG(PR_LOG_DEBUG, ("Ready state changed to %s", gReadyStateToString[aState]));

  // Handle raising of "waiting" event during seek (see 4.8.10.9)
  if (mPlayingBeforeSeek &&
      oldState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA) {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("waiting"));
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA && 
      !mLoadedFirstFrame)
  {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("loadeddata"));
    mLoadedFirstFrame = PR_TRUE;
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA) {
    mWaitingFired = PR_FALSE;
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA && 
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA) {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canplay"));
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) {
    NotifyAutoplayDataReady();
  }
  
  if (oldState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA && 
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA &&
      IsPotentiallyPlaying()) {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("playing"));
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canplaythrough"));
  }
}

void nsHTMLMediaElement::NotifyAutoplayDataReady()
{
  if (mAutoplaying &&
      mPaused &&
      HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
      mAutoplayEnabled) {
    mPaused = PR_FALSE;
    if (mDecoder) {
      SetPlayedOrSeeked(PR_TRUE);
      mDecoder->Play();
    }
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("play"));
  }
}

void nsHTMLMediaElement::Paint(gfxContext* aContext,
                               gfxPattern::GraphicsFilter aFilter,
                               const gfxRect& aRect) 
{
  if (mDecoder)
    mDecoder->Paint(aContext, aFilter, aRect);
}

nsresult nsHTMLMediaElement::DispatchSimpleEvent(const nsAString& aName)
{
  return nsContentUtils::DispatchTrustedEvent(GetOwnerDoc(), 
                                              static_cast<nsIContent*>(this), 
                                              aName, 
                                              PR_TRUE, 
                                              PR_TRUE);
}

nsresult nsHTMLMediaElement::DispatchAsyncSimpleEvent(const nsAString& aName)
{
  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this, PR_FALSE);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL); 
  return NS_OK;                           
}

nsresult nsHTMLMediaElement::DispatchAsyncProgressEvent(const nsAString& aName)
{
  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this, PR_TRUE);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL); 
  return NS_OK;                           
}

nsresult nsHTMLMediaElement::DispatchProgressEvent(const nsAString& aName)
{
  nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(GetOwnerDoc()));
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(static_cast<nsIContent*>(this)));
  NS_ENSURE_TRUE(docEvent && target, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = docEvent->CreateEvent(NS_LITERAL_STRING("ProgressEvent"), getter_AddRefs(event));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMProgressEvent> progressEvent(do_QueryInterface(event));
  NS_ENSURE_TRUE(progressEvent, NS_ERROR_FAILURE);

  PRInt64 totalBytes = 0;
  PRUint64 downloadPosition = 0;
  if (mDecoder) {
    nsMediaDecoder::Statistics stats = mDecoder->GetStatistics();
    totalBytes = stats.mTotalBytes;
    downloadPosition = stats.mDownloadPosition;
  }
  rv = progressEvent->InitProgressEvent(aName, PR_TRUE, PR_TRUE,
    totalBytes >= 0, downloadPosition, totalBytes);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool dummy;
  return target->DispatchEvent(event, &dummy);  
}

nsresult nsHTMLMediaElement::DoneAddingChildren(PRBool aHaveNotified)
{
  if (!mIsDoneAddingChildren) {
    mIsDoneAddingChildren = PR_TRUE;
  
    if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
      QueueSelectResourceTask();
    }
  }

  return NS_OK;
}

PRBool nsHTMLMediaElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

PRBool nsHTMLMediaElement::IsPotentiallyPlaying() const
{
  // TODO: 
  //   playback has not stopped due to errors, 
  //   and the element has not paused for user interaction
  return 
    !mPaused && 
    (mReadyState == nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA ||
    mReadyState == nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA) &&
    !IsPlaybackEnded();
}

PRBool nsHTMLMediaElement::IsPlaybackEnded() const
{
  // TODO:
  //   the current playback position is equal to the effective end of the media resource.
  //   See bug 449157.
  return mNetworkState >= nsIDOMHTMLMediaElement::HAVE_METADATA &&
    mDecoder ? mDecoder->IsEnded() : PR_FALSE;
}

already_AddRefed<nsIPrincipal> nsHTMLMediaElement::GetCurrentPrincipal()
{
  if (!mDecoder)
    return nsnull;

  return mDecoder->GetCurrentPrincipal();
}

void nsHTMLMediaElement::UpdateMediaSize(nsIntSize size)
{
  mMediaSize = size;
}

void nsHTMLMediaElement::DestroyContent()
{
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nsnull;
  }
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }
  nsGenericHTMLElement::DestroyContent();
}

void nsHTMLMediaElement::Freeze()
{
  mPausedBeforeFreeze = mPaused;
  if (!mPaused) {
    Pause();
  }
  if (mDecoder) {
    mDecoder->Suspend();
  }
}

void nsHTMLMediaElement::Thaw()
{
  if (!mPausedBeforeFreeze) {
    Play();
  }

  if (mDecoder) {
    mDecoder->Resume();
  }
}

PRBool
nsHTMLMediaElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eELEMENT | eMEDIA));
}

void nsHTMLMediaElement::NotifyAddedSource()
{
  if (mLoadWaitStatus == WAITING_FOR_SRC_OR_SOURCE) {
    QueueSelectResourceTask();
  } else if (mLoadWaitStatus == WAITING_FOR_SOURCE) { 
    QueueLoadFromSourceTask();
  }
}

already_AddRefed<nsIURI> nsHTMLMediaElement::GetNextSource()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNode> thisDomNode = do_QueryInterface(this);

  if (!mSourcePointer) {
    // First time this has been run, create a selection to cover children.
    mSourcePointer = do_CreateInstance("@mozilla.org/content/range;1");

    rv = mSourcePointer->SelectNodeContents(thisDomNode);
    if (NS_FAILED(rv)) return nsnull;

    rv = mSourcePointer->Collapse(PR_TRUE);
    if (NS_FAILED(rv)) return nsnull;
  }

  while (PR_TRUE) {
#ifdef DEBUG
    nsCOMPtr<nsIDOMNode> startContainer;
    rv = mSourcePointer->GetStartContainer(getter_AddRefs(startContainer));
    if (NS_FAILED(rv)) return nsnull;
    NS_ASSERTION(startContainer == thisDomNode,
                "Should only iterate over direct children");
#endif

    PRInt32 startOffset = 0;
    rv = mSourcePointer->GetStartOffset(&startOffset);
    NS_ENSURE_SUCCESS(rv, nsnull);

    if (startOffset == GetChildCount())
      return nsnull; // No more children.

    // Advance the range to the next child.
    rv = mSourcePointer->SetStart(thisDomNode, startOffset+1);
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsIContent* child = GetChildAt(startOffset);

    // If child is a <source> element, it may be the next candidate.
    if (child &&
        child->Tag() == nsGkAtoms::source &&
        child->IsHTML())
    {
      nsCOMPtr<nsIURI> uri;
      nsAutoString src,type;

      // Must have src attribute.
      if (!child->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src))
        continue;

      // If we have a type attribute, it must be a supported type.
      if (child->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type) &&
          GetCanPlay(type) == CANPLAY_NO)
        continue;
      
      NewURIFromString(src, getter_AddRefs(uri));
      return uri.forget();
    }
  }
  NS_NOTREACHED("Execution should not reach here!");
  return nsnull;
}

void nsHTMLMediaElement::ChangeDelayLoadStatus(PRBool aDelay) {
  if (mDelayingLoadEvent == aDelay)
    return;

  LOG(PR_LOG_DEBUG, ("ChangeDelayLoadStatus(%d) doc=0x%p", aDelay, mLoadBlockedDoc.get()));
  mDelayingLoadEvent = aDelay;

  if (aDelay) {
    mLoadBlockedDoc = GetOwnerDoc();
    mLoadBlockedDoc->BlockOnload();
  } else {
    if (mDecoder) {
      mDecoder->MoveLoadsToBackground();
    }
    NS_ASSERTION(mLoadBlockedDoc, "Need a doc to block on");
    mLoadBlockedDoc->UnblockOnload(PR_FALSE);
    mLoadBlockedDoc = nsnull;
  }
}

already_AddRefed<nsILoadGroup> nsHTMLMediaElement::GetDocumentLoadGroup()
{
  nsIDocument* doc = GetOwnerDoc();
  return doc ? doc->GetDocumentLoadGroup() : nsnull;
}
