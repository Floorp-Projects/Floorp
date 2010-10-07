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
#include "nsIDOMHTMLSourceElement.h"
#include "nsHTMLMediaElement.h"
#include "nsTimeRanges.h"
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
#include "nsIThreadInternal.h"
#include "nsContentUtils.h"

#include "nsFrameManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsIRenderingContext.h"
#include "nsITimer.h"

#include "nsEventDispatcher.h"
#include "nsIDOMDocumentEvent.h"
#include "nsMediaError.h"
#include "nsICategoryManager.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsMediaStream.h"

#include "nsIDOMHTMLVideoElement.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsCycleCollectionParticipant.h"
#include "nsICachingChannel.h"
#include "nsLayoutUtils.h"
#include "nsVideoFrame.h"
#include "BasicLayers.h"
#include <limits>
#include "nsIDocShellTreeItem.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"

#include "nsIPrivateDOMEvent.h"
#include "nsIDOMNotifyAudioAvailableEvent.h"

#ifdef MOZ_OGG
#include "nsOggDecoder.h"
#endif
#ifdef MOZ_WAVE
#include "nsWaveDecoder.h"
#endif
#ifdef MOZ_WEBM
#include "nsWebMDecoder.h"
#endif
#ifdef MOZ_RAW
#include "nsRawDecoder.h"
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo* gMediaElementLog;
static PRLogModuleInfo* gMediaElementEventsLog;
#define LOG(type, msg) PR_LOG(gMediaElementLog, type, msg)
#define LOG_EVENT(type, msg) PR_LOG(gMediaElementEventsLog, type, msg)
#else
#define LOG(type, msg)
#define LOG_EVENT(type, msg)
#endif

#include "nsIContentSecurityPolicy.h"
#include "nsIChannelPolicy.h"
#include "nsChannelPolicy.h"

using namespace mozilla::layers;

// Under certain conditions there may be no-one holding references to
// a media element from script, DOM parent, etc, but the element may still
// fire meaningful events in the future so we can't destroy it yet:
// 1) If the element is delaying the load event (or would be, if it were
// in a document), then events up to loadeddata or error could be fired,
// so we need to stay alive.
// 2) If the element is not paused and playback has not ended, then
// we will (or might) play, sending timeupdate and ended events and possibly
// audio output, so we need to stay alive.
// 3) if the element is seeking then we will fire seeking events and possibly
// start playing afterward, so we need to stay alive.
// 4) If autoplay could start playback in this element (if we got enough data),
// then we need to stay alive.
// 5) if the element is currently loading and not suspended,
// script might be waiting for progress events or a 'suspend' event,
// so we need to stay alive. If we're already suspended then (all other
// conditions being met) it's OK to just disappear without firing any more
// events, since we have the freedom to remain suspended indefinitely. Note
// that we could use this 'suspended' loophole to garbage-collect a suspended
// element in case 4 even if it had 'autoplay' set, but we choose not to.
// If someone throws away all references to a loading 'autoplay' element
// sound should still eventually play.
//
// Media elements owned by inactive documents (i.e. documents not contained in any
// document viewer) should never hold a self-reference because none of the
// above conditions are allowed: the element will stop loading and playing
// and never resume loading or playing unless its owner document changes to
// an active document (which can only happen if there is an external reference
// to the element).
// Media elements with no owner doc should be able to hold a self-reference.
// Something native must have created the element and may expect it to
// stay alive to play.

// It's very important that any change in state which could change the value of
// needSelfReference in AddRemoveSelfReference be followed by a call to
// AddRemoveSelfReference before this element could die!
// It's especially important if needSelfReference would change to 'true',
// since if we neglect to add a self-reference, this element might be
// garbage collected while there are still event listeners that should
// receive events. If we neglect to remove the self-reference then the element
// just lives longer than it needs to.

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

  nsRefPtr<nsHTMLMediaElement> mElement;
  PRUint32 mLoadID;
};

class nsAsyncEventRunner : public nsMediaEvent
{
private:
  nsString mName;

public:
  nsAsyncEventRunner(const nsAString& aName, nsHTMLMediaElement* aElement) :
    nsMediaEvent(aElement), mName(aName)
  {
  }

  NS_IMETHOD Run()
  {
    // Silently cancel if our load has been cancelled.
    if (IsCancelled())
      return NS_OK;

    return mElement->DispatchEvent(mName);
  }
};

class nsSourceErrorEventRunner : public nsMediaEvent
{
private:
  nsCOMPtr<nsIContent> mSource;
public:
  nsSourceErrorEventRunner(nsHTMLMediaElement* aElement,
                           nsIContent* aSource)
    : nsMediaEvent(aElement),
      mSource(aSource)
  {
  }

  NS_IMETHOD Run() {
    // Silently cancel if our load has been cancelled.
    if (IsCancelled())
      return NS_OK;
    LOG_EVENT(PR_LOG_DEBUG, ("%p Dispatching simple event source error", mElement.get()));
    return nsContentUtils::DispatchTrustedEvent(mElement->GetOwnerDoc(),
                                                mSource,
                                                NS_LITERAL_STRING("error"),
                                                PR_TRUE,
                                                PR_TRUE);
  }
};

/**
 * There is a reference cycle involving this class: MediaLoadListener
 * holds a reference to the nsHTMLMediaElement, which holds a reference
 * to an nsIChannel, which holds a reference to this listener.
 * We break the reference cycle in OnStartRequest by clearing mElement.
 */
class nsHTMLMediaElement::MediaLoadListener : public nsIStreamListener,
                                              public nsIChannelEventSink,
                                              public nsIInterfaceRequestor,
                                              public nsIObserver
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIOBSERVER
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

NS_IMPL_ISUPPORTS5(nsHTMLMediaElement::MediaLoadListener, nsIRequestObserver,
                   nsIStreamListener, nsIChannelEventSink,
                   nsIInterfaceRequestor, nsIObserver)

NS_IMETHODIMP
nsHTMLMediaElement::MediaLoadListener::Observe(nsISupports* aSubject,
                                               const char* aTopic, const PRUnichar* aData)
{
  nsContentUtils::UnregisterShutdownObserver(this);

  // Clear mElement to break cycle so we don't leak on shutdown
  mElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::MediaLoadListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  nsContentUtils::UnregisterShutdownObserver(this);

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

NS_IMETHODIMP nsHTMLMediaElement::MediaLoadListener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                                                            nsIChannel* aNewChannel,
                                                                            PRUint32 aFlags,
                                                                            nsIAsyncVerifyRedirectCallback* cb)
{
  // TODO is this really correct?? See bug #579329.
  if (mElement)
    mElement->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
  nsCOMPtr<nsIChannelEventSink> sink = do_QueryInterface(mNextListener);
  if (sink)
    return sink->AsyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, cb);

  cb->OnRedirectVerifyCallback(NS_OK);
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLoadBlockedDoc)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsHTMLMediaElement)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

// nsIDOMHTMLMediaElement
NS_IMPL_URI_ATTR(nsHTMLMediaElement, Src, src)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Controls, controls)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Autoplay, autoplay)
NS_IMPL_STRING_ATTR(nsHTMLMediaElement, Preload, preload)

/* readonly attribute nsIDOMHTMLMediaElement mozAutoplayEnabled; */
NS_IMETHODIMP nsHTMLMediaElement::GetMozAutoplayEnabled(PRBool *aAutoplayEnabled)
{
  *aAutoplayEnabled = mAutoplayEnabled;

  return NS_OK;
}

/* readonly attribute nsIDOMMediaError error; */
NS_IMETHODIMP nsHTMLMediaElement::GetError(nsIDOMMediaError * *aError)
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
  } else if (mLoadingSrc) {
    mLoadingSrc->GetSpec(src);
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

  // Handle forwarding of Range header so that the intial detection
  // of seeking support (via result code 206) works across redirects.
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aChannel);
  NS_ENSURE_STATE(http);

  NS_NAMED_LITERAL_CSTRING(rangeHdr, "Range");
 
  nsCAutoString rangeVal;
  if (NS_SUCCEEDED(http->GetRequestHeader(rangeHdr, rangeVal))) {
    NS_ENSURE_STATE(!rangeVal.IsEmpty());

    http = do_QueryInterface(aNewChannel);
    NS_ENSURE_STATE(http);
 
    nsresult rv = http->SetRequestHeader(rangeHdr, rangeVal, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }
 
  return NS_OK;
}

void nsHTMLMediaElement::AbortExistingLoads()
{
  // Abort any already-running instance of the resource selection algorithm.
  mLoadWaitStatus = NOT_WAITING;

  // Set a new load ID. This will cause events which were enqueued
  // with a different load ID to silently be cancelled.
  mCurrentLoadID++;

  PRBool fireTimeUpdate = PR_FALSE;
  if (mDecoder) {
    fireTimeUpdate = mDecoder->GetCurrentTime() != 0.0;
    mDecoder->Shutdown();
    mDecoder = nsnull;
  }

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING ||
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_IDLE)
  {
    DispatchEvent(NS_LITERAL_STRING("abort"));
  }

  mError = nsnull;
  mLoadedFirstFrame = PR_FALSE;
  mAutoplaying = PR_TRUE;
  mIsLoadingFromSrcAttribute = PR_FALSE;
  mSuspendedAfterFirstFrame = PR_FALSE;
  mAllowSuspendAfterFirstFrame = PR_TRUE;
  mSourcePointer = nsnull;

  // TODO: The playback rate must be set to the default playback rate.

  if (mNetworkState != nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING);
    mPaused = PR_TRUE;

    if (fireTimeUpdate) {
      // Since we destroyed the decoder above, the current playback position
      // will now be reported as 0. The playback position was non-zero when
      // we destroyed the decoder, so fire a timeupdate event so that the
      // change will be reflected in the controls.
      DispatchAsyncEvent(NS_LITERAL_STRING("timeupdate"));
    }
    DispatchEvent(NS_LITERAL_STRING("emptied"));
  }

  // We may have changed mPaused, mAutoplaying, mNetworkState and other
  // things which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  mIsRunningSelectResource = PR_FALSE;
}

void nsHTMLMediaElement::NoSupportedMediaSourceError()
{
  NS_ASSERTION(mDelayingLoadEvent, "Load event not delayed during source selection?");

  mError = new nsMediaError(nsIDOMMediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
  DispatchAsyncEvent(NS_LITERAL_STRING("error"));
  // This clears mDelayingLoadEvent, so AddRemoveSelfReference will be called
  ChangeDelayLoadStatus(PR_FALSE);
}

typedef void (nsHTMLMediaElement::*SyncSectionFn)();

// Runs a "synchronous section", a function that must run once the event loop
// has reached a "stable state". See:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/webappapis.html#synchronous-section
class nsSyncSection : public nsMediaEvent
{
private:
  SyncSectionFn mClosure;
public:
  nsSyncSection(nsHTMLMediaElement* aElement,
                SyncSectionFn aClosure) :
    nsMediaEvent(aElement),
    mClosure(aClosure)
  {
  }

  NS_IMETHOD Run() {
    // Silently cancel if our load has been cancelled.
    if (IsCancelled())
      return NS_OK;
    (mElement.get()->*mClosure)();
    return NS_OK;
  }
};

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

// Asynchronously awaits a stable state, whereupon aClosure runs on the main
// thread. This adds an event which run aClosure to the appshell's list of 
// sections synchronous the next time control returns to the event loop.
void AsyncAwaitStableState(nsHTMLMediaElement* aElement,
                           SyncSectionFn aClosure)
{
  nsCOMPtr<nsIRunnable> event = new nsSyncSection(aElement, aClosure);
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  appShell->RunInStableState(event);
}

void nsHTMLMediaElement::QueueLoadFromSourceTask()
{
  ChangeDelayLoadStatus(PR_TRUE);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  AsyncAwaitStableState(this, &nsHTMLMediaElement::LoadFromSourceChildren);
}

void nsHTMLMediaElement::QueueSelectResourceTask()
{
  // Don't allow multiple async select resource calls to be queued.
  if (mIsRunningSelectResource)
    return;
  mIsRunningSelectResource = PR_TRUE;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
  AsyncAwaitStableState(this, &nsHTMLMediaElement::SelectResource);
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
  NS_ASSERTION(!mDelayingLoadEvent,
    "Load event should not be delayed at start of resource selection.");
  if (!HasPotentialResource(this)) {
    // The media element has neither a src attribute nor any source
    // element children, abort the load.
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
    // This clears mDelayingLoadEvent, so AddRemoveSelfReference will be called
    ChangeDelayLoadStatus(PR_FALSE);
    mIsRunningSelectResource = PR_FALSE;
    return;
  }

  ChangeDelayLoadStatus(PR_TRUE);

  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  // Load event was delayed, and still is, so no need to call
  // AddRemoveSelfReference, since it must still be held
  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));

  nsAutoString src;
  nsCOMPtr<nsIURI> uri;

  // If we have a 'src' attribute, use that exclusively.
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    nsresult rv = NewURIFromString(src, getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      LOG(PR_LOG_DEBUG, ("%p Trying load from src=%s", this, NS_ConvertUTF16toUTF8(src).get()));
      mIsLoadingFromSrcAttribute = PR_TRUE;
      mLoadingSrc = uri;
      if (mPreloadAction == nsHTMLMediaElement::PRELOAD_NONE) {
        // preload:none media, suspend the load here before we make any
        // network requests.
        SuspendLoad(uri);
        mIsRunningSelectResource = PR_FALSE;
        return;
      }

      rv = LoadResource(uri);
      if (NS_SUCCEEDED(rv)) {
        mIsRunningSelectResource = PR_FALSE;
        return;
      }
    }
    NoSupportedMediaSourceError();
  } else {
    // Otherwise, the source elements will be used.
    LoadFromSourceChildren();
  }
  mIsRunningSelectResource = PR_FALSE;
}

void nsHTMLMediaElement::NotifyLoadError()
{
  if (mIsLoadingFromSrcAttribute) {
    LOG(PR_LOG_DEBUG, ("NotifyLoadError(), no supported media error"));
    NoSupportedMediaSourceError();
  } else {
    NS_ASSERTION(mSourceLoadCandidate, "Must know the source we were loading from!");
    DispatchAsyncSourceError(mSourceLoadCandidate);
    QueueLoadFromSourceTask();
  }
}

void nsHTMLMediaElement::NotifyAudioAvailable(float* aFrameBuffer,
                                              PRUint32 aFrameBufferLength,
                                              float aTime)
{
  // Auto manage the memory for the frame buffer, so that if we add an early
  // return-on-error here in future, we won't forget to release the memory.
  // Otherwise we hand ownership of the memory over to the event created by 
  // DispatchAudioAvailableEvent().
  nsAutoArrayPtr<float> frameBuffer(aFrameBuffer);
  // Do same-origin check on element and media before allowing MozAudioAvailable events.
  if (!mMediaSecurityVerified) {
    nsCOMPtr<nsIPrincipal> principal = GetCurrentPrincipal();
    nsresult rv = NodePrincipal()->Subsumes(principal, &mAllowAudioData);
    if (NS_FAILED(rv)) {
      mAllowAudioData = PR_FALSE;
    }
  }

  DispatchAudioAvailableEvent(frameBuffer.forget(), aFrameBufferLength, aTime);
}

PRBool nsHTMLMediaElement::MayHaveAudioAvailableEventListener()
{
  // Determine if the current element is focused, if it is not focused
  // then we should not try to blur.  Note: we allow for the case of
  // |var a = new Audio()| with no parent document.
  nsIDocument *document = GetDocument();
  if (!document) {
    return PR_TRUE;
  }

  nsPIDOMWindow *window = document->GetInnerWindow();
  if (!window) {
    return PR_TRUE;
  }

  return window->HasAudioAvailableEventListeners();
}

void nsHTMLMediaElement::LoadFromSourceChildren()
{
  NS_ASSERTION(mDelayingLoadEvent,
               "Should delay load event (if in document) during load");
  NS_ASSERTION(!mIsLoadingFromSrcAttribute,
               "Must remember we're loading from source children");
  while (PR_TRUE) {
    nsresult rv;
    nsIContent* child = GetNextSource();
    if (!child) {
      // Exhausted candidates, wait for more candidates to be appended to
      // the media element.
      mLoadWaitStatus = WAITING_FOR_SOURCE;
      mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
      ChangeDelayLoadStatus(PR_FALSE);
      return;
    }

    nsCOMPtr<nsIURI> uri;
    nsAutoString src,type;

    // Must have src attribute.
    if (!child->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
      DispatchAsyncSourceError(child);
      continue;
    }

    // If we have a type attribute, it must be a supported type.
    if (child->HasAttr(kNameSpaceID_None, nsGkAtoms::type) &&
        child->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type) &&
        GetCanPlay(type) == CANPLAY_NO)
    {
      DispatchAsyncSourceError(child);
      continue;
    }
    LOG(PR_LOG_DEBUG, ("%p Trying load from <source>=%s type=%s", this,
      NS_ConvertUTF16toUTF8(src).get(), NS_ConvertUTF16toUTF8(type).get()));
    NewURIFromString(src, getter_AddRefs(uri));
    if (!uri) {
      DispatchAsyncSourceError(child);
      continue;
    }

    mLoadingSrc = uri;
    NS_ASSERTION(mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING,
                 "Network state should be loading");

    if (mPreloadAction == nsHTMLMediaElement::PRELOAD_NONE) {
      // preload:none media, suspend the load here before we make any
      // network requests.
      SuspendLoad(uri);
      return;
    }

    rv = LoadResource(uri);
    if (NS_SUCCEEDED(rv))
      return;

    // If we fail to load, loop back and try loading the next resource.
    DispatchAsyncSourceError(child);
  }
  NS_NOTREACHED("Execution should not reach here!");
}

void nsHTMLMediaElement::SuspendLoad(nsIURI* aURI)
{
  mLoadIsSuspended = PR_TRUE;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
  ChangeDelayLoadStatus(PR_FALSE);
}

void nsHTMLMediaElement::ResumeLoad(PreloadAction aAction)
{
  NS_ASSERTION(mLoadIsSuspended, "Can only resume preload if halted for one");
  nsCOMPtr<nsIURI> uri = mLoadingSrc;
  mLoadIsSuspended = PR_FALSE;
  mPreloadAction = aAction;
  ChangeDelayLoadStatus(PR_TRUE);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  if (mIsLoadingFromSrcAttribute) {
    // We were loading from the element's src attribute.
    if (NS_FAILED(LoadResource(uri))) {
      NoSupportedMediaSourceError();
    }
  } else {
    // We were loading from a child <source> element. Try to resume the
    // load of that child, and if that fails, try the next child.
    if (NS_FAILED(LoadResource(uri))) {
      LoadFromSourceChildren();
    }
  }
}

static PRBool IsAutoplayEnabled()
{
  return nsContentUtils::GetBoolPref("media.autoplay.enabled");
}

void nsHTMLMediaElement::UpdatePreloadAction()
{
  PreloadAction nextAction = PRELOAD_UNDEFINED;
  // If autoplay is set, we should always preload data, as we'll need it
  // to play.
  if (IsAutoplayEnabled() && HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay)) {
    nextAction = nsHTMLMediaElement::PRELOAD_ENOUGH;
  } else {
    // Find the appropriate preload action by looking at the attribute.
    const nsAttrValue* val = mAttrsAndChildren.GetAttr(nsGkAtoms::preload,
                                                       kNameSpaceID_None);
    if (!val) {
      // Attribute is not set. The default is to load metadata.
      nextAction = nsHTMLMediaElement::PRELOAD_METADATA;
    } else if (val->Type() == nsAttrValue::eEnum) {
      PreloadAttrValue attr = static_cast<PreloadAttrValue>(val->GetEnumValue());
      if (attr == nsHTMLMediaElement::PRELOAD_ATTR_EMPTY ||
          attr == nsHTMLMediaElement::PRELOAD_ATTR_AUTO)
      {
        nextAction = nsHTMLMediaElement::PRELOAD_ENOUGH;
      } else if (attr == nsHTMLMediaElement::PRELOAD_ATTR_METADATA) {
        nextAction = nsHTMLMediaElement::PRELOAD_METADATA;
      } else if (attr == nsHTMLMediaElement::PRELOAD_ATTR_NONE) {
        nextAction = nsHTMLMediaElement::PRELOAD_NONE;
      }
    } else {
      // There was a value, but it wasn't an enumerated value.
      // Use the suggested "missing value default" of "metadata".
      nextAction = nsHTMLMediaElement::PRELOAD_METADATA;
    }
  }

  if ((mBegun || mIsRunningSelectResource) && nextAction < mPreloadAction) {
    // We've started a load or are already downloading, and the preload was
    // changed to a state where we buffer less. We don't support this case,
    // so don't change the preload behaviour.
    return;
  }

  mPreloadAction = nextAction;
  if (nextAction == nsHTMLMediaElement::PRELOAD_ENOUGH) {
    if (mLoadIsSuspended) {
      // Our load was previouly suspended due to the media having preload
      // value "none". The preload value has changed to preload:auto, so
      // resume the load.
      ResumeLoad(PRELOAD_ENOUGH);
    } else {
      // Preload as much of the video as we can, i.e. don't suspend after
      // the first frame.
      StopSuspendingAfterFirstFrame();
    }

  } else if (nextAction == nsHTMLMediaElement::PRELOAD_METADATA) {
    // Ensure that the video can be suspended after first frame.
    mAllowSuspendAfterFirstFrame = PR_TRUE;
    if (mLoadIsSuspended) {
      // Our load was previouly suspended due to the media having preload
      // value "none". The preload value has changed to preload:metadata, so
      // resume the load. We'll pause the load again after we've read the
      // metadata.
      ResumeLoad(PRELOAD_METADATA);
    }
  }

  return;
}

nsresult nsHTMLMediaElement::LoadResource(nsIURI* aURI)
{
  NS_ASSERTION(mDelayingLoadEvent,
               "Should delay load event (if in document) during load");
  nsresult rv;

  // If a previous call to mozSetup() was made, kill that media stream
  // in order to use this new src instead.
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
  }

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }

  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_MEDIA,
                                 aURI,
                                 NodePrincipal(),
                                 static_cast<nsGenericElement*>(this),
                                 EmptyCString(), // mime type
                                 nsnull, // extra
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv,rv);
  if (NS_CP_REJECTED(shouldLoad)) return NS_ERROR_FAILURE;

  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();

  // check for a Content Security Policy to pass down to the channel
  // created to load the media content
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv,rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_MEDIA);
  }
  rv = NS_NewChannel(getter_AddRefs(mChannel),
                     aURI,
                     nsnull,
                     loadGroup,
                     nsnull,
                     nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY,
                     channelPolicy);
  NS_ENSURE_SUCCESS(rv,rv);

  // The listener holds a strong reference to us.  This creates a reference
  // cycle which is manually broken in the listener's OnStartRequest method
  // after it is finished with the element. The cycle will also be
  // broken if we get a shutdown notification before OnStartRequest fires.
  // Necko guarantees that OnStartRequest will eventually fire if we
  // don't shut down first.
  nsRefPtr<MediaLoadListener> loadListener = new MediaLoadListener(this);
  if (!loadListener) return NS_ERROR_OUT_OF_MEMORY;

  // loadListener will be unregistered either on shutdown or when
  // OnStartRequest fires.
  nsContentUtils::RegisterShutdownObserver(loadListener);
  mChannel->SetNotificationCallbacks(loadListener);

  nsCOMPtr<nsIStreamListener> listener;
  if (ShouldCheckAllowOrigin()) {
    nsCrossSiteListenerProxy* crossSiteListener =
      new nsCrossSiteListenerProxy(loadListener,
                                   NodePrincipal(),
                                   mChannel,
                                   PR_FALSE,
                                   &rv);
    listener = crossSiteListener;
    NS_ENSURE_TRUE(crossSiteListener, NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_SUCCESS(rv, rv);
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

    SetRequestHeaders(hc);
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
  // a non-catastrophic failure, it will set a new task to continue loading
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

  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));

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

  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));

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

  if (!mDecoder) {
    LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) failed: no decoder", this, aCurrentTime));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) failed: no source", this, aCurrentTime));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // Detect for a NaN and invalid values.
  if (aCurrentTime != aCurrentTime) {
    LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) failed: bad time", this, aCurrentTime));
    return NS_ERROR_FAILURE;
  }

  // Clamp the time to [0, duration] as required by the spec
  float clampedTime = NS_MAX(0.0f, aCurrentTime);
  float duration = mDecoder->GetDuration();
  if (duration >= 0) {
    clampedTime = NS_MIN(clampedTime, duration);
  }

  mPlayingBeforeSeek = IsPotentiallyPlaying();
  // The media backend is responsible for dispatching the timeupdate
  // event if it changes the playback position as a result of the seek.
  LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) starting seek", this, aCurrentTime));
  nsresult rv = mDecoder->Seek(clampedTime);

  // We changed whether we're seeking so we need to AddRemoveSelfReference
  AddRemoveSelfReference();

  return rv;
}

/* readonly attribute float duration; */
NS_IMETHODIMP nsHTMLMediaElement::GetDuration(float *aDuration)
{
  *aDuration = mDecoder ? mDecoder->GetDuration() : std::numeric_limits<float>::quiet_NaN();
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
    LOG(PR_LOG_DEBUG, ("Loading due to Pause()"));
    nsresult rv = Load();
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (mDecoder) {
    mDecoder->Pause();
  }

  PRBool oldPaused = mPaused;
  mPaused = PR_TRUE;
  mAutoplaying = PR_FALSE;
  // We changed mPaused and mAutoplaying which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  if (!oldPaused) {
    DispatchAsyncEvent(NS_LITERAL_STRING("timeupdate"));
    DispatchAsyncEvent(NS_LITERAL_STRING("pause"));
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

  if (mDecoder && !mMuted) {
    mDecoder->SetVolume(mVolume);
  } else if (mAudioStream && !mMuted) {
    mAudioStream->SetVolume(mVolume);
  }

  DispatchAsyncEvent(NS_LITERAL_STRING("volumechange"));

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMediaElement::GetMozChannels(PRUint32 *aMozChannels)
{
  if (!mDecoder && !mAudioStream) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  *aMozChannels = mChannels;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMediaElement::GetMozSampleRate(PRUint32 *aMozSampleRate)
{
  if (!mDecoder && !mAudioStream) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  *aMozSampleRate = mRate;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMediaElement::GetMozFrameBufferLength(PRUint32 *aMozFrameBufferLength)
{
  // The framebuffer (via MozAudioAvailable events) is only available
  // when reading vs. writing audio directly.
  if (!mDecoder) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  *aMozFrameBufferLength = mDecoder->GetFrameBufferLength();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMediaElement::SetMozFrameBufferLength(PRUint32 aMozFrameBufferLength)
{
  if (!mDecoder)
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  return mDecoder->RequestFrameBufferLength(aMozFrameBufferLength);
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
  } else if (mAudioStream) {
    mAudioStream->SetVolume(mMuted ? 0.0 : mVolume);
  }

  DispatchAsyncEvent(NS_LITERAL_STRING("volumechange"));

  return NS_OK;
}

nsHTMLMediaElement::nsHTMLMediaElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                       PRUint32 aFromParser)
  : nsGenericHTMLElement(aNodeInfo),
    mCurrentLoadID(0),
    mNetworkState(nsIDOMHTMLMediaElement::NETWORK_EMPTY),
    mReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING),
    mLoadWaitStatus(NOT_WAITING),
    mVolume(1.0),
    mChannels(0),
    mRate(0),
    mPreloadAction(PRELOAD_UNDEFINED),
    mMediaSize(-1,-1),
    mAllowAudioData(PR_FALSE),
    mBegun(PR_FALSE),
    mLoadedFirstFrame(PR_FALSE),
    mAutoplaying(PR_TRUE),
    mAutoplayEnabled(PR_TRUE),
    mPaused(PR_TRUE),
    mMuted(PR_FALSE),
    mPlayingBeforeSeek(PR_FALSE),
    mPausedForInactiveDocument(PR_FALSE),
    mWaitingFired(PR_FALSE),
    mIsBindingToTree(PR_FALSE),
    mIsRunningLoadMethod(PR_FALSE),
    mIsLoadingFromSrcAttribute(PR_FALSE),
    mDelayingLoadEvent(PR_FALSE),
    mIsRunningSelectResource(PR_FALSE),
    mSuspendedAfterFirstFrame(PR_FALSE),
    mAllowSuspendAfterFirstFrame(PR_TRUE),
    mHasPlayedOrSeeked(PR_FALSE),
    mHasSelfReference(PR_FALSE),
    mShuttingDown(PR_FALSE),
    mLoadIsSuspended(PR_FALSE),
    mMediaSecurityVerified(PR_FALSE)
{
#ifdef PR_LOGGING
  if (!gMediaElementLog) {
    gMediaElementLog = PR_NewLogModule("nsMediaElement");
  }
  if (!gMediaElementEventsLog) {
    gMediaElementEventsLog = PR_NewLogModule("nsMediaElementEvents");
  }
#endif

  RegisterFreezableElement();
  NotifyOwnerDocumentActivityChanged();
}

nsHTMLMediaElement::~nsHTMLMediaElement()
{
  NS_ASSERTION(!mHasSelfReference,
               "How can we be destroyed if we're still holding a self reference?");

  UnregisterFreezableElement();
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nsnull;
  }
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
  }
}

void nsHTMLMediaElement::StopSuspendingAfterFirstFrame()
{
  mAllowSuspendAfterFirstFrame = PR_FALSE;
  if (!mSuspendedAfterFirstFrame)
    return;
  mSuspendedAfterFirstFrame = PR_FALSE;
  if (mDecoder) {
    mDecoder->Resume(PR_TRUE);
  }
}

void nsHTMLMediaElement::SetPlayedOrSeeked(PRBool aValue)
{
  if (aValue == mHasPlayedOrSeeked)
    return;

  mHasPlayedOrSeeked = aValue;

  // Force a reflow so that the poster frame hides or shows immediately.
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return;
  frame->PresContext()->PresShell()->FrameNeedsReflow(frame,
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
  }  else if (mLoadIsSuspended) {
    ResumeLoad(PRELOAD_ENOUGH);
  } else if (mDecoder) {
    if (mDecoder->IsEnded()) {
      SetCurrentTime(0);
    }
    if (!mPausedForInactiveDocument) {
      nsresult rv = mDecoder->Play();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // TODO: If the playback has ended, then the user agent must set
  // seek to the effective start.
  // TODO: The playback rate must be set to the default playback rate.
  if (mPaused) {
    DispatchAsyncEvent(NS_LITERAL_STRING("play"));
    switch (mReadyState) {
    case nsIDOMHTMLMediaElement::HAVE_METADATA:
    case nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA:
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      break;
    case nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA:
    case nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA:
      DispatchAsyncEvent(NS_LITERAL_STRING("playing"));
      break;
    }
  }

  mPaused = PR_FALSE;
  mAutoplaying = PR_FALSE;
  // We changed mPaused and mAutoplaying which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  return NS_OK;
}

PRBool nsHTMLMediaElement::ParseAttribute(PRInt32 aNamespaceID,
                                          nsIAtom* aAttribute,
                                          const nsAString& aValue,
                                          nsAttrValue& aResult)
{
  // Mappings from 'preload' attribute strings to an enumeration.
  static const nsAttrValue::EnumTable kPreloadTable[] = {
    { "",         nsHTMLMediaElement::PRELOAD_ATTR_EMPTY },
    { "none",     nsHTMLMediaElement::PRELOAD_ATTR_NONE },
    { "metadata", nsHTMLMediaElement::PRELOAD_ATTR_METADATA },
    { "auto",     nsHTMLMediaElement::PRELOAD_ATTR_AUTO },
    { 0 }
  };

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
    else if (aAttribute == nsGkAtoms::preload) {
      return aResult.ParseEnumValue(aValue, kPreloadTable, PR_FALSE);
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
  if (NS_FAILED(rv))
    return rv;
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::src) {
    Load();
  }
  if (aNotify && aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::autoplay) {
      StopSuspendingAfterFirstFrame();
      if (mReadyState == nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) {
        NotifyAutoplayDataReady();
      }
      // This attribute can affect AddRemoveSelfReference
      AddRemoveSelfReference();
      UpdatePreloadAction();
    } else if (aName == nsGkAtoms::preload) {
      UpdatePreloadAction();
    }
  }

  return rv;
}

nsresult nsHTMLMediaElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr,
                                       PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttr, aNotify);
  if (NS_FAILED(rv))
    return rv;
  if (aNotify && aNameSpaceID == kNameSpaceID_None) {
    if (aAttr == nsGkAtoms::autoplay) {
      // This attribute can affect AddRemoveSelfReference
      AddRemoveSelfReference();
      UpdatePreloadAction();
    } else if (aAttr == nsGkAtoms::preload) {
      UpdatePreloadAction();
    }
  }

  return rv;
}

nsresult nsHTMLMediaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                        nsIContent* aBindingParent,
                                        PRBool aCompileEventHandlers)
{
  if (aDocument) {
    mIsBindingToTree = PR_TRUE;
    mAutoplayEnabled =
      IsAutoplayEnabled() && (!aDocument || !aDocument->IsStaticDocument());
    // The preload action depends on the value of the autoplay attribute.
    // It's value may have changed, so update it.
    UpdatePreloadAction();
  }
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument,
                                                 aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
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

#ifdef MOZ_RAW
static const char gRawTypes[][16] = {
  "video/x-raw",
  "video/x-raw-yuv"
};

static const char* gRawCodecs[] = {
  nsnull
};

static PRBool IsRawEnabled()
{
  return nsContentUtils::GetBoolPref("media.raw.enabled");
}

static PRBool IsRawType(const nsACString& aType)
{
  if (!IsRawEnabled())
    return PR_FALSE;
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gRawTypes); ++i) {
    if (aType.EqualsASCII(gRawTypes[i]))
      return PR_TRUE;
  }
  return PR_FALSE;
}
#endif
#ifdef MOZ_OGG
// See http://www.rfc-editor.org/rfc/rfc5334.txt for the definitions
// of Ogg media types and codec types
const char nsHTMLMediaElement::gOggTypes[3][16] = {
  "video/ogg",
  "audio/ogg",
  "application/ogg"
};

char const *const nsHTMLMediaElement::gOggCodecs[3] = {
  "vorbis",
  "theora",
  nsnull
};

bool
nsHTMLMediaElement::IsOggEnabled()
{
  return nsContentUtils::GetBoolPref("media.ogg.enabled");
}

bool
nsHTMLMediaElement::IsOggType(const nsACString& aType)
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
const char nsHTMLMediaElement::gWaveTypes[4][16] = {
  "audio/x-wav",
  "audio/wav",
  "audio/wave",
  "audio/x-pn-wav"
};

char const *const nsHTMLMediaElement::gWaveCodecs[2] = {
  "1", // Microsoft PCM Format
  nsnull
};

bool
nsHTMLMediaElement::IsWaveEnabled()
{
  return nsContentUtils::GetBoolPref("media.wave.enabled");
}

bool
nsHTMLMediaElement::IsWaveType(const nsACString& aType)
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

#ifdef MOZ_WEBM
const char nsHTMLMediaElement::gWebMTypes[2][17] = {
  "video/webm",
  "audio/webm"
};

char const *const nsHTMLMediaElement::gWebMCodecs[4] = {
  "vp8",
  "vp8.0",
  "vorbis",
  nsnull
};

bool
nsHTMLMediaElement::IsWebMEnabled()
{
  return nsContentUtils::GetBoolPref("media.webm.enabled");
}

bool
nsHTMLMediaElement::IsWebMType(const nsACString& aType)
{
  if (!IsWebMEnabled())
    return PR_FALSE;
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gWebMTypes); ++i) {
    if (aType.EqualsASCII(gWebMTypes[i]))
      return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

/* static */
nsHTMLMediaElement::CanPlayStatus 
nsHTMLMediaElement::CanHandleMediaType(const char* aMIMEType,
                                       char const *const ** aCodecList)
{
#ifdef MOZ_RAW
  if (IsRawType(nsDependentCString(aMIMEType))) {
    *aCodecList = gRawCodecs;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType))) {
    *aCodecList = gOggCodecs;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType))) {
    *aCodecList = gWaveCodecs;
    return CANPLAY_MAYBE;
  }
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(nsDependentCString(aMIMEType))) {
    *aCodecList = gWebMCodecs;
    return CANPLAY_YES;
  }
#endif
  return CANPLAY_NO;
}

/* static */
PRBool nsHTMLMediaElement::ShouldHandleMediaType(const char* aMIMEType)
{
#ifdef MOZ_RAW
  if (IsRawType(nsDependentCString(aMIMEType)))
    return PR_TRUE;
#endif
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType)))
    return PR_TRUE;
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(nsDependentCString(aMIMEType)))
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
CodecListContains(char const *const * aCodecs, const nsAString& aCodec)
{
  for (PRInt32 i = 0; aCodecs[i]; ++i) {
    if (aCodec.EqualsASCII(aCodecs[i]))
      return PR_TRUE;
  }
  return PR_FALSE;
}

/* static */
nsHTMLMediaElement::CanPlayStatus
nsHTMLMediaElement::GetCanPlay(const nsAString& aType)
{
  nsContentTypeParser parser(aType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv))
    return CANPLAY_NO;

  NS_ConvertUTF16toUTF8 mimeTypeUTF8(mimeType);
  char const *const * supportedCodecs;
  CanPlayStatus status = CanHandleMediaType(mimeTypeUTF8.get(),
                                            &supportedCodecs);
  if (status == CANPLAY_NO)
    return CANPLAY_NO;

  nsAutoString codecs;
  rv = parser.GetParameter("codecs", codecs);
  if (NS_FAILED(rv)) {
    // Parameter not found or whatever
    return status;
  }

  CanPlayStatus result = CANPLAY_YES;
  // See http://www.rfc-editor.org/rfc/rfc4281.txt for the description
  // of the 'codecs' parameter
  nsCharSeparatedTokenizer tokenizer(codecs, ',');
  PRBool expectMoreTokens = PR_FALSE;
  while (tokenizer.hasMoreTokens()) {
    const nsSubstring& token = tokenizer.nextToken();

    if (!CodecListContains(supportedCodecs, token)) {
      // Totally unsupported codec
      return CANPLAY_NO;
    }
    expectMoreTokens = tokenizer.lastTokenEndedWithSeparator();
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

already_AddRefed<nsMediaDecoder>
nsHTMLMediaElement::CreateDecoder(const nsACString& aType)
{
#ifdef MOZ_RAW
  if (IsRawType(aType)) {
    nsRefPtr<nsRawDecoder> decoder = new nsRawDecoder();
    if (decoder && decoder->Init(this)) {
      return decoder.forget().get();
    }
  }
#endif
#ifdef MOZ_OGG
  if (IsOggType(aType)) {
    nsRefPtr<nsOggDecoder> decoder = new nsOggDecoder();
    if (decoder && decoder->Init(this)) {
      return decoder.forget().get();
    }
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    nsRefPtr<nsWaveDecoder> decoder = new nsWaveDecoder();
    if (decoder && decoder->Init(this)) {
      return decoder.forget().get();
    }
  }
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(aType)) {
    nsRefPtr<nsWebMDecoder> decoder = new nsWebMDecoder();
    if (decoder && decoder->Init(this)) {
      return decoder.forget().get();
    }
  }
#endif
  return nsnull;
}

nsresult nsHTMLMediaElement::InitializeDecoderAsClone(nsMediaDecoder* aOriginal)
{
  nsMediaStream* originalStream = aOriginal->GetCurrentStream();
  if (!originalStream)
    return NS_ERROR_FAILURE;
  nsRefPtr<nsMediaDecoder> decoder = aOriginal->Clone();
  if (!decoder)
    return NS_ERROR_FAILURE;

  LOG(PR_LOG_DEBUG, ("%p Cloned decoder %p from %p", this, decoder.get(), aOriginal));

  if (!decoder->Init(this)) {
    return NS_ERROR_FAILURE;
  }

  float duration = aOriginal->GetDuration();
  if (duration >= 0) {
    decoder->SetDuration(PRInt64(NS_round(duration * 1000)));
    decoder->SetSeekable(aOriginal->GetSeekable());
  }

  nsMediaStream* stream = originalStream->CloneData(decoder);
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;

  nsresult rv = decoder->Load(stream, nsnull, aOriginal);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return FinishDecoderSetup(decoder);
}

nsresult nsHTMLMediaElement::InitializeDecoderForChannel(nsIChannel *aChannel,
                                                         nsIStreamListener **aListener)
{
  nsCAutoString mimeType;
  aChannel->GetContentType(mimeType);

  nsRefPtr<nsMediaDecoder> decoder = CreateDecoder(mimeType);
  if (!decoder) {
    return NS_ERROR_FAILURE;
  }

  LOG(PR_LOG_DEBUG, ("%p Created decoder %p for type %s", this, decoder.get(), mimeType.get()));

  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;

  nsMediaStream* stream = nsMediaStream::Create(decoder, aChannel);
  if (!stream)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = decoder->Load(stream, aListener, nsnull);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Decoder successfully created, the decoder now owns the nsMediaStream
  // which owns the channel.
  mChannel = nsnull;

  return FinishDecoderSetup(decoder);
}

nsresult nsHTMLMediaElement::FinishDecoderSetup(nsMediaDecoder* aDecoder)
{
  mDecoder = aDecoder;

  // Decoder has assumed ownership responsibility for remembering the URI.
  mLoadingSrc = nsnull;

  // Force a same-origin check before allowing events for this media resource.
  mMediaSecurityVerified = PR_FALSE;

  // The new stream has not been suspended by us.
  mPausedForInactiveDocument = PR_FALSE;
  // But we may want to suspend it now.
  // This will also do an AddRemoveSelfReference.
  NotifyOwnerDocumentActivityChanged();

  nsresult rv = NS_OK;

  mDecoder->SetVolume(mMuted ? 0.0 : mVolume);

  if (!mPaused) {
    SetPlayedOrSeeked(PR_TRUE);
    if (!mPausedForInactiveDocument) {
      rv = mDecoder->Play();
    }
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

void nsHTMLMediaElement::MetadataLoaded(PRUint32 aChannels, PRUint32 aRate)
{
  mChannels = aChannels;
  mRate = aRate;
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
  DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  DispatchAsyncEvent(NS_LITERAL_STRING("loadedmetadata"));
}

void nsHTMLMediaElement::FirstFrameLoaded(PRBool aResourceFullyLoaded)
{
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
  ChangeDelayLoadStatus(PR_FALSE);

  NS_ASSERTION(!mSuspendedAfterFirstFrame, "Should not have already suspended");

  if (mDecoder && mAllowSuspendAfterFirstFrame && mPaused &&
      !aResourceFullyLoaded &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
      mPreloadAction == nsHTMLMediaElement::PRELOAD_METADATA) {
    mSuspendedAfterFirstFrame = PR_TRUE;
    mDecoder->Suspend();
  }
}

void nsHTMLMediaElement::ResourceLoaded()
{
  mBegun = PR_FALSE;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  AddRemoveSelfReference();
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
  // Ensure a progress event is dispatched at the end of download.
  DispatchAsyncEvent(NS_LITERAL_STRING("progress"));
  // The download has stopped.
  DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
}

void nsHTMLMediaElement::NetworkError()
{
  Error(nsIDOMMediaError::MEDIA_ERR_NETWORK);
}

void nsHTMLMediaElement::DecodeError()
{
  if (!mIsLoadingFromSrcAttribute) {
    NS_ASSERTION(mSourceLoadCandidate, "Must know the source we were loading from!");
    if (mDecoder) {
      mDecoder->Shutdown();
      mDecoder = nsnull;
    }
    mError = nsnull;
    DispatchAsyncSourceError(mSourceLoadCandidate);
    QueueLoadFromSourceTask();
  } else {
    Error(nsIDOMMediaError::MEDIA_ERR_DECODE);
  }
}

void nsHTMLMediaElement::LoadAborted()
{
  Error(nsIDOMMediaError::MEDIA_ERR_ABORTED);
}

void nsHTMLMediaElement::Error(PRUint16 aErrorCode)
{
  NS_ASSERTION(aErrorCode == nsIDOMMediaError::MEDIA_ERR_DECODE ||
               aErrorCode == nsIDOMMediaError::MEDIA_ERR_NETWORK ||
               aErrorCode == nsIDOMMediaError::MEDIA_ERR_ABORTED,
               "Only use nsIDOMMediaError codes!");
  mError = new nsMediaError(aErrorCode);
  mBegun = PR_FALSE;
  DispatchAsyncEvent(NS_LITERAL_STRING("error"));
  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
    DispatchAsyncEvent(NS_LITERAL_STRING("emptied"));
  } else {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  }
  AddRemoveSelfReference();
  ChangeDelayLoadStatus(PR_FALSE);
}

void nsHTMLMediaElement::PlaybackEnded()
{
  NS_ASSERTION(mDecoder->IsEnded(), "Decoder fired ended, but not in ended state");
  // We changed the state of IsPlaybackEnded which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  DispatchAsyncEvent(NS_LITERAL_STRING("ended"));
}

void nsHTMLMediaElement::SeekStarted()
{
  DispatchAsyncEvent(NS_LITERAL_STRING("seeking"));
  DispatchAsyncEvent(NS_LITERAL_STRING("timeupdate"));
}

void nsHTMLMediaElement::SeekCompleted()
{
  mPlayingBeforeSeek = PR_FALSE;
  SetPlayedOrSeeked(PR_TRUE);
  DispatchAsyncEvent(NS_LITERAL_STRING("seeked"));
  // We changed whether we're seeking so we need to AddRemoveSelfReference
  AddRemoveSelfReference();
}

void nsHTMLMediaElement::DownloadSuspended()
{
  if (mBegun) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
    AddRemoveSelfReference();
    DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
  }
}

void nsHTMLMediaElement::DownloadResumed()
{
  if (mBegun) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
    AddRemoveSelfReference();
  }
}

void nsHTMLMediaElement::DownloadStalled()
{
  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING) {
    DispatchAsyncEvent(NS_LITERAL_STRING("stalled"));
  }
}

PRBool nsHTMLMediaElement::ShouldCheckAllowOrigin()
{
  return nsContentUtils::GetBoolPref("media.enforce_same_site_origin",
                                     PR_TRUE);
}

void nsHTMLMediaElement::UpdateReadyStateForData(NextFrameStatus aNextFrame)
{
  if (mReadyState < nsIDOMHTMLMediaElement::HAVE_METADATA) {
    // aNextFrame might have a next frame because the decoder can advance
    // on its own thread before ResourceLoaded or MetadataLoaded gets
    // a chance to run.
    // The arrival of more data can't change us out of this readyState.
    return;
  }

  if (aNextFrame != NEXT_FRAME_AVAILABLE) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
    if (!mWaitingFired && aNextFrame == NEXT_FRAME_UNAVAILABLE_BUFFERING) {
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      mWaitingFired = PR_TRUE;
    }
    return;
  }

  // Now see if we should set HAVE_ENOUGH_DATA.
  // If it's something we don't know the size of, then we can't
  // make a real estimate, so we go straight to HAVE_ENOUGH_DATA once
  // we've downloaded enough data that our download rate is considered
  // reliable. We have to move to HAVE_ENOUGH_DATA at some point or
  // autoplay elements for live streams will never play. Otherwise we
  // move to HAVE_ENOUGH_DATA if we can play through the entire media
  // without stopping to buffer.
  nsMediaDecoder::Statistics stats = mDecoder->GetStatistics();
  if (stats.mTotalBytes < 0 ? stats.mDownloadRateReliable :
                              stats.mTotalBytes == stats.mDownloadPosition ||
      mDecoder->CanPlayThrough())
  {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
    return;
  }
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA);
}

#ifdef PR_LOGGING
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

  LOG(PR_LOG_DEBUG, ("%p Ready state changed to %s", this, gReadyStateToString[aState]));

  // Handle raising of "waiting" event during seek (see 4.8.10.9)
  if (mPlayingBeforeSeek &&
      oldState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA) {
    DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA &&
      !mLoadedFirstFrame)
  {
    DispatchAsyncEvent(NS_LITERAL_STRING("loadeddata"));
    mLoadedFirstFrame = PR_TRUE;
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA) {
    mWaitingFired = PR_FALSE;
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA) {
    DispatchAsyncEvent(NS_LITERAL_STRING("canplay"));
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) {
    NotifyAutoplayDataReady();
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA &&
      IsPotentiallyPlaying()) {
    DispatchAsyncEvent(NS_LITERAL_STRING("playing"));
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) {
    DispatchAsyncEvent(NS_LITERAL_STRING("canplaythrough"));
  }
}

PRBool nsHTMLMediaElement::CanActivateAutoplay()
{
  return mAutoplaying &&
         mPaused &&
         HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
         mAutoplayEnabled;
}

void nsHTMLMediaElement::NotifyAutoplayDataReady()
{
  if (CanActivateAutoplay()) {
    mPaused = PR_FALSE;
    // We changed mPaused which can affect AddRemoveSelfReference
    AddRemoveSelfReference();

    if (mDecoder) {
      SetPlayedOrSeeked(PR_TRUE);
      mDecoder->Play();
    }
    DispatchAsyncEvent(NS_LITERAL_STRING("play"));
  }
}

ImageContainer* nsHTMLMediaElement::GetImageContainer()
{
  if (mImageContainer)
    return mImageContainer;

  // If we have a print surface, this is just a static image so
  // no image container is required
  if (mPrintSurface)
    return nsnull;

  // Only video frames need an image container.
  nsCOMPtr<nsIDOMHTMLVideoElement> video =
    do_QueryInterface(static_cast<nsIContent*>(this));
  if (!video)
    return nsnull;

  nsRefPtr<LayerManager> manager = nsContentUtils::LayerManagerForDocument(GetOwnerDoc());
  if (!manager)
    return nsnull;

  mImageContainer = manager->CreateImageContainer();
  return mImageContainer;
}

nsresult nsHTMLMediaElement::DispatchAudioAvailableEvent(float* aFrameBuffer,
                                                         PRUint32 aFrameBufferLength,
                                                         float aTime)
{
  // Auto manage the memory for the frame buffer. If we fail and return
  // an error, this ensures we free the memory in the frame buffer. Otherwise
  // we hand off ownership of the frame buffer to the audioavailable event,
  // which frees the memory when it's destroyed.
  nsAutoArrayPtr<float> frameBuffer(aFrameBuffer);

  nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(GetOwnerDoc()));
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(static_cast<nsIContent*>(this)));
  NS_ENSURE_TRUE(docEvent && target, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = docEvent->CreateEvent(NS_LITERAL_STRING("MozAudioAvailableEvent"),
                                      getter_AddRefs(event));
  nsCOMPtr<nsIDOMNotifyAudioAvailableEvent> audioavailableEvent(do_QueryInterface(event));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = audioavailableEvent->InitAudioAvailableEvent(NS_LITERAL_STRING("MozAudioAvailable"),
                                                    PR_TRUE, PR_TRUE, frameBuffer.forget(), aFrameBufferLength,
                                                    aTime, mAllowAudioData);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool dummy;
  return target->DispatchEvent(event, &dummy);
}

nsresult nsHTMLMediaElement::DispatchEvent(const nsAString& aName)
{
  LOG_EVENT(PR_LOG_DEBUG, ("%p Dispatching event %s", this,
                          NS_ConvertUTF16toUTF8(aName).get()));

  // Save events that occur while in the bfcache. These will be dispatched
  // if the page comes out of the bfcache.
  if (mPausedForInactiveDocument) {
    mPendingEvents.AppendElement(aName);
    return NS_OK;
  }

  return nsContentUtils::DispatchTrustedEvent(GetOwnerDoc(),
                                              static_cast<nsIContent*>(this),
                                              aName,
                                              PR_TRUE,
                                              PR_TRUE);
}

nsresult nsHTMLMediaElement::DispatchAsyncEvent(const nsAString& aName)
{
  LOG_EVENT(PR_LOG_DEBUG, ("%p Queuing event %s", this,
            NS_ConvertUTF16toUTF8(aName).get()));

  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult nsHTMLMediaElement::DispatchPendingMediaEvents()
{
  NS_ASSERTION(!mPausedForInactiveDocument,
               "Must not be in bfcache when dispatching pending media events");

  PRUint32 count = mPendingEvents.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    DispatchAsyncEvent(mPendingEvents[i]);
  }
  mPendingEvents.Clear();

  return NS_OK;
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

void nsHTMLMediaElement::NotifyOwnerDocumentActivityChanged()
{
  nsIDocument* ownerDoc = GetOwnerDoc();
  // Don't pause if we have no ownerDoc. Something native must have created
  // us and be expecting us to work without a document.
  PRBool pauseForInactiveDocument =
    ownerDoc && (!ownerDoc->IsActive() || !ownerDoc->IsVisible());

  if (pauseForInactiveDocument != mPausedForInactiveDocument) {
    mPausedForInactiveDocument = pauseForInactiveDocument;
    if (mDecoder) {
      if (pauseForInactiveDocument) {
        mDecoder->Pause();
        mDecoder->Suspend();
      } else {
        mDecoder->Resume(PR_FALSE);
        DispatchPendingMediaEvents();
        if (!mPaused && !mDecoder->IsEnded()) {
          mDecoder->Play();
        }
      }
    }
  }

  AddRemoveSelfReference();
}

void nsHTMLMediaElement::AddRemoveSelfReference()
{
  // XXX we could release earlier here in many situations if we examined
  // which event listeners are attached. Right now we assume there is a
  // potential listener for every event. We would also have to keep the
  // element alive if it was playing and producing audio output --- right now
  // that's covered by the !mPaused check.
  nsIDocument* ownerDoc = GetOwnerDoc();

  // See the comment at the top of this file for the explanation of this
  // boolean expression.
  PRBool needSelfReference = !mShuttingDown &&
    (!ownerDoc || ownerDoc->IsActive()) &&
    (mDelayingLoadEvent ||
     (!mPaused && mDecoder && !mDecoder->IsEnded()) ||
     (mDecoder && mDecoder->IsSeeking()) ||
     CanActivateAutoplay() ||
     mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING);

  if (needSelfReference != mHasSelfReference) {
    mHasSelfReference = needSelfReference;
    if (needSelfReference) {
      // The observer service will hold a strong reference to us. This
      // will do to keep us alive. We need to know about shutdown so that
      // we can release our self-reference.
      nsContentUtils::RegisterShutdownObserver(this);
    } else {
      // Dispatch Release asynchronously so that we don't destroy this object
      // inside a call stack of method calls on this object
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(this, &nsHTMLMediaElement::DoRemoveSelfReference);
      NS_DispatchToMainThread(event);
    }
  }
}

void nsHTMLMediaElement::DoRemoveSelfReference()
{
  // We don't need the shutdown observer anymore. Unregistering releases
  // its reference to us, which we were using as our self-reference.
  nsContentUtils::UnregisterShutdownObserver(this);
}

nsresult nsHTMLMediaElement::Observe(nsISupports* aSubject,
                                     const char* aTopic, const PRUnichar* aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    mShuttingDown = PR_TRUE;
    AddRemoveSelfReference();
  }
  return NS_OK;
}

PRBool
nsHTMLMediaElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eMEDIA));
}

void nsHTMLMediaElement::DispatchAsyncSourceError(nsIContent* aSourceElement)
{
  LOG_EVENT(PR_LOG_DEBUG, ("%p Queuing simple source error event", this));

  nsCOMPtr<nsIRunnable> event = new nsSourceErrorEventRunner(this, aSourceElement);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

void nsHTMLMediaElement::NotifyAddedSource()
{
  // If a source element is inserted as a child of a media element
  // that has no src attribute and whose networkState has the value
  // NETWORK_EMPTY, the user agent must invoke the media element's
  // resource selection algorithm.
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::src) &&
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY)
  {
    QueueSelectResourceTask();
  }

  // A load was paused in the resource selection algorithm, waiting for
  // a new source child to be added, resume the resource selction algorithm.
  if (mLoadWaitStatus == WAITING_FOR_SOURCE) {
    QueueLoadFromSourceTask();
  }
}

nsIContent* nsHTMLMediaElement::GetNextSource()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNode> thisDomNode =
    do_QueryInterface(static_cast<nsGenericElement*>(this));

  mSourceLoadCandidate = nsnull;

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

    if (PRUint32(startOffset) == GetChildCount())
      return nsnull; // No more children.

    // Advance the range to the next child.
    rv = mSourcePointer->SetStart(thisDomNode, startOffset+1);
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsIContent* child = GetChildAt(startOffset);

    // If child is a <source> element, it is the next candidate.
    if (child &&
        child->Tag() == nsGkAtoms::source &&
        child->IsHTML())
    {
      mSourceLoadCandidate = child;
      return child;
    }
  }
  NS_NOTREACHED("Execution should not reach here!");
  return nsnull;
}

void nsHTMLMediaElement::ChangeDelayLoadStatus(PRBool aDelay) {
  if (mDelayingLoadEvent == aDelay)
    return;

  mDelayingLoadEvent = aDelay;

  if (aDelay) {
    mLoadBlockedDoc = GetOwnerDoc();
    mLoadBlockedDoc->BlockOnload();
    LOG(PR_LOG_DEBUG, ("%p ChangeDelayLoadStatus(%d) doc=0x%p", this, aDelay, mLoadBlockedDoc.get()));
  } else {
    if (mDecoder) {
      mDecoder->MoveLoadsToBackground();
    }
    LOG(PR_LOG_DEBUG, ("%p ChangeDelayLoadStatus(%d) doc=0x%p", this, aDelay, mLoadBlockedDoc.get()));
    // mLoadBlockedDoc might be null due to GC unlinking
    if (mLoadBlockedDoc) {
      mLoadBlockedDoc->UnblockOnload(PR_FALSE);
      mLoadBlockedDoc = nsnull;
    }
  }

  // We changed mDelayingLoadEvent which can affect AddRemoveSelfReference
  AddRemoveSelfReference();
}

already_AddRefed<nsILoadGroup> nsHTMLMediaElement::GetDocumentLoadGroup()
{
  nsIDocument* doc = GetOwnerDoc();
  return doc ? doc->GetDocumentLoadGroup() : nsnull;
}

nsresult
nsHTMLMediaElement::CopyInnerTo(nsGenericElement* aDest) const
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDest->GetOwnerDoc()->IsStaticDocument()) {
    nsHTMLMediaElement* dest = static_cast<nsHTMLMediaElement*>(aDest);
    if (mPrintSurface) {
      dest->mPrintSurface = mPrintSurface;
      dest->mMediaSize = mMediaSize;
    } else {
      nsIFrame* frame = GetPrimaryFrame();
      nsCOMPtr<nsIDOMElement> elem;
      if (frame && frame->GetType() == nsGkAtoms::HTMLVideoFrame &&
          static_cast<nsVideoFrame*>(frame)->ShouldDisplayPoster()) {
        elem = do_QueryInterface(static_cast<nsVideoFrame*>(frame)->
                                 GetPosterImage());
      } else {
        elem = do_QueryInterface(
          static_cast<nsGenericElement*>(const_cast<nsHTMLMediaElement*>(this)));
      }

      nsLayoutUtils::SurfaceFromElementResult res =
        nsLayoutUtils::SurfaceFromElement(elem,
                                          nsLayoutUtils::SFE_WANT_NEW_SURFACE);
      dest->mPrintSurface = res.mSurface;
      dest->mMediaSize = nsIntSize(res.mSize.width, res.mSize.height);
    }
  }
  return rv;
}

nsresult nsHTMLMediaElement::GetBuffered(nsIDOMTimeRanges** aBuffered)
{
  nsTimeRanges* ranges = new nsTimeRanges();
  NS_ADDREF(*aBuffered = ranges);
  if (mReadyState >= nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA && mDecoder) {
    // If GetBuffered fails we ignore the error result and just return the
    // time ranges we found up till the error.
    mDecoder->GetBuffered(ranges);
  }
  return NS_OK;
}

void nsHTMLMediaElement::SetRequestHeaders(nsIHttpChannel* aChannel)
{
  // Send Accept header for video and audio types only (Bug 489071)
  SetAcceptHeader(aChannel);

  // Set the Referer header
  nsIDocument* doc = GetOwnerDoc();
  if (doc) {
    aChannel->SetReferrer(doc->GetDocumentURI());
  }
}
