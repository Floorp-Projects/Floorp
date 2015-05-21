/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_RilConnector_h
#define mozilla_ipc_RilConnector_h

#include "mozilla/ipc/UnixSocketConnector.h"

namespace mozilla {
namespace ipc {

/**
 * |RilConnector| creates sockets for connecting to rild.
 */
class RilConnector final : public UnixSocketConnector
{
public:
  RilConnector(const nsACString& aAddressString,
               unsigned long aClientId);
  ~RilConnector();

  // Methods for |UnixSocketConnector|
  //

  nsresult ConvertAddressToString(const struct sockaddr& aAddress,
                                  socklen_t aAddressLength,
                                  nsACString& aAddressString) override;

  nsresult CreateListenSocket(struct sockaddr* aAddress,
                              socklen_t* aAddressLength,
                              int& aListenFd) override;

  nsresult AcceptStreamSocket(int aListenFd,
                              struct sockaddr* aAddress,
                              socklen_t* aAddressLen,
                              int& aStreamFd) override;

  nsresult CreateStreamSocket(struct sockaddr* aAddress,
                              socklen_t* aAddressLength,
                              int& aStreamFd) override;

  nsresult Duplicate(UnixSocketConnector*& aConnector) override;


private:
  nsresult CreateSocket(int aDomain, int& aFd) const;
  nsresult SetSocketFlags(int aFd) const;
  nsresult CreateAddress(int aDomain,
                         struct sockaddr& aAddress,
                         socklen_t& aAddressLength) const;

  nsCString mAddressString;
  unsigned long mClientId;
};

}
}

#endif
