/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TVChannel.h"
#include "mozilla/dom/TVCurrentChannelChangedEvent.h"
#include "mozilla/dom/TVEITBroadcastedEvent.h"
#include "mozilla/dom/TVListeners.h"
#include "mozilla/dom/TVScanningStateChangedEvent.h"
#include "mozilla/dom/TVServiceCallbacks.h"
#include "mozilla/dom/TVServiceFactory.h"
#include "mozilla/dom/TVTuner.h"
#include "mozilla/dom/TVUtils.h"
#include "nsITVService.h"
#include "nsServiceManagerUtils.h"
#include "TVSource.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(TVSource)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TVSource,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTVService)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTuner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentChannel)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TVSource,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTVService)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTuner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCurrentChannel)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(TVSource, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TVSource, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TVSource)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TVSource::TVSource(nsPIDOMWindow* aWindow,
                   TVSourceType aType,
                   TVTuner* aTuner)
  : DOMEventTargetHelper(aWindow)
  , mTuner(aTuner)
  , mType(aType)
  , mIsScanning(false)
{
  MOZ_ASSERT(mTuner);
}

TVSource::~TVSource()
{
  Shutdown();
}

/* static */ already_AddRefed<TVSource>
TVSource::Create(nsPIDOMWindow* aWindow,
                 TVSourceType aType,
                 TVTuner* aTuner)
{
  nsRefPtr<TVSource> source = new TVSource(aWindow, aType, aTuner);
  return (source->Init()) ? source.forget() : nullptr;
}

bool
TVSource::Init()
{
  mTVService = TVServiceFactory::AutoCreateTVService();
  NS_ENSURE_TRUE(mTVService, false);

  nsCOMPtr<nsITVSourceListener> sourceListener;
  mTVService->GetSourceListener(getter_AddRefs(sourceListener));
  NS_ENSURE_TRUE(sourceListener, false);
  (static_cast<TVSourceListener*>(sourceListener.get()))->RegisterSource(this);

  return true;
}

void
TVSource::Shutdown()
{
  if (!mTVService) {
    return;
  }

  nsCOMPtr<nsITVSourceListener> sourceListener;
  mTVService->GetSourceListener(getter_AddRefs(sourceListener));
  if (!sourceListener) {
    return;
  }
  (static_cast<TVSourceListener*>(sourceListener.get()))->UnregisterSource(this);
}

/* virtual */ JSObject*
TVSource::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TVSourceBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
TVSource::SetCurrentChannel(nsITVChannelData* aChannelData)
{
  if (!aChannelData) {
    return NS_ERROR_INVALID_ARG;
  }

  nsString newChannelNumber;
  nsresult rv = aChannelData->GetNumber(newChannelNumber);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (newChannelNumber.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mCurrentChannel) {
    nsString currentChannelNumber;
    mCurrentChannel->GetNumber(currentChannelNumber);
    if (newChannelNumber.Equals(currentChannelNumber)) {
      // No actual change.
      return NS_OK;
    }
  }

  mCurrentChannel = TVChannel::Create(GetOwner(), this, aChannelData);
  NS_ENSURE_TRUE(mCurrentChannel, NS_ERROR_DOM_ABORT_ERR);

  nsRefPtr<TVSource> currentSource = mTuner->GetCurrentSource();
  if (currentSource && mType == currentSource->Type()) {
    rv = mTuner->ReloadMediaStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return DispatchCurrentChannelChangedEvent(mCurrentChannel);
}

nsresult
TVSource::UnsetCurrentChannel()
{
  mCurrentChannel = nullptr;
  return DispatchCurrentChannelChangedEvent(mCurrentChannel);
}

void
TVSource::SetIsScanning(bool aIsScanning)
{
  mIsScanning = aIsScanning;
}

nsresult
TVSource::DispatchTVEvent(nsIDOMEvent* aEvent)
{
  return DispatchTrustedEvent(aEvent);
}

already_AddRefed<Promise>
TVSource::GetChannels(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // The operation is prohibited when the source is scanning channels.
  if (mIsScanning) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsString tunerId;
  mTuner->GetId(tunerId);

  nsCOMPtr<nsITVServiceCallback> callback =
    new TVServiceChannelGetterCallback(this, promise);
  nsresult rv =
    mTVService->GetChannels(tunerId, ToTVSourceTypeStr(mType), callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
TVSource::SetCurrentChannel(const nsAString& aChannelNumber,
                            ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // The operation is prohibited when the source is scanning channels.
  if (mIsScanning) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsString tunerId;
  mTuner->GetId(tunerId);

  nsCOMPtr<nsITVServiceCallback> callback =
    new TVServiceChannelSetterCallback(this, promise, aChannelNumber);
  nsresult rv =
    mTVService->SetChannel(tunerId, ToTVSourceTypeStr(mType), aChannelNumber, callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
TVSource::StartScanning(const TVStartScanningOptions& aOptions,
                        ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsString tunerId;
  mTuner->GetId(tunerId);

  bool isRescanned = aOptions.mIsRescanned.WasPassed() &&
                     aOptions.mIsRescanned.Value();

  if (isRescanned) {
    nsresult rv = mTVService->ClearScannedChannelsCache();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }

    rv = DispatchScanningStateChangedEvent(TVScanningState::Cleared, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }
  }

  // |SetIsScanning(bool)| should be called once |notifySuccess()| of this
  // callback is invoked.
  nsCOMPtr<nsITVServiceCallback> callback =
    new TVServiceChannelScanCallback(this, promise, true);
  nsresult rv =
    mTVService->StartScanningChannels(tunerId, ToTVSourceTypeStr(mType), callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
TVSource::StopScanning(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsString tunerId;
  mTuner->GetId(tunerId);

  // |SetIsScanning(bool)| should be called once |notifySuccess()| of this
  // callback is invoked.
  nsCOMPtr<nsITVServiceCallback> callback =
    new TVServiceChannelScanCallback(this, promise, false);
  nsresult rv =
    mTVService->StopScanningChannels(tunerId, ToTVSourceTypeStr(mType), callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}

already_AddRefed<TVTuner>
TVSource::Tuner() const
{
  nsRefPtr<TVTuner> tuner = mTuner;
  return tuner.forget();
}

TVSourceType
TVSource::Type() const
{
  return mType;
}

bool
TVSource::IsScanning() const
{
  return mIsScanning;
}

already_AddRefed<TVChannel>
TVSource::GetCurrentChannel() const
{
  nsRefPtr<TVChannel> currentChannel = mCurrentChannel;
  return currentChannel.forget();
}

nsresult
TVSource::NotifyChannelScanned(nsITVChannelData* aChannelData)
{
  nsRefPtr<TVChannel> channel = TVChannel::Create(GetOwner(), this, aChannelData);
  NS_ENSURE_TRUE(channel, NS_ERROR_DOM_ABORT_ERR);

  return DispatchScanningStateChangedEvent(TVScanningState::Scanned, channel);
}

nsresult
TVSource::NotifyChannelScanComplete()
{
  SetIsScanning(false);
  return DispatchScanningStateChangedEvent(TVScanningState::Completed, nullptr);
}

nsresult
TVSource::NotifyChannelScanStopped()
{
  SetIsScanning(false);
  return DispatchScanningStateChangedEvent(TVScanningState::Stopped, nullptr);
}

nsresult
TVSource::NotifyEITBroadcasted(nsITVChannelData* aChannelData,
                               nsITVProgramData** aProgramDataList,
                               uint32_t aCount)
{
  nsRefPtr<TVChannel> channel = TVChannel::Create(GetOwner(), this, aChannelData);
  Sequence<OwningNonNull<TVProgram>> programs;
  for (uint32_t i = 0; i < aCount; i++) {
    nsRefPtr<TVProgram> program =
      new TVProgram(GetOwner(), channel, aProgramDataList[i]);
    *programs.AppendElement(fallible) = program;
  }
  return DispatchEITBroadcastedEvent(programs);
}

nsresult
TVSource::DispatchCurrentChannelChangedEvent(TVChannel* aChannel)
{
  TVCurrentChannelChangedEventInit init;
  init.mChannel = aChannel;
  nsCOMPtr<nsIDOMEvent> event =
    TVCurrentChannelChangedEvent::Constructor(this,
                                              NS_LITERAL_STRING("currentchannelchanged"),
                                              init);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethodWithArg<nsCOMPtr<nsIDOMEvent>>(this,
                                                       &TVSource::DispatchTVEvent,
                                                       event);
  return NS_DispatchToCurrentThread(runnable);
}

nsresult
TVSource::DispatchScanningStateChangedEvent(TVScanningState aState,
                                            TVChannel* aChannel)
{
  TVScanningStateChangedEventInit init;
  init.mState = aState;
  init.mChannel = aChannel;
  nsCOMPtr<nsIDOMEvent> event =
    TVScanningStateChangedEvent::Constructor(this,
                                             NS_LITERAL_STRING("scanningstatechanged"),
                                             init);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethodWithArg<nsCOMPtr<nsIDOMEvent>>(this,
                                                       &TVSource::DispatchTVEvent,
                                                       event);
  return NS_DispatchToCurrentThread(runnable);
}

nsresult
TVSource::DispatchEITBroadcastedEvent(const Sequence<OwningNonNull<TVProgram>>& aPrograms)
{
  TVEITBroadcastedEventInit init;
  init.mPrograms = aPrograms;
  nsCOMPtr<nsIDOMEvent> event =
    TVEITBroadcastedEvent::Constructor(this,
                                       NS_LITERAL_STRING("eitbroadcasted"),
                                       init);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethodWithArg<nsCOMPtr<nsIDOMEvent>>(this,
                                                       &TVSource::DispatchTVEvent,
                                                       event);
  return NS_DispatchToCurrentThread(runnable);
}

} // namespace dom
} // namespace mozilla
