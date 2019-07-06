/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebVTTListener.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/dom/VTTRegionBinding.h"
#include "nsComponentManagerUtils.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIInputStream.h"
#include "nsIWebVTTParserWrapper.h"

extern mozilla::LazyLogModule gTextTrackLog;
#define LOG(msg, ...)                     \
  MOZ_LOG(gTextTrackLog, LogLevel::Debug, \
          ("WebVTTListener=%p, " msg, this, ##__VA_ARGS__))
#define LOG_WIHTOUT_ADDRESS(msg, ...) \
  MOZ_LOG(gTextTrackLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(WebVTTListener, mElement, mParserWrapper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebVTTListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebVTTListener)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebVTTListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebVTTListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebVTTListener)

WebVTTListener::WebVTTListener(HTMLTrackElement* aElement)
    : mElement(aElement), mParserWrapperError(NS_OK) {
  MOZ_ASSERT(mElement, "Must pass an element to the callback");
  LOG("Created listener for track element %p", aElement);
  MOZ_DIAGNOSTIC_ASSERT(
      CycleCollectedJSContext::Get() &&
      !CycleCollectedJSContext::Get()->IsInStableOrMetaStableState());
  mParserWrapper = do_CreateInstance(NS_WEBVTTPARSERWRAPPER_CONTRACTID,
                                     &mParserWrapperError);
  if (NS_SUCCEEDED(mParserWrapperError)) {
    nsPIDOMWindowInner* window = mElement->OwnerDoc()->GetInnerWindow();
    mParserWrapperError = mParserWrapper->LoadParser(window);
  }
  if (NS_SUCCEEDED(mParserWrapperError)) {
    mParserWrapperError = mParserWrapper->Watch(this);
  }
}

WebVTTListener::~WebVTTListener() { LOG("destroyed."); }

NS_IMETHODIMP
WebVTTListener::GetInterface(const nsIID& aIID, void** aResult) {
  return QueryInterface(aIID, aResult);
}

nsresult WebVTTListener::LoadResource() {
  if (IsCanceled()) {
    return NS_OK;
  }
  // Exit if we failed to create the WebVTTParserWrapper (vtt.jsm)
  NS_ENSURE_SUCCESS(mParserWrapperError, mParserWrapperError);

  mElement->SetReadyState(TextTrackReadyState::Loading);
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                       nsIChannel* aNewChannel, uint32_t aFlags,
                                       nsIAsyncVerifyRedirectCallback* cb) {
  if (IsCanceled()) {
    return NS_OK;
  }
  if (mElement) {
    mElement->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
  }
  cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnStartRequest(nsIRequest* aRequest) {
  if (IsCanceled()) {
    return NS_OK;
  }

  LOG("OnStartRequest");
  mElement->DispatchTestEvent(NS_LITERAL_STRING("mozStartedLoadingTextTrack"));
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  if (IsCanceled()) {
    return NS_OK;
  }

  LOG("OnStopRequest");
  if (NS_FAILED(aStatus)) {
    LOG("Got error status");
    mElement->SetReadyState(TextTrackReadyState::FailedToLoad);
  }
  // Attempt to parse any final data the parser might still have.
  mParserWrapper->Flush();
  if (mElement->ReadyState() != TextTrackReadyState::FailedToLoad) {
    mElement->SetReadyState(TextTrackReadyState::Loaded);
  }

  mElement->CancelChannelAndListener();

  return aStatus;
}

nsresult WebVTTListener::ParseChunk(nsIInputStream* aInStream, void* aClosure,
                                    const char* aFromSegment,
                                    uint32_t aToOffset, uint32_t aCount,
                                    uint32_t* aWriteCount) {
  nsCString buffer(aFromSegment, aCount);
  WebVTTListener* listener = static_cast<WebVTTListener*>(aClosure);
  MOZ_ASSERT(!listener->IsCanceled());

  if (NS_FAILED(listener->mParserWrapper->Parse(buffer))) {
    LOG_WIHTOUT_ADDRESS(
        "WebVTTListener=%p, Unable to parse chunk of WEBVTT text. Aborting.",
        listener);
    *aWriteCount = 0;
    return NS_ERROR_FAILURE;
  }

  *aWriteCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aStream,
                                uint64_t aOffset, uint32_t aCount) {
  if (IsCanceled()) {
    return NS_OK;
  }

  LOG("OnDataAvailable");
  uint32_t count = aCount;
  while (count > 0) {
    uint32_t read;
    nsresult rv = aStream->ReadSegments(ParseChunk, this, count, &read);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!read) {
      return NS_ERROR_FAILURE;
    }
    count -= read;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnCue(JS::Handle<JS::Value> aCue, JSContext* aCx) {
  MOZ_ASSERT(!IsCanceled());
  if (!aCue.isObject()) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> obj(aCx, &aCue.toObject());
  TextTrackCue* cue = nullptr;
  nsresult rv = UNWRAP_OBJECT(VTTCue, &obj, cue);
  NS_ENSURE_SUCCESS(rv, rv);

  cue->SetTrackElement(mElement);
  mElement->mTrack->AddCue(*cue);

  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnRegion(JS::Handle<JS::Value> aRegion, JSContext* aCx) {
  MOZ_ASSERT(!IsCanceled());
  // Nothing for this callback to do.
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnParsingError(int32_t errorCode, JSContext* cx) {
  MOZ_ASSERT(!IsCanceled());
  // We only care about files that have a bad WebVTT file signature right now
  // as that means the file failed to load.
  if (errorCode == ErrorCodes::BadSignature) {
    LOG("parsing error");
    mElement->SetReadyState(TextTrackReadyState::FailedToLoad);
  }
  return NS_OK;
}

bool WebVTTListener::IsCanceled() const { return mCancel; }

void WebVTTListener::Cancel() {
  MOZ_ASSERT(!IsCanceled(), "Do not cancel canceled listener again!");
  LOG("Cancel listen to channel's response.");
  mCancel = true;
  mParserWrapper->Cancel();
  mParserWrapper = nullptr;
  mElement = nullptr;
}

}  // namespace dom
}  // namespace mozilla
