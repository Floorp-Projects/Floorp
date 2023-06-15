/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientInfo.h"

#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla::dom {

using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalInfoToPrincipal;

ClientInfo::ClientInfo(const nsID& aId, ClientType aType,
                       const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                       const TimeStamp& aCreationTime)
    : mData(MakeUnique<IPCClientInfo>(aId, mozilla::Nothing(), aType,
                                      aPrincipalInfo, aCreationTime, ""_ns,
                                      mozilla::dom::FrameType::None,
                                      mozilla::Nothing(), mozilla::Nothing())) {
}

ClientInfo::ClientInfo(const IPCClientInfo& aData)
    : mData(MakeUnique<IPCClientInfo>(aData)) {}

ClientInfo::ClientInfo(const ClientInfo& aRight) { operator=(aRight); }

ClientInfo& ClientInfo::operator=(const ClientInfo& aRight) {
  mData.reset();
  mData = MakeUnique<IPCClientInfo>(*aRight.mData);
  return *this;
}

ClientInfo::ClientInfo(ClientInfo&& aRight) noexcept
    : mData(std::move(aRight.mData)) {}

ClientInfo& ClientInfo::operator=(ClientInfo&& aRight) noexcept {
  mData.reset();
  mData = std::move(aRight.mData);
  return *this;
}

ClientInfo::~ClientInfo() = default;

bool ClientInfo::operator==(const ClientInfo& aRight) const {
  return *mData == *aRight.mData;
}

bool ClientInfo::operator!=(const ClientInfo& aRight) const {
  return *mData != *aRight.mData;
}

const nsID& ClientInfo::Id() const { return mData->id(); }

void ClientInfo::SetAgentClusterId(const nsID& aId) {
  MOZ_ASSERT(mData->agentClusterId().isNothing() ||
             mData->agentClusterId().ref().Equals(aId));
  mData->agentClusterId() = Some(aId);
}

const Maybe<nsID>& ClientInfo::AgentClusterId() const {
  return mData->agentClusterId();
}

ClientType ClientInfo::Type() const { return mData->type(); }

const mozilla::ipc::PrincipalInfo& ClientInfo::PrincipalInfo() const {
  return mData->principalInfo();
}

const TimeStamp& ClientInfo::CreationTime() const {
  return mData->creationTime();
}

const nsCString& ClientInfo::URL() const { return mData->url(); }

void ClientInfo::SetURL(const nsACString& aURL) { mData->url() = aURL; }

FrameType ClientInfo::FrameType() const { return mData->frameType(); }

void ClientInfo::SetFrameType(mozilla::dom::FrameType aFrameType) {
  mData->frameType() = aFrameType;
}

const IPCClientInfo& ClientInfo::ToIPC() const { return *mData; }

bool ClientInfo::IsPrivateBrowsing() const {
  switch (PrincipalInfo().type()) {
    case PrincipalInfo::TContentPrincipalInfo: {
      const auto& p = PrincipalInfo().get_ContentPrincipalInfo();
      return p.attrs().mPrivateBrowsingId != 0;
    }
    case PrincipalInfo::TSystemPrincipalInfo: {
      return false;
    }
    case PrincipalInfo::TNullPrincipalInfo: {
      const auto& p = PrincipalInfo().get_NullPrincipalInfo();
      return p.attrs().mPrivateBrowsingId != 0;
    }
    default: {
      // clients should never be expanded principals
      MOZ_CRASH("unexpected principal type!");
    }
  }
}

Result<nsCOMPtr<nsIPrincipal>, nsresult> ClientInfo::GetPrincipal() const {
  return PrincipalInfoToPrincipal(PrincipalInfo());
}

const Maybe<mozilla::ipc::CSPInfo>& ClientInfo::GetCspInfo() const {
  return mData->cspInfo();
}

void ClientInfo::SetCspInfo(const mozilla::ipc::CSPInfo& aCSPInfo) {
  mData->cspInfo() = Some(aCSPInfo);
}

const Maybe<mozilla::ipc::CSPInfo>& ClientInfo::GetPreloadCspInfo() const {
  return mData->preloadCspInfo();
}

void ClientInfo::SetPreloadCspInfo(
    const mozilla::ipc::CSPInfo& aPreloadCSPInfo) {
  mData->preloadCspInfo() = Some(aPreloadCSPInfo);
}

}  // namespace mozilla::dom
