/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaObject.h"

#include "CanonicalQuotaObject.h"
#include "mozilla/dom/quota/IPCQuotaObject.h"
#include "mozilla/dom/quota/PRemoteQuotaObject.h"
#include "mozilla/dom/quota/RemoteQuotaObjectChild.h"
#include "mozilla/dom/quota/RemoteQuotaObjectParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "RemoteQuotaObject.h"

namespace mozilla::dom::quota {

CanonicalQuotaObject* QuotaObject::AsCanonicalQuotaObject() {
  return mIsRemote ? nullptr : static_cast<CanonicalQuotaObject*>(this);
}

RemoteQuotaObject* QuotaObject::AsRemoteQuotaObject() {
  return mIsRemote ? static_cast<RemoteQuotaObject*>(this) : nullptr;
}

IPCQuotaObject QuotaObject::Serialize() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!mozilla::ipc::IsOnBackgroundThread());
  MOZ_RELEASE_ASSERT(!GetCurrentThreadWorkerPrivate());
  MOZ_RELEASE_ASSERT(!mIsRemote);

  mozilla::ipc::Endpoint<PRemoteQuotaObjectParent> parentEndpoint;
  mozilla::ipc::Endpoint<PRemoteQuotaObjectChild> childEndpoint;
  MOZ_ALWAYS_SUCCEEDS(
      PRemoteQuotaObject::CreateEndpoints(&parentEndpoint, &childEndpoint));

  auto actor = MakeRefPtr<RemoteQuotaObjectParent>(AsCanonicalQuotaObject());

  parentEndpoint.Bind(actor);

  IPCQuotaObject ipcQuotaObject;
  ipcQuotaObject.childEndpoint() = std::move(childEndpoint);

  return ipcQuotaObject;
}

// static
RefPtr<QuotaObject> QuotaObject::Deserialize(IPCQuotaObject& aQuotaObject) {
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!mozilla::ipc::IsOnBackgroundThread());
  MOZ_RELEASE_ASSERT(!GetCurrentThreadWorkerPrivate());

  auto actor = MakeRefPtr<RemoteQuotaObjectChild>();

  aQuotaObject.childEndpoint().Bind(actor);

  return MakeRefPtr<RemoteQuotaObject>(actor);
}

}  // namespace mozilla::dom::quota

namespace IPC {

void ParamTraits<mozilla::dom::quota::QuotaObject*>::Write(
    MessageWriter* aWriter, mozilla::dom::quota::QuotaObject* aParam) {
  mozilla::dom::quota::IPCQuotaObject ipcQuotaObject = aParam->Serialize();

  WriteParam(aWriter, std::move(ipcQuotaObject));
}

bool ParamTraits<mozilla::dom::quota::QuotaObject*>::Read(
    MessageReader* aReader, RefPtr<mozilla::dom::quota::QuotaObject>* aResult) {
  mozilla::dom::quota::IPCQuotaObject ipcQuotaObject;
  if (!ReadParam(aReader, &ipcQuotaObject)) {
    return false;
  }

  *aResult = mozilla::dom::quota::QuotaObject::Deserialize(ipcQuotaObject);
  return true;
}

}  // namespace IPC
