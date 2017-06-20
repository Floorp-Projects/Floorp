/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayUtils.h"
#include "PaymentRequestUtils.h"
#include "nsIMutableArray.h"
#include "nsISupportsPrimitives.h"

namespace mozilla {
namespace dom {

nsresult
ConvertStringstoISupportsStrings(const nsTArray<nsString>& aStrings,
                                 nsIArray** aIStrings)
{
  NS_ENSURE_ARG_POINTER(aIStrings);
  *aIStrings = nullptr;
  nsCOMPtr<nsIMutableArray> iStrings = do_CreateInstance(NS_ARRAY_CONTRACTID);
  for (const nsString& string : aStrings) {
    nsCOMPtr<nsISupportsString> iString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
    if (NS_WARN_IF(!iString)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = iString->SetData(string);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = iStrings->AppendElement(iString, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  iStrings.forget(aIStrings);
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
