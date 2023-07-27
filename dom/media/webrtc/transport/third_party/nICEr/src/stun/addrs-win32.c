/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WIN32

#include "addrs-win32.h"
#include <csi_platform.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "stun_util.h"
#include "util.h"
#include <r_macros.h>
#include "nr_crypto.h"

#include <winsock2.h>
#include <iphlpapi.h>
#include <tchar.h>

#define WIN32_MAX_NUM_INTERFACES  20

#define NR_MD5_HASH_LENGTH 16

#define _NR_MAX_KEY_LENGTH 256
#define _NR_MAX_NAME_LENGTH 512

#define _ADAPTERS_BASE_REG "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

static int nr_win32_get_adapter_friendly_name(char *adapter_GUID, char **friendly_name)
{
    int r,_status;
    HKEY adapter_reg;
    TCHAR adapter_key[_NR_MAX_KEY_LENGTH];
    TCHAR keyval_buf[_NR_MAX_KEY_LENGTH];
    TCHAR adapter_GUID_tchar[_NR_MAX_NAME_LENGTH];
    DWORD keyval_len, key_type;
    size_t converted_chars, newlen;
    char *my_fn = 0;

#ifdef _UNICODE
    mbstowcs_s(&converted_chars, adapter_GUID_tchar, strlen(adapter_GUID)+1,
               adapter_GUID, _TRUNCATE);
#else
    strlcpy(adapter_GUID_tchar, adapter_GUID, _NR_MAX_NAME_LENGTH);
#endif

    _tcscpy_s(adapter_key, _NR_MAX_KEY_LENGTH, TEXT(_ADAPTERS_BASE_REG));
    _tcscat_s(adapter_key, _NR_MAX_KEY_LENGTH, TEXT("\\"));
    _tcscat_s(adapter_key, _NR_MAX_KEY_LENGTH, adapter_GUID_tchar);
    _tcscat_s(adapter_key, _NR_MAX_KEY_LENGTH, TEXT("\\Connection"));

    r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, adapter_key, 0, KEY_READ, &adapter_reg);

    if (r != ERROR_SUCCESS) {
      r_log(NR_LOG_STUN, LOG_ERR, "Got error %d opening adapter reg key\n", r);
      ABORT(R_INTERNAL);
    }

    keyval_len = sizeof(keyval_buf);
    r = RegQueryValueEx(adapter_reg, TEXT("Name"), NULL, &key_type,
                        (BYTE *)keyval_buf, &keyval_len);

    RegCloseKey(adapter_reg);

#ifdef UNICODE
    newlen = wcslen(keyval_buf)+1;
    my_fn = (char *) RCALLOC(newlen);
    if (!my_fn) {
      ABORT(R_NO_MEMORY);
    }
    wcstombs_s(&converted_chars, my_fn, newlen, keyval_buf, _TRUNCATE);
#else
    my_fn = r_strdup(keyval_buf);
#endif

    *friendly_name = my_fn;
    _status=0;

abort:
    if (_status) {
      if (my_fn) free(my_fn);
    }
    return(_status);
}

static int stun_win32_address_disallowed(IP_ADAPTER_UNICAST_ADDRESS *addr)
{
  return (addr->DadState != NldsPreferred) &&
          (addr->DadState != IpDadStatePreferred);
}

static int stun_win32_address_temp_v6(IP_ADAPTER_UNICAST_ADDRESS *addr)
{
  return (addr->Address.lpSockaddr->sa_family == AF_INET6) &&
      (addr->SuffixOrigin == IpSuffixOriginRandom);
}

int
stun_getaddrs_filtered(nr_local_addr addrs[], int maxaddrs, int *count)
{
    int r, _status;
    PIP_ADAPTER_ADDRESSES AdapterAddresses = NULL, tmpAddress = NULL;
    // recomended per https://msdn.microsoft.com/en-us/library/windows/desktop/aa365915(v=vs.85).aspx
    static const ULONG initialBufLen = 15000;
    ULONG buflen = initialBufLen;
    char bin_hashed_ifname[NR_MD5_HASH_LENGTH];
    char hex_hashed_ifname[MAXIFNAME];
    int n = 0;

    *count = 0;

    if (maxaddrs <= 0)
      ABORT(R_BAD_ARGS);

    /* According to MSDN (see above) we have try GetAdapterAddresses() multiple times */
    for (n = 0; n < 5; n++) {
      AdapterAddresses = (PIP_ADAPTER_ADDRESSES) RMALLOC(buflen);
      if (AdapterAddresses == NULL) {
        r_log(NR_LOG_STUN, LOG_ERR, "Error allocating buf for GetAdaptersAddresses()");
        ABORT(R_NO_MEMORY);
      }

      r = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, AdapterAddresses, &buflen);
      if (r == NO_ERROR) {
        break;
      }
      r_log(NR_LOG_STUN, LOG_ERR, "GetAdaptersAddresses() returned error (%d)", r);
      RFREE(AdapterAddresses);
      AdapterAddresses = NULL;
    }

    if (n >= 5) {
      r_log(NR_LOG_STUN, LOG_ERR, "5 failures calling GetAdaptersAddresses()");
      ABORT(R_INTERNAL);
    }

    n = 0;

    /* Loop through the adapters */

    for (tmpAddress = AdapterAddresses; tmpAddress != NULL; tmpAddress = tmpAddress->Next) {

      if (tmpAddress->OperStatus != IfOperStatusUp)
        continue;

      if ((tmpAddress->IfIndex != 0) || (tmpAddress->Ipv6IfIndex != 0)) {
        IP_ADAPTER_UNICAST_ADDRESS *u = 0;

        if(r=nr_crypto_md5((UCHAR *)tmpAddress->FriendlyName,
                           wcslen(tmpAddress->FriendlyName) * sizeof(wchar_t),
                           bin_hashed_ifname))
          ABORT(r);
        if(r=nr_bin2hex(bin_hashed_ifname, sizeof(bin_hashed_ifname),
          hex_hashed_ifname))
          ABORT(r);

        for (u = tmpAddress->FirstUnicastAddress; u != 0; u = u->Next) {
          SOCKET_ADDRESS *sa_addr = &u->Address;

          if ((sa_addr->lpSockaddr->sa_family != AF_INET) &&
              (sa_addr->lpSockaddr->sa_family != AF_INET6)) {
            r_log(NR_LOG_STUN, LOG_DEBUG, "Unrecognized sa_family for address on adapter %lu", tmpAddress->IfIndex);
            continue;
          }

          if (stun_win32_address_disallowed(u)) {
            continue;
          }

          if ((r=nr_sockaddr_to_transport_addr((struct sockaddr*)sa_addr->lpSockaddr, IPPROTO_UDP, 0, &(addrs[n].addr)))) {
            ABORT(r);
          }

          strlcpy(addrs[n].addr.ifname, hex_hashed_ifname, sizeof(addrs[n].addr.ifname));
          if (tmpAddress->IfType == IF_TYPE_ETHERNET_CSMACD) {
            addrs[n].interface.type = NR_INTERFACE_TYPE_WIRED;
          } else if (tmpAddress->IfType == IF_TYPE_IEEE80211) {
            /* Note: this only works for >= Win Vista */
            addrs[n].interface.type = NR_INTERFACE_TYPE_WIFI;
          } else {
            addrs[n].interface.type = NR_INTERFACE_TYPE_UNKNOWN;
          }
          addrs[n].interface.estimated_speed = tmpAddress->TransmitLinkSpeed / 1000;
          if (stun_win32_address_temp_v6(u)) {
            addrs[n].flags |= NR_ADDR_FLAG_TEMPORARY;
          }

          if (++n >= maxaddrs)
            goto done;
        }
      }
    }

   done:
    *count = n;
    _status = 0;

  abort:
    RFREE(AdapterAddresses);
    return _status;
}

#endif //WIN32

