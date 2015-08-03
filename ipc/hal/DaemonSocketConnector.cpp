/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DaemonSocketConnector.h"
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <sys/un.h>
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR
#include "prrng.h"

#ifdef CHROMIUM_LOG
#undef CHROMIUM_LOG
#endif

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "I/O", args);
#else
#include <stdio.h>
#define IODEBUG true
#define CHROMIUM_LOG(args...) if (IODEBUG) printf(args);
#endif

namespace mozilla {
namespace ipc {

nsresult
DaemonSocketConnector::CreateRandomAddressString(
  const nsACString& aPrefix, unsigned long aPostfixLength,
  nsACString& aAddress)
{
  static const char sHexChar[16] = {
    [0x0] = '0', [0x1] = '1', [0x2] = '2', [0x3] = '3',
    [0x4] = '4', [0x5] = '5', [0x6] = '6', [0x7] = '7',
    [0x8] = '8', [0x9] = '9', [0xa] = 'a', [0xb] = 'b',
    [0xc] = 'c', [0xd] = 'd', [0xe] = 'e', [0xf] = 'f'
  };

  unsigned short seed[3];

  if (NS_WARN_IF(!PR_GetRandomNoise(seed, sizeof(seed)))) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  aAddress = aPrefix;
  aAddress.Append('-');

  while (aPostfixLength) {
    // Android doesn't provide rand_r, so we use nrand48 here,
    // even though it's deprecated.
    long value = nrand48(seed);

    size_t bits = sizeof(value) * CHAR_BIT;

    while ((bits > 4) && aPostfixLength) {
      aAddress.Append(sHexChar[value&0xf]);
      bits -= 4;
      value >>= 4;
      --aPostfixLength;
    }
  }

  return NS_OK;
}

DaemonSocketConnector::DaemonSocketConnector(const nsACString& aSocketName)
  : mSocketName(aSocketName)
{
  MOZ_COUNT_CTOR_INHERITED(DaemonSocketConnector, UnixSocketConnector);
}

DaemonSocketConnector::~DaemonSocketConnector()
{
  MOZ_COUNT_CTOR_INHERITED(DaemonSocketConnector, UnixSocketConnector);
}

nsresult
DaemonSocketConnector::CreateSocket(int& aFd) const
{
  aFd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (aFd < 0) {
    CHROMIUM_LOG("Could not open daemon socket!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
DaemonSocketConnector::SetSocketFlags(int aFd) const
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
DaemonSocketConnector::CreateAddress(struct sockaddr& aAddress,
                                        socklen_t& aAddressLength) const
{
  static const size_t sNameOffset = 1;

  struct sockaddr_un* address =
    reinterpret_cast<struct sockaddr_un*>(&aAddress);

  size_t namesiz = mSocketName.Length() + 1; // include trailing '\0'

  if (NS_WARN_IF((sNameOffset + namesiz) > sizeof(address->sun_path))) {
    return NS_ERROR_FAILURE;
  }

  address->sun_family = AF_UNIX;
  memset(address->sun_path, '\0', sNameOffset); // abstract socket
  memcpy(address->sun_path + sNameOffset, mSocketName.get(), namesiz);

  aAddressLength =
    offsetof(struct sockaddr_un, sun_path) + sNameOffset + namesiz;

  return NS_OK;
}

// |UnixSocketConnector|

nsresult
DaemonSocketConnector::ConvertAddressToString(
  const struct sockaddr& aAddress, socklen_t aAddressLength,
  nsACString& aAddressString)
{
  MOZ_ASSERT(aAddress.sa_family == AF_UNIX);

  const struct sockaddr_un* un =
    reinterpret_cast<const struct sockaddr_un*>(&aAddress);

  size_t len = aAddressLength - offsetof(struct sockaddr_un, sun_path);

  aAddressString.Assign(un->sun_path, len);

  return NS_OK;
}

nsresult
DaemonSocketConnector::CreateListenSocket(struct sockaddr* aAddress,
                                             socklen_t* aAddressLength,
                                             int& aListenFd)
{
  ScopedClose fd;

  nsresult rv = CreateSocket(fd.rwget());
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aAddress && aAddressLength) {
    rv = CreateAddress(*aAddress, *aAddressLength);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  aListenFd = fd.forget();

  return NS_OK;
}

nsresult
DaemonSocketConnector::AcceptStreamSocket(int aListenFd,
                                             struct sockaddr* aAddress,
                                             socklen_t* aAddressLength,
                                             int& aStreamFd)
{
  ScopedClose fd(
    TEMP_FAILURE_RETRY(accept(aListenFd, aAddress, aAddressLength)));
  if (fd < 0) {
    CHROMIUM_LOG("Cannot accept file descriptor!");
    return NS_ERROR_FAILURE;
  }
  nsresult rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aStreamFd = fd.forget();

  return NS_OK;
}

nsresult
DaemonSocketConnector::CreateStreamSocket(struct sockaddr* aAddress,
                                             socklen_t* aAddressLength,
                                             int& aStreamFd)
{
  MOZ_CRASH("|DaemonSocketConnector| does not support "
            "creating stream sockets.");
  return NS_ERROR_ABORT;
}

nsresult
DaemonSocketConnector::Duplicate(UnixSocketConnector*& aConnector)
{
  aConnector = new DaemonSocketConnector(*this);

  return NS_OK;
}

} // namespace ipc
} // namespace mozilla
