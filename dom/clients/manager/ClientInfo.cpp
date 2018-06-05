/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientInfo.h"

#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalInfoToPrincipal;

ClientInfo::ClientInfo(const nsID& aId,
                       ClientType aType,
                       const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                       const TimeStamp& aCreationTime)
  : mData(MakeUnique<IPCClientInfo>(aId, aType, aPrincipalInfo, aCreationTime,
                                    EmptyCString(),
                                    mozilla::dom::FrameType::None))
{
}

ClientInfo::ClientInfo(const IPCClientInfo& aData)
  : mData(MakeUnique<IPCClientInfo>(aData))
{
}

ClientInfo::ClientInfo(const ClientInfo& aRight)
{
  operator=(aRight);
}

ClientInfo&
ClientInfo::operator=(const ClientInfo& aRight)
{
  mData.reset();
  mData = MakeUnique<IPCClientInfo>(*aRight.mData);
  return *this;
}

ClientInfo::ClientInfo(ClientInfo&& aRight)
  : mData(std::move(aRight.mData))
{
}

ClientInfo&
ClientInfo::operator=(ClientInfo&& aRight)
{
  mData.reset();
  mData = std::move(aRight.mData);
  return *this;
}

ClientInfo::~ClientInfo()
{
}

bool
ClientInfo::operator==(const ClientInfo& aRight) const
{
  return *mData == *aRight.mData;
}

const nsID&
ClientInfo::Id() const
{
  return mData->id();
}

ClientType
ClientInfo::Type() const
{
  return mData->type();
}

const mozilla::ipc::PrincipalInfo&
ClientInfo::PrincipalInfo() const
{
  return mData->principalInfo();
}

const TimeStamp&
ClientInfo::CreationTime() const
{
  return mData->creationTime();
}

const nsCString&
ClientInfo::URL() const
{
  return mData->url();
}

void
ClientInfo::SetURL(const nsACString& aURL)
{
  mData->url() = aURL;
}

FrameType
ClientInfo::FrameType() const
{
  return mData->frameType();
}

void
ClientInfo::SetFrameType(mozilla::dom::FrameType aFrameType)
{
  mData->frameType() = aFrameType;
}

const IPCClientInfo&
ClientInfo::ToIPC() const
{
  return *mData;
}

bool
ClientInfo::IsPrivateBrowsing() const
{
  switch(PrincipalInfo().type()) {
    case PrincipalInfo::TContentPrincipalInfo:
    {
      auto& p = PrincipalInfo().get_ContentPrincipalInfo();
      return p.attrs().mPrivateBrowsingId != 0;
    }
    case PrincipalInfo::TSystemPrincipalInfo:
    {
      return false;
    }
    case PrincipalInfo::TNullPrincipalInfo:
    {
      auto& p = PrincipalInfo().get_NullPrincipalInfo();
      return p.attrs().mPrivateBrowsingId != 0;
    }
    default:
    {
      // clients should never be expanded principals
      MOZ_CRASH("unexpected principal type!");
    }
  }
}

nsCOMPtr<nsIPrincipal>
ClientInfo::GetPrincipal() const
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIPrincipal> ref = PrincipalInfoToPrincipal(PrincipalInfo());
  return ref;
}

} // namespace dom
} // namespace mozilla
