/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChannelInfo.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "mozilla/dom/Document.h"
#include "nsIGlobalObject.h"
#include "nsIHttpChannel.h"
#include "nsSerializationHelper.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "nsNetUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

void ChannelInfo::InitFromDocument(Document* aDoc) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInited, "Cannot initialize the object twice");

  nsCOMPtr<nsISupports> securityInfoSupports(aDoc->GetSecurityInfo());
  nsCOMPtr<nsITransportSecurityInfo> securityInfo(
      do_QueryInterface(securityInfoSupports));
  if (securityInfo) {
    SetSecurityInfo(securityInfo);
  }

  mInited = true;
}

void ChannelInfo::InitFromChannel(nsIChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInited, "Cannot initialize the object twice");

  nsCOMPtr<nsISupports> securityInfoSupports;
  aChannel->GetSecurityInfo(getter_AddRefs(securityInfoSupports));
  nsCOMPtr<nsITransportSecurityInfo> securityInfo(
      do_QueryInterface(securityInfoSupports));
  if (securityInfo) {
    SetSecurityInfo(securityInfo);
  }

  mInited = true;
}

void ChannelInfo::InitFromChromeGlobal(nsIGlobalObject* aGlobal) {
  MOZ_ASSERT(!mInited, "Cannot initialize the object twice");
  MOZ_ASSERT(aGlobal);

  MOZ_RELEASE_ASSERT(aGlobal->PrincipalOrNull()->IsSystemPrincipal());

  mSecurityInfo = nullptr;
  mInited = true;
}

void ChannelInfo::InitFromTransportSecurityInfo(
    nsITransportSecurityInfo* aSecurityInfo) {
  MOZ_ASSERT(!mInited, "Cannot initialize the object twice");

  mSecurityInfo = aSecurityInfo;
  mInited = true;
}

void ChannelInfo::SetSecurityInfo(nsITransportSecurityInfo* aSecurityInfo) {
  MOZ_ASSERT(!mSecurityInfo, "security info should only be set once");
  mSecurityInfo = aSecurityInfo;
}

nsresult ChannelInfo::ResurrectInfoOnChannel(nsIChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInited);

  if (mSecurityInfo) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
    MOZ_ASSERT(httpChannel);
    net::HttpBaseChannel* httpBaseChannel =
        static_cast<net::HttpBaseChannel*>(httpChannel.get());
    nsresult rv = httpBaseChannel->OverrideSecurityInfo(mSecurityInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

already_AddRefed<nsITransportSecurityInfo> ChannelInfo::SecurityInfo() const {
  // This may be called when mInited is false, for example if we try to store
  // a synthesized Response object into the Cache.  Uninitialized and empty
  // ChannelInfo objects are indistinguishable at the IPC level, so this is
  // fine.
  nsCOMPtr<nsITransportSecurityInfo> securityInfo(mSecurityInfo);
  return securityInfo.forget();
}
