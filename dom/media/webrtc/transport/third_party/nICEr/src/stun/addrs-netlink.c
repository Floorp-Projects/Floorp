/*
Copyright (c) 2011, The WebRTC project authors. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * Neither the name of Google nor the names of its contributors may
    be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#if defined(LINUX)
#include <net/if.h>
#include "addrs-netlink.h"
#include <csi_platform.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "stun_util.h"
#include "util.h"
#include <r_macros.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#ifdef ANDROID
/* Work around an Android NDK < r8c bug */
#undef __unused
#else
#include <linux/if.h> /* struct ifreq, IFF_POINTTOPOINT */
#include <linux/wireless.h> /* struct iwreq */
#include <linux/ethtool.h> /* struct ethtool_cmd */
#include <linux/sockios.h> /* SIOCETHTOOL */
#endif /* ANDROID */


struct netlinkrequest {
  struct nlmsghdr header;
  struct ifaddrmsg msg;
};

static const int kMaxReadSize = 4096;

static void set_ifname(nr_local_addr *addr, struct ifaddrmsg* msg) {
  assert(sizeof(addr->addr.ifname) > IF_NAMESIZE);
  if_indextoname(msg->ifa_index, addr->addr.ifname);
}

static int get_siocgifflags(nr_local_addr *addr) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {
    assert(0);
    return 0;
  }
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, addr->addr.ifname, IFNAMSIZ - 1);
  int rc = ioctl(fd, SIOCGIFFLAGS, &ifr);
  close(fd);
  if (rc == -1) {
    assert(0);
    return 0;
  }
  return ifr.ifr_flags;
}

static int set_sockaddr(nr_local_addr *addr, struct ifaddrmsg* msg, struct rtattr* rta) {
  assert(rta->rta_type == IFA_ADDRESS || rta->rta_type == IFA_LOCAL);
  void *data = RTA_DATA(rta);
  size_t len = RTA_PAYLOAD(rta);
  if (msg->ifa_family == AF_INET) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = AF_INET;
    memcpy(&sa.sin_addr, data, len);
    return nr_sockaddr_to_transport_addr((struct sockaddr*)&sa, IPPROTO_UDP, 0, &(addr->addr));
  } else if (msg->ifa_family == AF_INET6) {
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(struct sockaddr_in6));
    sa.sin6_family = AF_INET6;
    /* We do not set sin6_scope_id to ifa_index, because that is only valid for
     * link local addresses, and we don't use those anyway */
    memcpy(&sa.sin6_addr, data, len);
    return nr_sockaddr_to_transport_addr((struct sockaddr*)&sa, IPPROTO_UDP, 0, &(addr->addr));
  }

  return R_BAD_ARGS;
}

static int
stun_ifaddr_is_disallowed_v6(int flags) {
  return flags & (IFA_F_TENTATIVE | IFA_F_OPTIMISTIC | IFA_F_DADFAILED | IFA_F_DEPRECATED);
}

static int
stun_convert_netlink(nr_local_addr *addr, struct ifaddrmsg *address_msg, struct rtattr* rta)
{
  int r = set_sockaddr(addr, address_msg, rta);
  if (r) {
    r_log(NR_LOG_STUN, LOG_ERR, "set_sockaddr error r = %d", r);
    return r;
  }

  set_ifname(addr, address_msg);

  if (address_msg->ifa_flags & IFA_F_TEMPORARY) {
    addr->flags |= NR_ADDR_FLAG_TEMPORARY;
  }

  int flags = get_siocgifflags(addr);
  if (flags & IFF_POINTOPOINT)
  {
    addr->interface.type = NR_INTERFACE_TYPE_UNKNOWN | NR_INTERFACE_TYPE_VPN;
    /* TODO (Bug 896913): find backend network type of this VPN */
  }

#if defined(LINUX) && !defined(ANDROID)
  struct ethtool_cmd ecmd;
  struct ifreq ifr;
  struct iwreq wrq;
  int e;
  int s = socket(AF_INET, SOCK_DGRAM, 0);

  strncpy(ifr.ifr_name, addr->addr.ifname, sizeof(ifr.ifr_name));
  /* TODO (Bug 896851): interface property for Android */
  /* Getting ethtool for ethernet information. */
  ecmd.cmd = ETHTOOL_GSET;
  /* In/out param */
  ifr.ifr_data = (void*)&ecmd;

  e = ioctl(s, SIOCETHTOOL, &ifr);
  if (e == 0)
  {
    /* For wireless network, we won't get ethtool, it's a wired
     * connection */
    addr->interface.type = NR_INTERFACE_TYPE_WIRED;
#ifdef DONT_HAVE_ETHTOOL_SPEED_HI
    addr->interface.estimated_speed = ecmd.speed;
#else
    addr->interface.estimated_speed = ((ecmd.speed_hi << 16) | ecmd.speed) * 1000;
#endif
  }

  strncpy(wrq.ifr_name, addr->addr.ifname, sizeof(wrq.ifr_name));
  e = ioctl(s, SIOCGIWRATE, &wrq);
  if (e == 0)
  {
    addr->interface.type = NR_INTERFACE_TYPE_WIFI;
    addr->interface.estimated_speed = wrq.u.bitrate.value / 1000;
  }

  close(s);

#else
  addr->interface.type = NR_INTERFACE_TYPE_UNKNOWN;
  addr->interface.estimated_speed = 0;
#endif
  return 0;
}

int
stun_getaddrs_filtered(nr_local_addr addrs[], int maxaddrs, int *count)
{
  int _status;
  int fd = 0;

  /* Scope everything else since we're using ABORT. */
  {
    *count = 0;

    if (maxaddrs <= 0)
      ABORT(R_BAD_ARGS);

    fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
      ABORT(R_INTERNAL);
    }

    struct netlinkrequest ifaddr_request;
    memset(&ifaddr_request, 0, sizeof(ifaddr_request));
    ifaddr_request.header.nlmsg_flags = NLM_F_ROOT | NLM_F_REQUEST;
    ifaddr_request.header.nlmsg_type = RTM_GETADDR;
    ifaddr_request.header.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));

    ssize_t bytes = send(fd, &ifaddr_request, ifaddr_request.header.nlmsg_len, 0);
    if ((size_t)bytes != ifaddr_request.header.nlmsg_len) {
      ABORT(R_INTERNAL);
    }

    char buf[kMaxReadSize];
    ssize_t amount_read = recv(fd, &buf, kMaxReadSize, 0);
    while ((amount_read > 0) && (*count != maxaddrs)) {
      struct nlmsghdr* header = (struct nlmsghdr*)&buf[0];
      size_t header_size = (size_t)amount_read;
      for ( ; NLMSG_OK(header, header_size) && (*count != maxaddrs);
            header = NLMSG_NEXT(header, header_size)) {
        switch (header->nlmsg_type) {
          case NLMSG_DONE:
            /* Success. Return. */
            close(fd);
            return 0;
          case NLMSG_ERROR:
            ABORT(R_INTERNAL);
          case RTM_NEWADDR: {
            struct ifaddrmsg* address_msg =
                (struct ifaddrmsg*)NLMSG_DATA(header);
            struct rtattr* rta = IFA_RTA(address_msg);
            ssize_t payload_len = IFA_PAYLOAD(header);
            bool found = false;
            while (RTA_OK(rta, payload_len)) {
              // This is a bit convoluted. IFA_ADDRESS and IFA_LOCAL are the
              // same thing except when using a POINTTOPOINT interface, in
              // which case IFA_LOCAL is the local address, and IFA_ADDRESS is
              // the remote address. In a reasonable world, that would mean we
              // could just use IFA_LOCAL all the time. Sadly, IFA_LOCAL is not
              // always set (IPv6 in particular). So, we have to be on the
              // lookout for both, and prefer IFA_LOCAL when present.
              if (rta->rta_type == IFA_ADDRESS || rta->rta_type == IFA_LOCAL) {
                int family = address_msg->ifa_family;
                if ((family == AF_INET || family == AF_INET6) &&
                    !stun_ifaddr_is_disallowed_v6(address_msg->ifa_flags) &&
                    !stun_convert_netlink(&addrs[*count], address_msg, rta)) {
                  found = true;
                  if (rta->rta_type == IFA_LOCAL) {
                    // IFA_LOCAL is what we really want; if we find it we're
                    // done. If this is IFA_ADDRESS instead, we do not proceed
                    // yet, and allow a subsequent IFA_LOCAL to overwrite what
                    // we just put in |addrs|.
                    break;
                  }
                }
              }
              /* TODO: Use IFA_LABEL instead of if_indextoname? We would need
               * to remember how many nr_local_addr we've converted for this
               * ifaddrmsg, and set the label on all of them. */
              rta = RTA_NEXT(rta, payload_len);
            }

            if (found) {
              ++(*count);
            }
            break;
          }
        }
      }
      amount_read = recv(fd, &buf, kMaxReadSize, 0);
    }
  }

  _status=0;
abort:
  close(fd);
  return(_status);
}

#endif  /* defined(LINUX) */
