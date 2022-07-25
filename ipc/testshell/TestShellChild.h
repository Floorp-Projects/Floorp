/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_testshell_TestShellChild_h
#define ipc_testshell_TestShellChild_h 1

#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/PTestShellChild.h"
#include "mozilla/ipc/PTestShellCommandChild.h"
#include "mozilla/ipc/XPCShellEnvironment.h"

namespace mozilla {

namespace ipc {

class XPCShellEnvironment;

class TestShellChild : public PTestShellChild {
 public:
  TestShellChild();

  mozilla::ipc::IPCResult RecvExecuteCommand(const nsAString& aCommand);

  PTestShellCommandChild* AllocPTestShellCommandChild(
      const nsAString& aCommand);

  mozilla::ipc::IPCResult RecvPTestShellCommandConstructor(
      PTestShellCommandChild* aActor, const nsAString& aCommand) override;

  bool DeallocPTestShellCommandChild(PTestShellCommandChild* aCommand);

 private:
  UniquePtr<XPCShellEnvironment> mXPCShell;
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* ipc_testshell_TestShellChild_h */
