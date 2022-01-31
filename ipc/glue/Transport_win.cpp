/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/message_loop.h"

#include "mozilla/ipc/Transport.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include <windows.h>

using base::ProcessHandle;

namespace mozilla {
namespace ipc {

nsresult CreateTransport(TransportDescriptor* aOne, TransportDescriptor* aTwo) {
  auto id = IPC::Channel::GenerateVerifiedChannelID();

  // Use MODE_SERVER to force creation of the pipe
  // NB: we create the server pipe immediately, instead of just
  // grabbing an ID, on purpose.  In the current setup, the client
  // needs to connect to an existing server pipe, so to prevent race
  // conditions, we create the server side here. When we send the pipe
  // to the server, we DuplicateHandle it to the server process to give it
  // access.
  Transport t(id, Transport::MODE_SERVER, nullptr);
  HANDLE serverPipe = t.GetServerPipeHandle();
  if (!serverPipe) {
    return NS_ERROR_TRANSPORT_INIT;
  }

  // Make a copy of the handle owned by the `Transport` which will be
  // transferred to the actual server process.
  if (!::DuplicateHandle(GetCurrentProcess(), serverPipe, GetCurrentProcess(),
                         &aOne->mServerPipeHandle, 0, false,
                         DUPLICATE_SAME_ACCESS)) {
    return NS_ERROR_DUPLICATE_HANDLE;
  }

  aOne->mPipeName = aTwo->mPipeName = id;
  aTwo->mServerPipeHandle = INVALID_HANDLE_VALUE;
  return NS_OK;
}

UniquePtr<Transport> OpenDescriptor(const TransportDescriptor& aTd,
                                    Transport::Mode aMode) {
  return MakeUnique<Transport>(aTd.mPipeName, aTd.mServerPipeHandle, aMode,
                               nullptr);
}

TransportDescriptor DuplicateDescriptor(const TransportDescriptor& aTd) {
  // We're duplicating this handle in our own process for bookkeeping purposes.

  if (aTd.mServerPipeHandle == INVALID_HANDLE_VALUE) {
    return aTd;
  }

  HANDLE serverDup;
  bool ok = ::DuplicateHandle(GetCurrentProcess(), aTd.mServerPipeHandle,
                              GetCurrentProcess(), &serverDup, 0, false,
                              DUPLICATE_SAME_ACCESS);
  if (!ok) {
    AnnotateSystemError();
  }
  MOZ_RELEASE_ASSERT(ok);

  TransportDescriptor desc = aTd;
  desc.mServerPipeHandle = serverDup;
  return desc;
}

void CloseDescriptor(const TransportDescriptor& aTd) {
  // We're closing our own local copy of the pipe.

  CloseHandle(aTd.mServerPipeHandle);
}

}  // namespace ipc
}  // namespace mozilla
