/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

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
#include "nsNetUtil.h"
#include "nsXPCOMStrings.h"
#include "xpcpublic.h"
#include "nsThreadUtils.h"
#include "nsIThreadInternal.h"
#include "nsContentUtils.h"
#include "nsIRequest.h"

#include "nsFrameManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsITimer.h"

#include "nsEventDispatcher.h"
#include "nsMediaError.h"
#include "nsICategoryManager.h"
#include "nsCharSeparatedTokenizer.h"
#include "MediaResource.h"

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
#include "nsMediaFragmentURIParser.h"
#include "nsURIHashKey.h"
#include "nsJSUtils.h"
#include "MediaStreamGraph.h"
#include "nsDOMMediaStream.h"
#include "nsIScriptError.h"

#include "nsCSSParser.h"
#include "nsIMediaList.h"

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
#ifdef MOZ_GSTREAMER
#include "nsGStreamerDecoder.h"
#endif
#ifdef MOZ_MEDIA_PLUGINS
#include "nsMediaPluginHost.h"
#include "nsMediaPluginDecoder.h"
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

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

// Number of milliseconds between timeupdate events as defined by spec
#define TIMEUPDATE_MS 250

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
  bool IsCancelled() {
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
    return nsContentUtils::DispatchTrustedEvent(mElement->OwnerDoc(),
                                                mSource,
                                                NS_LITERAL_STRING("error"),
                                                false,
                                                true);
  }
};

/**
 * There is a reference cycle involving this class: MediaLoadListener
 * holds a reference to the nsHTMLMediaElement, which holds a reference
 * to an nsIChannel, which holds a reference to this listener.
 * We break the reference cycle in OnStartRequest by clearing mElement.
 */
class nsHTMLMediaElement::MediaLoadListener MOZ_FINAL : public nsIStreamListener,
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
    : mElement(aElement),
      mLoadID(aElement->GetCurrentLoadID())
  {
    NS_ABORT_IF_FALSE(mElement, "Must pass an element to call back");
  }

private:
  nsRefPtr<nsHTMLMediaElement> mElement;
  nsCOMPtr<nsIStreamListener> mNextListener;
  PRUint32 mLoadID;
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

void nsHTMLMediaElement::ReportLoadError(const char* aMsg,
                                         const PRUnichar** aParams,
                                         PRUint32 aParamCount)
{
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "Media",
                                  OwnerDoc(),
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aMsg,
                                  aParams,
                                  aParamCount);
}


NS_IMETHODIMP nsHTMLMediaElement::MediaLoadListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  nsContentUtils::UnregisterShutdownObserver(this);

  if (!mElement) {
    // We've been notified by the shutdown observer, and are shutting down.
    return NS_BINDING_ABORTED;
  }

  // The element is only needed until we've had a chance to call
  // InitializeDecoderForChannel. So make sure mElement is cleared here.
  nsRefPtr<nsHTMLMediaElement> element;
  element.swap(mElement);

  if (mLoadID != element->GetCurrentLoadID()) {
    // The channel has been cancelled before we had a chance to create
    // a decoder. Abort, don't dispatch an "error" event, as the new load
    // may not be in an error state.
    return NS_BINDING_ABORTED;
  }

  // Don't continue to load if the request failed or has been canceled.
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_FAILED(status)) {
    if (element)
      element->NotifyLoadError();
    return status;
  }

  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(aRequest);
  bool succeeded;
  if (hc && NS_SUCCEEDED(hc->GetRequestSucceeded(&succeeded)) && !succeeded) {
    element->NotifyLoadError();
    PRUint32 responseStatus = 0;
    hc->GetResponseStatus(&responseStatus);
    nsAutoString code;
    code.AppendInt(responseStatus);
    nsAutoString src;
    element->GetCurrentSrc(src);
    const PRUnichar* params[] = { code.get(), src.get() };
    element->ReportLoadError("MediaLoadHttpError", params, ArrayLength(params));
    return NS_BINDING_ABORTED;
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSrcAttrStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSourcePointer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLoadBlockedDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSourceLoadCandidate)
  for (PRUint32 i = 0; i < tmp->mOutputStreams.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOutputStreams[i].mStream);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLMediaElement, nsGenericHTMLElement)
  if (tmp->mStream) {
    // Need to EndMediaStreamPlayback to clear mStream and make sure everything
    // gets unhooked correctly.
    tmp->EndMediaStreamPlayback();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSrcAttrStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSourcePointer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLoadBlockedDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSourceLoadCandidate)
  for (PRUint32 i = 0; i < tmp->mOutputStreams.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOutputStreams[i].mStream);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsHTMLMediaElement)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

// nsIDOMHTMLMediaElement
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Controls, controls)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Autoplay, autoplay)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Loop, loop)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, DefaultMuted, muted)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLMediaElement, Preload, preload, NULL)

NS_IMETHODIMP
nsHTMLMediaElement::GetSrc(JSContext* aCtx, jsval *aParams)
{
  if (mSrcAttrStream) {
    NS_ASSERTION(mSrcAttrStream->GetStream(), "MediaStream should have been set up properly");
    return nsContentUtils::WrapNative(aCtx, JS_GetGlobalForScopeChain(aCtx),
                                      mSrcAttrStream, aParams);
  }

  nsAutoString str;
  nsresult rv = GetURIAttr(nsGkAtoms::src, nsnull, str);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!xpc::StringToJsval(aCtx, str, aParams)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMediaElement::SetSrc(JSContext* aCtx, const jsval & aParams)
{
  if (aParams.isObject()) { // not JSVAL_IS_OBJECT() because a null object is still an object
    nsCOMPtr<nsIDOMMediaStream> stream;
    stream = do_QueryInterface(nsContentUtils::XPConnect()->
        GetNativeOfWrapper(aCtx, JSVAL_TO_OBJECT(aParams)));
    if (stream) {
      mSrcAttrStream = static_cast<nsDOMMediaStream*>(stream.get());
      UnsetAttr(kNameSpaceID_None, nsGkAtoms::src, true);
      Load();
      return NS_OK;
    }
  }

  mSrcAttrStream = nsnull;
  JSString* jsStr = JS_ValueToString(aCtx, aParams);
  if (!jsStr)
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  nsDependentJSString str;
  if (!str.init(aCtx, jsStr))
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  // Will trigger Load()
  return SetAttrHelper(nsGkAtoms::src, str);
}

/* readonly attribute nsIDOMHTMLMediaElement mozAutoplayEnabled; */
NS_IMETHODIMP nsHTMLMediaElement::GetMozAutoplayEnabled(bool *aAutoplayEnabled)
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
NS_IMETHODIMP nsHTMLMediaElement::GetEnded(bool *aEnded)
{
  if (mStream) {
    *aEnded = GetMediaStream()->IsFinished();
  } else if (mDecoder) {
    *aEnded = mDecoder->IsEnded();
  }
  return NS_OK;
}

/* readonly attribute DOMString currentSrc; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentSrc(nsAString & aCurrentSrc)
{
  nsCAutoString src;
  GetCurrentSpec(src);
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
 
    nsresult rv = http->SetRequestHeader(rangeHdr, rangeVal, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
 
  return NS_OK;
}

void nsHTMLMediaElement::ShutdownDecoder()
{
  RemoveMediaElementFromURITable();
  NS_ASSERTION(mDecoder, "Must have decoder to shut down");
  mDecoder->Shutdown();
  mDecoder = nsnull;
  // Discard all output streams. mDecoder->Shutdown() will have finished all
  // its output streams.
  // XXX For now we ignore mFinishWhenEnded. We'll fix this later. The
  // immediate goal is to not crash when reloading a media element with
  // output streams.
  mOutputStreams.Clear();
}

void nsHTMLMediaElement::AbortExistingLoads()
{
  // Abort any already-running instance of the resource selection algorithm.
  mLoadWaitStatus = NOT_WAITING;

  // Set a new load ID. This will cause events which were enqueued
  // with a different load ID to silently be cancelled.
  mCurrentLoadID++;

  bool fireTimeUpdate = false;

  if (mDecoder) {
    fireTimeUpdate = mDecoder->GetCurrentTime() != 0.0;
    ShutdownDecoder();
  }
  if (mStream) {
    EndMediaStreamPlayback();
  }
  mLoadingSrc = nsnull;

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING ||
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_IDLE)
  {
    DispatchEvent(NS_LITERAL_STRING("abort"));
  }

  mError = nsnull;
  mLoadedFirstFrame = false;
  mAutoplaying = true;
  mIsLoadingFromSourceChildren = false;
  mSuspendedAfterFirstFrame = false;
  mAllowSuspendAfterFirstFrame = true;
  mHaveQueuedSelectResource = false;
  mSuspendedForPreloadNone = false;
  mDownloadSuspendedByCache = false;
  mSourcePointer = nsnull;

  // TODO: The playback rate must be set to the default playback rate.

  if (mNetworkState != nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
    NS_ASSERTION(!mDecoder && !mStream, "How did someone setup a new stream/decoder already?");
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING);
    mPaused = true;

    if (fireTimeUpdate) {
      // Since we destroyed the decoder above, the current playback position
      // will now be reported as 0. The playback position was non-zero when
      // we destroyed the decoder, so fire a timeupdate event so that the
      // change will be reflected in the controls.
      FireTimeUpdate(false);
    }
    DispatchEvent(NS_LITERAL_STRING("emptied"));
  }

  // We may have changed mPaused, mAutoplaying, mNetworkState and other
  // things which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  mIsRunningSelectResource = false;
}

void nsHTMLMediaElement::NoSupportedMediaSourceError()
{
  NS_ASSERTION(mDelayingLoadEvent, "Load event not delayed during source selection?");

  mError = new nsMediaError(nsIDOMMediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
  DispatchAsyncEvent(NS_LITERAL_STRING("error"));
  // This clears mDelayingLoadEvent, so AddRemoveSelfReference will be called
  ChangeDelayLoadStatus(false);
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
  ChangeDelayLoadStatus(true);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  AsyncAwaitStableState(this, &nsHTMLMediaElement::LoadFromSourceChildren);
}

void nsHTMLMediaElement::QueueSelectResourceTask()
{
  // Don't allow multiple async select resource calls to be queued.
  if (mHaveQueuedSelectResource)
    return;
  mHaveQueuedSelectResource = true;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
  AsyncAwaitStableState(this, &nsHTMLMediaElement::SelectResourceWrapper);
}

/* void load (); */
NS_IMETHODIMP nsHTMLMediaElement::Load()
{
  if (mIsRunningLoadMethod)
    return NS_OK;
  SetPlayedOrSeeked(false);
  mIsRunningLoadMethod = true;
  AbortExistingLoads();
  QueueSelectResourceTask();
  mIsRunningLoadMethod = false;
  return NS_OK;
}

static bool HasSourceChildren(nsIContent *aElement)
{
  for (nsIContent* child = aElement->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTML(nsGkAtoms::source))
    {
      return true;
    }
  }
  return false;
}

void nsHTMLMediaElement::SelectResourceWrapper()
{
  SelectResource();
  mIsRunningSelectResource = false;
  mHaveQueuedSelectResource = false;
}

void nsHTMLMediaElement::SelectResource()
{
  if (!mSrcAttrStream && !HasAttr(kNameSpaceID_None, nsGkAtoms::src) &&
      !HasSourceChildren(this)) {
    // The media element has neither a src attribute nor any source
    // element children, abort the load.
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
    // This clears mDelayingLoadEvent, so AddRemoveSelfReference will be called
    ChangeDelayLoadStatus(false);
    return;
  }

  ChangeDelayLoadStatus(true);

  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  // Load event was delayed, and still is, so no need to call
  // AddRemoveSelfReference, since it must still be held
  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));

  // Delay setting mIsRunningSeletResource until after UpdatePreloadAction
  // so that we don't lose our state change by bailing out of the preload
  // state update
  UpdatePreloadAction();
  mIsRunningSelectResource = true;

  // If we have a 'src' attribute, use that exclusively.
  nsAutoString src;
  if (mSrcAttrStream) {
    SetupMediaStreamPlayback();
  } else if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NewURIFromString(src, getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      LOG(PR_LOG_DEBUG, ("%p Trying load from src=%s", this, NS_ConvertUTF16toUTF8(src).get()));
      NS_ASSERTION(!mIsLoadingFromSourceChildren,
        "Should think we're not loading from source children by default");
      mLoadingSrc = uri;
      if (mPreloadAction == nsHTMLMediaElement::PRELOAD_NONE) {
        // preload:none media, suspend the load here before we make any
        // network requests.
        SuspendLoad();
        return;
      }

      rv = LoadResource();
      if (NS_SUCCEEDED(rv)) {
        return;
      }
    } else {
      const PRUnichar* params[] = { src.get() };
      ReportLoadError("MediaLoadInvalidURI", params, ArrayLength(params));
    }
    NoSupportedMediaSourceError();
  } else {
    // Otherwise, the source elements will be used.
    mIsLoadingFromSourceChildren = true;
    LoadFromSourceChildren();
  }
}

void nsHTMLMediaElement::NotifyLoadError()
{
  if (!mIsLoadingFromSourceChildren) {
    LOG(PR_LOG_DEBUG, ("NotifyLoadError(), no supported media error"));
    NoSupportedMediaSourceError();
  } else if (mSourceLoadCandidate) {
    DispatchAsyncSourceError(mSourceLoadCandidate);
    QueueLoadFromSourceTask();
  } else {
    NS_WARNING("Should know the source we were loading from!");
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
      mAllowAudioData = false;
    }
  }

  DispatchAudioAvailableEvent(frameBuffer.forget(), aFrameBufferLength, aTime);
}

void nsHTMLMediaElement::LoadFromSourceChildren()
{
  NS_ASSERTION(mDelayingLoadEvent,
               "Should delay load event (if in document) during load");
  NS_ASSERTION(mIsLoadingFromSourceChildren,
               "Must remember we're loading from source children");

  nsIDocument* parentDoc = OwnerDoc()->GetParentDocument();
  if (parentDoc) {
    parentDoc->FlushPendingNotifications(Flush_Layout);
  }

  while (true) {
    nsIContent* child = GetNextSource();
    if (!child) {
      // Exhausted candidates, wait for more candidates to be appended to
      // the media element.
      mLoadWaitStatus = WAITING_FOR_SOURCE;
      mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
      ChangeDelayLoadStatus(false);
      ReportLoadError("MediaLoadExhaustedCandidates");
      return;
    }

    // Must have src attribute.
    nsAutoString src;
    if (!child->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
      ReportLoadError("MediaLoadSourceMissingSrc");
      DispatchAsyncSourceError(child);
      continue;
    }

    // If we have a type attribute, it must be a supported type.
    nsAutoString type;
    if (child->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type) &&
        GetCanPlay(type) == CANPLAY_NO) {
      DispatchAsyncSourceError(child);
      const PRUnichar* params[] = { type.get(), src.get() };
      ReportLoadError("MediaLoadUnsupportedTypeAttribute", params, ArrayLength(params));
      continue;
    }
    nsAutoString media;
    if (child->GetAttr(kNameSpaceID_None, nsGkAtoms::media, media) && !media.IsEmpty()) {
      nsCSSParser cssParser;
      nsRefPtr<nsMediaList> mediaList(new nsMediaList());
      cssParser.ParseMediaList(media, NULL, 0, mediaList, false);
      nsIPresShell* presShell = OwnerDoc()->GetShell();
      if (presShell && !mediaList->Matches(presShell->GetPresContext(), NULL)) {
        DispatchAsyncSourceError(child);
        const PRUnichar* params[] = { media.get(), src.get() };
        ReportLoadError("MediaLoadSourceMediaNotMatched", params, ArrayLength(params));
        continue;
      }
    }
    LOG(PR_LOG_DEBUG, ("%p Trying load from <source>=%s type=%s media=%s", this,
      NS_ConvertUTF16toUTF8(src).get(), NS_ConvertUTF16toUTF8(type).get(),
      NS_ConvertUTF16toUTF8(media).get()));

    nsCOMPtr<nsIURI> uri;
    NewURIFromString(src, getter_AddRefs(uri));
    if (!uri) {
      DispatchAsyncSourceError(child);
      const PRUnichar* params[] = { src.get() };
      ReportLoadError("MediaLoadInvalidURI", params, ArrayLength(params));
      continue;
    }

    mLoadingSrc = uri;
    NS_ASSERTION(mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING,
                 "Network state should be loading");

    if (mPreloadAction == nsHTMLMediaElement::PRELOAD_NONE) {
      // preload:none media, suspend the load here before we make any
      // network requests.
      SuspendLoad();
      return;
    }

    if (NS_SUCCEEDED(LoadResource())) {
      return;
    }

    // If we fail to load, loop back and try loading the next resource.
    DispatchAsyncSourceError(child);
  }
  NS_NOTREACHED("Execution should not reach here!");
}

void nsHTMLMediaElement::SuspendLoad()
{
  mSuspendedForPreloadNone = true;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
  ChangeDelayLoadStatus(false);
}

void nsHTMLMediaElement::ResumeLoad(PreloadAction aAction)
{
  NS_ASSERTION(mSuspendedForPreloadNone,
    "Must be halted for preload:none to resume from preload:none suspended load.");
  mSuspendedForPreloadNone = false;
  mPreloadAction = aAction;
  ChangeDelayLoadStatus(true);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  if (!mIsLoadingFromSourceChildren) {
    // We were loading from the element's src attribute.
    if (NS_FAILED(LoadResource())) {
      NoSupportedMediaSourceError();
    }
  } else {
    // We were loading from a child <source> element. Try to resume the
    // load of that child, and if that fails, try the next child.
    if (NS_FAILED(LoadResource())) {
      LoadFromSourceChildren();
    }
  }
}

static bool IsAutoplayEnabled()
{
  return Preferences::GetBool("media.autoplay.enabled");
}

void nsHTMLMediaElement::UpdatePreloadAction()
{
  PreloadAction nextAction = PRELOAD_UNDEFINED;
  // If autoplay is set, or we're playing, we should always preload data,
  // as we'll need it to play.
  if ((IsAutoplayEnabled() && HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay)) ||
      !mPaused)
  {
    nextAction = nsHTMLMediaElement::PRELOAD_ENOUGH;
  } else {
    // Find the appropriate preload action by looking at the attribute.
    const nsAttrValue* val = mAttrsAndChildren.GetAttr(nsGkAtoms::preload,
                                                       kNameSpaceID_None);
    PRUint32 preloadDefault =
      Preferences::GetInt("media.preload.default",
                          nsHTMLMediaElement::PRELOAD_ATTR_METADATA);
    PRUint32 preloadAuto =
      Preferences::GetInt("media.preload.auto",
                          nsHTMLMediaElement::PRELOAD_ENOUGH);
    if (!val) {
      // Attribute is not set. Use the preload action specified by the 
      // media.preload.default pref, or just preload metadata if not present.
      nextAction = static_cast<PreloadAction>(preloadDefault);
    } else if (val->Type() == nsAttrValue::eEnum) {
      PreloadAttrValue attr = static_cast<PreloadAttrValue>(val->GetEnumValue());
      if (attr == nsHTMLMediaElement::PRELOAD_ATTR_EMPTY ||
          attr == nsHTMLMediaElement::PRELOAD_ATTR_AUTO)
      {
        nextAction = static_cast<PreloadAction>(preloadAuto);
      } else if (attr == nsHTMLMediaElement::PRELOAD_ATTR_METADATA) {
        nextAction = nsHTMLMediaElement::PRELOAD_METADATA;
      } else if (attr == nsHTMLMediaElement::PRELOAD_ATTR_NONE) {
        nextAction = nsHTMLMediaElement::PRELOAD_NONE;
      }
    } else {
      // Use the suggested "missing value default" of "metadata", or the value
      // specified by the media.preload.default, if present.
      nextAction = static_cast<PreloadAction>(preloadDefault);
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
    if (mSuspendedForPreloadNone) {
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
    mAllowSuspendAfterFirstFrame = true;
    if (mSuspendedForPreloadNone) {
      // Our load was previouly suspended due to the media having preload
      // value "none". The preload value has changed to preload:metadata, so
      // resume the load. We'll pause the load again after we've read the
      // metadata.
      ResumeLoad(PRELOAD_METADATA);
    }
  }
}

nsresult nsHTMLMediaElement::LoadResource()
{
  NS_ASSERTION(mDelayingLoadEvent,
               "Should delay load event (if in document) during load");

  // If a previous call to mozSetup() was made, kill that media resource
  // in order to use this new src instead.
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
  }

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }

  // Set the media element's CORS mode only when loading a resource
  mCORSMode = AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));

  nsHTMLMediaElement* other = LookupMediaElementURITable(mLoadingSrc);
  if (other) {
    // Clone it.
    nsresult rv = InitializeDecoderAsClone(other->mDecoder);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_MEDIA,
                                          mLoadingSrc,
                                          NodePrincipal(),
                                          static_cast<nsGenericElement*>(this),
                                          EmptyCString(), // mime type
                                          nsnull, // extra
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_FAILURE;
  }

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
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     mLoadingSrc,
                     nsnull,
                     loadGroup,
                     nsnull,
                     nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY,
                     channelPolicy);
  NS_ENSURE_SUCCESS(rv,rv);

  // The listener holds a strong reference to us.  This creates a
  // reference cycle, once we've set mChannel, which is manually broken
  // in the listener's OnStartRequest method after it is finished with
  // the element. The cycle will also be broken if we get a shutdown
  // notification before OnStartRequest fires.  Necko guarantees that
  // OnStartRequest will eventually fire if we don't shut down first.
  nsRefPtr<MediaLoadListener> loadListener = new MediaLoadListener(this);

  channel->SetNotificationCallbacks(loadListener);

  nsCOMPtr<nsIStreamListener> listener;
  if (ShouldCheckAllowOrigin()) {
    listener =
      new nsCORSListenerProxy(loadListener,
                              NodePrincipal(),
                              channel,
                              GetCORSMode() == CORS_USE_CREDENTIALS,
                              &rv);
  } else {
    rv = nsContentUtils::GetSecurityManager()->
           CheckLoadURIWithPrincipal(NodePrincipal(),
                                     mLoadingSrc,
                                     nsIScriptSecurityManager::STANDARD);
    listener = loadListener;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(channel);
  if (hc) {
    // Use a byte range request from the start of the resource.
    // This enables us to detect if the stream supports byte range
    // requests, and therefore seeking, early.
    hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"),
                         NS_LITERAL_CSTRING("bytes=0-"),
                         false);

    SetRequestHeaders(hc);
  }

  rv = channel->AsyncOpen(listener, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Else the channel must be open and starting to download. If it encounters
  // a non-catastrophic failure, it will set a new task to continue loading
  // another candidate.  It's safe to set it as mChannel now.
  mChannel = channel;

  // loadListener will be unregistered either on shutdown or when
  // OnStartRequest for the channel we just opened fires.
  nsContentUtils::RegisterShutdownObserver(loadListener);
  return NS_OK;
}

nsresult nsHTMLMediaElement::LoadWithChannel(nsIChannel *aChannel,
                                             nsIStreamListener **aListener)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aListener);

  *aListener = nsnull;

  // Make sure we don't reenter during synchronous abort events.
  if (mIsRunningLoadMethod)
    return NS_OK;
  mIsRunningLoadMethod = true;
  AbortExistingLoads();
  mIsRunningLoadMethod = false;

  nsresult rv = aChannel->GetOriginalURI(getter_AddRefs(mLoadingSrc));
  NS_ENSURE_SUCCESS(rv, rv);

  ChangeDelayLoadStatus(true);
  rv = InitializeDecoderForChannel(aChannel, aListener);
  if (NS_FAILED(rv)) {
    ChangeDelayLoadStatus(false);
    return rv;
  }

  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));

  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::MozLoadFrom(nsIDOMHTMLMediaElement* aOther)
{
  NS_ENSURE_ARG_POINTER(aOther);

  // Make sure we don't reenter during synchronous abort events.
  if (mIsRunningLoadMethod)
    return NS_OK;
  mIsRunningLoadMethod = true;
  AbortExistingLoads();
  mIsRunningLoadMethod = false;

  nsCOMPtr<nsIContent> content = do_QueryInterface(aOther);
  nsHTMLMediaElement* other = static_cast<nsHTMLMediaElement*>(content.get());
  if (!other || !other->mDecoder)
    return NS_OK;

  ChangeDelayLoadStatus(true);

  mLoadingSrc = other->mLoadingSrc;
  nsresult rv = InitializeDecoderAsClone(other->mDecoder);
  if (NS_FAILED(rv)) {
    ChangeDelayLoadStatus(false);
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
NS_IMETHODIMP nsHTMLMediaElement::GetSeeking(bool *aSeeking)
{
  *aSeeking = mDecoder && mDecoder->IsSeeking();

  return NS_OK;
}

/* attribute double currentTime; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentTime(double *aCurrentTime)
{
  if (mStream) {
    *aCurrentTime = MediaTimeToSeconds(GetMediaStream()->GetCurrentTime());
  } else if (mDecoder) {
    *aCurrentTime = mDecoder->GetCurrentTime();
  } else {
    *aCurrentTime = 0.0;
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetCurrentTime(double aCurrentTime)
{
  StopSuspendingAfterFirstFrame();

  if (mStream) {
    // do nothing since streams aren't seekable; we effectively clamp to
    // the current time.
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mCurrentPlayRangeStart != -1.0) {
    double rangeEndTime = 0;
    GetCurrentTime(&rangeEndTime);
    LOG(PR_LOG_DEBUG, ("%p Adding \'played\' a range : [%f, %f]", this, mCurrentPlayRangeStart, rangeEndTime));
    // Multiple seek without playing, or seek while playing.
    if (mCurrentPlayRangeStart != rangeEndTime) {
      mPlayed.Add(mCurrentPlayRangeStart, rangeEndTime);
    }
  }

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

  // Clamp the time to [0, duration] as required by the spec.
  double clampedTime = NS_MAX(0.0, aCurrentTime);
  double duration = mDecoder->GetDuration();
  if (duration >= 0) {
    clampedTime = NS_MIN(clampedTime, duration);
  }

  mPlayingBeforeSeek = IsPotentiallyPlaying();
  // The media backend is responsible for dispatching the timeupdate
  // event if it changes the playback position as a result of the seek.
  LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) starting seek", this, aCurrentTime));
  nsresult rv = mDecoder->Seek(clampedTime);
  // Start a new range at position we seeked to.
  mCurrentPlayRangeStart = clampedTime;

  // We changed whether we're seeking so we need to AddRemoveSelfReference.
  AddRemoveSelfReference();

  return rv;
}

/* readonly attribute double duration; */
NS_IMETHODIMP nsHTMLMediaElement::GetDuration(double *aDuration)
{
  if (mStream) {
    *aDuration = std::numeric_limits<double>::infinity();
  } else if (mDecoder) {
    *aDuration = mDecoder->GetDuration();
  } else {
    *aDuration = std::numeric_limits<double>::quiet_NaN();
  }
  return NS_OK;
}

/* readonly attribute nsIDOMHTMLTimeRanges seekable; */
NS_IMETHODIMP nsHTMLMediaElement::GetSeekable(nsIDOMTimeRanges** aSeekable)
{
  nsRefPtr<nsTimeRanges> ranges = new nsTimeRanges();
  if (mDecoder && mReadyState > nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    mDecoder->GetSeekable(ranges);
  }
  ranges.forget(aSeekable);
  return NS_OK;
}


/* readonly attribute boolean paused; */
NS_IMETHODIMP nsHTMLMediaElement::GetPaused(bool *aPaused)
{
  *aPaused = mPaused;

  return NS_OK;
}

/* readonly attribute nsIDOMHTMLTimeRanges played; */
NS_IMETHODIMP nsHTMLMediaElement::GetPlayed(nsIDOMTimeRanges** aPlayed)
{
  nsTimeRanges* ranges = new nsTimeRanges();
  NS_ADDREF(*aPlayed = ranges);

  PRUint32 timeRangeCount = 0;
  mPlayed.GetLength(&timeRangeCount);
  for (PRUint32 i = 0; i < timeRangeCount; i++) {
    double begin;
    double end;
    mPlayed.Start(i, &begin);
    mPlayed.End(i, &end);
    ranges->Add(begin, end);
  }

  if (mCurrentPlayRangeStart != -1.0) {
    double now = 0.0;
    GetCurrentTime(&now);
    if (mCurrentPlayRangeStart != now) {
      ranges->Add(mCurrentPlayRangeStart, now);
    }
  }

  ranges->Normalize();

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

  bool oldPaused = mPaused;
  mPaused = true;
  mAutoplaying = false;
  // We changed mPaused and mAutoplaying which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  if (!oldPaused) {
    if (mStream) {
      GetMediaStream()->ChangeExplicitBlockerCount(1);
    }
    FireTimeUpdate(false);
    DispatchAsyncEvent(NS_LITERAL_STRING("pause"));
  }

  return NS_OK;
}

/* attribute double volume; */
NS_IMETHODIMP nsHTMLMediaElement::GetVolume(double *aVolume)
{
  *aVolume = mVolume;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetVolume(double aVolume)
{
  if (aVolume < 0.0 || aVolume > 1.0)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  if (aVolume == mVolume)
    return NS_OK;

  mVolume = aVolume;

  if (!mMuted) {
    if (mDecoder) {
      mDecoder->SetVolume(mVolume);
    } else if (mAudioStream) {
      mAudioStream->SetVolume(mVolume);
    } else if (mStream) {
      GetMediaStream()->SetAudioOutputVolume(this, float(mVolume));
    }
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
NS_IMETHODIMP nsHTMLMediaElement::GetMuted(bool *aMuted)
{
  *aMuted = mMuted;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetMuted(bool aMuted)
{
  if (aMuted == mMuted)
    return NS_OK;

  mMuted = aMuted;

  float effectiveVolume = mMuted ? 0.0f : float(mVolume);
  if (mDecoder) {
    mDecoder->SetVolume(effectiveVolume);
  } else if (mAudioStream) {
    mAudioStream->SetVolume(effectiveVolume);
  } else if (mStream) {
    GetMediaStream()->SetAudioOutputVolume(this, effectiveVolume);
  }

  DispatchAsyncEvent(NS_LITERAL_STRING("volumechange"));

  return NS_OK;
}

already_AddRefed<nsDOMMediaStream>
nsHTMLMediaElement::CaptureStreamInternal(bool aFinishWhenEnded)
{
  OutputMediaStream* out = mOutputStreams.AppendElement();
  out->mStream = nsDOMMediaStream::CreateInputStream();
  nsRefPtr<nsIPrincipal> principal = GetCurrentPrincipal();
  out->mStream->CombineWithPrincipal(principal);
  out->mFinishWhenEnded = aFinishWhenEnded;

  mAudioCaptured = true;
  if (mDecoder) {
    mDecoder->SetAudioCaptured(true);
    mDecoder->AddOutputStream(
        out->mStream->GetStream()->AsSourceStream(), aFinishWhenEnded);
  }
  nsRefPtr<nsDOMMediaStream> result = out->mStream;
  return result.forget();
}

NS_IMETHODIMP nsHTMLMediaElement::MozCaptureStream(nsIDOMMediaStream** aStream)
{
  *aStream = CaptureStreamInternal(false).get();
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::MozCaptureStreamUntilEnded(nsIDOMMediaStream** aStream)
{
  *aStream = CaptureStreamInternal(true).get();
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::GetMozAudioCaptured(bool *aCaptured)
{
  *aCaptured = mAudioCaptured;
  return NS_OK;
}

class MediaElementSetForURI : public nsURIHashKey {
public:
  MediaElementSetForURI(const nsIURI* aKey) : nsURIHashKey(aKey) {}
  MediaElementSetForURI(const MediaElementSetForURI& toCopy)
    : nsURIHashKey(toCopy), mElements(toCopy.mElements) {}
  nsTArray<nsHTMLMediaElement*> mElements;
};

typedef nsTHashtable<MediaElementSetForURI> MediaElementURITable;
// Elements in this table must have non-null mDecoder and mLoadingSrc, and those
// can't change while the element is in the table. The table is keyed by
// the element's mLoadingSrc. Each entry has a list of all elements with the
// same mLoadingSrc.
static MediaElementURITable* gElementTable;

#ifdef DEBUG
// Returns the number of times aElement appears in the media element table
// for aURI. If this returns other than 0 or 1, there's a bug somewhere!
static unsigned
MediaElementTableCount(nsHTMLMediaElement* aElement, nsIURI* aURI)
{
  if (!gElementTable || !aElement || !aURI) {
    return 0;
  }
  MediaElementSetForURI* entry = gElementTable->GetEntry(aURI);
  if (!entry) {
    return 0;
  }
  PRUint32 count = 0;
  for (PRUint32 i = 0; i < entry->mElements.Length(); ++i) {
    nsHTMLMediaElement* elem = entry->mElements[i];
    if (elem == aElement) {
      count++;
    }
  }
  return count;
}
#endif

void
nsHTMLMediaElement::AddMediaElementToURITable()
{
  NS_ASSERTION(mDecoder && mDecoder->GetResource(), "Call this only with decoder Load called");
  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 0,
    "Should not have entry for element in element table before addition");
  if (!gElementTable) {
    gElementTable = new MediaElementURITable();
    gElementTable->Init();
  }
  MediaElementSetForURI* entry = gElementTable->PutEntry(mLoadingSrc);
  entry->mElements.AppendElement(this);
  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 1,
    "Should have a single entry for element in element table after addition");
}

void
nsHTMLMediaElement::RemoveMediaElementFromURITable()
{
  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 1,
    "Before remove, should have a single entry for element in element table");
  NS_ASSERTION(mDecoder, "Don't call this without decoder!");
  NS_ASSERTION(mLoadingSrc, "Can't have decoder without source!");
  if (!gElementTable)
    return;
  MediaElementSetForURI* entry = gElementTable->GetEntry(mLoadingSrc);
  if (!entry)
    return;
  entry->mElements.RemoveElement(this);
  if (entry->mElements.IsEmpty()) {
    gElementTable->RemoveEntry(mLoadingSrc);
    if (gElementTable->Count() == 0) {
      delete gElementTable;
      gElementTable = nsnull;
    }
  }
  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 0,
    "After remove, should no longer have an entry in element table");
}

nsHTMLMediaElement*
nsHTMLMediaElement::LookupMediaElementURITable(nsIURI* aURI)
{
  if (!gElementTable)
    return nsnull;
  MediaElementSetForURI* entry = gElementTable->GetEntry(aURI);
  if (!entry)
    return nsnull;
  for (PRUint32 i = 0; i < entry->mElements.Length(); ++i) {
    nsHTMLMediaElement* elem = entry->mElements[i];
    bool equal;
    // Look for elements that have the same principal and CORS mode.
    // Ditto for anything else that could cause us to send different headers.
    if (NS_SUCCEEDED(elem->NodePrincipal()->Equals(NodePrincipal(), &equal)) && equal &&
        elem->mCORSMode == mCORSMode) {
      NS_ASSERTION(elem->mDecoder && elem->mDecoder->GetResource(), "Decoder gone");
      MediaResource* resource = elem->mDecoder->GetResource();
      if (resource->CanClone()) {
        return elem;
      }
    }
  }
  return nsnull;
}

nsHTMLMediaElement::nsHTMLMediaElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mStreamListener(nsnull),
    mCurrentLoadID(0),
    mNetworkState(nsIDOMHTMLMediaElement::NETWORK_EMPTY),
    mReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING),
    mLoadWaitStatus(NOT_WAITING),
    mVolume(1.0),
    mChannels(0),
    mRate(0),
    mPreloadAction(PRELOAD_UNDEFINED),
    mMediaSize(-1,-1),
    mLastCurrentTime(0.0),
    mFragmentStart(-1.0),
    mFragmentEnd(-1.0),
    mCurrentPlayRangeStart(-1.0),
    mAllowAudioData(false),
    mBegun(false),
    mLoadedFirstFrame(false),
    mAutoplaying(true),
    mAutoplayEnabled(true),
    mPaused(true),
    mMuted(false),
    mAudioCaptured(false),
    mPlayingBeforeSeek(false),
    mPausedForInactiveDocument(false),
    mWaitingFired(false),
    mIsRunningLoadMethod(false),
    mIsLoadingFromSourceChildren(false),
    mDelayingLoadEvent(false),
    mIsRunningSelectResource(false),
    mHaveQueuedSelectResource(false),
    mSuspendedAfterFirstFrame(false),
    mAllowSuspendAfterFirstFrame(true),
    mHasPlayedOrSeeked(false),
    mHasSelfReference(false),
    mShuttingDown(false),
    mSuspendedForPreloadNone(false),
    mMediaSecurityVerified(false),
    mCORSMode(CORS_NONE),
    mHasAudio(false),
    mDownloadSuspendedByCache(false)
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

  if (mVideoFrameContainer) {
    mVideoFrameContainer->ForgetElement();
  }
  UnregisterFreezableElement();
  if (mDecoder) {
    ShutdownDecoder();
  }
  if (mStream) {
    EndMediaStreamPlayback();
  }

  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 0,
    "Destroyed media element should no longer be in element table");

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
  if (mAudioStream) {
    mAudioStream->Shutdown();
  }
}

void nsHTMLMediaElement::StopSuspendingAfterFirstFrame()
{
  mAllowSuspendAfterFirstFrame = false;
  if (!mSuspendedAfterFirstFrame)
    return;
  mSuspendedAfterFirstFrame = false;
  if (mDecoder) {
    mDecoder->Resume(true);
  }
}

void nsHTMLMediaElement::SetPlayedOrSeeked(bool aValue)
{
  if (aValue == mHasPlayedOrSeeked) {
    return;
  }

  mHasPlayedOrSeeked = aValue;

  // Force a reflow so that the poster frame hides or shows immediately.
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return;
  }
  frame->PresContext()->PresShell()->FrameNeedsReflow(frame,
                                                      nsIPresShell::eTreeChange,
                                                      NS_FRAME_IS_DIRTY);
}

NS_IMETHODIMP nsHTMLMediaElement::Play()
{
  StopSuspendingAfterFirstFrame();
  SetPlayedOrSeeked(true);

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    nsresult rv = Load();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (mSuspendedForPreloadNone) {
    ResumeLoad(PRELOAD_ENOUGH);
  }
  // Even if we just did Load() or ResumeLoad(), we could already have a decoder
  // here if we managed to clone an existing decoder.
  if (mDecoder) {
    if (mDecoder->IsEnded()) {
      SetCurrentTime(0);
    }
    if (!mPausedForInactiveDocument) {
      nsresult rv = mDecoder->Play();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (mCurrentPlayRangeStart == -1.0) {
    GetCurrentTime(&mCurrentPlayRangeStart);
  }

  // TODO: If the playback has ended, then the user agent must set
  // seek to the effective start.
  // TODO: The playback rate must be set to the default playback rate.
  if (mPaused) {
    if (mStream) {
      GetMediaStream()->ChangeExplicitBlockerCount(-1);
    }
    DispatchAsyncEvent(NS_LITERAL_STRING("play"));
    switch (mReadyState) {
    case nsIDOMHTMLMediaElement::HAVE_NOTHING:
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      break;
    case nsIDOMHTMLMediaElement::HAVE_METADATA:
    case nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA:
      FireTimeUpdate(false);
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      break;
    case nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA:
    case nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA:
      DispatchAsyncEvent(NS_LITERAL_STRING("playing"));
      break;
    }
  }

  mPaused = false;
  mAutoplaying = false;
  // We changed mPaused and mAutoplaying which can affect AddRemoveSelfReference
  // and our preload status.
  AddRemoveSelfReference();
  UpdatePreloadAction();

  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLMediaElement, Crossorigin, crossorigin)

bool nsHTMLMediaElement::ParseAttribute(PRInt32 aNamespaceID,
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
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return true;
    }
    if (aAttribute == nsGkAtoms::crossorigin) {
      ParseCORSValue(aValue, aResult);
      return true;
    }
    if (aAttribute == nsGkAtoms::preload) {
      return aResult.ParseEnumValue(aValue, kPreloadTable, false);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void nsHTMLMediaElement::DoneCreatingElement()
{
   if (HasAttr(kNameSpaceID_None, nsGkAtoms::muted))
     mMuted = true; 
}

nsresult nsHTMLMediaElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                     nsIAtom* aPrefix, const nsAString& aValue,
                                     bool aNotify)
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
                                       bool aNotify)
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
                                        bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument,
                                                 aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  if (aDocument) {
    mAutoplayEnabled =
      IsAutoplayEnabled() && (!aDocument || !aDocument->IsStaticDocument()) &&
      !IsEditable();
    // The preload action depends on the value of the autoplay attribute.
    // It's value may have changed, so update it.
    UpdatePreloadAction();

    if (aDocument->HasAudioAvailableListeners()) {
      // The document already has listeners for the "MozAudioAvailable"
      // event, so the decoder must be notified so it initiates
      // "MozAudioAvailable" event dispatch.
      NotifyAudioAvailableListener();
    }
  }

  return rv;
}

void nsHTMLMediaElement::UnbindFromTree(bool aDeep,
                                        bool aNullParent)
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

bool
nsHTMLMediaElement::IsRawEnabled()
{
  return Preferences::GetBool("media.raw.enabled");
}

static bool IsRawType(const nsACString& aType)
{
  if (!nsHTMLMediaElement::IsRawEnabled()) {
    return false;
  }

  for (PRUint32 i = 0; i < ArrayLength(gRawTypes); ++i) {
    if (aType.EqualsASCII(gRawTypes[i])) {
      return true;
    }
  }

  return false;
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

char const *const nsHTMLMediaElement::gOggCodecsWithOpus[4] = {
  "vorbis",
  "opus",
  "theora",
  nsnull
};

bool
nsHTMLMediaElement::IsOpusEnabled()
{
#ifdef MOZ_OPUS
  return Preferences::GetBool("media.opus.enabled");
#else
  return false;
#endif
}

bool
nsHTMLMediaElement::IsOggEnabled()
{
  return Preferences::GetBool("media.ogg.enabled");
}

bool
nsHTMLMediaElement::IsOggType(const nsACString& aType)
{
  if (!IsOggEnabled()) {
    return false;
  }

  for (PRUint32 i = 0; i < ArrayLength(gOggTypes); ++i) {
    if (aType.EqualsASCII(gOggTypes[i])) {
      return true;
    }
  }

  return false;
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
  return Preferences::GetBool("media.wave.enabled");
}

bool
nsHTMLMediaElement::IsWaveType(const nsACString& aType)
{
  if (!IsWaveEnabled()) {
    return false;
  }

  for (PRUint32 i = 0; i < ArrayLength(gWaveTypes); ++i) {
    if (aType.EqualsASCII(gWaveTypes[i])) {
      return true;
    }
  }

  return false;
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
  return Preferences::GetBool("media.webm.enabled");
}

bool
nsHTMLMediaElement::IsWebMType(const nsACString& aType)
{
  if (!IsWebMEnabled()) {
    return false;
  }

  for (PRUint32 i = 0; i < ArrayLength(gWebMTypes); ++i) {
    if (aType.EqualsASCII(gWebMTypes[i])) {
      return true;
    }
  }

  return false;
}
#endif

#ifdef MOZ_GSTREAMER
const char nsHTMLMediaElement::gH264Types[3][17] = {
  "video/mp4",
  "video/3gpp",
  "video/quicktime",
};

char const *const nsHTMLMediaElement::gH264Codecs[6] = {
  "avc1.42E01E",
  "avc1.58A01E",
  "avc1.4D401E",
  "avc1.64001E",
  "mp4a.40.2",
  nsnull
};

bool
nsHTMLMediaElement::IsH264Enabled()
{
  return Preferences::GetBool("media.h264.enabled");
}

bool
nsHTMLMediaElement::IsH264Type(const nsACString& aType)
{
  if (!IsH264Enabled()) {
    return false;
  }

  for (PRUint32 i = 0; i < ArrayLength(gH264Types); ++i) {
    if (aType.EqualsASCII(gH264Types[i])) {
      return true;
    }
  }

  return false;
}
#endif

#ifdef MOZ_MEDIA_PLUGINS
bool
nsHTMLMediaElement::IsMediaPluginsEnabled()
{
  return Preferences::GetBool("media.plugins.enabled");
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
    *aCodecList = IsOpusEnabled() ? gOggCodecsWithOpus : gOggCodecs;
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

#ifdef MOZ_GSTREAMER
  if (IsH264Type(nsDependentCString(aMIMEType))) {
    *aCodecList = gH264Codecs;
    return CANPLAY_YES;
  }
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (GetMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), aCodecList))
    return CANPLAY_MAYBE;
#endif
  return CANPLAY_NO;
}

/* static */
bool nsHTMLMediaElement::ShouldHandleMediaType(const char* aMIMEType)
{
#ifdef MOZ_RAW
  if (IsRawType(nsDependentCString(aMIMEType)))
    return true;
#endif
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType)))
    return true;
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(nsDependentCString(aMIMEType)))
    return true;
#endif
#ifdef MOZ_GSTREAMER
  if (IsH264Type(nsDependentCString(aMIMEType)))
    return true;
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (GetMediaPluginHost()->FindDecoder(nsDependentCString(aMIMEType), NULL))
    return true;
#endif
  // We should not return true for Wave types, since there are some
  // Wave codecs actually in use in the wild that we don't support, and
  // we should allow those to be handled by plugins or helper apps.
  // Furthermore people can play Wave files on most platforms by other
  // means.
  return false;
}

static bool
CodecListContains(char const *const * aCodecs, const nsAString& aCodec)
{
  for (PRInt32 i = 0; aCodecs[i]; ++i) {
    if (aCodec.EqualsASCII(aCodecs[i]))
      return true;
  }
  return false;
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
  bool expectMoreTokens = false;
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
  case CANPLAY_NO:
    aResult.Truncate();
    break;
  case CANPLAY_YES:
    aResult.AssignLiteral("probably");
    break;
  default:
  case CANPLAY_MAYBE:
    aResult.AssignLiteral("maybe");
    break;
  }
  return NS_OK;
}

already_AddRefed<nsMediaDecoder>
nsHTMLMediaElement::CreateDecoder(const nsACString& aType)
{
#ifdef MOZ_RAW
  if (IsRawType(aType)) {
    nsRefPtr<nsRawDecoder> decoder = new nsRawDecoder();
    if (decoder->Init(this)) {
      return decoder.forget();
    }
  }
#endif
#ifdef MOZ_OGG
  if (IsOggType(aType)) {
#ifdef MOZ_GSTREAMER 
    nsRefPtr<nsGStreamerDecoder> decoder = new nsGStreamerDecoder();
#else
    nsRefPtr<nsOggDecoder> decoder = new nsOggDecoder();
#endif
    if (decoder->Init(this)) {
      return decoder.forget();
    }
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    nsRefPtr<nsWaveDecoder> decoder = new nsWaveDecoder();
    if (decoder->Init(this)) {
      return decoder.forget();
    }
  }
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (GetMediaPluginHost()->FindDecoder(aType, NULL)) {
    nsRefPtr<nsMediaPluginDecoder> decoder = new nsMediaPluginDecoder(aType);
    if (decoder->Init(this)) {
      return decoder.forget();
    }
  }
#endif
#ifdef MOZ_WEBM
  if (IsWebMType(aType)) {
#ifdef MOZ_GSTREAMER 
    nsRefPtr<nsGStreamerDecoder> decoder = new nsGStreamerDecoder();
#else
    nsRefPtr<nsWebMDecoder> decoder = new nsWebMDecoder();
#endif
    if (decoder->Init(this)) {
      return decoder.forget();
    }
  }
#endif

#ifdef MOZ_GSTREAMER 
  if (IsH264Type(aType)) {
    nsRefPtr<nsGStreamerDecoder> decoder = new nsGStreamerDecoder();
    if (decoder->Init(this)) {
      return decoder.forget();
    }
  }
#endif
  return nsnull;
}

nsresult nsHTMLMediaElement::InitializeDecoderAsClone(nsMediaDecoder* aOriginal)
{
  NS_ASSERTION(mLoadingSrc, "mLoadingSrc must already be set");
  NS_ASSERTION(mDecoder == nsnull, "Shouldn't have a decoder");

  MediaResource* originalResource = aOriginal->GetResource();
  if (!originalResource)
    return NS_ERROR_FAILURE;
  nsRefPtr<nsMediaDecoder> decoder = aOriginal->Clone();
  if (!decoder)
    return NS_ERROR_FAILURE;

  LOG(PR_LOG_DEBUG, ("%p Cloned decoder %p from %p", this, decoder.get(), aOriginal));

  if (!decoder->Init(this)) {
    LOG(PR_LOG_DEBUG, ("%p Failed to init cloned decoder %p", this, decoder.get()));
    return NS_ERROR_FAILURE;
  }

  double duration = aOriginal->GetDuration();
  if (duration >= 0) {
    decoder->SetDuration(duration);
    decoder->SetSeekable(aOriginal->IsSeekable());
  }

  MediaResource* resource = originalResource->CloneData(decoder);
  if (!resource) {
    LOG(PR_LOG_DEBUG, ("%p Failed to cloned stream for decoder %p", this, decoder.get()));
    return NS_ERROR_FAILURE;
  }

  return FinishDecoderSetup(decoder, resource, nsnull, aOriginal);
}

nsresult nsHTMLMediaElement::InitializeDecoderForChannel(nsIChannel *aChannel,
                                                         nsIStreamListener **aListener)
{
  NS_ASSERTION(mLoadingSrc, "mLoadingSrc must already be set");
  NS_ASSERTION(mDecoder == nsnull, "Shouldn't have a decoder");

  nsCAutoString mimeType;
  aChannel->GetContentType(mimeType);

  nsRefPtr<nsMediaDecoder> decoder = CreateDecoder(mimeType);
  if (!decoder) {
    nsAutoString src;
    GetCurrentSrc(src);
    NS_ConvertUTF8toUTF16 mimeUTF16(mimeType);
    const PRUnichar* params[] = { mimeUTF16.get(), src.get() };
    ReportLoadError("MediaLoadUnsupportedMimeType", params, ArrayLength(params));
    return NS_ERROR_FAILURE;
  }

  LOG(PR_LOG_DEBUG, ("%p Created decoder %p for type %s", this, decoder.get(), mimeType.get()));

  MediaResource* resource = MediaResource::Create(decoder, aChannel);
  if (!resource)
    return NS_ERROR_OUT_OF_MEMORY;

  // stream successfully created, the stream now owns the channel.
  mChannel = nsnull;

  return FinishDecoderSetup(decoder, resource, aListener, nsnull);
}

nsresult nsHTMLMediaElement::FinishDecoderSetup(nsMediaDecoder* aDecoder,
                                                MediaResource* aStream,
                                                nsIStreamListener **aListener,
                                                nsMediaDecoder* aCloneDonor)
{
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;

  // Force a same-origin check before allowing events for this media resource.
  mMediaSecurityVerified = false;

  // The new stream has not been suspended by us.
  mPausedForInactiveDocument = false;

  aDecoder->SetAudioCaptured(mAudioCaptured);
  aDecoder->SetVolume(mMuted ? 0.0 : mVolume);
  for (PRUint32 i = 0; i < mOutputStreams.Length(); ++i) {
    OutputMediaStream* ms = &mOutputStreams[i];
    aDecoder->AddOutputStream(ms->mStream->GetStream()->AsSourceStream(),
        ms->mFinishWhenEnded);
  }

  nsresult rv = aDecoder->Load(aStream, aListener, aCloneDonor);
  if (NS_FAILED(rv)) {
    LOG(PR_LOG_DEBUG, ("%p Failed to load for decoder %p", this, aDecoder));
    return rv;
  }

  // Decoder successfully created, the decoder now owns the MediaResource
  // which owns the channel.
  mChannel = nsnull;

  mDecoder = aDecoder;
  AddMediaElementToURITable();
  NotifyDecoderPrincipalChanged();

  // We may want to suspend the new stream now.
  // This will also do an AddRemoveSelfReference.
  NotifyOwnerDocumentActivityChanged();

  if (!mPaused) {
    SetPlayedOrSeeked(true);
    if (!mPausedForInactiveDocument) {
      rv = mDecoder->Play();
    }
  }

  if (OwnerDoc()->HasAudioAvailableListeners()) {
    NotifyAudioAvailableListener();
  }

  if (NS_FAILED(rv)) {
    ShutdownDecoder();
  }

  NS_ASSERTION(NS_SUCCEEDED(rv) == (MediaElementTableCount(this, mLoadingSrc) == 1),
    "Media element should have single table entry if decode initialized");

  mBegun = true;
  return rv;
}

class nsHTMLMediaElement::StreamListener : public MediaStreamListener {
public:
  StreamListener(nsHTMLMediaElement* aElement) :
    mElement(aElement),
    mMutex("nsHTMLMediaElement::StreamListener"),
    mPendingNotifyOutput(false)
  {}
  void Forget() { mElement = nsnull; }

  // Main thread
  void DoNotifyFinished()
  {
    if (mElement) {
      mElement->PlaybackEnded();
    }
  }
  void DoNotifyBlocked()
  {
    if (mElement) {
      mElement->UpdateReadyStateForData(NEXT_FRAME_UNAVAILABLE_BUFFERING);
    }
  }
  void DoNotifyUnblocked()
  {
    if (mElement) {
      mElement->UpdateReadyStateForData(NEXT_FRAME_AVAILABLE);
    }
  }
  void DoNotifyOutput()
  {
    {
      MutexAutoLock lock(mMutex);
      mPendingNotifyOutput = false;
    }
    if (mElement) {
      mElement->FireTimeUpdate(true);
    }
  }

  // These notifications run on the media graph thread so we need to
  // dispatch events to the main thread.
  virtual void NotifyBlockingChanged(MediaStreamGraph* aGraph, Blocking aBlocked)
  {
    nsCOMPtr<nsIRunnable> event;
    if (aBlocked == BLOCKED) {
      event = NS_NewRunnableMethod(this, &StreamListener::DoNotifyBlocked);
    } else {
      event = NS_NewRunnableMethod(this, &StreamListener::DoNotifyUnblocked);
    }
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event);
  }
  virtual void NotifyFinished(MediaStreamGraph* aGraph)
  {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &StreamListener::DoNotifyFinished);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event);
  }
  virtual void NotifyOutput(MediaStreamGraph* aGraph)
  {
    MutexAutoLock lock(mMutex);
    if (mPendingNotifyOutput)
      return;
    mPendingNotifyOutput = true;
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &StreamListener::DoNotifyOutput);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event);
  }

private:
  nsHTMLMediaElement* mElement;

  Mutex mMutex;
  bool mPendingNotifyOutput;
};

void nsHTMLMediaElement::SetupMediaStreamPlayback()
{
  NS_ASSERTION(!mStream && !mStreamListener, "Should have been ended already");

  mStream = mSrcAttrStream;
  // XXX if we ever support capturing the output of a media element which is
  // playing a stream, we'll need to add a CombineWithPrincipal call here.
  mStreamListener = new StreamListener(this);
  NS_ADDREF(mStreamListener);
  GetMediaStream()->AddListener(mStreamListener);
  if (mPaused) {
    GetMediaStream()->ChangeExplicitBlockerCount(1);
  }
  if (mPausedForInactiveDocument) {
    GetMediaStream()->ChangeExplicitBlockerCount(1);
  }
  ChangeDelayLoadStatus(false);
  GetMediaStream()->AddAudioOutput(this);
  GetMediaStream()->SetAudioOutputVolume(this, float(mMuted ? 0.0 : mVolume));
  VideoFrameContainer* container = GetVideoFrameContainer();
  if (container) {
    GetMediaStream()->AddVideoOutput(container);
  }
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
  DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  DispatchAsyncEvent(NS_LITERAL_STRING("loadedmetadata"));
  ResourceLoaded();
}

void nsHTMLMediaElement::EndMediaStreamPlayback()
{
  GetMediaStream()->RemoveListener(mStreamListener);
  // Kill its reference to this element
  mStreamListener->Forget();
  NS_RELEASE(mStreamListener); // sets to null
  GetMediaStream()->RemoveAudioOutput(this);
  VideoFrameContainer* container = GetVideoFrameContainer();
  if (container) {
    GetMediaStream()->RemoveVideoOutput(container);
  }
  if (mPaused) {
    GetMediaStream()->ChangeExplicitBlockerCount(-1);
  }
  if (mPausedForInactiveDocument) {
    GetMediaStream()->ChangeExplicitBlockerCount(-1);
  }
  mStream = nsnull;
}

nsresult nsHTMLMediaElement::NewURIFromString(const nsAutoString& aURISpec, nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  *aURI = nsnull;

  nsCOMPtr<nsIDocument> doc = OwnerDoc();

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                                          aURISpec,
                                                          doc,
                                                          baseURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool equal;
  if (aURISpec.IsEmpty() &&
      doc->GetDocumentURI() &&
      NS_SUCCEEDED(doc->GetDocumentURI()->Equals(uri, &equal)) &&
      equal) {
    // It's not possible for a media resource to be embedded in the current
    // document we extracted aURISpec from, so there's no point returning
    // the current document URI just to let the caller attempt and fail to
    // decode it.
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  uri.forget(aURI);
  return NS_OK;
}

void nsHTMLMediaElement::ProcessMediaFragmentURI()
{
  nsCAutoString ref;
  GetCurrentSpec(ref);
  nsMediaFragmentURIParser parser(ref);
  parser.Parse();
  double start = parser.GetStartTime();
  if (mDecoder) {
    double end = parser.GetEndTime();
    if (end < 0.0 || end > start) {
      mFragmentEnd = end;
    }
    else {
      start = -1.0;
      end = -1.0;
    }
  }
  if (start > 0.0) {
    SetCurrentTime(start);
    mFragmentStart = start;
  }
}

void nsHTMLMediaElement::MetadataLoaded(PRUint32 aChannels, PRUint32 aRate, bool aHasAudio)
{
  mChannels = aChannels;
  mRate = aRate;
  mHasAudio = aHasAudio;
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
  DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  DispatchAsyncEvent(NS_LITERAL_STRING("loadedmetadata"));
  if (mDecoder && mDecoder->IsSeekable()) {
    ProcessMediaFragmentURI();
    mDecoder->SetEndTime(mFragmentEnd);
  }
}

void nsHTMLMediaElement::FirstFrameLoaded(bool aResourceFullyLoaded)
{
  ChangeReadyState(aResourceFullyLoaded ?
    nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA :
    nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
  ChangeDelayLoadStatus(false);

  NS_ASSERTION(!mSuspendedAfterFirstFrame, "Should not have already suspended");

  if (mDecoder && mAllowSuspendAfterFirstFrame && mPaused &&
      !aResourceFullyLoaded &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
      mPreloadAction == nsHTMLMediaElement::PRELOAD_METADATA) {
    mSuspendedAfterFirstFrame = true;
    mDecoder->Suspend();
  } else if (mLoadedFirstFrame &&
             mDownloadSuspendedByCache && 
             mDecoder &&
             !mDecoder->IsEnded()) {
    // We've already loaded the first frame, and the decoder has signalled
    // that the download has been suspended by the media cache. So move
    // readyState into HAVE_ENOUGH_DATA, in case there's script waiting
    // for a "canplaythrough" event; without this forced transition, we will
    // never fire the "canplaythrough" event if the media cache is so small
    // that the download was suspended before the first frame was loaded.
    // Don't force this transition if the decoder is in ended state; the
    // readyState should remain at HAVE_CURRENT_DATA in this case.
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
    return;
  }
}

void nsHTMLMediaElement::ResourceLoaded()
{
  mBegun = false;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  AddRemoveSelfReference();
  if (mReadyState >= nsIDOMHTMLMediaElement::HAVE_METADATA) {
    // MediaStream sources are put into HAVE_CURRENT_DATA state here on setup. If the
    // stream is not blocked, we will receive a notification that will put it
    // into HAVE_ENOUGH_DATA state.
    ChangeReadyState(mStream ? nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA
                     : nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
  }
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
  nsAutoString src;
  GetCurrentSrc(src);
  const PRUnichar* params[] = { src.get() };
  ReportLoadError("MediaLoadDecodeError", params, ArrayLength(params));

  if (mDecoder) {
    ShutdownDecoder();
  }
  mLoadingSrc = nsnull;
  if (mIsLoadingFromSourceChildren) {
    mError = nsnull;
    if (mSourceLoadCandidate) {
      DispatchAsyncSourceError(mSourceLoadCandidate);
      QueueLoadFromSourceTask();
    } else {
      NS_WARNING("Should know the source we were loading from!");
    }
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
  mBegun = false;
  DispatchAsyncEvent(NS_LITERAL_STRING("error"));
  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
    DispatchAsyncEvent(NS_LITERAL_STRING("emptied"));
  } else {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  }
  AddRemoveSelfReference();
  ChangeDelayLoadStatus(false);
}

void nsHTMLMediaElement::PlaybackEnded()
{
  // We changed state which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  NS_ASSERTION(!mDecoder || mDecoder->IsEnded(),
               "Decoder fired ended, but not in ended state");
  if (mStream || (mDecoder && mDecoder->IsInfinite())) {
    LOG(PR_LOG_DEBUG, ("%p, got duration by reaching the end of the resource", this));
    DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::loop)) {
    SetCurrentTime(0);
    return;
  }

  FireTimeUpdate(false);
  DispatchAsyncEvent(NS_LITERAL_STRING("ended"));
}

void nsHTMLMediaElement::SeekStarted()
{
  DispatchAsyncEvent(NS_LITERAL_STRING("seeking"));
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
  FireTimeUpdate(false);
}

void nsHTMLMediaElement::SeekCompleted()
{
  mPlayingBeforeSeek = false;
  SetPlayedOrSeeked(true);
  DispatchAsyncEvent(NS_LITERAL_STRING("seeked"));
  // We changed whether we're seeking so we need to AddRemoveSelfReference
  AddRemoveSelfReference();
}

void nsHTMLMediaElement::NotifySuspendedByCache(bool aIsSuspended)
{
  mDownloadSuspendedByCache = aIsSuspended;
}

void nsHTMLMediaElement::DownloadSuspended()
{
  DispatchAsyncEvent(NS_LITERAL_STRING("progress"));
  if (mBegun) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
    AddRemoveSelfReference();
    DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
  }
}

void nsHTMLMediaElement::DownloadResumed(bool aForceNetworkLoading)
{
  if (mBegun || aForceNetworkLoading) {
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

bool nsHTMLMediaElement::ShouldCheckAllowOrigin()
{
  return mCORSMode != CORS_NONE;
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

  if (mReadyState > nsIDOMHTMLMediaElement::HAVE_METADATA &&
      mDownloadSuspendedByCache &&
      mDecoder &&
      !mDecoder->IsEnded()) {
    // The decoder has signalled that the download has been suspended by the
    // media cache. So move readyState into HAVE_ENOUGH_DATA, in case there's
    // script waiting for a "canplaythrough" event; without this forced
    // transition, we will never fire the "canplaythrough" event if the
    // media cache is too small, and scripts are bound to fail. Don't force
    // this transition if the decoder is in ended state; the readyState
    // should remain at HAVE_CURRENT_DATA in this case.
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
    return;
  }

  if (aNextFrame != NEXT_FRAME_AVAILABLE) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
    if (!mWaitingFired && aNextFrame == NEXT_FRAME_UNAVAILABLE_BUFFERING) {
      FireTimeUpdate(false);
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      mWaitingFired = true;
    }
    return;
  }

  if (mStream) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
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
    mLoadedFirstFrame = true;
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA) {
    mWaitingFired = false;
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

bool nsHTMLMediaElement::CanActivateAutoplay()
{
  return mAutoplaying &&
         mPaused &&
         HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
         mAutoplayEnabled &&
         !IsEditable();
}

void nsHTMLMediaElement::NotifyAutoplayDataReady()
{
  if (CanActivateAutoplay()) {
    mPaused = false;
    // We changed mPaused which can affect AddRemoveSelfReference
    AddRemoveSelfReference();

    if (mDecoder) {
      SetPlayedOrSeeked(true);
      if (mCurrentPlayRangeStart == -1.0) {
        GetCurrentTime(&mCurrentPlayRangeStart);
      }
      mDecoder->Play();
    } else if (mStream) {
      SetPlayedOrSeeked(true);
      GetMediaStream()->ChangeExplicitBlockerCount(-1);
    }
    DispatchAsyncEvent(NS_LITERAL_STRING("play"));
  }
}

VideoFrameContainer* nsHTMLMediaElement::GetVideoFrameContainer()
{
  if (mVideoFrameContainer)
    return mVideoFrameContainer;

  // If we have a print surface, this is just a static image so
  // no image container is required
  if (mPrintSurface)
    return nsnull;

  // Only video frames need an image container.
  nsCOMPtr<nsIDOMHTMLVideoElement> video = do_QueryObject(this);
  if (!video)
    return nsnull;

  mVideoFrameContainer =
    new VideoFrameContainer(this, LayerManager::CreateImageContainer());
  return mVideoFrameContainer;
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

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(OwnerDoc());
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryObject(this));
  NS_ENSURE_TRUE(domDoc && target, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = domDoc->CreateEvent(NS_LITERAL_STRING("MozAudioAvailableEvent"),
                                    getter_AddRefs(event));
  nsCOMPtr<nsIDOMNotifyAudioAvailableEvent> audioavailableEvent(do_QueryInterface(event));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = audioavailableEvent->InitAudioAvailableEvent(NS_LITERAL_STRING("MozAudioAvailable"),
                                                    true, true, frameBuffer.forget(), aFrameBufferLength,
                                                    aTime, mAllowAudioData);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
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

  return nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                              static_cast<nsIContent*>(this),
                                              aName,
                                              false,
                                              true);
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

bool nsHTMLMediaElement::IsPotentiallyPlaying() const
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

bool nsHTMLMediaElement::IsPlaybackEnded() const
{
  // TODO:
  //   the current playback position is equal to the effective end of the media resource.
  //   See bug 449157.
  return mNetworkState >= nsIDOMHTMLMediaElement::HAVE_METADATA &&
    mDecoder ? mDecoder->IsEnded() : false;
}

already_AddRefed<nsIPrincipal> nsHTMLMediaElement::GetCurrentPrincipal()
{
  if (mDecoder) {
    return mDecoder->GetCurrentPrincipal();
  }
  if (mStream) {
    nsRefPtr<nsIPrincipal> principal = mStream->GetPrincipal();
    return principal.forget();
  }
  return nsnull;
}

void nsHTMLMediaElement::NotifyDecoderPrincipalChanged()
{
  for (PRUint32 i = 0; i < mOutputStreams.Length(); ++i) {
    OutputMediaStream* ms = &mOutputStreams[i];
    nsRefPtr<nsIPrincipal> principal = GetCurrentPrincipal();
    ms->mStream->CombineWithPrincipal(principal);
  }
}

void nsHTMLMediaElement::UpdateMediaSize(nsIntSize size)
{
  mMediaSize = size;
}

void nsHTMLMediaElement::NotifyOwnerDocumentActivityChanged()
{
  nsIDocument* ownerDoc = OwnerDoc();
  bool pauseForInactiveDocument =
    !ownerDoc->IsActive() || !ownerDoc->IsVisible();

  if (pauseForInactiveDocument != mPausedForInactiveDocument) {
    mPausedForInactiveDocument = pauseForInactiveDocument;
    if (pauseForInactiveDocument) {
      if (mDecoder) {
        mDecoder->Pause();
        mDecoder->Suspend();
      } else if (mStream) {
        GetMediaStream()->ChangeExplicitBlockerCount(1);
      }
    } else {
      if (mDecoder) {
        mDecoder->Resume(false);
        if (!mPaused && !mDecoder->IsEnded()) {
          mDecoder->Play();
        }
      } else if (mStream) {
        GetMediaStream()->ChangeExplicitBlockerCount(-1);
      }
      DispatchPendingMediaEvents();
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
  nsIDocument* ownerDoc = OwnerDoc();

  // See the comment at the top of this file for the explanation of this
  // boolean expression.
  bool needSelfReference = !mShuttingDown &&
    ownerDoc->IsActive() &&
    (mDelayingLoadEvent ||
     (!mPaused && mDecoder && !mDecoder->IsEnded()) ||
     (!mPaused && mStream && !mStream->IsFinished()) ||
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
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);
  
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    mShuttingDown = true;
    AddRemoveSelfReference();
  }
  return NS_OK;
}

bool
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
  // a new source child to be added, resume the resource selection algorithm.
  if (mLoadWaitStatus == WAITING_FOR_SOURCE) {
    QueueLoadFromSourceTask();
  }
}

nsIContent* nsHTMLMediaElement::GetNextSource()
{
  nsCOMPtr<nsIDOMNode> thisDomNode = do_QueryObject(this);

  mSourceLoadCandidate = nsnull;

  nsresult rv = NS_OK;
  if (!mSourcePointer) {
    // First time this has been run, create a selection to cover children.
    mSourcePointer = new nsRange();

    rv = mSourcePointer->SelectNodeContents(thisDomNode);
    if (NS_FAILED(rv)) return nsnull;

    rv = mSourcePointer->Collapse(true);
    if (NS_FAILED(rv)) return nsnull;
  }

  while (true) {
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
    rv = mSourcePointer->SetStart(thisDomNode, startOffset + 1);
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsIContent* child = GetChildAt(startOffset);

    // If child is a <source> element, it is the next candidate.
    if (child && child->IsHTML(nsGkAtoms::source)) {
      mSourceLoadCandidate = child;
      return child;
    }
  }
  NS_NOTREACHED("Execution should not reach here!");
  return nsnull;
}

void nsHTMLMediaElement::ChangeDelayLoadStatus(bool aDelay)
{
  if (mDelayingLoadEvent == aDelay)
    return;

  mDelayingLoadEvent = aDelay;

  if (aDelay) {
    mLoadBlockedDoc = OwnerDoc();
    mLoadBlockedDoc->BlockOnload();
    LOG(PR_LOG_DEBUG, ("%p ChangeDelayLoadStatus(%d) doc=0x%p", this, aDelay, mLoadBlockedDoc.get()));
  } else {
    if (mDecoder) {
      mDecoder->MoveLoadsToBackground();
    }
    LOG(PR_LOG_DEBUG, ("%p ChangeDelayLoadStatus(%d) doc=0x%p", this, aDelay, mLoadBlockedDoc.get()));
    // mLoadBlockedDoc might be null due to GC unlinking
    if (mLoadBlockedDoc) {
      mLoadBlockedDoc->UnblockOnload(false);
      mLoadBlockedDoc = nsnull;
    }
  }

  // We changed mDelayingLoadEvent which can affect AddRemoveSelfReference
  AddRemoveSelfReference();
}

already_AddRefed<nsILoadGroup> nsHTMLMediaElement::GetDocumentLoadGroup()
{
  if (!OwnerDoc()->IsActive()) {
    NS_WARNING("Load group requested for media element in inactive document.");
  }
  return OwnerDoc()->GetDocumentLoadGroup();
}

nsresult
nsHTMLMediaElement::CopyInnerTo(nsGenericElement* aDest) const
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    nsHTMLMediaElement* dest = static_cast<nsHTMLMediaElement*>(aDest);
    if (mPrintSurface) {
      dest->mPrintSurface = mPrintSurface;
      dest->mMediaSize = mMediaSize;
    } else {
      nsIFrame* frame = GetPrimaryFrame();
      Element* element;
      if (frame && frame->GetType() == nsGkAtoms::HTMLVideoFrame &&
          static_cast<nsVideoFrame*>(frame)->ShouldDisplayPoster()) {
        nsIContent* content = static_cast<nsVideoFrame*>(frame)->GetPosterImage();
        element = content ? content->AsElement() : NULL;
      } else {
        element = const_cast<nsHTMLMediaElement*>(this);
      }

      nsLayoutUtils::SurfaceFromElementResult res =
        nsLayoutUtils::SurfaceFromElement(element,
                                          nsLayoutUtils::SFE_WANT_NEW_SURFACE);
      dest->mPrintSurface = res.mSurface;
      dest->mMediaSize = nsIntSize(res.mSize.width, res.mSize.height);
    }
  }
  return rv;
}

nsresult nsHTMLMediaElement::GetBuffered(nsIDOMTimeRanges** aBuffered)
{
  nsRefPtr<nsTimeRanges> ranges = new nsTimeRanges();
  if (mReadyState >= nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA && mDecoder) {
    // If GetBuffered fails we ignore the error result and just return the
    // time ranges we found up till the error.
    mDecoder->GetBuffered(ranges);
  }
  ranges.forget(aBuffered);
  return NS_OK;
}

void nsHTMLMediaElement::SetRequestHeaders(nsIHttpChannel* aChannel)
{
  // Send Accept header for video and audio types only (Bug 489071)
  SetAcceptHeader(aChannel);

  // Media elements are likely candidates for HTTP Pipeline head of line
  // blocking problems, so disable pipelines.
  nsLoadFlags loadflags;
  aChannel->GetLoadFlags(&loadflags);
  loadflags |= nsIRequest::INHIBIT_PIPELINE;
  aChannel->SetLoadFlags(loadflags);

  // Apache doesn't send Content-Length when gzip transfer encoding is used,
  // which prevents us from estimating the video length (if explicit Content-Duration
  // and a length spec in the container are not present either) and from seeking.
  // So, disable the standard "Accept-Encoding: gzip,deflate" that we usually send.
  // See bug 614760.
  aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept-Encoding"),
                             EmptyCString(), false);

  // Set the Referer header
  aChannel->SetReferrer(OwnerDoc()->GetDocumentURI());
}

void nsHTMLMediaElement::FireTimeUpdate(bool aPeriodic)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  TimeStamp now = TimeStamp::Now();
  double time = 0;
  GetCurrentTime(&time);

  // Fire a timeupdate event if this is not a periodic update (i.e. it's a
  // timeupdate event mandated by the spec), or if it's a periodic update
  // and TIMEUPDATE_MS has passed since the last timeupdate event fired and
  // the time has changed.
  if (!aPeriodic ||
      (mLastCurrentTime != time &&
       (mTimeUpdateTime.IsNull() ||
        now - mTimeUpdateTime >= TimeDuration::FromMilliseconds(TIMEUPDATE_MS)))) {
    DispatchAsyncEvent(NS_LITERAL_STRING("timeupdate"));
    mTimeUpdateTime = now;
    mLastCurrentTime = time;
  }
  if (mFragmentEnd >= 0.0 && time >= mFragmentEnd) {
    Pause();
    mFragmentEnd = -1.0;
    mFragmentStart = -1.0;
    mDecoder->SetEndTime(mFragmentEnd);
  }
}

void nsHTMLMediaElement::GetCurrentSpec(nsCString& aString)
{
  if (mLoadingSrc) {
    mLoadingSrc->GetSpec(aString);
  } else {
    aString.Truncate();
  }
}

/* attribute double initialTime; */
NS_IMETHODIMP nsHTMLMediaElement::GetInitialTime(double *aTime)
{
  // If there is no start fragment then the initalTime is zero.
  // Clamp to duration if it is greater than duration.
  double duration = 0.0;
  nsresult rv = GetDuration(&duration);
  NS_ENSURE_SUCCESS(rv, rv);

  *aTime = mFragmentStart < 0.0 ? 0.0 : mFragmentStart;
  if (*aTime > duration) {
    *aTime = duration;
  }
  return NS_OK;
}

/* attribute double mozFragmentEnd; */
NS_IMETHODIMP nsHTMLMediaElement::GetMozFragmentEnd(double *aTime)
{
  double duration = 0.0;
  nsresult rv = GetDuration(&duration);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there is no end fragment, or the fragment end is greater than the
  // duration, return the duration.
  *aTime = (mFragmentEnd < 0.0 || mFragmentEnd > duration) ? duration : mFragmentEnd;
  return NS_OK;
}

void nsHTMLMediaElement::NotifyAudioAvailableListener()
{
  if (mDecoder) {
    mDecoder->NotifyAudioAvailableListener();
  }
}
