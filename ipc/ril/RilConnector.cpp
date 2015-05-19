/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RilConnector.h"
#include <fcntl.h>
#include <sys/socket.h>
#include "nsThreadUtils.h" // For NS_IsMainThread.

#ifdef AF_INET
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
#ifdef AF_UNIX
#include <sys/un.h>
#endif

namespace mozilla {
namespace ipc {

static const uint16_t RIL_TEST_PORT = 6200;

RilConnector::RilConnector(const nsACString& aAddressString,
                           unsigned long aClientId)
  : mAddressString(aAddressString)
  , mClientId(aClientId)
{ }

RilConnector::~RilConnector()
{ }

nsresult
RilConnector::CreateSocket(int aDomain, int& aFd) const
{
  aFd = socket(aDomain, SOCK_STREAM, 0);
  if (aFd < 0) {
    NS_WARNING("Could not open RIL socket!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
RilConnector::SetSocketFlags(int aFd) const
{
  static const int sReuseAddress = 1;

  // Set close-on-exec bit.
  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFD));
  if (flags < 0) {
    return NS_ERROR_FAILURE;
  }
  flags |= FD_CLOEXEC;
  int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFD, flags));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  // Set non-blocking status flag.
  flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  if (flags < 0) {
    return NS_ERROR_FAILURE;
  }
  flags |= O_NONBLOCK;
  res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  // Set socket addr to be reused even if kernel is still waiting to close.
  res = setsockopt(aFd, SOL_SOCKET, SO_REUSEADDR, &sReuseAddress,
                   sizeof(sReuseAddress));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
RilConnector::CreateAddress(int aDomain,
                            struct sockaddr& aAddress,
                            socklen_t& aAddressLength) const
{
  switch (aDomain) {
#ifdef AF_UNIX
    case AF_UNIX: {
        struct sockaddr_un* address =
          reinterpret_cast<struct sockaddr_un*>(&aAddress);
        address->sun_family = aDomain;
        size_t siz = mAddressString.Length() + 1;
        if (siz > sizeof(address->sun_path)) {
          NS_WARNING("Address too long for socket struct!");
          return NS_ERROR_FAILURE;
        }
        memcpy(address->sun_path, mAddressString.get(), siz);
        aAddressLength = offsetof(struct sockaddr_un, sun_path) + siz;
      }
      break;
#endif
#ifdef AF_INET
    case AF_INET: {
        struct sockaddr_in* address =
          reinterpret_cast<struct sockaddr_in*>(&aAddress);
        address->sin_family = aDomain;
        address->sin_port = htons(RIL_TEST_PORT + mClientId);
        address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        aAddressLength = sizeof(*address);
      }
      break;
#endif
    default:
      NS_WARNING("Address family not handled by connector!");
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// |UnixSocketConnector|

nsresult
RilConnector::ConvertAddressToString(const struct sockaddr& aAddress,
                                     socklen_t aAddressLength,
                                     nsACString& aAddressString)
{
#ifdef AF_UNIX
  if (aAddress.sa_family == AF_UNIX) {
    const struct sockaddr_un* un =
      reinterpret_cast<const struct sockaddr_un*>(&aAddress);

    size_t len = aAddressLength - offsetof(struct sockaddr_un, sun_path);

    aAddressString.Assign(un->sun_path, len);
  } else
#endif
#ifdef AF_INET
  if (aAddress.sa_family == AF_INET) {
    const struct sockaddr_in* in =
      reinterpret_cast<const struct sockaddr_in*>(&aAddress);

    aAddressString.Assign(nsDependentCString(inet_ntoa(in->sin_addr)));
  } else
#endif
  {
    NS_WARNING("Address family not handled by connector!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
RilConnector::CreateListenSocket(struct sockaddr* aAddress,
                                 socklen_t* aAddressLength,
                                 int& aListenFd)
{
  MOZ_CRASH("|RilConnector| does not support listening sockets.");
}

nsresult
RilConnector::AcceptStreamSocket(int aListenFd,
                                 struct sockaddr* aAddress,
                                 socklen_t* aAddressLen,
                                 int& aStreamFd)
{
  MOZ_CRASH("|RilConnector| does not support accepting sockets.");
}

nsresult
RilConnector::CreateStreamSocket(struct sockaddr* aAddress,
                                 socklen_t* aAddressLength,
                                 int& aStreamFd)
{
#ifdef MOZ_WIDGET_GONK
  static const int sDomain = AF_UNIX;
#else
  static const int sDomain = AF_INET;
#endif

  ScopedClose fd;

  nsresult rv = CreateSocket(sDomain, fd.rwget());
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aAddress && aAddressLength) {
    rv = CreateAddress(sDomain, *aAddress, *aAddressLength);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  aStreamFd = fd.forget();

  return NS_OK;
}

// Deprecated

int
RilConnector::Create()
{
  MOZ_ASSERT(!NS_IsMainThread());

  int fd = -1;

#if defined(MOZ_WIDGET_GONK)
  fd = socket(AF_LOCAL, SOCK_STREAM, 0);
#else
  // If we can't hit a local loopback, fail later in connect.
  fd = socket(AF_INET, SOCK_STREAM, 0);
#endif

  if (fd < 0) {
    NS_WARNING("Could not open ril socket!");
    return -1;
  }

  if (!SetUp(fd)) {
    NS_WARNING("Could not set up socket!");
  }
  return fd;
}

bool
RilConnector::CreateAddr(bool aIsServer,
                         socklen_t& aAddrSize,
                         sockaddr_any& aAddr,
                         const char* aAddress)
{
  // We never open ril socket as server.
  MOZ_ASSERT(!aIsServer);
  uint32_t af;
#if defined(MOZ_WIDGET_GONK)
  af = AF_LOCAL;
#else
  af = AF_INET;
#endif
  switch (af) {
  case AF_LOCAL:
    aAddr.un.sun_family = af;
    if(strlen(aAddress) > sizeof(aAddr.un.sun_path)) {
      NS_WARNING("Address too long for socket struct!");
      return false;
    }
    strcpy((char*)&aAddr.un.sun_path, aAddress);
    aAddrSize = strlen(aAddress) + offsetof(struct sockaddr_un, sun_path) + 1;
    break;
  case AF_INET:
    aAddr.in.sin_family = af;
    aAddr.in.sin_port = htons(RIL_TEST_PORT + mClientId);
    aAddr.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    aAddrSize = sizeof(sockaddr_in);
    break;
  default:
    NS_WARNING("Socket type not handled by connector!");
    return false;
  }
  return true;
}

bool
RilConnector::SetUp(int aFd)
{
  // Nothing to do here.
  return true;
}

bool
RilConnector::SetUpListenSocket(int aFd)
{
  // Nothing to do here.
  return true;
}

void
RilConnector::GetSocketAddr(const sockaddr_any& aAddr, nsAString& aAddrStr)
{
  MOZ_CRASH("This should never be called!");
}

}
}
