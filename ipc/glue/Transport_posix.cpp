/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>

#include <string>

#include "base/eintr_wrapper.h"

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/ProtocolUtils.h"

using base::ProcessHandle;

namespace mozilla {
namespace ipc {

nsresult CreateTransport(TransportDescriptor* aOne, TransportDescriptor* aTwo) {
  auto id = IPC::Channel::GenerateVerifiedChannelID();
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
  if (fd1 < 0) {
    AnnotateCrashReportWithErrno(
        CrashReporter::Annotation::IpcCreateTransportDupErrno, errno);
  }
  fd2 = dup(fd2);
  if (fd2 < 0) {
    AnnotateCrashReportWithErrno(
        CrashReporter::Annotation::IpcCreateTransportDupErrno, errno);
  }

  if (fd1 < 0 || fd2 < 0) {
    IGNORE_EINTR(close(fd1));
    IGNORE_EINTR(close(fd2));
    return NS_ERROR_DUPLICATE_HANDLE;
  }

  aOne->mFd = fd1;
  aTwo->mFd = fd2;
  return NS_OK;
}

UniquePtr<Transport> OpenDescriptor(const TransportDescriptor& aTd,
                                    Transport::Mode aMode) {
  return MakeUnique<Transport>(aTd.mFd, aMode, nullptr);
}

TransportDescriptor DuplicateDescriptor(const TransportDescriptor& aTd) {
  TransportDescriptor result = aTd;
  result.mFd = dup(aTd.mFd);
  if (result.mFd == -1) {
    AnnotateSystemError();
  }
  MOZ_RELEASE_ASSERT(result.mFd != -1, "DuplicateDescriptor failed");
  return result;
}

void CloseDescriptor(const TransportDescriptor& aTd) { close(aTd.mFd); }

}  // namespace ipc
}  // namespace mozilla
