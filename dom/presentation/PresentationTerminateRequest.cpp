/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationTerminateRequest.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDevice.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(PresentationTerminateRequest, nsIPresentationTerminateRequest)

PresentationTerminateRequest::PresentationTerminateRequest(
                                 nsIPresentationDevice* aDevice,
                                 const nsAString& aPresentationId,
                                 nsIPresentationControlChannel* aControlChannel,
                                 bool aIsFromReceiver)
  : mPresentationId(aPresentationId)
  , mDevice(aDevice)
  , mControlChannel(aControlChannel)
  , mIsFromReceiver(aIsFromReceiver)
{
}

PresentationTerminateRequest::~PresentationTerminateRequest()
{
}

// nsIPresentationTerminateRequest
NS_IMETHODIMP
PresentationTerminateRequest::GetDevice(nsIPresentationDevice** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  nsCOMPtr<nsIPresentationDevice> device = mDevice;
  device.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP
PresentationTerminateRequest::GetPresentationId(nsAString& aRetVal)
{
  aRetVal = mPresentationId;

  return NS_OK;
}

NS_IMETHODIMP
PresentationTerminateRequest::GetControlChannel(
                                        nsIPresentationControlChannel** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  nsCOMPtr<nsIPresentationControlChannel> controlChannel = mControlChannel;
  controlChannel.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP
PresentationTerminateRequest::GetIsFromReceiver(bool* aRetVal)
{
  *aRetVal = mIsFromReceiver;

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
