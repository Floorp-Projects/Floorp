/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>

#include <string>

#include "chrome/common/child_process_info.h"

#include "mozilla/ipc/Transport.h"
#include "mozilla/ipc/FileDescriptor.h"

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
  // Use MODE_SERVER to force creation of the socketpair
  Transport t(id, Transport::MODE_SERVER, nullptr);
  int fd1 = t.GetFileDescriptor();
  int fd2, dontcare;
  t.GetClientFileDescriptorMapping(&fd2, &dontcare);
  if (fd1 < 0 || fd2 < 0) {
    return NS_ERROR_TRANSPORT_INIT;
  }

  // The Transport closes these fds when it goes out of scope, so we
  // dup them here
  fd1 = dup(fd1);
  fd2 = dup(fd2);
  if (fd1 < 0 || fd2 < 0) {
    return NS_ERROR_DUPLICATE_HANDLE;
  }

  aOne->mFd = base::FileDescriptor(fd1, true/*close after sending*/);
  aTwo->mFd = base::FileDescriptor(fd2, true/*close after sending*/);
  return NS_OK;
}

Transport*
OpenDescriptor(const TransportDescriptor& aTd, Transport::Mode aMode)
{
  return new Transport(aTd.mFd.fd, aMode, nullptr);
}

Transport*
OpenDescriptor(const FileDescriptor& aFd, Transport::Mode aMode)
{
  return new Transport(aFd.PlatformHandle(), aMode, nullptr);
}

void
CloseDescriptor(const TransportDescriptor& aTd)
{
  close(aTd.mFd.fd);
}

} // namespace ipc
} // namespace mozilla
