/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Abstraction on top of the network support from libnetutils that we
 * use to set up network connections.
 */

#ifndef NetUtils_h
#define NetUtils_h

#include "arpa/inet.h"

// Copied from ifc.h
#define RESET_IPV4_ADDRESSES 0x01
#define RESET_IPV6_ADDRESSES 0x02
#define RESET_ALL_ADDRESSES  (RESET_IPV4_ADDRESSES | RESET_IPV6_ADDRESSES)

// Implements netutils functions. No need for an abstract class here since we
// only have a one sdk specific method (dhcp_do_request)
class NetUtils
{
public:
  static void* GetSharedLibrary();

  int32_t do_ifc_enable(const char *ifname);
  int32_t do_ifc_disable(const char *ifname);
  int32_t do_ifc_configure(const char *ifname,
                           in_addr_t address,
                           uint32_t prefixLength,
                           in_addr_t gateway,
                           in_addr_t dns1,
                           in_addr_t dns2);
  int32_t do_ifc_reset_connections(const char *ifname, const int32_t resetMask);
  int32_t do_dhcp_stop(const char *ifname);
  int32_t do_dhcp_do_request(const char *ifname,
                             char *ipaddr,
                             char *gateway,
                             uint32_t *prefixLength,
                             char *dns1,
                             char *dns2,
                             char *server,
                             uint32_t  *lease,
                             char* vendorinfo);

  static int32_t SdkVersion();
};

// Defines a function type with the right arguments and return type.
#define DEFINE_DLFUNC(name, ret, args...) typedef ret (*FUNC##name)(args);

// Set up a dlsymed function ready to use.
#define USE_DLFUNC(name)                                                      \
  FUNC##name name = (FUNC##name) dlsym(GetSharedLibrary(), #name);            \
  if (!name) {                                                                \
    MOZ_ASSUME_UNREACHABLE("Symbol not found in shared library : " #name);         \
  }

#endif // NetUtils_h
