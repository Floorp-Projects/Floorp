/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSPMessageUtils.h"
#include "nsISerializable.h"
#include "nsSerializationHelper.h"

namespace IPC {

void ParamTraits<nsIContentSecurityPolicy*>::Write(
    Message* aMsg, nsIContentSecurityPolicy* aParam) {
  bool isNull = !aParam;
  WriteParam(aMsg, isNull);
  if (isNull) {
    return;
  }

  nsCString cspString;
  nsresult rv = NS_SerializeToString(aParam, cspString);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Unable to serialize csp.");
    return;
  }

  WriteParam(aMsg, cspString);
}

bool ParamTraits<nsIContentSecurityPolicy*>::Read(
    const Message* aMsg, PickleIterator* aIter,
    RefPtr<nsIContentSecurityPolicy>* aResult) {
  bool isNull;
  if (!ReadParam(aMsg, aIter, &isNull)) {
    return false;
  }

  if (isNull) {
    *aResult = nullptr;
    return true;
  }

  nsCString cspString;
  if (!ReadParam(aMsg, aIter, &cspString)) {
    return false;
  }

  nsCOMPtr<nsISupports> iSupports;
  nsresult rv = NS_DeserializeObject(cspString, getter_AddRefs(iSupports));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIContentSecurityPolicy> csp = do_QueryInterface(iSupports);
  NS_ENSURE_TRUE(csp, false);

  *aResult = csp.forget();
  return true;
}

}  // namespace IPC
