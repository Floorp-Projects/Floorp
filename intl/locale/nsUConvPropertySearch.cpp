/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUConvPropertySearch.h"
#include "nsCRT.h"
#include "nsString.h"
#include "mozilla/BinarySearch.h"

namespace {

struct PropertyComparator
{
  const nsCString& mKey;
  explicit PropertyComparator(const nsCString& aKey) : mKey(aKey) {}
  int operator()(const char* (&aProperty)[3]) const {
    return mKey.Compare(aProperty[0]);
  }
};

} // namespace

// static
nsresult
nsUConvPropertySearch::SearchPropertyValue(const char* aProperties[][3],
                                           int32_t aNumberOfProperties,
                                           const nsACString& aKey,
                                           nsACString& aValue)
{
  using mozilla::BinarySearchIf;

  const nsCString& flat = PromiseFlatCString(aKey);
  size_t index;
  if (BinarySearchIf(aProperties, 0, aNumberOfProperties,
                     PropertyComparator(flat), &index)) {
    nsDependentCString val(aProperties[index][1],
                           NS_PTR_TO_UINT32(aProperties[index][2]));
    aValue.Assign(val);
    return NS_OK;
  }

  aValue.Truncate();
  return NS_ERROR_FAILURE;
}
