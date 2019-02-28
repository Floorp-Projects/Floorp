/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PermissionMessageUtils.h"
#include "nsISerializable.h"
#include "nsSerializationHelper.h"

namespace mozilla {
namespace ipc {

void IPDLParamTraits<nsIPrincipal>::Write(IPC::Message* aMsg, IProtocol* aActor,
                                          nsIPrincipal* aParam) {
  bool isNull = !aParam;
  WriteIPDLParam(aMsg, aActor, isNull);
  if (isNull) {
    return;
  }

  nsCString principalString;
  nsresult rv = NS_SerializeToString(aParam, principalString);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Unable to serialize principal.");
    return;
  }

  WriteIPDLParam(aMsg, aActor, principalString);
}

bool IPDLParamTraits<nsIPrincipal>::Read(const IPC::Message* aMsg,
                                         PickleIterator* aIter,
                                         IProtocol* aActor,
                                         RefPtr<nsIPrincipal>* aResult) {
  bool isNull;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &isNull)) {
    return false;
  }

  if (isNull) {
    *aResult = nullptr;
    return true;
  }

  nsCString principalString;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &principalString)) {
    return false;
  }

  nsCOMPtr<nsISupports> iSupports;
  nsresult rv =
      NS_DeserializeObject(principalString, getter_AddRefs(iSupports));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(iSupports);
  NS_ENSURE_TRUE(principal, false);

  *aResult = principal.forget();
  return true;
}

}  // namespace ipc
}  // namespace mozilla
