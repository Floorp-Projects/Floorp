/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Socket_h
#define mozilla_ipc_Socket_h

namespace mozilla {
namespace ipc {

int
GetNewSocket(int type, const char* aAddress, int channel, bool auth, bool encrypt);

int
CloseSocket(int aFd);

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_Socket_h
