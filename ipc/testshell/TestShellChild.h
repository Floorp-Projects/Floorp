/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_testshell_TestShellChild_h
#define ipc_testshell_TestShellChild_h 1

#include "mozilla/ipc/PTestShellChild.h"
#include "mozilla/ipc/PTestShellCommandChild.h"
#include "mozilla/ipc/XPCShellEnvironment.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

namespace ipc {

class XPCShellEnvironment;

class TestShellChild : public PTestShellChild {
 public:
  TestShellChild();

  mozilla::ipc::IPCResult RecvExecuteCommand(const nsString& aCommand);

  PTestShellCommandChild* AllocPTestShellCommandChild(const nsString& aCommand);

  mozilla::ipc::IPCResult RecvPTestShellCommandConstructor(
      PTestShellCommandChild* aActor, const nsString& aCommand) override;

  bool DeallocPTestShellCommandChild(PTestShellCommandChild* aCommand);

 private:
  UniquePtr<XPCShellEnvironment> mXPCShell;
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* ipc_testshell_TestShellChild_h */
