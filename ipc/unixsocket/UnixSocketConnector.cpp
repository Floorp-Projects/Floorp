/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UnixSocketConnector.h"

namespace mozilla {
namespace ipc {

UnixSocketConnector::UnixSocketConnector()
{ }

UnixSocketConnector::~UnixSocketConnector()
{ }

nsresult
UnixSocketConnector::ConvertAddressToString(const struct sockaddr& aAddress,
                                            socklen_t aAddressLength,
                                            nsACString& aAddressString)
{
  MOZ_CRASH("|UnixSocketConnector| does not convert addresses to strings.");
  return NS_ERROR_ABORT;
}

nsresult
UnixSocketConnector::CreateListenSocket(struct sockaddr* aAddress,
                                        socklen_t* aAddressLength,
                                        int& aListenFd)
{
  MOZ_CRASH("|UnixSocketConnector| does not support listening sockets.");
}

nsresult
UnixSocketConnector::AcceptStreamSocket(int aListenFd,
                                        struct sockaddr* aAddress,
                                        socklen_t* aAddressLen,
                                        int& aStreamFd)
{
  MOZ_CRASH("|UnixSocketConnector| does not support accepting stream sockets.");
}

nsresult
UnixSocketConnector::CreateStreamSocket(struct sockaddr* aAddress,
                                        socklen_t* aAddressLength,
                                        int& aStreamFd)
{
  MOZ_CRASH("|UnixSocketConnector| does not support creating stream sockets.");
}

}
}
