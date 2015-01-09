/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetIdManager.h"

NetIdManager::NetIdManager()
  : mNextNetId(MIN_NET_ID)
{
}

int NetIdManager::getNextNetId()
{
  // Modified from
  // http://androidxref.com/5.0.0_r2/xref/frameworks/base/services/
  //        core/java/com/android/server/ConnectivityService.java#764

  int netId = mNextNetId;
  if (++mNextNetId > MAX_NET_ID) {
    mNextNetId = MIN_NET_ID;
  }

  return netId;
}

void NetIdManager::acquire(const nsString& aInterfaceName,
                           NetIdInfo* aNetIdInfo)
{
  // Lookup or create one.
  if (!mInterfaceToNetIdHash.Get(aInterfaceName, aNetIdInfo)) {
    aNetIdInfo->mNetId = getNextNetId();
    aNetIdInfo->mRefCnt = 1;
  } else {
    aNetIdInfo->mRefCnt++;
  }

  // Update hash and return.
  mInterfaceToNetIdHash.Put(aInterfaceName, *aNetIdInfo);

  return;
}

bool NetIdManager::lookup(const nsString& aInterfaceName,
                          NetIdInfo* aNetIdInfo)
{
  return mInterfaceToNetIdHash.Get(aInterfaceName, aNetIdInfo);
}

bool NetIdManager::release(const nsString& aInterfaceName,
                           NetIdInfo* aNetIdInfo)
{
  if (!mInterfaceToNetIdHash.Get(aInterfaceName, aNetIdInfo)) {
    return false; // No such key.
  }

  aNetIdInfo->mRefCnt--;

  // Update the hash if still be referenced.
  if (aNetIdInfo->mRefCnt > 0) {
    mInterfaceToNetIdHash.Put(aInterfaceName, *aNetIdInfo);
    return true;
  }

  // No longer be referenced. Remove the entry.
  mInterfaceToNetIdHash.Remove(aInterfaceName);

  return true;
}