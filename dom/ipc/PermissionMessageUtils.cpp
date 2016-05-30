/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PermissionMessageUtils.h"
#include "nsISerializable.h"
#include "nsSerializationHelper.h"

namespace IPC {

void
ParamTraits<Principal>::Write(Message* aMsg, const paramType& aParam) {
  bool isNull = !aParam.mPrincipal;
  WriteParam(aMsg, isNull);
  if (isNull) {
    return;
  }

  bool isSerialized = false;
  nsCString principalString;
  nsCOMPtr<nsISerializable> serializable = do_QueryInterface(aParam.mPrincipal);
  if (serializable) {
    nsresult rv = NS_SerializeToString(serializable, principalString);
    if (NS_SUCCEEDED(rv)) {
      isSerialized = true;
    }
  }

  if (!isSerialized) {
    NS_RUNTIMEABORT("Unable to serialize principal.");
    return;
  }

  WriteParam(aMsg, principalString);
}

bool
ParamTraits<Principal>::Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
{
  bool isNull;
  if (!ReadParam(aMsg, aIter, &isNull)) {
    return false;
  }

  if (isNull) {
    aResult->mPrincipal = nullptr;
    return true;
  }

  nsCString principalString;
  if (!ReadParam(aMsg, aIter, &principalString)) {
    return false;
  }

  nsCOMPtr<nsISupports> iSupports;
  nsresult rv = NS_DeserializeObject(principalString, getter_AddRefs(iSupports));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(iSupports);
  NS_ENSURE_TRUE(principal, false);

  principal.swap(aResult->mPrincipal);
  return true;
}

} // namespace IPC

