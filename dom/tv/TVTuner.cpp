/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMediaStream.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TVCurrentSourceChangedEvent.h"
#include "mozilla/dom/TVServiceCallbacks.h"
#include "mozilla/dom/TVServiceFactory.h"
#include "mozilla/dom/TVSource.h"
#include "mozilla/dom/TVUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsITVService.h"
#include "nsITVSimulatorService.h"
#include "nsServiceManagerUtils.h"
#include "TVTuner.h"
#include "mozilla/dom/HTMLVideoElement.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(TVTuner, DOMEventTargetHelper,
                                   mTVService, mStream, mCurrentSource, mSources)

NS_IMPL_ADDREF_INHERITED(TVTuner, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TVTuner, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TVTuner)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TVTuner::TVTuner(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mStreamType(0)
{
}

TVTuner::~TVTuner()
{
}

/* static */ already_AddRefed<TVTuner>
TVTuner::Create(nsPIDOMWindowInner* aWindow,
                nsITVTunerData* aData)
{
  RefPtr<TVTuner> tuner = new TVTuner(aWindow);
  return (tuner->Init(aData)) ? tuner.forget() : nullptr;
}

bool
TVTuner::Init(nsITVTunerData* aData)
{
  NS_ENSURE_TRUE(aData, false);

  nsresult rv = aData->GetId(mId);
  NS_ENSURE_SUCCESS(rv, false);
  if (NS_WARN_IF(mId.IsEmpty())) {
    return false;
  }

  uint32_t count;
  char** supportedSourceTypes;
  rv = aData->GetSupportedSourceTypes(&count, &supportedSourceTypes);
  NS_ENSURE_SUCCESS(rv, false);

  for (uint32_t i = 0; i < count; i++) {
    TVSourceType sourceType = ToTVSourceType(supportedSourceTypes[i]);
    if (NS_WARN_IF(sourceType == TVSourceType::EndGuard_)) {
      continue;
    }

    // Generate the source instance based on the supported source type.
    RefPtr<TVSource> source = TVSource::Create(GetOwner(), sourceType, this);
    if (NS_WARN_IF(!source)) {
      continue;
    }

    mSupportedSourceTypes.AppendElement(sourceType);
    mSources.AppendElement(source);
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, supportedSourceTypes);

  mTVService = TVServiceFactory::AutoCreateTVService();
  NS_ENSURE_TRUE(mTVService, false);

  rv = aData->GetStreamType(&mStreamType);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

/* virtual */ JSObject*
TVTuner::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TVTunerBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
TVTuner::SetCurrentSource(TVSourceType aSourceType)
{
  ErrorResult error;
  if (mCurrentSource) {
    if (aSourceType == mCurrentSource->Type()) {
      // No actual change.
      return NS_OK;
    }

    // No need to stay tuned for non-current sources.
    nsresult rv = mCurrentSource->UnsetCurrentChannel();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  for (uint32_t i = 0; i < mSources.Length(); i++) {
    if (aSourceType == mSources[i]->Type()) {
      mCurrentSource = mSources[i];
      break;
    }
  }

  nsresult rv = InitMediaStream();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return DispatchCurrentSourceChangedEvent(mCurrentSource);
}

nsresult
TVTuner::DispatchTVEvent(nsIDOMEvent* aEvent)
{
  return DispatchTrustedEvent(aEvent);
}

void
TVTuner::GetSupportedSourceTypes(nsTArray<TVSourceType>& aSourceTypes,
                                 ErrorResult& aRv) const
{
  aSourceTypes = mSupportedSourceTypes;
}

already_AddRefed<Promise>
TVTuner::GetSources(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  promise->MaybeResolve(mSources);

  return promise.forget();
}

already_AddRefed<Promise>
TVTuner::SetCurrentSource(const TVSourceType aSourceType, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // |SetCurrentSource(const TVSourceType)| will be called once |notifySuccess|
  // of the callback is invoked.
  nsCOMPtr<nsITVServiceCallback> callback =
    new TVServiceSourceSetterCallback(this, promise, aSourceType);
  nsresult rv = mTVService->SetSource(mId, ToTVSourceTypeStr(aSourceType), callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}

void
TVTuner::GetId(nsAString& aId) const
{
  aId = mId;
}

already_AddRefed<TVSource>
TVTuner::GetCurrentSource() const
{
  RefPtr<TVSource> currentSource = mCurrentSource;
  return currentSource.forget();
}

already_AddRefed<DOMMediaStream>
TVTuner::GetStream() const
{
  RefPtr<DOMMediaStream> stream = mStream;
  return stream.forget();
}

nsresult
TVTuner::ReloadMediaStream()
{
  return InitMediaStream();
}

nsresult
TVTuner::InitMediaStream()
{
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  RefPtr<DOMMediaStream> stream = nullptr;
  if (mStreamType == nsITVTunerData::TV_STREAM_TYPE_HW) {
    stream = DOMHwMediaStream::CreateHwStream(window);
  } else if (mStreamType == nsITVTunerData::TV_STREAM_TYPE_SIMULATOR) {
    stream = CreateSimulatedMediaStream();
  }

  mStream = stream.forget();
  return NS_OK;
}

already_AddRefed<DOMMediaStream>
TVTuner::CreateSimulatedMediaStream()
{
  nsCOMPtr<nsPIDOMWindowInner> domWin = GetOwner();
  if (NS_WARN_IF(!domWin)) {
    return nullptr;
  }

  nsIDocument* doc = domWin->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }

  ErrorResult error;
  RefPtr<Element> element = doc->CreateElement(VIDEO_TAG, error);
  if (NS_WARN_IF(error.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(element));
  if (NS_WARN_IF(!content)) {
    return nullptr;
  }

  HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(content.get());
  if (NS_WARN_IF(!mediaElement)) {
    return nullptr;
  }

  mediaElement->SetAutoplay(true, error);
  if (NS_WARN_IF(error.Failed())) {
    return nullptr;
  }

  mediaElement->SetLoop(true, error);
  if (NS_WARN_IF(error.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsITVSimulatorService> simService(do_QueryInterface(mTVService));
  if (NS_WARN_IF(!simService)) {
    return nullptr;
  }

  if (NS_WARN_IF(!mCurrentSource)) {
    return nullptr;
  }

  RefPtr<TVChannel> currentChannel = mCurrentSource->GetCurrentChannel();
  if (NS_WARN_IF(!currentChannel)) {
    return nullptr;
  }

  nsString currentChannelNumber;
  currentChannel->GetNumber(currentChannelNumber);
  if (currentChannelNumber.IsEmpty()) {
    return nullptr;
  }

  nsString currentVideoBlobUrl;
  nsresult rv = simService->GetSimulatorVideoBlobURL(mId,
                                                     ToTVSourceTypeStr(mCurrentSource->Type()),
                                                     currentChannelNumber,
                                                     domWin,
                                                     currentVideoBlobUrl);
  if (NS_WARN_IF(NS_FAILED(rv) || currentVideoBlobUrl.IsEmpty())) {
    return nullptr;
  }

  mediaElement->SetSrc(currentVideoBlobUrl, error);
  if (NS_WARN_IF(error.Failed())) {
    return nullptr;
  }

  // See Media Capture from DOM Elements spec.
  // http://www.w3.org/TR/mediacapture-fromelement/
  RefPtr<DOMMediaStream> stream = mediaElement->MozCaptureStream(error);
  if (NS_WARN_IF(error.Failed())) {
    return nullptr;
  }

  return stream.forget();
}

nsresult
TVTuner::DispatchCurrentSourceChangedEvent(TVSource* aSource)
{
  TVCurrentSourceChangedEventInit init;
  init.mSource = aSource;
  nsCOMPtr<nsIDOMEvent> event =
    TVCurrentSourceChangedEvent::Constructor(this,
                                             NS_LITERAL_STRING("currentsourcechanged"),
                                             init);
  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod<nsCOMPtr<nsIDOMEvent>>(this,
                                             &TVTuner::DispatchTVEvent,
                                             event);
  return NS_DispatchToCurrentThread(runnable);
}

nsresult
TVTuner::NotifyImageSizeChanged(uint32_t aWidth, uint32_t aHeight)
{
  DOMHwMediaStream* hwMediaStream = mStream->AsDOMHwMediaStream();

  if (hwMediaStream) {
    hwMediaStream->SetImageSize(aWidth, aHeight);
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
