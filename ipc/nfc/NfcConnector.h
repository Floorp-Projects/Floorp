/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=2 ts=8 et ft=cpp: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_NfcConnector_h
#define mozilla_ipc_NfcConnector_h

#include "mozilla/ipc/UnixSocketConnector.h"

namespace mozilla {
namespace ipc {

/**
 * |NfcConnector| creates sockets for communicating with
 * the NFC daemon.
 */
class NfcConnector final : public UnixSocketConnector
{
public:
  NfcConnector(const nsACString& aAddressString);
  ~NfcConnector();

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

private:
  nsresult CreateSocket(int& aFd) const;
  nsresult SetSocketFlags(int aFd) const;
  nsresult CreateAddress(struct sockaddr& aAddress,
                         socklen_t& aAddressLength) const;

  nsCString mAddressString;
};

}
}

#endif
