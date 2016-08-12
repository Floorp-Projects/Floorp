/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TVServiceCallbacks.h"
#include "mozilla/dom/TVServiceFactory.h"
#include "mozilla/dom/TVSource.h"
#include "mozilla/dom/TVTuner.h"
#include "mozilla/dom/TVUtils.h"
#include "nsITVService.h"
#include "nsServiceManagerUtils.h"
#include "TVChannel.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(TVChannel, DOMEventTargetHelper,
                                   mTVService, mSource)

NS_IMPL_ADDREF_INHERITED(TVChannel, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TVChannel, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TVChannel)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TVChannel::TVChannel(nsPIDOMWindowInner* aWindow,
                     TVSource* aSource)
  : DOMEventTargetHelper(aWindow)
  , mSource(aSource)
{
  MOZ_ASSERT(mSource);
}

TVChannel::~TVChannel()
{
}

/* static */ already_AddRefed<TVChannel>
TVChannel::Create(nsPIDOMWindowInner* aWindow,
                  TVSource* aSource,
                  nsITVChannelData* aData)
{
  RefPtr<TVChannel> channel = new TVChannel(aWindow, aSource);
  return (channel->Init(aData)) ? channel.forget() : nullptr;
}

bool
TVChannel::Init(nsITVChannelData* aData)
{
  NS_ENSURE_TRUE(aData, false);

  nsString channelType;
  aData->GetType(channelType);
  mType = ToTVChannelType(channelType);
  if (NS_WARN_IF(mType == TVChannelType::EndGuard_)) {
    return false;
  }

  aData->GetNetworkId(mNetworkId);
  aData->GetTransportStreamId(mTransportStreamId);
  aData->GetServiceId(mServiceId);
  aData->GetName(mName);
  aData->GetNumber(mNumber);
  mIsEmergency = aData->GetIsEmergency();
  mIsFree = aData->GetIsFree();

  mTVService = TVServiceFactory::AutoCreateTVService();
  NS_ENSURE_TRUE(mTVService, false);

  return true;
}

/* virtual */ JSObject*
TVChannel::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TVChannelBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
TVChannel::DispatchTVEvent(nsIDOMEvent* aEvent)
{
  return DispatchTrustedEvent(aEvent);
}

already_AddRefed<Promise>
TVChannel::GetPrograms(const TVGetProgramsOptions& aOptions, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<TVTuner> tuner = mSource->Tuner();
  nsString tunerId;
  tuner->GetId(tunerId);

  uint64_t startTime = aOptions.mStartTime.WasPassed() ?
                       aOptions.mStartTime.Value() :
                       PR_Now();
  uint64_t endTime = aOptions.mDuration.WasPassed() ?
                     (startTime + aOptions.mDuration.Value()) :
                     std::numeric_limits<uint64_t>::max();
  nsCOMPtr<nsITVServiceCallback> callback =
    new TVServiceProgramGetterCallback(this, promise, false);
  nsresult rv =
    mTVService->GetPrograms(tunerId,
                            ToTVSourceTypeStr(mSource->Type()),
                            mNumber,
                            startTime,
                            endTime,
                            callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}

void
TVChannel::GetNetworkId(nsAString& aNetworkId) const
{
  aNetworkId = mNetworkId;
}

void
TVChannel::GetTransportStreamId(nsAString& aTransportStreamId) const
{
  aTransportStreamId = mTransportStreamId;
}

void
TVChannel::GetServiceId(nsAString& aServiceId) const
{
  aServiceId = mServiceId;
}

already_AddRefed<TVSource>
TVChannel::Source() const
{
  RefPtr<TVSource> source = mSource;
  return source.forget();
}

TVChannelType
TVChannel::Type() const
{
  return mType;
}

void
TVChannel::GetName(nsAString& aName) const
{
  aName = mName;
}

void
TVChannel::GetNumber(nsAString& aNumber) const
{
  aNumber = mNumber;
}

bool
TVChannel::IsEmergency() const
{
  return mIsEmergency;
}

bool
TVChannel::IsFree() const
{
  return mIsFree;
}

already_AddRefed<Promise>
TVChannel::GetCurrentProgram(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<TVTuner> tuner = mSource->Tuner();
  nsString tunerId;
  tuner->GetId(tunerId);

  // Get only one program from now on.
  nsCOMPtr<nsITVServiceCallback> callback =
    new TVServiceProgramGetterCallback(this, promise, true);
  nsresult rv =
    mTVService->GetPrograms(tunerId,
                            ToTVSourceTypeStr(mSource->Type()),
                            mNumber,
                            PR_Now(),
                            std::numeric_limits<uint64_t>::max(),
                            callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}

} // namespace dom
} // namespace mozilla
