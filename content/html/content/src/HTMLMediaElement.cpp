/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLMediaElementBinding.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Util.h"

#include "base/basictypes.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsIDOMHTMLSourceElement.h"
#include "TimeRanges.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsError.h"
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
#include "MediaError.h"
#include "MediaDecoder.h"
#include "nsICategoryManager.h"
#include "MediaResource.h"

#include "nsIDOMHTMLVideoElement.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsCycleCollectionParticipant.h"
#include "nsICachingChannel.h"
#include "nsLayoutUtils.h"
#include "nsVideoFrame.h"
#include "BasicLayers.h"
#include <limits>
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "nsIDOMNotifyAudioAvailableEvent.h"
#include "nsMediaFragmentURIParser.h"
#include "nsURIHashKey.h"
#include "nsJSUtils.h"
#include "MediaStreamGraph.h"
#include "nsIScriptError.h"
#include "nsHostObjectProtocolHandler.h"
#include "MediaMetadataManager.h"
#include "mozilla/dom/EnableWebAudioCheck.h"

#include "AudioChannelService.h"

#include "nsCSSParser.h"
#include "nsIMediaList.h"

#include "ImageContainer.h"
#include "nsIPowerManagerService.h"
#include <algorithm>

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

#include "nsIPermissionManager.h"

using namespace mozilla::layers;
using mozilla::net::nsMediaFragmentURIParser;

namespace mozilla {
namespace dom {

// Number of milliseconds between timeupdate events as defined by spec
#define TIMEUPDATE_MS 250

// These constants are arbitrary
// Minimum playbackRate for a media
static const double MIN_PLAYBACKRATE = 0.25;
// Maximum playbackRate for a media
static const double MAX_PLAYBACKRATE = 5.0;
// These are the limits beyonds which SoundTouch does not perform too well and when
// speech is hard to understand anyway.
// Threshold above which audio is muted
static const double THRESHOLD_HIGH_PLAYBACKRATE_AUDIO = 4.0;
// Threshold under which audio is muted
static const double THRESHOLD_LOW_PLAYBACKRATE_AUDIO = 0.5;

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

  nsMediaEvent(HTMLMediaElement* aElement) :
    mElement(aElement),
    mLoadID(mElement->GetCurrentLoadID()) {}
  ~nsMediaEvent() {}

  NS_IMETHOD Run() = 0;

protected:
  bool IsCancelled() {
    return mElement->GetCurrentLoadID() != mLoadID;
  }

  nsRefPtr<HTMLMediaElement> mElement;
  uint32_t mLoadID;
};

class nsAsyncEventRunner : public nsMediaEvent
{
private:
  nsString mName;

public:
  nsAsyncEventRunner(const nsAString& aName, HTMLMediaElement* aElement) :
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
  nsSourceErrorEventRunner(HTMLMediaElement* aElement,
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
                                                false);
  }
};

/**
 * There is a reference cycle involving this class: MediaLoadListener
 * holds a reference to the HTMLMediaElement, which holds a reference
 * to an nsIChannel, which holds a reference to this listener.
 * We break the reference cycle in OnStartRequest by clearing mElement.
 */
class HTMLMediaElement::MediaLoadListener MOZ_FINAL : public nsIStreamListener,
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
  MediaLoadListener(HTMLMediaElement* aElement)
    : mElement(aElement),
      mLoadID(aElement->GetCurrentLoadID())
  {
    NS_ABORT_IF_FALSE(mElement, "Must pass an element to call back");
  }

private:
  nsRefPtr<HTMLMediaElement> mElement;
  nsCOMPtr<nsIStreamListener> mNextListener;
  uint32_t mLoadID;
};

NS_IMPL_ISUPPORTS5(HTMLMediaElement::MediaLoadListener, nsIRequestObserver,
                   nsIStreamListener, nsIChannelEventSink,
                   nsIInterfaceRequestor, nsIObserver)

NS_IMETHODIMP
HTMLMediaElement::MediaLoadListener::Observe(nsISupports* aSubject,
                                             const char* aTopic, const PRUnichar* aData)
{
  nsContentUtils::UnregisterShutdownObserver(this);

  // Clear mElement to break cycle so we don't leak on shutdown
  mElement = nullptr;
  return NS_OK;
}

void HTMLMediaElement::ReportLoadError(const char* aMsg,
                                       const PRUnichar** aParams,
                                       uint32_t aParamCount)
{
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "Media",
                                  OwnerDoc(),
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aMsg,
                                  aParams,
                                  aParamCount);
}


NS_IMETHODIMP HTMLMediaElement::MediaLoadListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  nsContentUtils::UnregisterShutdownObserver(this);

  if (!mElement) {
    // We've been notified by the shutdown observer, and are shutting down.
    return NS_BINDING_ABORTED;
  }

  // The element is only needed until we've had a chance to call
  // InitializeDecoderForChannel. So make sure mElement is cleared here.
  nsRefPtr<HTMLMediaElement> element;
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
    uint32_t responseStatus = 0;
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

NS_IMETHODIMP HTMLMediaElement::MediaLoadListener::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                                                                 nsresult aStatus)
{
  if (mNextListener) {
    return mNextListener->OnStopRequest(aRequest, aContext, aStatus);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLMediaElement::MediaLoadListener::OnDataAvailable(nsIRequest* aRequest,
                                                     nsISupports* aContext,
                                                     nsIInputStream* aStream,
                                                     uint64_t aOffset,
                                                     uint32_t aCount)
{
  if (!mNextListener) {
    NS_ERROR("Must have a chained listener; OnStartRequest should have canceled this request");
    return NS_BINDING_ABORTED;
  }
  return mNextListener->OnDataAvailable(aRequest, aContext, aStream, aOffset, aCount);
}

NS_IMETHODIMP HTMLMediaElement::MediaLoadListener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                                                          nsIChannel* aNewChannel,
                                                                          uint32_t aFlags,
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

NS_IMETHODIMP HTMLMediaElement::MediaLoadListener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

NS_IMPL_ADDREF_INHERITED(HTMLMediaElement, nsGenericHTMLElement)
NS_IMPL_RELEASE_INHERITED(HTMLMediaElement, nsGenericHTMLElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLMediaElement, nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSrcStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSrcAttrStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourcePointer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoadBlockedDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceLoadCandidate)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioChannelAgent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
  for (uint32_t i = 0; i < tmp->mOutputStreams.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOutputStreams[i].mStream);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlayed);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLMediaElement, nsGenericHTMLElement)
  if (tmp->mSrcStream) {
    // Need to EndMediaStreamPlayback to clear mStream and make sure everything
    // gets unhooked correctly.
    tmp->EndSrcMediaStreamPlayback();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSrcAttrStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourcePointer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoadBlockedDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceLoadCandidate)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAudioChannelAgent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
  for (uint32_t i = 0; i < tmp->mOutputStreams.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mOutputStreams[i].mStream);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPlayed);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLMediaElement)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

// nsIDOMHTMLMediaElement
NS_IMPL_URI_ATTR(HTMLMediaElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLMediaElement, CrossOrigin, crossorigin)
NS_IMPL_BOOL_ATTR(HTMLMediaElement, Controls, controls)
NS_IMPL_BOOL_ATTR(HTMLMediaElement, Autoplay, autoplay)
NS_IMPL_BOOL_ATTR(HTMLMediaElement, Loop, loop)
NS_IMPL_BOOL_ATTR(HTMLMediaElement, DefaultMuted, muted)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(HTMLMediaElement, Preload, preload, nullptr)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(HTMLMediaElement, MozAudioChannelType, mozaudiochannel, "normal")

already_AddRefed<DOMMediaStream>
HTMLMediaElement::GetMozSrcObject() const
{
  NS_ASSERTION(!mSrcAttrStream || mSrcAttrStream->GetStream(),
               "MediaStream should have been set up properly");
  nsRefPtr<DOMMediaStream> stream = mSrcAttrStream;
  return stream.forget();
}

NS_IMETHODIMP
HTMLMediaElement::GetMozSrcObject(nsIDOMMediaStream** aStream)
{
  nsRefPtr<DOMMediaStream> stream = GetMozSrcObject();
  stream.forget(aStream);
  return NS_OK;
}

void
HTMLMediaElement::SetMozSrcObject(DOMMediaStream& aValue)
{
  mSrcAttrStream = &aValue;
  Load();
}

NS_IMETHODIMP
HTMLMediaElement::SetMozSrcObject(nsIDOMMediaStream* aStream)
{
  DOMMediaStream* stream = static_cast<DOMMediaStream*>(aStream);
  SetMozSrcObject(*stream);
  return NS_OK;
}

/* readonly attribute nsIDOMHTMLMediaElement mozAutoplayEnabled; */
NS_IMETHODIMP HTMLMediaElement::GetMozAutoplayEnabled(bool *aAutoplayEnabled)
{
  *aAutoplayEnabled = mAutoplayEnabled;

  return NS_OK;
}

/* readonly attribute nsIDOMMediaError error; */
NS_IMETHODIMP HTMLMediaElement::GetError(nsIDOMMediaError * *aError)
{
  NS_IF_ADDREF(*aError = mError);

  return NS_OK;
}

/* readonly attribute boolean ended; */
bool
HTMLMediaElement::Ended()
{
  if (mSrcStream) {
    return GetSrcMediaStream()->IsFinished();
  }

  if (mDecoder) {
    return mDecoder->IsEnded();
  }

  return false;
}

NS_IMETHODIMP HTMLMediaElement::GetEnded(bool* aEnded)
{
  *aEnded = Ended();
  return NS_OK;
}

/* readonly attribute DOMString currentSrc; */
NS_IMETHODIMP HTMLMediaElement::GetCurrentSrc(nsAString & aCurrentSrc)
{
  nsAutoCString src;
  GetCurrentSpec(src);
  aCurrentSrc = NS_ConvertUTF8toUTF16(src);
  return NS_OK;
}

/* readonly attribute unsigned short networkState; */
NS_IMETHODIMP HTMLMediaElement::GetNetworkState(uint16_t* aNetworkState)
{
  *aNetworkState = NetworkState();
  return NS_OK;
}

nsresult
HTMLMediaElement::OnChannelRedirect(nsIChannel* aChannel,
                                    nsIChannel* aNewChannel,
                                    uint32_t aFlags)
{
  NS_ASSERTION(aChannel == mChannel, "Channels should match!");
  mChannel = aNewChannel;

  // Handle forwarding of Range header so that the intial detection
  // of seeking support (via result code 206) works across redirects.
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aChannel);
  NS_ENSURE_STATE(http);

  NS_NAMED_LITERAL_CSTRING(rangeHdr, "Range");

  nsAutoCString rangeVal;
  if (NS_SUCCEEDED(http->GetRequestHeader(rangeHdr, rangeVal))) {
    NS_ENSURE_STATE(!rangeVal.IsEmpty());

    http = do_QueryInterface(aNewChannel);
    NS_ENSURE_STATE(http);

    nsresult rv = http->SetRequestHeader(rangeHdr, rangeVal, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void HTMLMediaElement::ShutdownDecoder()
{
  RemoveMediaElementFromURITable();
  NS_ASSERTION(mDecoder, "Must have decoder to shut down");
  mDecoder->Shutdown();
  mDecoder = nullptr;
}

void HTMLMediaElement::AbortExistingLoads()
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
  if (mSrcStream) {
    EndSrcMediaStreamPlayback();
  }
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
  }

  mLoadingSrc = nullptr;

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING ||
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_IDLE)
  {
    DispatchEvent(NS_LITERAL_STRING("abort"));
  }

  mError = nullptr;
  mLoadedFirstFrame = false;
  mAutoplaying = true;
  mIsLoadingFromSourceChildren = false;
  mSuspendedAfterFirstFrame = false;
  mAllowSuspendAfterFirstFrame = true;
  mHaveQueuedSelectResource = false;
  mSuspendedForPreloadNone = false;
  mDownloadSuspendedByCache = false;
  mSourcePointer = nullptr;

  mChannels = 0;
  mRate = 0;
  mTags = nullptr;

  if (mNetworkState != nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_EMPTY;
    NS_ASSERTION(!mDecoder && !mSrcStream, "How did someone setup a new stream/decoder already?");
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

void HTMLMediaElement::NoSupportedMediaSourceError()
{
  NS_ASSERTION(mDelayingLoadEvent, "Load event not delayed during source selection?");

  mError = new MediaError(this, nsIDOMMediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
  DispatchAsyncEvent(NS_LITERAL_STRING("error"));
  // This clears mDelayingLoadEvent, so AddRemoveSelfReference will be called
  ChangeDelayLoadStatus(false);
}

typedef void (HTMLMediaElement::*SyncSectionFn)();

// Runs a "synchronous section", a function that must run once the event loop
// has reached a "stable state". See:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/webappapis.html#synchronous-section
class nsSyncSection : public nsMediaEvent
{
private:
  SyncSectionFn mClosure;
public:
  nsSyncSection(HTMLMediaElement* aElement,
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
void AsyncAwaitStableState(HTMLMediaElement* aElement,
                           SyncSectionFn aClosure)
{
  nsCOMPtr<nsIRunnable> event = new nsSyncSection(aElement, aClosure);
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  appShell->RunInStableState(event);
}

void HTMLMediaElement::QueueLoadFromSourceTask()
{
  ChangeDelayLoadStatus(true);
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
  AsyncAwaitStableState(this, &HTMLMediaElement::LoadFromSourceChildren);
}

void HTMLMediaElement::QueueSelectResourceTask()
{
  // Don't allow multiple async select resource calls to be queued.
  if (mHaveQueuedSelectResource)
    return;
  mHaveQueuedSelectResource = true;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE;
  AsyncAwaitStableState(this, &HTMLMediaElement::SelectResourceWrapper);
}

/* void load (); */
NS_IMETHODIMP HTMLMediaElement::Load()
{
  if (mIsRunningLoadMethod)
    return NS_OK;

  SetPlayedOrSeeked(false);
  mIsRunningLoadMethod = true;
  AbortExistingLoads();
  SetPlaybackRate(mDefaultPlaybackRate);
  QueueSelectResourceTask();
  ResetState();
  mIsRunningLoadMethod = false;

  return NS_OK;
}

void HTMLMediaElement::ResetState()
{
  mMediaSize = nsIntSize(-1, -1);
  VideoFrameContainer* container = GetVideoFrameContainer();
  if (container) {
    container->Reset();
  }
}

static bool HasSourceChildren(nsIContent* aElement)
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

void HTMLMediaElement::SelectResourceWrapper()
{
  SelectResource();
  mIsRunningSelectResource = false;
  mHaveQueuedSelectResource = false;
}

void HTMLMediaElement::SelectResource()
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
    SetupSrcMediaStreamPlayback(mSrcAttrStream);
  } else if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NewURIFromString(src, getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      LOG(PR_LOG_DEBUG, ("%p Trying load from src=%s", this, NS_ConvertUTF16toUTF8(src).get()));
      NS_ASSERTION(!mIsLoadingFromSourceChildren,
        "Should think we're not loading from source children by default");

      mLoadingSrc = uri;
      if (mPreloadAction == HTMLMediaElement::PRELOAD_NONE) {
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

void HTMLMediaElement::NotifyLoadError()
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

void HTMLMediaElement::NotifyAudioAvailable(float* aFrameBuffer,
                                            uint32_t aFrameBufferLength,
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

void HTMLMediaElement::LoadFromSourceChildren()
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
      cssParser.ParseMediaList(media, nullptr, 0, mediaList, false);
      nsIPresShell* presShell = OwnerDoc()->GetShell();
      if (presShell && !mediaList->Matches(presShell->GetPresContext(), nullptr)) {
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

    if (mPreloadAction == HTMLMediaElement::PRELOAD_NONE) {
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

void HTMLMediaElement::SuspendLoad()
{
  mSuspendedForPreloadNone = true;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
  ChangeDelayLoadStatus(false);
}

void HTMLMediaElement::ResumeLoad(PreloadAction aAction)
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

static bool UseAudioChannelService()
{
  return Preferences::GetBool("media.useAudioChannelService");
}

void HTMLMediaElement::UpdatePreloadAction()
{
  PreloadAction nextAction = PRELOAD_UNDEFINED;
  // If autoplay is set, or we're playing, we should always preload data,
  // as we'll need it to play.
  if ((IsAutoplayEnabled() && HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay)) ||
      !mPaused)
  {
    nextAction = HTMLMediaElement::PRELOAD_ENOUGH;
  } else {
    // Find the appropriate preload action by looking at the attribute.
    const nsAttrValue* val = mAttrsAndChildren.GetAttr(nsGkAtoms::preload,
                                                       kNameSpaceID_None);
    uint32_t preloadDefault =
      Preferences::GetInt("media.preload.default",
                          HTMLMediaElement::PRELOAD_ATTR_METADATA);
    uint32_t preloadAuto =
      Preferences::GetInt("media.preload.auto",
                          HTMLMediaElement::PRELOAD_ENOUGH);
    if (!val) {
      // Attribute is not set. Use the preload action specified by the
      // media.preload.default pref, or just preload metadata if not present.
      nextAction = static_cast<PreloadAction>(preloadDefault);
    } else if (val->Type() == nsAttrValue::eEnum) {
      PreloadAttrValue attr = static_cast<PreloadAttrValue>(val->GetEnumValue());
      if (attr == HTMLMediaElement::PRELOAD_ATTR_EMPTY ||
          attr == HTMLMediaElement::PRELOAD_ATTR_AUTO)
      {
        nextAction = static_cast<PreloadAction>(preloadAuto);
      } else if (attr == HTMLMediaElement::PRELOAD_ATTR_METADATA) {
        nextAction = HTMLMediaElement::PRELOAD_METADATA;
      } else if (attr == HTMLMediaElement::PRELOAD_ATTR_NONE) {
        nextAction = HTMLMediaElement::PRELOAD_NONE;
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
  if (nextAction == HTMLMediaElement::PRELOAD_ENOUGH) {
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

  } else if (nextAction == HTMLMediaElement::PRELOAD_METADATA) {
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

nsresult HTMLMediaElement::LoadResource()
{
  NS_ASSERTION(mDelayingLoadEvent,
               "Should delay load event (if in document) during load");

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_MEDIA,
                                          mLoadingSrc,
                                          NodePrincipal(),
                                          static_cast<Element*>(this),
                                          EmptyCString(), // mime type
                                          nullptr, // extra
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_FAILURE;
  }

  // Set the media element's CORS mode only when loading a resource
  mCORSMode = AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));

  HTMLMediaElement* other = LookupMediaElementURITable(mLoadingSrc);
  if (other && other->mDecoder) {
    // Clone it.
    nsresult rv = InitializeDecoderAsClone(other->mDecoder);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  if (IsMediaStreamURI(mLoadingSrc)) {
    nsCOMPtr<nsIDOMMediaStream> stream;
    rv = NS_GetStreamForMediaStreamURI(mLoadingSrc, getter_AddRefs(stream));
    if (NS_FAILED(rv)) {
      nsCString specUTF8;
      mLoadingSrc->GetSpec(specUTF8);
      NS_ConvertUTF8toUTF16 spec(specUTF8);
      const PRUnichar* params[] = { spec.get() };
      ReportLoadError("MediaLoadInvalidURI", params, ArrayLength(params));
      return rv;
    }
    SetupSrcMediaStreamPlayback(static_cast<DOMMediaStream*>(stream.get()));
    return NS_OK;
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
                     nullptr,
                     loadGroup,
                     nullptr,
                     nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY |
                     nsIChannel::LOAD_TREAT_APPLICATION_OCTET_STREAM_AS_UNKNOWN,
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
    nsRefPtr<nsCORSListenerProxy> corsListener =
      new nsCORSListenerProxy(loadListener,
                              NodePrincipal(),
                              GetCORSMode() == CORS_USE_CREDENTIALS);
    rv = corsListener->Init(channel);
    NS_ENSURE_SUCCESS(rv, rv);
    listener = corsListener;
  } else {
    rv = nsContentUtils::GetSecurityManager()->
           CheckLoadURIWithPrincipal(NodePrincipal(),
                                     mLoadingSrc,
                                     nsIScriptSecurityManager::STANDARD);
    listener = loadListener;
    NS_ENSURE_SUCCESS(rv, rv);
  }

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

  rv = channel->AsyncOpen(listener, nullptr);
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

nsresult HTMLMediaElement::LoadWithChannel(nsIChannel* aChannel,
                                           nsIStreamListener** aListener)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aListener);

  *aListener = nullptr;

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

  SetPlaybackRate(mDefaultPlaybackRate);
  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));

  return NS_OK;
}

void
HTMLMediaElement::MozLoadFrom(HTMLMediaElement& aOther, ErrorResult& aRv)
{
  // Make sure we don't reenter during synchronous abort events.
  if (mIsRunningLoadMethod) {
    return;
  }

  mIsRunningLoadMethod = true;
  AbortExistingLoads();
  mIsRunningLoadMethod = false;

  if (!aOther.mDecoder) {
    return;
  }

  ChangeDelayLoadStatus(true);

  mLoadingSrc = aOther.mLoadingSrc;
  aRv = InitializeDecoderAsClone(aOther.mDecoder);
  if (aRv.Failed()) {
    ChangeDelayLoadStatus(false);
    return;
  }

  SetPlaybackRate(mDefaultPlaybackRate);
  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));
}

NS_IMETHODIMP HTMLMediaElement::MozLoadFrom(nsIDOMHTMLMediaElement* aOther)
{
  NS_ENSURE_ARG_POINTER(aOther);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aOther);
  HTMLMediaElement* other = static_cast<HTMLMediaElement*>(content.get());

  if (!other) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  MozLoadFrom(*other, rv);

  return rv.ErrorCode();
}

/* readonly attribute unsigned short readyState; */
NS_IMETHODIMP HTMLMediaElement::GetReadyState(uint16_t* aReadyState)
{
  *aReadyState = ReadyState();

  return NS_OK;
}

/* readonly attribute boolean seeking; */
bool
HTMLMediaElement::Seeking() const
{
  return mDecoder && mDecoder->IsSeeking();
}

NS_IMETHODIMP HTMLMediaElement::GetSeeking(bool* aSeeking)
{
  *aSeeking = Seeking();
  return NS_OK;
}

/* attribute double currentTime; */
double
HTMLMediaElement::CurrentTime() const
{
  if (mSrcStream) {
    return MediaTimeToSeconds(GetSrcMediaStream()->GetCurrentTime());
  }

  if (mDecoder) {
    return mDecoder->GetCurrentTime();
  }

  return 0.0;
}

NS_IMETHODIMP HTMLMediaElement::GetCurrentTime(double* aCurrentTime)
{
  *aCurrentTime = CurrentTime();
  return NS_OK;
}

void
HTMLMediaElement::SetCurrentTime(double aCurrentTime, ErrorResult& aRv)
{
  MOZ_ASSERT(aCurrentTime == aCurrentTime);

  StopSuspendingAfterFirstFrame();

  if (mSrcStream) {
    // do nothing since streams aren't seekable; we effectively clamp to
    // the current time.
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mCurrentPlayRangeStart != -1.0) {
    double rangeEndTime = CurrentTime();
    LOG(PR_LOG_DEBUG, ("%p Adding \'played\' a range : [%f, %f]", this, mCurrentPlayRangeStart, rangeEndTime));
    // Multiple seek without playing, or seek while playing.
    if (mCurrentPlayRangeStart != rangeEndTime) {
      mPlayed->Add(mCurrentPlayRangeStart, rangeEndTime);
    }
  }

  if (!mDecoder) {
    LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) failed: no decoder", this, aCurrentTime));
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) failed: no source", this, aCurrentTime));
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Clamp the time to [0, duration] as required by the spec.
  double clampedTime = std::max(0.0, aCurrentTime);
  double duration = mDecoder->GetDuration();
  if (duration >= 0) {
    clampedTime = std::min(clampedTime, duration);
  }

  mPlayingBeforeSeek = IsPotentiallyPlaying();
  // The media backend is responsible for dispatching the timeupdate
  // event if it changes the playback position as a result of the seek.
  LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) starting seek", this, aCurrentTime));
  aRv = mDecoder->Seek(clampedTime);
  // Start a new range at position we seeked to.
  mCurrentPlayRangeStart = mDecoder->GetCurrentTime();

  // We changed whether we're seeking so we need to AddRemoveSelfReference.
  AddRemoveSelfReference();
}

NS_IMETHODIMP HTMLMediaElement::SetCurrentTime(double aCurrentTime)
{
  // Detect for a NaN and invalid values.
  if (aCurrentTime != aCurrentTime) {
    LOG(PR_LOG_DEBUG, ("%p SetCurrentTime(%f) failed: bad time", this, aCurrentTime));
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  SetCurrentTime(aCurrentTime, rv);
  return rv.ErrorCode();
}

/* readonly attribute double duration; */
double
HTMLMediaElement::Duration() const
{
  if (mSrcStream) {
    return std::numeric_limits<double>::infinity();
  }

  if (mDecoder) {
    return mDecoder->GetDuration();
  }

  return std::numeric_limits<double>::quiet_NaN();
}

NS_IMETHODIMP HTMLMediaElement::GetDuration(double* aDuration)
{
  *aDuration = Duration();
  return NS_OK;
}

already_AddRefed<TimeRanges>
HTMLMediaElement::Seekable() const
{
  nsRefPtr<TimeRanges> ranges = new TimeRanges();
  if (mDecoder && mReadyState > nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    mDecoder->GetSeekable(ranges);
  }
  return ranges.forget();
}

/* readonly attribute nsIDOMHTMLTimeRanges seekable; */
NS_IMETHODIMP HTMLMediaElement::GetSeekable(nsIDOMTimeRanges** aSeekable)
{
  nsRefPtr<TimeRanges> ranges = Seekable();
  ranges.forget(aSeekable);
  return NS_OK;
}

/* readonly attribute boolean paused; */
NS_IMETHODIMP HTMLMediaElement::GetPaused(bool* aPaused)
{
  *aPaused = Paused();

  return NS_OK;
}

already_AddRefed<TimeRanges>
HTMLMediaElement::Played()
{
  nsRefPtr<TimeRanges> ranges = new TimeRanges();

  uint32_t timeRangeCount = 0;
  mPlayed->GetLength(&timeRangeCount);
  for (uint32_t i = 0; i < timeRangeCount; i++) {
    double begin;
    double end;
    mPlayed->Start(i, &begin);
    mPlayed->End(i, &end);
    ranges->Add(begin, end);
  }

  if (mCurrentPlayRangeStart != -1.0) {
    double now = CurrentTime();
    if (mCurrentPlayRangeStart != now) {
      ranges->Add(mCurrentPlayRangeStart, now);
    }
  }

  ranges->Normalize();
  return ranges.forget();
}

/* readonly attribute nsIDOMHTMLTimeRanges played; */
NS_IMETHODIMP HTMLMediaElement::GetPlayed(nsIDOMTimeRanges** aPlayed)
{
  nsRefPtr<TimeRanges> ranges = Played();
  ranges.forget(aPlayed);
  return NS_OK;
}

/* void pause (); */
void
HTMLMediaElement::Pause(ErrorResult& aRv)
{
  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    LOG(PR_LOG_DEBUG, ("Loading due to Pause()"));
    aRv = Load();
    if (aRv.Failed()) {
      return;
    }
  } else if (mDecoder) {
    mDecoder->Pause();
  }

  bool oldPaused = mPaused;
  mPaused = true;
  mAutoplaying = false;
  // We changed mPaused and mAutoplaying which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  if (!oldPaused) {
    if (mSrcStream) {
      GetSrcMediaStream()->ChangeExplicitBlockerCount(1);
    }
    FireTimeUpdate(false);
    DispatchAsyncEvent(NS_LITERAL_STRING("pause"));
  }
}

NS_IMETHODIMP HTMLMediaElement::Pause()
{
  ErrorResult rv;
  Pause(rv);
  return rv.ErrorCode();
}

/* attribute double volume; */
NS_IMETHODIMP HTMLMediaElement::GetVolume(double* aVolume)
{
  *aVolume = Volume();
  return NS_OK;
}

void
HTMLMediaElement::SetVolume(double aVolume, ErrorResult& aRv)
{
  if (aVolume < 0.0 || aVolume > 1.0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (aVolume == mVolume)
    return;

  mVolume = aVolume;

  if (!mMuted) {
    if (mDecoder) {
      mDecoder->SetVolume(mVolume);
    } else if (mAudioStream) {
      mAudioStream->SetVolume(mVolume);
    } else if (mSrcStream) {
      GetSrcMediaStream()->SetAudioOutputVolume(this, float(mVolume));
    }
  }

  DispatchAsyncEvent(NS_LITERAL_STRING("volumechange"));
}

NS_IMETHODIMP HTMLMediaElement::SetVolume(double aVolume)
{
  ErrorResult rv;
  SetVolume(aVolume, rv);
  return rv.ErrorCode();
}

uint32_t
HTMLMediaElement::GetMozChannels(ErrorResult& aRv) const
{
  if (!mDecoder && !mAudioStream) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return 0;
  }

  return mChannels;
}

NS_IMETHODIMP
HTMLMediaElement::GetMozChannels(uint32_t* aMozChannels)
{
  ErrorResult rv;
  *aMozChannels = GetMozChannels(rv);
 return rv.ErrorCode();
}

uint32_t
HTMLMediaElement::GetMozSampleRate(ErrorResult& aRv) const
{
  if (!mDecoder && !mAudioStream) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return 0;
  }

  return mRate;
}

NS_IMETHODIMP
HTMLMediaElement::GetMozSampleRate(uint32_t* aMozSampleRate)
{
  ErrorResult rv;
  *aMozSampleRate = GetMozSampleRate(rv);
  return rv.ErrorCode();
}

// Helper struct with arguments for our hash iterator.
typedef struct {
  JSContext* cx;
  JSObject*  tags;
  bool error;
} MetadataIterCx;

PLDHashOperator
HTMLMediaElement::BuildObjectFromTags(nsCStringHashKey::KeyType aKey,
                                      nsCString aValue,
                                      void* aUserArg)
{
  MetadataIterCx* args = static_cast<MetadataIterCx*>(aUserArg);

  nsString wideValue = NS_ConvertUTF8toUTF16(aValue);
  JSString* string = JS_NewUCStringCopyZ(args->cx, wideValue.Data());
  if (!string) {
    NS_WARNING("Failed to perform string copy");
    args->error = true;
    return PL_DHASH_STOP;
  }
  JS::Value value = STRING_TO_JSVAL(string);
  if (!JS_DefineProperty(args->cx, args->tags, aKey.Data(), value,
                         nullptr, nullptr, JSPROP_ENUMERATE)) {
    NS_WARNING("Failed to set metadata property");
    args->error = true;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

JSObject*
HTMLMediaElement::MozGetMetadata(JSContext* cx, ErrorResult& aRv)
{
  if (mReadyState < nsIDOMHTMLMediaElement::HAVE_METADATA) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  JSObject* tags = JS_NewObject(cx, nullptr, nullptr, nullptr);
  if (!tags) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  if (mTags) {
    MetadataIterCx iter = {cx, tags, false};
    mTags->EnumerateRead(BuildObjectFromTags, static_cast<void*>(&iter));
    if (iter.error) {
      NS_WARNING("couldn't create metadata object!");
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }

  return tags;
}

NS_IMETHODIMP
HTMLMediaElement::MozGetMetadata(JSContext* cx, JS::Value* aValue)
{
  ErrorResult rv;

  JSObject* obj = MozGetMetadata(cx, rv);
  if (!rv.Failed()) {
    MOZ_ASSERT(obj);
    *aValue = JS::ObjectValue(*obj);
  }

  return rv.ErrorCode();
}

uint32_t
HTMLMediaElement::GetMozFrameBufferLength(ErrorResult& aRv) const
{
  // The framebuffer (via MozAudioAvailable events) is only available
  // when reading vs. writing audio directly.
  if (!mDecoder) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return 0;
  }

  return mDecoder->GetFrameBufferLength();
}

NS_IMETHODIMP
HTMLMediaElement::GetMozFrameBufferLength(uint32_t* aMozFrameBufferLength)
{
  ErrorResult rv;
  *aMozFrameBufferLength = GetMozFrameBufferLength(rv);
  return rv.ErrorCode();
}

void
HTMLMediaElement::SetMozFrameBufferLength(uint32_t aMozFrameBufferLength, ErrorResult& aRv)
{
  if (!mDecoder) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  aRv = mDecoder->RequestFrameBufferLength(aMozFrameBufferLength);
}

NS_IMETHODIMP
HTMLMediaElement::SetMozFrameBufferLength(uint32_t aMozFrameBufferLength)
{
  ErrorResult rv;
  SetMozFrameBufferLength(aMozFrameBufferLength, rv);
  return rv.ErrorCode();
}

/* attribute boolean muted; */
NS_IMETHODIMP HTMLMediaElement::GetMuted(bool* aMuted)
{
  *aMuted = Muted();
  return NS_OK;
}

void HTMLMediaElement::SetMutedInternal(bool aMuted)
{
  float effectiveVolume = aMuted ? 0.0f : float(mVolume);

  if (mDecoder) {
    mDecoder->SetVolume(effectiveVolume);
  } else if (mAudioStream) {
    mAudioStream->SetVolume(effectiveVolume);
  } else if (mSrcStream) {
    GetSrcMediaStream()->SetAudioOutputVolume(this, effectiveVolume);
  }
}

NS_IMETHODIMP HTMLMediaElement::SetMuted(bool aMuted)
{
  if (aMuted == mMuted)
    return NS_OK;

  mMuted = aMuted;
  SetMutedInternal(aMuted);

  DispatchAsyncEvent(NS_LITERAL_STRING("volumechange"));
  return NS_OK;
}

already_AddRefed<DOMMediaStream>
HTMLMediaElement::CaptureStreamInternal(bool aFinishWhenEnded)
{
  nsIDOMWindow* window = OwnerDoc()->GetInnerWindow();
  if (!window) {
    return nullptr;
  }

  OutputMediaStream* out = mOutputStreams.AppendElement();
  out->mStream = DOMMediaStream::CreateTrackUnionStream(window);
  nsRefPtr<nsIPrincipal> principal = GetCurrentPrincipal();
  out->mStream->CombineWithPrincipal(principal);
  out->mFinishWhenEnded = aFinishWhenEnded;

  mAudioCaptured = true;
  // Block the output stream initially.
  // Decoders are responsible for removing the block while they are playing
  // back into the output stream.
  out->mStream->GetStream()->ChangeExplicitBlockerCount(1);
  if (mDecoder) {
    mDecoder->SetAudioCaptured(true);
    mDecoder->AddOutputStream(
        out->mStream->GetStream()->AsProcessedStream(), aFinishWhenEnded);
  }
  nsRefPtr<DOMMediaStream> result = out->mStream;
  return result.forget();
}

already_AddRefed<DOMMediaStream>
HTMLMediaElement::MozCaptureStream(ErrorResult& aRv)
{
  nsRefPtr<DOMMediaStream> stream = CaptureStreamInternal(false);
  if (!stream) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return stream.forget();
}

NS_IMETHODIMP HTMLMediaElement::MozCaptureStream(nsIDOMMediaStream** aStream)
{
  ErrorResult rv;
  *aStream = MozCaptureStream(rv).get();
  return rv.ErrorCode();
}

already_AddRefed<DOMMediaStream>
HTMLMediaElement::MozCaptureStreamUntilEnded(ErrorResult& aRv)
{
  nsRefPtr<DOMMediaStream> stream = CaptureStreamInternal(true);
  if (!stream) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return stream.forget();
}

NS_IMETHODIMP HTMLMediaElement::MozCaptureStreamUntilEnded(nsIDOMMediaStream** aStream)
{
  ErrorResult rv;
  *aStream = MozCaptureStreamUntilEnded(rv).get();
  return rv.ErrorCode();
}

NS_IMETHODIMP HTMLMediaElement::GetMozAudioCaptured(bool* aCaptured)
{
  *aCaptured = MozAudioCaptured();
  return NS_OK;
}

class MediaElementSetForURI : public nsURIHashKey {
public:
  MediaElementSetForURI(const nsIURI* aKey) : nsURIHashKey(aKey) {}
  MediaElementSetForURI(const MediaElementSetForURI& toCopy)
    : nsURIHashKey(toCopy), mElements(toCopy.mElements) {}
  nsTArray<HTMLMediaElement*> mElements;
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
MediaElementTableCount(HTMLMediaElement* aElement, nsIURI* aURI)
{
  if (!gElementTable || !aElement || !aURI) {
    return 0;
  }
  MediaElementSetForURI* entry = gElementTable->GetEntry(aURI);
  if (!entry) {
    return 0;
  }
  uint32_t count = 0;
  for (uint32_t i = 0; i < entry->mElements.Length(); ++i) {
    HTMLMediaElement* elem = entry->mElements[i];
    if (elem == aElement) {
      count++;
    }
  }
  return count;
}
#endif

void
HTMLMediaElement::AddMediaElementToURITable()
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
HTMLMediaElement::RemoveMediaElementFromURITable()
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
      gElementTable = nullptr;
    }
  }
  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 0,
    "After remove, should no longer have an entry in element table");
}

HTMLMediaElement*
HTMLMediaElement::LookupMediaElementURITable(nsIURI* aURI)
{
  if (!gElementTable)
    return nullptr;
  MediaElementSetForURI* entry = gElementTable->GetEntry(aURI);
  if (!entry)
    return nullptr;
  for (uint32_t i = 0; i < entry->mElements.Length(); ++i) {
    HTMLMediaElement* elem = entry->mElements[i];
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
  return nullptr;
}

HTMLMediaElement::HTMLMediaElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mSrcStreamListener(nullptr),
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
    mDefaultPlaybackRate(1.0),
    mPlaybackRate(1.0),
    mPreservesPitch(true),
    mPlayed(new TimeRanges),
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
    mPausedForInactiveDocumentOrChannel(false),
    mEventDeliveryPaused(false),
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
    mDownloadSuspendedByCache(false),
    mAudioChannelType(AUDIO_CHANNEL_NORMAL),
    mChannelSuspended(false),
    mPlayingThroughTheAudioChannel(false)
{
#ifdef PR_LOGGING
  if (!gMediaElementLog) {
    gMediaElementLog = PR_NewLogModule("nsMediaElement");
  }
  if (!gMediaElementEventsLog) {
    gMediaElementEventsLog = PR_NewLogModule("nsMediaElementEvents");
  }
#endif

  mPaused.SetOuter(this);

  RegisterFreezableElement();
  NotifyOwnerDocumentActivityChanged();
}

HTMLMediaElement::~HTMLMediaElement()
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
  if (mSrcStream) {
    EndSrcMediaStreamPlayback();
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

void
HTMLMediaElement::GetItemValueText(nsAString& aValue)
{
  // Can't call GetSrc because we don't have a JSContext
  GetURIAttr(nsGkAtoms::src, nullptr, aValue);
}

void
HTMLMediaElement::SetItemValueText(const nsAString& aValue)
{
  // Can't call SetSrc because we don't have a JSContext
  SetAttr(kNameSpaceID_None, nsGkAtoms::src, aValue, true);
}

void HTMLMediaElement::StopSuspendingAfterFirstFrame()
{
  mAllowSuspendAfterFirstFrame = false;
  if (!mSuspendedAfterFirstFrame)
    return;
  mSuspendedAfterFirstFrame = false;
  if (mDecoder) {
    mDecoder->Resume(true);
  }
}

void HTMLMediaElement::SetPlayedOrSeeked(bool aValue)
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

void
HTMLMediaElement::Play(ErrorResult& aRv)
{
  StopSuspendingAfterFirstFrame();
  SetPlayedOrSeeked(true);

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    aRv = Load();
    if (aRv.Failed()) {
      return;
    }
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
    if (!mPausedForInactiveDocumentOrChannel) {
      aRv = mDecoder->Play();
      if (aRv.Failed()) {
        return;
      }
    }
  }

  if (mCurrentPlayRangeStart == -1.0) {
    mCurrentPlayRangeStart = CurrentTime();
  }

  // TODO: If the playback has ended, then the user agent must set
  // seek to the effective start.
  if (mPaused) {
    if (mSrcStream) {
      GetSrcMediaStream()->ChangeExplicitBlockerCount(-1);
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

  SetPlaybackRate(mDefaultPlaybackRate);

  mPaused = false;
  mAutoplaying = false;
  // We changed mPaused and mAutoplaying which can affect AddRemoveSelfReference
  // and our preload status.
  AddRemoveSelfReference();
  UpdatePreloadAction();
}

NS_IMETHODIMP HTMLMediaElement::Play()
{
  ErrorResult rv;
  Play(rv);
  return rv.ErrorCode();
}

HTMLMediaElement::WakeLockBoolWrapper& HTMLMediaElement::WakeLockBoolWrapper::operator=(bool val) {
  if (mValue == val)
    return *this;
  if (!mWakeLock && !val && mOuter) {
    nsCOMPtr<nsIPowerManagerService> pmService =
      do_GetService(POWERMANAGERSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(pmService, *this);

    pmService->NewWakeLock(NS_LITERAL_STRING("Playing_media"), mOuter->OwnerDoc()->GetWindow(), getter_AddRefs(mWakeLock));
  } else if (mWakeLock && val) {
    mWakeLock->Unlock();
    mWakeLock = nullptr;
  }
  mValue = val;
  return *this;
}

bool HTMLMediaElement::ParseAttribute(int32_t aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  // Mappings from 'preload' attribute strings to an enumeration.
  static const nsAttrValue::EnumTable kPreloadTable[] = {
    { "",         HTMLMediaElement::PRELOAD_ATTR_EMPTY },
    { "none",     HTMLMediaElement::PRELOAD_ATTR_NONE },
    { "metadata", HTMLMediaElement::PRELOAD_ATTR_METADATA },
    { "auto",     HTMLMediaElement::PRELOAD_ATTR_AUTO },
    { 0 }
  };

  // Mappings from 'mozaudiochannel' attribute strings to an enumeration.
  static const nsAttrValue::EnumTable kMozAudioChannelAttributeTable[] = {
    { "normal",             AUDIO_CHANNEL_NORMAL },
    { "content",            AUDIO_CHANNEL_CONTENT },
    { "notification",       AUDIO_CHANNEL_NOTIFICATION },
    { "alarm",              AUDIO_CHANNEL_ALARM },
    { "telephony",          AUDIO_CHANNEL_TELEPHONY },
    { "ringer",             AUDIO_CHANNEL_RINGER },
    { "publicnotification", AUDIO_CHANNEL_PUBLICNOTIFICATION },
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

    if (aAttribute == nsGkAtoms::mozaudiochannel) {
      bool parsed = aResult.ParseEnumValue(aValue, kMozAudioChannelAttributeTable, false,
                                           &kMozAudioChannelAttributeTable[0]);
      if (!parsed) {
        return false;
      }

      AudioChannelType audioChannelType = static_cast<AudioChannelType>(aResult.GetEnumValue());

      if (audioChannelType != mAudioChannelType &&
          !mDecoder &&
          CheckAudioChannelPermissions(aValue)) {
        mAudioChannelType = audioChannelType;
      }

      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

bool HTMLMediaElement::CheckAudioChannelPermissions(const nsAString& aString)
{
  if (!UseAudioChannelService()) {
    return true;
  }

  // Only normal channel doesn't need permission.
  if (!aString.EqualsASCII("normal")) {
    nsCOMPtr<nsIPermissionManager> permissionManager =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
    if (!permissionManager) {
      return false;
    }

    uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
    permissionManager->TestExactPermissionFromPrincipal(NodePrincipal(),
      nsCString(NS_LITERAL_CSTRING("audio-channel-") + NS_ConvertUTF16toUTF8(aString)).get(), &perm);
    if (perm != nsIPermissionManager::ALLOW_ACTION) {
      return false;
    }
  }

  return true;
}

void HTMLMediaElement::DoneCreatingElement()
{
   if (HasAttr(kNameSpaceID_None, nsGkAtoms::muted))
     mMuted = true;
}

bool HTMLMediaElement::IsHTMLFocusable(bool aWithMouse,
                                       bool* aIsFocusable,
                                       int32_t* aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  *aIsFocusable = true;
  return false;
}

int32_t HTMLMediaElement::TabIndexDefault()
{
  return 0;
}

nsresult HTMLMediaElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
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
      CheckAutoplayDataReady();
      // This attribute can affect AddRemoveSelfReference
      AddRemoveSelfReference();
      UpdatePreloadAction();
    } else if (aName == nsGkAtoms::preload) {
      UpdatePreloadAction();
    }
  }

  return rv;
}

nsresult HTMLMediaElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttr,
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

nsresult HTMLMediaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
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

void HTMLMediaElement::UnbindFromTree(bool aDeep,
                                      bool aNullParent)
{
  if (!mPaused && mNetworkState != nsIDOMHTMLMediaElement::NETWORK_EMPTY)
    Pause();
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

/* static */
CanPlayStatus
HTMLMediaElement::GetCanPlay(const nsAString& aType)
{
  nsContentTypeParser parser(aType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv))
    return CANPLAY_NO;

  nsAutoString codecs;
  rv = parser.GetParameter("codecs", codecs);

  NS_ConvertUTF16toUTF8 mimeTypeUTF8(mimeType);
  return DecoderTraits::CanHandleMediaType(mimeTypeUTF8.get(),
                                           NS_SUCCEEDED(rv),
                                           codecs);
}

NS_IMETHODIMP
HTMLMediaElement::CanPlayType(const nsAString& aType, nsAString& aResult)
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

  LOG(PR_LOG_DEBUG, ("%p CanPlayType(%s) = \"%s\"", this,
                     NS_ConvertUTF16toUTF8(aType).get(),
                     NS_ConvertUTF16toUTF8(aResult).get()));

  return NS_OK;
}

nsresult HTMLMediaElement::InitializeDecoderAsClone(MediaDecoder* aOriginal)
{
  NS_ASSERTION(mLoadingSrc, "mLoadingSrc must already be set");
  NS_ASSERTION(mDecoder == nullptr, "Shouldn't have a decoder");

  MediaResource* originalResource = aOriginal->GetResource();
  if (!originalResource)
    return NS_ERROR_FAILURE;
  nsRefPtr<MediaDecoder> decoder = aOriginal->Clone();
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
    decoder->SetTransportSeekable(aOriginal->IsTransportSeekable());
    decoder->SetMediaSeekable(aOriginal->IsMediaSeekable());
  }

  MediaResource* resource = originalResource->CloneData(decoder);
  if (!resource) {
    LOG(PR_LOG_DEBUG, ("%p Failed to cloned stream for decoder %p", this, decoder.get()));
    return NS_ERROR_FAILURE;
  }

  return FinishDecoderSetup(decoder, resource, nullptr, aOriginal);
}

nsresult HTMLMediaElement::InitializeDecoderForChannel(nsIChannel* aChannel,
                                                       nsIStreamListener** aListener)
{
  NS_ASSERTION(mLoadingSrc, "mLoadingSrc must already be set");
  NS_ASSERTION(mDecoder == nullptr, "Shouldn't have a decoder");

  nsAutoCString mimeType;

  aChannel->GetContentType(mimeType);
  NS_ASSERTION(!mimeType.IsEmpty(), "We should have the Content-Type.");

  nsRefPtr<MediaDecoder> decoder = DecoderTraits::CreateDecoder(mimeType, this);
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
  mChannel = nullptr;

  return FinishDecoderSetup(decoder, resource, aListener, nullptr);
}

nsresult HTMLMediaElement::FinishDecoderSetup(MediaDecoder* aDecoder,
                                              MediaResource* aStream,
                                              nsIStreamListener** aListener,
                                              MediaDecoder* aCloneDonor)
{
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;

  // Force a same-origin check before allowing events for this media resource.
  mMediaSecurityVerified = false;

  // The new stream has not been suspended by us.
  mPausedForInactiveDocumentOrChannel = false;
  mEventDeliveryPaused = false;
  mPendingEvents.Clear();

  aDecoder->SetAudioChannelType(mAudioChannelType);
  aDecoder->SetAudioCaptured(mAudioCaptured);
  aDecoder->SetVolume(mMuted ? 0.0 : mVolume);
  aDecoder->SetPreservesPitch(mPreservesPitch);
  aDecoder->SetPlaybackRate(mPlaybackRate);

  for (uint32_t i = 0; i < mOutputStreams.Length(); ++i) {
    OutputMediaStream* ms = &mOutputStreams[i];
    aDecoder->AddOutputStream(ms->mStream->GetStream()->AsProcessedStream(),
        ms->mFinishWhenEnded);
  }

  nsresult rv = aDecoder->Load(aStream, aListener, aCloneDonor);
  if (NS_FAILED(rv)) {
    LOG(PR_LOG_DEBUG, ("%p Failed to load for decoder %p", this, aDecoder));
    return rv;
  }

  // Decoder successfully created, the decoder now owns the MediaResource
  // which owns the channel.
  mChannel = nullptr;

  mDecoder = aDecoder;
  AddMediaElementToURITable();
  NotifyDecoderPrincipalChanged();

  // We may want to suspend the new stream now.
  // This will also do an AddRemoveSelfReference.
  NotifyOwnerDocumentActivityChanged();

  if (!mPaused) {
    SetPlayedOrSeeked(true);
    if (!mPausedForInactiveDocumentOrChannel) {
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

class HTMLMediaElement::StreamListener : public MediaStreamListener {
public:
  StreamListener(HTMLMediaElement* aElement) :
    mElement(aElement),
    mHaveCurrentData(false),
    mBlocked(false),
    mMutex("HTMLMediaElement::StreamListener"),
    mPendingNotifyOutput(false)
  {}
  void Forget() { mElement = nullptr; }

  // Main thread
  void DoNotifyFinished()
  {
    if (mElement) {
      mElement->PlaybackEnded();
    }
  }
  void UpdateReadyStateForData()
  {
    if (mElement && mHaveCurrentData) {
      mElement->UpdateReadyStateForData(
        mBlocked ? MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING :
                   MediaDecoderOwner::NEXT_FRAME_AVAILABLE);
    }
  }
  void DoNotifyBlocked()
  {
    mBlocked = true;
    UpdateReadyStateForData();
  }
  void DoNotifyUnblocked()
  {
    mBlocked = false;
    UpdateReadyStateForData();
  }
  void DoNotifyOutput()
  {
    {
      MutexAutoLock lock(mMutex);
      mPendingNotifyOutput = false;
    }
    if (mElement && mHaveCurrentData) {
      mElement->FireTimeUpdate(true);
    }
  }
  void DoNotifyHaveCurrentData()
  {
    mHaveCurrentData = true;
    if (mElement) {
      mElement->FirstFrameLoaded(false);
    }
    UpdateReadyStateForData();
    DoNotifyOutput();
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
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
  }
  virtual void NotifyFinished(MediaStreamGraph* aGraph)
  {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &StreamListener::DoNotifyFinished);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
  }
  virtual void NotifyHasCurrentData(MediaStreamGraph* aGraph)
  {
    MutexAutoLock lock(mMutex);
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &StreamListener::DoNotifyHaveCurrentData);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
  }
  virtual void NotifyOutput(MediaStreamGraph* aGraph)
  {
    MutexAutoLock lock(mMutex);
    if (mPendingNotifyOutput)
      return;
    mPendingNotifyOutput = true;
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &StreamListener::DoNotifyOutput);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
  }

private:
  // These fields may only be accessed on the main thread
  HTMLMediaElement* mElement;
  bool mHaveCurrentData;
  bool mBlocked;

  // mMutex protects the fields below; they can be accessed on any thread
  Mutex mMutex;
  bool mPendingNotifyOutput;
};

void HTMLMediaElement::SetupSrcMediaStreamPlayback(DOMMediaStream* aStream)
{
  NS_ASSERTION(!mSrcStream && !mSrcStreamListener, "Should have been ended already");

  mSrcStream = aStream;
  // XXX if we ever support capturing the output of a media element which is
  // playing a stream, we'll need to add a CombineWithPrincipal call here.
  mSrcStreamListener = new StreamListener(this);
  GetSrcMediaStream()->AddListener(mSrcStreamListener);
  if (mPaused) {
    GetSrcMediaStream()->ChangeExplicitBlockerCount(1);
  }
  if (mPausedForInactiveDocumentOrChannel) {
    GetSrcMediaStream()->ChangeExplicitBlockerCount(1);
  }
  ChangeDelayLoadStatus(false);
  GetSrcMediaStream()->AddAudioOutput(this);
  GetSrcMediaStream()->SetAudioOutputVolume(this, float(mMuted ? 0.0 : mVolume));
  VideoFrameContainer* container = GetVideoFrameContainer();
  if (container) {
    GetSrcMediaStream()->AddVideoOutput(container);
  }
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
  DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  DispatchAsyncEvent(NS_LITERAL_STRING("loadedmetadata"));
  DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  AddRemoveSelfReference();
  // FirstFrameLoaded(false) will be called when the stream has current data,
  // to complete the setup by entering the HAVE_CURRENT_DATA state.
}

void HTMLMediaElement::EndSrcMediaStreamPlayback()
{
  GetSrcMediaStream()->RemoveListener(mSrcStreamListener);
  // Kill its reference to this element
  mSrcStreamListener->Forget();
  mSrcStreamListener = nullptr;
  GetSrcMediaStream()->RemoveAudioOutput(this);
  VideoFrameContainer* container = GetVideoFrameContainer();
  if (container) {
    GetSrcMediaStream()->RemoveVideoOutput(container);
    container->ClearCurrentFrame();
  }
  if (mPaused) {
    GetSrcMediaStream()->ChangeExplicitBlockerCount(-1);
  }
  if (mPausedForInactiveDocumentOrChannel) {
    GetSrcMediaStream()->ChangeExplicitBlockerCount(-1);
  }
  mSrcStream = nullptr;
}

nsresult HTMLMediaElement::NewURIFromString(const nsAutoString& aURISpec, nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  *aURI = nullptr;

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

void HTMLMediaElement::ProcessMediaFragmentURI()
{
  nsMediaFragmentURIParser parser(mLoadingSrc);

  if (mDecoder && parser.HasEndTime()) {
    mFragmentEnd = parser.GetEndTime();
  }

  if (parser.HasStartTime()) {
    SetCurrentTime(parser.GetStartTime());
    mFragmentStart = parser.GetStartTime();
  }
}

void HTMLMediaElement::MetadataLoaded(int aChannels,
                                      int aRate,
                                      bool aHasAudio,
                                      bool aHasVideo,
                                      const MetadataTags* aTags)
{
  mChannels = aChannels;
  mRate = aRate;
  mHasAudio = aHasAudio;
  mTags = aTags;
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
  DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  DispatchAsyncEvent(NS_LITERAL_STRING("loadedmetadata"));
  if (mDecoder && mDecoder->IsTransportSeekable() && mDecoder->IsMediaSeekable()) {
    ProcessMediaFragmentURI();
    mDecoder->SetFragmentEndTime(mFragmentEnd);
  }

  // If this element had a video track, but consists only of an audio track now,
  // delete the VideoFrameContainer. This happens when the src is changed to an
  // audio only file.
  if (!aHasVideo) {
    mVideoFrameContainer = nullptr;
  }
}

void HTMLMediaElement::FirstFrameLoaded(bool aResourceFullyLoaded)
{
  ChangeReadyState(aResourceFullyLoaded ?
    nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA :
    nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
  ChangeDelayLoadStatus(false);

  NS_ASSERTION(!mSuspendedAfterFirstFrame, "Should not have already suspended");

  if (mDecoder && mAllowSuspendAfterFirstFrame && mPaused &&
      !aResourceFullyLoaded &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
      mPreloadAction == HTMLMediaElement::PRELOAD_METADATA) {
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

void HTMLMediaElement::ResourceLoaded()
{
  NS_ASSERTION(!mSrcStream, "Don't call this for streams");

  mBegun = false;
  mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
  AddRemoveSelfReference();
  if (mReadyState >= nsIDOMHTMLMediaElement::HAVE_METADATA) {
    // MediaStream sources are put into HAVE_CURRENT_DATA state here on setup. If the
    // stream is not blocked, we will receive a notification that will put it
    // into HAVE_ENOUGH_DATA state.
    ChangeReadyState(mSrcStream ? nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA
                     : nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
  }
  // Ensure a progress event is dispatched at the end of download.
  DispatchAsyncEvent(NS_LITERAL_STRING("progress"));
  // The download has stopped.
  DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
}

void HTMLMediaElement::NetworkError()
{
  Error(nsIDOMMediaError::MEDIA_ERR_NETWORK);
}

void HTMLMediaElement::DecodeError()
{
  nsAutoString src;
  GetCurrentSrc(src);
  const PRUnichar* params[] = { src.get() };
  ReportLoadError("MediaLoadDecodeError", params, ArrayLength(params));

  if (mDecoder) {
    ShutdownDecoder();
  }
  mLoadingSrc = nullptr;
  if (mIsLoadingFromSourceChildren) {
    mError = nullptr;
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

void HTMLMediaElement::LoadAborted()
{
  Error(nsIDOMMediaError::MEDIA_ERR_ABORTED);
}

void HTMLMediaElement::Error(uint16_t aErrorCode)
{
  NS_ASSERTION(aErrorCode == nsIDOMMediaError::MEDIA_ERR_DECODE ||
               aErrorCode == nsIDOMMediaError::MEDIA_ERR_NETWORK ||
               aErrorCode == nsIDOMMediaError::MEDIA_ERR_ABORTED,
               "Only use nsIDOMMediaError codes!");
  mError = new MediaError(this, aErrorCode);
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

void HTMLMediaElement::PlaybackEnded()
{
  // We changed state which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  NS_ASSERTION(!mDecoder || mDecoder->IsEnded(),
               "Decoder fired ended, but not in ended state");

  // Discard all output streams that have finished now.
  for (int32_t i = mOutputStreams.Length() - 1; i >= 0; --i) {
    if (mOutputStreams[i].mFinishWhenEnded) {
      mOutputStreams.RemoveElementAt(i);
    }
  }

  if (mSrcStream || (mDecoder && mDecoder->IsInfinite())) {
    LOG(PR_LOG_DEBUG, ("%p, got duration by reaching the end of the resource", this));
    DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::loop)) {
    SetCurrentTime(0);
    return;
  }

  Pause();

  FireTimeUpdate(false);
  DispatchAsyncEvent(NS_LITERAL_STRING("ended"));
}

void HTMLMediaElement::SeekStarted()
{
  DispatchAsyncEvent(NS_LITERAL_STRING("seeking"));
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
  FireTimeUpdate(false);
}

void HTMLMediaElement::SeekCompleted()
{
  mPlayingBeforeSeek = false;
  SetPlayedOrSeeked(true);
  DispatchAsyncEvent(NS_LITERAL_STRING("seeked"));
  // We changed whether we're seeking so we need to AddRemoveSelfReference
  AddRemoveSelfReference();
}

void HTMLMediaElement::NotifySuspendedByCache(bool aIsSuspended)
{
  mDownloadSuspendedByCache = aIsSuspended;
  // If this is an autoplay element, we may need to kick off its autoplaying
  // now so we consume data and hopefully free up cache space.
  CheckAutoplayDataReady();
}

void HTMLMediaElement::DownloadSuspended()
{
  DispatchAsyncEvent(NS_LITERAL_STRING("progress"));
  if (mBegun) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_IDLE;
    AddRemoveSelfReference();
    DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
  }
}

void HTMLMediaElement::DownloadResumed(bool aForceNetworkLoading)
{
  if (mBegun || aForceNetworkLoading) {
    mNetworkState = nsIDOMHTMLMediaElement::NETWORK_LOADING;
    AddRemoveSelfReference();
  }
}

void HTMLMediaElement::DownloadStalled()
{
  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING) {
    DispatchAsyncEvent(NS_LITERAL_STRING("stalled"));
  }
}

bool HTMLMediaElement::ShouldCheckAllowOrigin()
{
  return mCORSMode != CORS_NONE;
}

void HTMLMediaElement::UpdateReadyStateForData(MediaDecoderOwner::NextFrameStatus aNextFrame)
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

  if (aNextFrame != MediaDecoderOwner::NEXT_FRAME_AVAILABLE) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
    if (!mWaitingFired && aNextFrame == MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING) {
      FireTimeUpdate(false);
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      mWaitingFired = true;
    }
    return;
  }

  if (mSrcStream) {
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
  MediaDecoder::Statistics stats = mDecoder->GetStatistics();
  if (stats.mTotalBytes < 0 ? stats.mDownloadRateReliable
                            : stats.mTotalBytes == stats.mDownloadPosition ||
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

void HTMLMediaElement::ChangeReadyState(nsMediaReadyState aState)
{
  nsMediaReadyState oldState = mReadyState;
  mReadyState = aState;

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY ||
      oldState == mReadyState) {
    return;
  }

  LOG(PR_LOG_DEBUG, ("%p Ready state changed to %s", this, gReadyStateToString[aState]));

  UpdateAudioChannelPlayingState();

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

  CheckAutoplayDataReady();

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

bool HTMLMediaElement::CanActivateAutoplay()
{
  // For stream inputs, we activate autoplay on HAVE_CURRENT_DATA because
  // this element itself might be blocking the stream from making progress by
  // being paused.
  return mAutoplaying &&
         mPaused &&
         (mDownloadSuspendedByCache ||
          (mDecoder && mReadyState >= nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) ||
          (mSrcStream && mReadyState >= nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA)) &&
         HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
         mAutoplayEnabled &&
         !IsEditable();
}

void HTMLMediaElement::CheckAutoplayDataReady()
{
  if (CanActivateAutoplay()) {
    mPaused = false;
    // We changed mPaused which can affect AddRemoveSelfReference
    AddRemoveSelfReference();

    if (mDecoder) {
      SetPlayedOrSeeked(true);
      if (mCurrentPlayRangeStart == -1.0) {
        mCurrentPlayRangeStart = CurrentTime();
      }
      mDecoder->Play();
    } else if (mSrcStream) {
      SetPlayedOrSeeked(true);
      GetSrcMediaStream()->ChangeExplicitBlockerCount(-1);
    }
    DispatchAsyncEvent(NS_LITERAL_STRING("play"));
  }
}

VideoFrameContainer* HTMLMediaElement::GetVideoFrameContainer()
{
  // If we have loaded the metadata, and the size of the video is still
  // (-1, -1), the media has no video. Don't go a create a video frame
  // container.
  if (mReadyState >= nsIDOMHTMLMediaElement::HAVE_METADATA &&
      mMediaSize == nsIntSize(-1, -1)) {
    return nullptr;
  }

  if (mVideoFrameContainer)
    return mVideoFrameContainer;

  // If we have a print surface, this is just a static image so
  // no image container is required
  if (mPrintSurface)
    return nullptr;

  // Only video frames need an image container.
  nsCOMPtr<nsIDOMHTMLVideoElement> video = do_QueryObject(this);
  if (!video)
    return nullptr;

  mVideoFrameContainer =
    new VideoFrameContainer(this, LayerManager::CreateAsynchronousImageContainer());

  return mVideoFrameContainer;
}

nsresult HTMLMediaElement::DispatchAudioAvailableEvent(float* aFrameBuffer,
                                                       uint32_t aFrameBufferLength,
                                                       float aTime)
{
  // Auto manage the memory for the frame buffer. If we fail and return
  // an error, this ensures we free the memory in the frame buffer. Otherwise
  // we hand off ownership of the frame buffer to the audioavailable event,
  // which frees the memory when it's destroyed.
  nsAutoArrayPtr<float> frameBuffer(aFrameBuffer);

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(OwnerDoc());
  nsRefPtr<HTMLMediaElement> kungFuDeathGrip = this;
  NS_ENSURE_TRUE(domDoc, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = domDoc->CreateEvent(NS_LITERAL_STRING("MozAudioAvailableEvent"),
                                    getter_AddRefs(event));
  nsCOMPtr<nsIDOMNotifyAudioAvailableEvent> audioavailableEvent(do_QueryInterface(event));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = audioavailableEvent->InitAudioAvailableEvent(NS_LITERAL_STRING("MozAudioAvailable"),
                                                    false, false, frameBuffer.forget(), aFrameBufferLength,
                                                    aTime, mAllowAudioData);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  return DispatchEvent(event, &dummy);
}

nsresult HTMLMediaElement::DispatchEvent(const nsAString& aName)
{
  LOG_EVENT(PR_LOG_DEBUG, ("%p Dispatching event %s", this,
                          NS_ConvertUTF16toUTF8(aName).get()));

  // Save events that occur while in the bfcache. These will be dispatched
  // if the page comes out of the bfcache.
  if (mEventDeliveryPaused) {
    mPendingEvents.AppendElement(aName);
    return NS_OK;
  }

  return nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                              static_cast<nsIContent*>(this),
                                              aName,
                                              false,
                                              false);
}

nsresult HTMLMediaElement::DispatchAsyncEvent(const nsAString& aName)
{
  LOG_EVENT(PR_LOG_DEBUG, ("%p Queuing event %s", this,
            NS_ConvertUTF16toUTF8(aName).get()));

  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult HTMLMediaElement::DispatchPendingMediaEvents()
{
  NS_ASSERTION(!mEventDeliveryPaused,
               "Must not be in bfcache when dispatching pending media events");

  uint32_t count = mPendingEvents.Length();
  for (uint32_t i = 0; i < count; ++i) {
    DispatchAsyncEvent(mPendingEvents[i]);
  }
  mPendingEvents.Clear();

  return NS_OK;
}

bool HTMLMediaElement::IsPotentiallyPlaying() const
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

bool HTMLMediaElement::IsPlaybackEnded() const
{
  // TODO:
  //   the current playback position is equal to the effective end of the media resource.
  //   See bug 449157.
  return mNetworkState >= nsIDOMHTMLMediaElement::HAVE_METADATA &&
    mDecoder ? mDecoder->IsEnded() : false;
}

already_AddRefed<nsIPrincipal> HTMLMediaElement::GetCurrentPrincipal()
{
  if (mDecoder) {
    return mDecoder->GetCurrentPrincipal();
  }
  if (mSrcStream) {
    nsRefPtr<nsIPrincipal> principal = mSrcStream->GetPrincipal();
    return principal.forget();
  }
  return nullptr;
}

void HTMLMediaElement::NotifyDecoderPrincipalChanged()
{
  for (uint32_t i = 0; i < mOutputStreams.Length(); ++i) {
    OutputMediaStream* ms = &mOutputStreams[i];
    nsRefPtr<nsIPrincipal> principal = GetCurrentPrincipal();
    ms->mStream->CombineWithPrincipal(principal);
  }
}

void HTMLMediaElement::UpdateMediaSize(nsIntSize size)
{
  mMediaSize = size;
}

void HTMLMediaElement::SuspendOrResumeElement(bool aPauseElement, bool aSuspendEvents)
{
  if (aPauseElement != mPausedForInactiveDocumentOrChannel) {
    mPausedForInactiveDocumentOrChannel = aPauseElement;
    if (aPauseElement) {
      if (mDecoder) {
        mDecoder->Pause();
        mDecoder->Suspend();
      } else if (mSrcStream) {
        GetSrcMediaStream()->ChangeExplicitBlockerCount(1);
      }
      mEventDeliveryPaused = aSuspendEvents;
    } else {
      if (mDecoder) {
        mDecoder->Resume(false);
        if (!mPaused && !mDecoder->IsEnded()) {
          mDecoder->Play();
        }
      } else if (mSrcStream) {
        GetSrcMediaStream()->ChangeExplicitBlockerCount(-1);
      }
      if (mEventDeliveryPaused) {
        mEventDeliveryPaused = false;
        DispatchPendingMediaEvents();
      }
    }
  }
}

void HTMLMediaElement::NotifyOwnerDocumentActivityChanged()
{
  nsIDocument* ownerDoc = OwnerDoc();
  if (UseAudioChannelService()) {
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(OwnerDoc());
    if (domDoc) {
      bool hidden = false;
      domDoc->GetHidden(&hidden);
      // SetVisibilityState will update mChannelSuspended via the CanPlayChanged callback.
      if (mPlayingThroughTheAudioChannel && mAudioChannelAgent) {
        mAudioChannelAgent->SetVisibilityState(!hidden);
      }
    }
  }
  bool suspendEvents = !ownerDoc->IsActive() || !ownerDoc->IsVisible();
  bool pauseElement = suspendEvents || mChannelSuspended;

  SuspendOrResumeElement(pauseElement, suspendEvents);

  AddRemoveSelfReference();
}

void HTMLMediaElement::AddRemoveSelfReference()
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
     (!mPaused && mSrcStream && !mSrcStream->IsFinished()) ||
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
        NS_NewRunnableMethod(this, &HTMLMediaElement::DoRemoveSelfReference);
      NS_DispatchToMainThread(event);
    }
  }

  UpdateAudioChannelPlayingState();
}

void HTMLMediaElement::DoRemoveSelfReference()
{
  // We don't need the shutdown observer anymore. Unregistering releases
  // its reference to us, which we were using as our self-reference.
  nsContentUtils::UnregisterShutdownObserver(this);
}

nsresult HTMLMediaElement::Observe(nsISupports* aSubject,
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
HTMLMediaElement::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~(eCONTENT | eMEDIA));
}

void HTMLMediaElement::DispatchAsyncSourceError(nsIContent* aSourceElement)
{
  LOG_EVENT(PR_LOG_DEBUG, ("%p Queuing simple source error event", this));

  nsCOMPtr<nsIRunnable> event = new nsSourceErrorEventRunner(this, aSourceElement);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

void HTMLMediaElement::NotifyAddedSource()
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

nsIContent* HTMLMediaElement::GetNextSource()
{
  nsCOMPtr<nsIDOMNode> thisDomNode = do_QueryObject(this);

  mSourceLoadCandidate = nullptr;

  nsresult rv = NS_OK;
  if (!mSourcePointer) {
    // First time this has been run, create a selection to cover children.
    mSourcePointer = new nsRange(this);

    rv = mSourcePointer->SelectNodeContents(thisDomNode);
    if (NS_FAILED(rv)) return nullptr;

    rv = mSourcePointer->Collapse(true);
    if (NS_FAILED(rv)) return nullptr;
  }

  while (true) {
#ifdef DEBUG
    nsCOMPtr<nsIDOMNode> startContainer;
    rv = mSourcePointer->GetStartContainer(getter_AddRefs(startContainer));
    if (NS_FAILED(rv)) return nullptr;
    NS_ASSERTION(startContainer == thisDomNode,
                "Should only iterate over direct children");
#endif

    int32_t startOffset = 0;
    rv = mSourcePointer->GetStartOffset(&startOffset);
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (uint32_t(startOffset) == GetChildCount())
      return nullptr; // No more children.

    // Advance the range to the next child.
    rv = mSourcePointer->SetStart(thisDomNode, startOffset + 1);
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsIContent* child = GetChildAt(startOffset);

    // If child is a <source> element, it is the next candidate.
    if (child && child->IsHTML(nsGkAtoms::source)) {
      mSourceLoadCandidate = child;
      return child;
    }
  }
  NS_NOTREACHED("Execution should not reach here!");
  return nullptr;
}

void HTMLMediaElement::ChangeDelayLoadStatus(bool aDelay)
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
      mLoadBlockedDoc = nullptr;
    }
  }

  // We changed mDelayingLoadEvent which can affect AddRemoveSelfReference
  AddRemoveSelfReference();
}

already_AddRefed<nsILoadGroup> HTMLMediaElement::GetDocumentLoadGroup()
{
  if (!OwnerDoc()->IsActive()) {
    NS_WARNING("Load group requested for media element in inactive document.");
  }
  return OwnerDoc()->GetDocumentLoadGroup();
}

nsresult
HTMLMediaElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    HTMLMediaElement* dest = static_cast<HTMLMediaElement*>(aDest);
    if (mPrintSurface) {
      dest->mPrintSurface = mPrintSurface;
      dest->mMediaSize = mMediaSize;
    } else {
      nsIFrame* frame = GetPrimaryFrame();
      Element* element;
      if (frame && frame->GetType() == nsGkAtoms::HTMLVideoFrame &&
          static_cast<nsVideoFrame*>(frame)->ShouldDisplayPoster()) {
        nsIContent* content = static_cast<nsVideoFrame*>(frame)->GetPosterImage();
        element = content ? content->AsElement() : nullptr;
      } else {
        element = const_cast<HTMLMediaElement*>(this);
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

already_AddRefed<TimeRanges>
HTMLMediaElement::Buffered() const
{
  nsRefPtr<TimeRanges> ranges = new TimeRanges();
  if (mReadyState > nsIDOMHTMLMediaElement::HAVE_NOTHING && mDecoder) {
    // If GetBuffered fails we ignore the error result and just return the
    // time ranges we found up till the error.
    mDecoder->GetBuffered(ranges);
  }
  return ranges.forget();
}

nsresult HTMLMediaElement::GetBuffered(nsIDOMTimeRanges** aBuffered)
{
  nsRefPtr<TimeRanges> ranges = Buffered();
  ranges.forget(aBuffered);
  return NS_OK;
}

void HTMLMediaElement::SetRequestHeaders(nsIHttpChannel* aChannel)
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

void HTMLMediaElement::FireTimeUpdate(bool aPeriodic)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  TimeStamp now = TimeStamp::Now();
  double time = CurrentTime();

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
    mDecoder->SetFragmentEndTime(mFragmentEnd);
  }
}

void HTMLMediaElement::GetCurrentSpec(nsCString& aString)
{
  if (mLoadingSrc) {
    mLoadingSrc->GetSpec(aString);
  } else {
    aString.Truncate();
  }
}

/* attribute double mozFragmentEnd; */
double
HTMLMediaElement::MozFragmentEnd()
{
  double duration = Duration();

  // If there is no end fragment, or the fragment end is greater than the
  // duration, return the duration.
  return (mFragmentEnd < 0.0 || mFragmentEnd > duration) ? duration : mFragmentEnd;
}

NS_IMETHODIMP HTMLMediaElement::GetMozFragmentEnd(double* aTime)
{
  *aTime = MozFragmentEnd();
  return NS_OK;
}

void HTMLMediaElement::NotifyAudioAvailableListener()
{
  if (dom::EnableWebAudioCheck::PrefEnabled()) {
    OwnerDoc()->WarnOnceAbout(nsIDocument::eMozAudioData);
  }
  if (mDecoder) {
    mDecoder->NotifyAudioAvailableListener();
  }
}

static double ClampPlaybackRate(double aPlaybackRate)
{
  if (aPlaybackRate == 0.0) {
    return aPlaybackRate;
  }
  if (Abs(aPlaybackRate) < MIN_PLAYBACKRATE) {
    return aPlaybackRate < 0 ? -MIN_PLAYBACKRATE : MIN_PLAYBACKRATE;
  }
  if (Abs(aPlaybackRate) > MAX_PLAYBACKRATE) {
    return aPlaybackRate < 0 ? -MAX_PLAYBACKRATE : MAX_PLAYBACKRATE;
  }
  return aPlaybackRate;
}

/* attribute double defaultPlaybackRate; */
NS_IMETHODIMP HTMLMediaElement::GetDefaultPlaybackRate(double* aDefaultPlaybackRate)
{
  *aDefaultPlaybackRate = DefaultPlaybackRate();
  return NS_OK;
}

void
HTMLMediaElement::SetDefaultPlaybackRate(double aDefaultPlaybackRate, ErrorResult& aRv)
{
  if (aDefaultPlaybackRate < 0) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  mDefaultPlaybackRate = ClampPlaybackRate(aDefaultPlaybackRate);
  DispatchAsyncEvent(NS_LITERAL_STRING("ratechange"));
}

NS_IMETHODIMP HTMLMediaElement::SetDefaultPlaybackRate(double aDefaultPlaybackRate)
{
  ErrorResult rv;
  SetDefaultPlaybackRate(aDefaultPlaybackRate, rv);
  return rv.ErrorCode();
}

/* attribute double playbackRate; */
NS_IMETHODIMP HTMLMediaElement::GetPlaybackRate(double* aPlaybackRate)
{
  *aPlaybackRate = PlaybackRate();
  return NS_OK;
}

void
HTMLMediaElement::SetPlaybackRate(double aPlaybackRate, ErrorResult& aRv)
{
  // Changing the playback rate of a media that has more than two channels is
  // not supported.
  if (aPlaybackRate < 0 || (mChannels > 2 && aPlaybackRate != 1.0)) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  mPlaybackRate = ClampPlaybackRate(aPlaybackRate);

  if (!mMuted) {
    if (mPlaybackRate < 0 ||
        mPlaybackRate > THRESHOLD_HIGH_PLAYBACKRATE_AUDIO ||
        mPlaybackRate < THRESHOLD_LOW_PLAYBACKRATE_AUDIO) {
      SetMutedInternal(true);
    } else {
      SetMutedInternal(false);
    }
  }

  if (mDecoder) {
    mDecoder->SetPlaybackRate(mPlaybackRate);
  }
  DispatchAsyncEvent(NS_LITERAL_STRING("ratechange"));
}

NS_IMETHODIMP HTMLMediaElement::SetPlaybackRate(double aPlaybackRate)
{
  ErrorResult rv;
  SetPlaybackRate(aPlaybackRate, rv);
  return rv.ErrorCode();
}

/* attribute bool mozPreservesPitch; */
NS_IMETHODIMP HTMLMediaElement::GetMozPreservesPitch(bool* aPreservesPitch)
{
  *aPreservesPitch = MozPreservesPitch();
  return NS_OK;
}

NS_IMETHODIMP HTMLMediaElement::SetMozPreservesPitch(bool aPreservesPitch)
{
  mPreservesPitch = aPreservesPitch;
  if (mDecoder) {
    mDecoder->SetPreservesPitch(mPreservesPitch);
  }
  return NS_OK;
}

ImageContainer* HTMLMediaElement::GetImageContainer()
{
  VideoFrameContainer* container = GetVideoFrameContainer();
  return container ? container->GetImageContainer() : nullptr;
}

nsresult HTMLMediaElement::UpdateChannelMuteState(bool aCanPlay)
{
  if (!UseAudioChannelService()) {
    return NS_OK;
  }

  // We have to mute this channel:
  if (!aCanPlay && !mChannelSuspended) {
    mChannelSuspended = true;
    DispatchAsyncEvent(NS_LITERAL_STRING("mozinterruptbegin"));
  } else if (aCanPlay && mChannelSuspended) {
    mChannelSuspended = false;
    DispatchAsyncEvent(NS_LITERAL_STRING("mozinterruptend"));
  }

  SuspendOrResumeElement(mChannelSuspended, false);
  return NS_OK;
}

void HTMLMediaElement::UpdateAudioChannelPlayingState()
{
  if (!UseAudioChannelService()) {
    return;
  }

  bool playingThroughTheAudioChannel =
     (!mPaused &&
      (HasAttr(kNameSpaceID_None, nsGkAtoms::loop) ||
       (mReadyState >= nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA &&
        !IsPlaybackEnded())));
  if (playingThroughTheAudioChannel != mPlayingThroughTheAudioChannel) {
    mPlayingThroughTheAudioChannel = playingThroughTheAudioChannel;

    if (!mAudioChannelAgent) {
      nsresult rv;
      mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1", &rv);
      if (!mAudioChannelAgent) {
        return;
      }
      // Use a weak ref so the audio channel agent can't leak |this|.
      mAudioChannelAgent->InitWithWeakCallback(mAudioChannelType, this);

      nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(OwnerDoc());
      if (domDoc) {
        bool hidden = false;
        domDoc->GetHidden(&hidden);
        mAudioChannelAgent->SetVisibilityState(!hidden);
      }
    }

    if (mPlayingThroughTheAudioChannel) {
      bool canPlay;
      mAudioChannelAgent->StartPlaying(&canPlay);
    } else {
      mAudioChannelAgent->StopPlaying();
      mAudioChannelAgent = nullptr;
    }
  }
}

/* void canPlayChanged (in boolean canPlay); */
NS_IMETHODIMP HTMLMediaElement::CanPlayChanged(bool canPlay)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);

  UpdateChannelMuteState(canPlay);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
