/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TVManager.h"
#include "mozilla/dom/TVSource.h"
#include "mozilla/dom/TVTuner.h"
#include "TVServiceCallbacks.h"

namespace mozilla {
namespace dom {

/*
 * Implementation of TVServiceSourceSetterCallback
 */

NS_IMPL_CYCLE_COLLECTION(TVServiceSourceSetterCallback, mTuner, mPromise)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVServiceSourceSetterCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVServiceSourceSetterCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVServiceSourceSetterCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITVServiceCallback)
NS_INTERFACE_MAP_END

TVServiceSourceSetterCallback::TVServiceSourceSetterCallback(TVTuner* aTuner,
                                                             Promise* aPromise,
                                                             TVSourceType aSourceType)
  : mTuner(aTuner)
  , mPromise(aPromise)
  , mSourceType(aSourceType)
{
  MOZ_ASSERT(mTuner);
  MOZ_ASSERT(mPromise);
}

TVServiceSourceSetterCallback::~TVServiceSourceSetterCallback()
{
}

/* virtual */ NS_IMETHODIMP
TVServiceSourceSetterCallback::NotifySuccess(nsIArray* aDataList)
{
  // |aDataList| is expected to be null for setter callbacks.
  if (aDataList) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Update correspondent fields in |mTuner| in follow-up patches.

  mPromise->MaybeResolve(JS::UndefinedHandleValue);
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVServiceSourceSetterCallback::NotifyError(uint16_t aErrorCode)
{
  switch (aErrorCode) {
  case nsITVServiceCallback::TV_ERROR_FAILURE:
  case nsITVServiceCallback::TV_ERROR_INVALID_ARG:
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NO_SIGNAL:
    mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NOT_SUPPORTED:
    mPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return NS_OK;
  }

  mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  return NS_ERROR_ILLEGAL_VALUE;
}


/*
 * Implementation of TVServiceChannelScanCallback
 */

NS_IMPL_CYCLE_COLLECTION(TVServiceChannelScanCallback, mSource, mPromise)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVServiceChannelScanCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVServiceChannelScanCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVServiceChannelScanCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITVServiceCallback)
NS_INTERFACE_MAP_END

TVServiceChannelScanCallback::TVServiceChannelScanCallback(TVSource* aSource,
                                                           Promise* aPromise,
                                                           bool aIsScanning)
  : mSource(aSource)
  , mPromise(aPromise)
  , mIsScanning(aIsScanning)
{
  MOZ_ASSERT(mSource);
  MOZ_ASSERT(mPromise);
}

TVServiceChannelScanCallback::~TVServiceChannelScanCallback()
{
}

/* virtual */ NS_IMETHODIMP
TVServiceChannelScanCallback::NotifySuccess(nsIArray* aDataList)
{
  // |aDataList| is expected to be null for setter callbacks.
  if (aDataList) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Update correspondent fields in |mSource| in follow-up patches.

  mPromise->MaybeResolve(JS::UndefinedHandleValue);
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVServiceChannelScanCallback::NotifyError(uint16_t aErrorCode)
{
  switch (aErrorCode) {
  case nsITVServiceCallback::TV_ERROR_FAILURE:
  case nsITVServiceCallback::TV_ERROR_INVALID_ARG:
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NO_SIGNAL:
    mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NOT_SUPPORTED:
    mPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return NS_OK;
  }

  mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  return NS_ERROR_ILLEGAL_VALUE;
}


/*
 * Implementation of TVServiceChannelSetterCallback
 */

NS_IMPL_CYCLE_COLLECTION(TVServiceChannelSetterCallback, mSource, mPromise)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVServiceChannelSetterCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVServiceChannelSetterCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVServiceChannelSetterCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITVServiceCallback)
NS_INTERFACE_MAP_END

TVServiceChannelSetterCallback::TVServiceChannelSetterCallback(TVSource* aSource,
                                                               Promise* aPromise,
                                                               const nsAString& aChannelNumber)
  : mSource(aSource)
  , mPromise(aPromise)
  , mChannelNumber(aChannelNumber)
{
  MOZ_ASSERT(mSource);
  MOZ_ASSERT(mPromise);
  MOZ_ASSERT(!mChannelNumber.IsEmpty());
}

TVServiceChannelSetterCallback::~TVServiceChannelSetterCallback()
{
}

/* virtual */ NS_IMETHODIMP
TVServiceChannelSetterCallback::NotifySuccess(nsIArray* aDataList)
{
  // |aDataList| is expected to be with only one element.
  if (!aDataList) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  nsresult rv = aDataList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  if (length != 1) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Update correspondent fields in |mSource| in follow-up patches.

  mPromise->MaybeResolve(JS::UndefinedHandleValue);
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVServiceChannelSetterCallback::NotifyError(uint16_t aErrorCode)
{
  switch (aErrorCode) {
  case nsITVServiceCallback::TV_ERROR_FAILURE:
  case nsITVServiceCallback::TV_ERROR_INVALID_ARG:
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NO_SIGNAL:
    mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NOT_SUPPORTED:
    mPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return NS_OK;
  }

  mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  return NS_ERROR_ILLEGAL_VALUE;
}


/*
 * Implementation of TVServiceTunerGetterCallback
 */

NS_IMPL_CYCLE_COLLECTION(TVServiceTunerGetterCallback, mManager, mPromise)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVServiceTunerGetterCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVServiceTunerGetterCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVServiceTunerGetterCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITVServiceCallback)
NS_INTERFACE_MAP_END

TVServiceTunerGetterCallback::TVServiceTunerGetterCallback(TVManager* aManager,
                                                           Promise* aPromise)
  : mManager(aManager)
  , mPromise(aPromise)
{
  MOZ_ASSERT(mManager);
  MOZ_ASSERT(mPromise);
}

TVServiceTunerGetterCallback::~TVServiceTunerGetterCallback()
{
}

/* virtual */ NS_IMETHODIMP
TVServiceTunerGetterCallback::NotifySuccess(nsIArray* aDataList)
{
  if (!aDataList) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Implement in follow-up patches.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVServiceTunerGetterCallback::NotifyError(uint16_t aErrorCode)
{
  switch (aErrorCode) {
  case nsITVServiceCallback::TV_ERROR_FAILURE:
  case nsITVServiceCallback::TV_ERROR_INVALID_ARG:
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NO_SIGNAL:
    mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NOT_SUPPORTED:
    mPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return NS_OK;
  }

  mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  return NS_ERROR_ILLEGAL_VALUE;
}


/*
 * Implementation of TVServiceChannelGetterCallback
 */

NS_IMPL_CYCLE_COLLECTION(TVServiceChannelGetterCallback, mSource, mPromise)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVServiceChannelGetterCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVServiceChannelGetterCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVServiceChannelGetterCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITVServiceCallback)
NS_INTERFACE_MAP_END

TVServiceChannelGetterCallback::TVServiceChannelGetterCallback(TVSource* aSource,
                                                               Promise* aPromise)
  : mSource(aSource)
  , mPromise(aPromise)
{
  MOZ_ASSERT(mSource);
  MOZ_ASSERT(mPromise);
}

TVServiceChannelGetterCallback::~TVServiceChannelGetterCallback()
{
}

/* virtual */ NS_IMETHODIMP
TVServiceChannelGetterCallback::NotifySuccess(nsIArray* aDataList)
{
  if (!aDataList) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Implement in follow-up patches.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVServiceChannelGetterCallback::NotifyError(uint16_t aErrorCode)
{
  switch (aErrorCode) {
  case nsITVServiceCallback::TV_ERROR_FAILURE:
  case nsITVServiceCallback::TV_ERROR_INVALID_ARG:
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NO_SIGNAL:
    mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NOT_SUPPORTED:
    mPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return NS_OK;
  }

  mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  return NS_ERROR_ILLEGAL_VALUE;
}

/*
 * Implementation of TVServiceProgramGetterCallback
 */

NS_IMPL_CYCLE_COLLECTION_0(TVServiceProgramGetterCallback)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVServiceProgramGetterCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVServiceProgramGetterCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVServiceProgramGetterCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITVServiceCallback)
NS_INTERFACE_MAP_END

TVServiceProgramGetterCallback::TVServiceProgramGetterCallback(TVChannel* aChannel,
                                                               Promise* aPromise,
                                                               bool aIsSingular)
  : mChannel(aChannel)
  , mPromise(aPromise)
  , mIsSingular(aIsSingular)
{
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(mPromise);
}

TVServiceProgramGetterCallback::~TVServiceProgramGetterCallback()
{
}

/* virtual */ NS_IMETHODIMP
TVServiceProgramGetterCallback::NotifySuccess(nsIArray* aDataList)
{
  if (!aDataList) {
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Store the results to IndexedDB in follow-up patches.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVServiceProgramGetterCallback::NotifyError(uint16_t aErrorCode)
{
  switch (aErrorCode) {
  case nsITVServiceCallback::TV_ERROR_FAILURE:
  case nsITVServiceCallback::TV_ERROR_INVALID_ARG:
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NO_SIGNAL:
    mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    return NS_OK;
  case nsITVServiceCallback::TV_ERROR_NOT_SUPPORTED:
    mPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return NS_OK;
  }

  mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  return NS_ERROR_ILLEGAL_VALUE;
}

} // namespace dom
} // namespace mozilla
