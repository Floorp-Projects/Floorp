/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PeerIdentity.h"

#include "mozilla/DebugOnly.h"
#include "nsCOMPtr.h"
#include "nsIIDNService.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

bool
PeerIdentity::Equals(const PeerIdentity& aOther) const
{
  return Equals(aOther.mPeerIdentity);
}

bool
PeerIdentity::Equals(const nsAString& aOtherString) const
{
  nsString user;
  GetUser(mPeerIdentity, user);
  nsString otherUser;
  GetUser(aOtherString, otherUser);
  if (user != otherUser) {
    return false;
  }

  nsString host;
  GetHost(mPeerIdentity, host);
  nsString otherHost;
  GetHost(aOtherString, otherHost);

  nsresult rv;
  nsCOMPtr<nsIIDNService> idnService
    = do_GetService("@mozilla.org/network/idn-service;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return host == otherHost;
  }

  nsCString normHost;
  GetNormalizedHost(idnService, host, normHost);
  nsCString normOtherHost;
  GetNormalizedHost(idnService, otherHost, normOtherHost);
  return normHost == normOtherHost;
}

/* static */ void
PeerIdentity::GetUser(const nsAString& aPeerIdentity, nsAString& aUser)
{
  int32_t at = aPeerIdentity.FindChar('@');
  if (at >= 0) {
    aUser = Substring(aPeerIdentity, 0, at);
  } else {
    aUser.Truncate();
  }
}

/* static */ void
PeerIdentity::GetHost(const nsAString& aPeerIdentity, nsAString& aHost)
{
  int32_t at = aPeerIdentity.FindChar('@');
  if (at >= 0) {
    aHost = Substring(aPeerIdentity, at + 1);
  } else {
    aHost = aPeerIdentity;
  }
}

/* static */ void
PeerIdentity::GetNormalizedHost(const nsCOMPtr<nsIIDNService>& aIdnService,
                                const nsAString& aHost,
                                nsACString& aNormalizedHost)
{
  const nsCString chost = NS_ConvertUTF16toUTF8(aHost);
  DebugOnly<nsresult> rv = aIdnService->ConvertUTF8toACE(chost, aNormalizedHost);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to convert UTF-8 host to ASCII");
}

} /* namespace mozilla */
