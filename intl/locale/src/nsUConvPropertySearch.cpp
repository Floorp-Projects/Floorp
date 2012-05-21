/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUConvPropertySearch.h"
#include "nsCRT.h"

// static
nsresult
nsUConvPropertySearch::SearchPropertyValue(const char* aProperties[][3],
                                           PRInt32 aNumberOfProperties,
                                           const nsACString& aKey,
                                           nsACString& aValue)
{
  const char* key = PromiseFlatCString(aKey).get();
  PRInt32 lo = 0;
  PRInt32 hi = aNumberOfProperties - 1;
  while (lo <= hi) {
    PRUint32 mid = (lo + hi) / 2;
    PRInt32 comp = nsCRT::strcmp(aProperties[mid][0], key);
    if (comp > 0) {
      hi = mid - 1;
    } else if (comp < 0) {
      lo = mid + 1;
    } else {
      nsDependentCString val(aProperties[mid][1],
                             NS_PTR_TO_UINT32(aProperties[mid][2]));
      aValue.Assign(val);
      return NS_OK;
    }
  }
  aValue.Truncate();
  return NS_ERROR_FAILURE;
}
