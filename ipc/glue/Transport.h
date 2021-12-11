/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Transport_h
#define mozilla_ipc_Transport_h 1

#include "base/process_util.h"
#include "chrome/common/ipc_channel.h"

#ifdef OS_POSIX
#  include "mozilla/ipc/Transport_posix.h"
#elif OS_WIN
#  include "mozilla/ipc/Transport_win.h"
#endif
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace ipc {

class FileDescriptor;

typedef IPC::Channel Transport;

nsresult CreateTransport(base::ProcessId aProcIdOne, TransportDescriptor* aOne,
                         TransportDescriptor* aTwo);

UniquePtr<Transport> OpenDescriptor(const TransportDescriptor& aTd,
                                    Transport::Mode aMode);

UniquePtr<Transport> OpenDescriptor(const FileDescriptor& aFd,
                                    Transport::Mode aMode);

TransportDescriptor DuplicateDescriptor(const TransportDescriptor& aTd);

void CloseDescriptor(const TransportDescriptor& aTd);

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_Transport_h
