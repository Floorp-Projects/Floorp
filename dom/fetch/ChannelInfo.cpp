/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChannelInfo.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIDocument.h"
#include "nsIHttpChannel.h"
#include "nsSerializationHelper.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/ipc/ChannelInfo.h"
#include "nsNetUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

void
ChannelInfo::InitFromDocument(nsIDocument* aDoc)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInited, "Cannot initialize the object twice");

  nsCOMPtr<nsISupports> securityInfo = aDoc->GetSecurityInfo();
  if (securityInfo) {
    SetSecurityInfo(securityInfo);
  }

  mInited = true;
}

void
ChannelInfo::InitFromChannel(nsIChannel* aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInited, "Cannot initialize the object twice");

  nsCOMPtr<nsISupports> securityInfo;
  aChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
  if (securityInfo) {
    SetSecurityInfo(securityInfo);
  }

  mInited = true;
}

void
ChannelInfo::InitFromChromeGlobal(nsIGlobalObject* aGlobal)
{
  MOZ_ASSERT(!mInited, "Cannot initialize the object twice");
  MOZ_ASSERT(aGlobal);

  MOZ_RELEASE_ASSERT(
    nsContentUtils::IsSystemPrincipal(aGlobal->PrincipalOrNull()));

  mSecurityInfo.Truncate();
  mInited = true;
}

void
ChannelInfo::InitFromIPCChannelInfo(const mozilla::ipc::IPCChannelInfo& aChannelInfo)
{
  MOZ_ASSERT(!mInited, "Cannot initialize the object twice");

  mSecurityInfo = aChannelInfo.securityInfo();

  mInited = true;
}

void
ChannelInfo::SetSecurityInfo(nsISupports* aSecurityInfo)
{
  MOZ_ASSERT(mSecurityInfo.IsEmpty(), "security info should only be set once");
  nsCOMPtr<nsISerializable> serializable = do_QueryInterface(aSecurityInfo);
  if (!serializable) {
    NS_WARNING("A non-serializable object was passed to InternalResponse::SetSecurityInfo");
    return;
  }
  NS_SerializeToString(serializable, mSecurityInfo);
}

nsresult
ChannelInfo::ResurrectInfoOnChannel(nsIChannel* aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInited);

  if (!mSecurityInfo.IsEmpty()) {
    nsCOMPtr<nsISupports> infoObj;
    nsresult rv = NS_DeserializeObject(mSecurityInfo, getter_AddRefs(infoObj));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    nsCOMPtr<nsIHttpChannel> httpChannel =
      do_QueryInterface(aChannel);
    MOZ_ASSERT(httpChannel);
    net::HttpBaseChannel* httpBaseChannel =
      static_cast<net::HttpBaseChannel*>(httpChannel.get());
    rv = httpBaseChannel->OverrideSecurityInfo(infoObj);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

mozilla::ipc::IPCChannelInfo
ChannelInfo::AsIPCChannelInfo() const
{
  // This may be called when mInited is false, for example if we try to store
  // a synthesized Response object into the Cache.  Uninitialized and empty
  // ChannelInfo objects are indistinguishable at the IPC level, so this is
  // fine.

  IPCChannelInfo ipcInfo;

  ipcInfo.securityInfo() = mSecurityInfo;

  return ipcInfo;
}
