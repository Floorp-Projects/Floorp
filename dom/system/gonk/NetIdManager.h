/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetIdManager_h
#define NetIdManager_h

#include "nsString.h"
#include "nsDataHashtable.h"

// NetId is a logical network identifier defined by netd.
// A network is typically a physical one (i.e. PhysicalNetwork.cpp)
// for netd but it could be a virtual network as well.
// We currently use physical network only and use one-to-one
// network-interface mapping.

class NetIdManager {
public:
  // keep in sync with system/netd/NetworkController.cpp
  enum {
    MIN_NET_ID = 100,
    MAX_NET_ID = 65535,
  };

  // We need to count the number of references since different
  // application like data and mms may use the same interface.
  struct NetIdInfo {
    int mNetId;
    int mRefCnt;
  };

public:
  NetIdManager();

  bool lookup(const nsString& aInterfaceName, NetIdInfo* aNetIdInfo);
  void acquire(const nsString& aInterfaceName, NetIdInfo* aNetIdInfo);
  bool release(const nsString& aInterfaceName, NetIdInfo* aNetIdInfo);

private:
  int getNextNetId();
  int mNextNetId;
  nsDataHashtable<nsStringHashKey, NetIdInfo> mInterfaceToNetIdHash;
};

#endif