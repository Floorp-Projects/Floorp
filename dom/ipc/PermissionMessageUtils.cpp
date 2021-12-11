/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsCOMPtr.h"
#include "nsIPrincipal.h"

namespace mozilla::ipc {

void IPDLParamTraits<nsIPrincipal*>::Write(IPC::Message* aMsg,
                                           IProtocol* aActor,
                                           nsIPrincipal* aParam) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  Maybe<PrincipalInfo> info;
  if (aParam) {
    info.emplace();
    nsresult rv = PrincipalToPrincipalInfo(aParam, info.ptr());
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }

  WriteIPDLParam(aMsg, aActor, info);
}

bool IPDLParamTraits<nsIPrincipal*>::Read(const IPC::Message* aMsg,
                                          PickleIterator* aIter,
                                          IProtocol* aActor,
                                          RefPtr<nsIPrincipal>* aResult) {
  Maybe<PrincipalInfo> info;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &info)) {
    return false;
  }

  if (info.isNothing()) {
    return true;
  }

  auto principalOrErr = PrincipalInfoToPrincipal(info.ref());

  if (NS_WARN_IF(principalOrErr.isErr())) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();
  *aResult = principal;
  return true;
}

}  // namespace mozilla::ipc
