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

using namespace base;
using namespace std;

namespace mozilla {
namespace ipc {

bool
CreateTransport(ProcessHandle /*unused*/, ProcessHandle /*unused*/,
                TransportDescriptor* aOne, TransportDescriptor* aTwo)
{
  // Gecko doesn't care about this random ID, and the argument to this
  // function isn't really necessary, it can be just any random
  // pointer value
  wstring id = ChildProcessInfo::GenerateRandomChannelID(aOne);
  // Use MODE_SERVER to force creation of the socketpair
  Transport t(id, Transport::MODE_SERVER, nsnull);
  int fd1 = t.GetServerFileDescriptor();
  int fd2, dontcare;
  t.GetClientFileDescriptorMapping(&fd2, &dontcare);
  if (fd1 < 0 || fd2 < 0) {
    return false;
  }

  // The Transport closes these fds when it goes out of scope, so we
  // dup them here
  fd1 = dup(fd1);
  fd2 = dup(fd2);
  if (fd1 < 0 || fd2 < 0) {
    return false;
  }

  aOne->mFd = FileDescriptor(fd1, true/*close after sending*/);
  aTwo->mFd = FileDescriptor(fd2, true/*close after sending*/);
  return true;
}

Transport*
OpenDescriptor(const TransportDescriptor& aTd, Transport::Mode aMode)
{
  return new Transport(aTd.mFd.fd, aMode, nsnull);
}

void
CloseDescriptor(const TransportDescriptor& aTd)
{
  close(aTd.mFd.fd);
}

} // namespace ipc
} // namespace mozilla
