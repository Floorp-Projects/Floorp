/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Transport_h
#define mozilla_ipc_Transport_h 1

#include "base/process_util.h"
#include "chrome/common/ipc_channel.h"

#ifdef OS_POSIX
# include "mozilla/ipc/Transport_posix.h"
#elif OS_WIN
# include "mozilla/ipc/Transport_win.h"
#endif

namespace mozilla {
namespace ipc {

class FileDescriptor;

typedef IPC::Channel Transport;

bool CreateTransport(base::ProcessHandle aProcOne, base::ProcessHandle aProcTwo,
                     TransportDescriptor* aOne, TransportDescriptor* aTwo);

Transport* OpenDescriptor(const TransportDescriptor& aTd,
                          Transport::Mode aMode);

Transport* OpenDescriptor(const FileDescriptor& aFd,
                          Transport::Mode aMode);

void CloseDescriptor(const TransportDescriptor& aTd);

} // namespace ipc
} // namespace mozilla

#endif  // mozilla_ipc_Transport_h
