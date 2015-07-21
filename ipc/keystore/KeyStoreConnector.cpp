/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyStoreConnector.h"
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR
#include "nsThreadUtils.h" // For NS_IsMainThread.

#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define KEYSTORE_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define KEYSTORE_LOG(args...)  printf(args);
#endif

namespace mozilla {
namespace ipc {

static const char KEYSTORE_SOCKET_PATH[] = "/dev/socket/keystore";

KeyStoreConnector::KeyStoreConnector(const char** const aAllowedUsers)
  : mAllowedUsers(aAllowedUsers)
{
  MOZ_COUNT_CTOR_INHERITED(KeyStoreConnector, UnixSocketConnector);
}

KeyStoreConnector::~KeyStoreConnector()
{
  MOZ_COUNT_DTOR_INHERITED(KeyStoreConnector, UnixSocketConnector);
}

nsresult
KeyStoreConnector::CreateSocket(int& aFd) const
{
  unlink(KEYSTORE_SOCKET_PATH);

  aFd = socket(AF_LOCAL, SOCK_STREAM, 0);
  if (aFd < 0) {
    KEYSTORE_LOG("Could not open KeyStore socket!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
KeyStoreConnector::SetSocketFlags(int aFd) const
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
KeyStoreConnector::CheckPermission(int aFd) const
{
  struct ucred userCred;
  socklen_t len = sizeof(userCred);

  if (getsockopt(aFd, SOL_SOCKET, SO_PEERCRED, &userCred, &len)) {
    return NS_ERROR_FAILURE;
  }

  const struct passwd* userInfo = getpwuid(userCred.uid);
  if (!userInfo) {
    return NS_ERROR_FAILURE;
  }

  if (!mAllowedUsers) {
    return NS_ERROR_FAILURE;
  }

  for (const char** user = mAllowedUsers; *user; ++user) {
    if (!strcmp(*user, userInfo->pw_name)) {
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
KeyStoreConnector::CreateAddress(struct sockaddr& aAddress,
                                 socklen_t& aAddressLength) const
{
  struct sockaddr_un* address =
    reinterpret_cast<struct sockaddr_un*>(&aAddress);

  size_t namesiz = strlen(KEYSTORE_SOCKET_PATH) + 1; // include trailing '\0'

  if (namesiz > sizeof(address->sun_path)) {
      KEYSTORE_LOG("Address too long for socket struct!");
      return NS_ERROR_FAILURE;
  }

  address->sun_family = AF_UNIX;
  memcpy(address->sun_path, KEYSTORE_SOCKET_PATH, namesiz);

  aAddressLength = offsetof(struct sockaddr_un, sun_path) + namesiz;

  return NS_OK;
}

// |UnixSocketConnector|

nsresult
KeyStoreConnector::ConvertAddressToString(const struct sockaddr& aAddress,
                                          socklen_t aAddressLength,
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
KeyStoreConnector::CreateListenSocket(struct sockaddr* aAddress,
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

  // Allow access for wpa_supplicant (different user, different group)
  //
  // TODO: Improve this by setting specific user/group for
  //       wpa_supplicant by calling |fchmod| and |fchown|.
  //
  chmod(KEYSTORE_SOCKET_PATH, S_IRUSR|S_IWUSR|
                              S_IRGRP|S_IWGRP|
                              S_IROTH|S_IWOTH);

  aListenFd = fd.forget();

  return NS_OK;
}

nsresult
KeyStoreConnector::AcceptStreamSocket(int aListenFd,
                                      struct sockaddr* aAddress,
                                      socklen_t* aAddressLength,
                                      int& aStreamFd)
{
  ScopedClose fd(
    TEMP_FAILURE_RETRY(accept(aListenFd, aAddress, aAddressLength)));
  if (fd < 0) {
    NS_WARNING("Cannot accept file descriptor!");
    return NS_ERROR_FAILURE;
  }
  nsresult rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = CheckPermission(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aStreamFd = fd.forget();

  return NS_OK;
}

nsresult
KeyStoreConnector::CreateStreamSocket(struct sockaddr* aAddress,
                                      socklen_t* aAddressLength,
                                      int& aStreamFd)
{
  MOZ_CRASH("|KeyStoreConnector| does not support creating stream sockets.");
  return NS_ERROR_FAILURE;
}

nsresult
KeyStoreConnector::Duplicate(UnixSocketConnector*& aConnector)
{
  aConnector = new KeyStoreConnector(*this);

  return NS_OK;
}

}
}
