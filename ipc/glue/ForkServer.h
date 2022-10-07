/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __FORKSERVER_H_
#define __FORKSERVER_H_

#include "mozilla/UniquePtr.h"
#include "base/process_util.h"
#include "mozilla/ipc/MiniTransceiver.h"

namespace mozilla {
namespace ipc {

class ForkServer {
 public:
  // NOTE: This can re-use the same ID as the initial IPC::Channel, as the
  // initial IPC::Channel will not be used by the fork server.
  static constexpr int kClientPipeFd = 3;

  ForkServer();
  ~ForkServer(){};

  void InitProcess(int* aArgc, char*** aArgv);
  bool HandleMessages();

  // Called when a message is received.
  void OnMessageReceived(UniquePtr<IPC::Message> message);

  static bool RunForkServer(int* aArgc, char*** aArgv);

 private:
  UniquePtr<MiniTransceiver> mTcver;
  UniquePtr<base::AppProcessBuilder> mAppProcBuilder;
};

enum {
  Msg_ForkNewSubprocess__ID = 0x7f0,  // a random picked number
  Reply_ForkNewSubprocess__ID,
};

}  // namespace ipc
}  // namespace mozilla

#endif  // __FORKSERVER_H_
