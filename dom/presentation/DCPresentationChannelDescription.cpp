/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCPresentationChannelDescription.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(DCPresentationChannelDescription,
                  nsIPresentationChannelDescription)

NS_IMETHODIMP
DCPresentationChannelDescription::GetType(uint8_t* aRetVal)
{
  if (NS_WARN_IF(!aRetVal)) {
    return NS_ERROR_INVALID_POINTER;
  }

  *aRetVal = nsIPresentationChannelDescription::TYPE_DATACHANNEL;
  return NS_OK;
}

NS_IMETHODIMP
DCPresentationChannelDescription::GetTcpAddress(nsIArray** aRetVal)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DCPresentationChannelDescription::GetTcpPort(uint16_t* aRetVal)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DCPresentationChannelDescription::GetDataChannelSDP(nsAString& aDataChannelSDP)
{
  aDataChannelSDP = mSDP;
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
