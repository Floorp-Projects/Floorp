/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/message_loop.h"

#include "mozilla/ipc/Transport.h"
#include "mozilla/ipc/ProtocolUtils.h"

using namespace std;

using base::ProcessHandle;

namespace mozilla {
namespace ipc {

nsresult
CreateTransport(base::ProcessId aProcIdOne,
                TransportDescriptor* aOne,
                TransportDescriptor* aTwo)
{
  wstring id = IPC::Channel::GenerateVerifiedChannelID(std::wstring());
  // Use MODE_SERVER to force creation of the pipe
  Transport t(id, Transport::MODE_SERVER, nullptr);
  HANDLE serverPipe = t.GetServerPipeHandle();
  if (!serverPipe) {
    return NS_ERROR_TRANSPORT_INIT;
  }

  // NB: we create the server pipe immediately, instead of just
  // grabbing an ID, on purpose.  In the current setup, the client
  // needs to connect to an existing server pipe, so to prevent race
  // conditions, we create the server side here. When we send the pipe
  // to the server, we DuplicateHandle it to the server process to give it
  // access.
  HANDLE serverDup;
  DWORD access = 0;
  DWORD options = DUPLICATE_SAME_ACCESS;
  if (!DuplicateHandle(serverPipe, base::GetCurrentProcId(), &serverDup, access, options)) {
    return NS_ERROR_DUPLICATE_HANDLE;
  }

  aOne->mPipeName = aTwo->mPipeName = id;
  aOne->mServerPipeHandle = serverDup;
  aOne->mDestinationProcessId = aProcIdOne;
  aTwo->mServerPipeHandle = INVALID_HANDLE_VALUE;
  aTwo->mDestinationProcessId = 0;
  return NS_OK;
}

HANDLE
TransferHandleToProcess(HANDLE source, base::ProcessId pid)
{
  // At this point we're sending the handle to another process.

  if (source == INVALID_HANDLE_VALUE) {
    return source;
  }
  HANDLE handleDup;
  DWORD access = 0;
  DWORD options = DUPLICATE_SAME_ACCESS;
  bool ok = DuplicateHandle(source, pid, &handleDup, access, options);
  if (!ok) {
    return nullptr;
  }

  // Now close our own copy of the handle (we're supposed to be transferring,
  // not copying).
  CloseHandle(source);

  return handleDup;
}

UniquePtr<Transport>
OpenDescriptor(const TransportDescriptor& aTd, Transport::Mode aMode)
{
  if (aTd.mServerPipeHandle != INVALID_HANDLE_VALUE) {
    MOZ_RELEASE_ASSERT(aTd.mDestinationProcessId == base::GetCurrentProcId());
  }
  return MakeUnique<Transport>(aTd.mPipeName, aTd.mServerPipeHandle, aMode, nullptr);
}

UniquePtr<Transport>
OpenDescriptor(const FileDescriptor& aFd, Transport::Mode aMode)
{
  MOZ_ASSERT_UNREACHABLE("Not implemented!");
  return nullptr;
}

TransportDescriptor
DuplicateDescriptor(const TransportDescriptor& aTd)
{
  // We're duplicating this handle in our own process for bookkeeping purposes.

  if (aTd.mServerPipeHandle == INVALID_HANDLE_VALUE) {
    return aTd;
  }

  HANDLE serverDup;
  DWORD access = 0;
  DWORD options = DUPLICATE_SAME_ACCESS;
  bool ok = DuplicateHandle(aTd.mServerPipeHandle, base::GetCurrentProcId(),
                            &serverDup, access, options);
  if (!ok) {
    AnnotateSystemError();
  }
  MOZ_RELEASE_ASSERT(ok);

  TransportDescriptor desc = aTd;
  desc.mServerPipeHandle = serverDup;
  return desc;
}

void
CloseDescriptor(const TransportDescriptor& aTd)
{
  // We're closing our own local copy of the pipe.

  CloseHandle(aTd.mServerPipeHandle);
}

} // namespace ipc
} // namespace mozilla
