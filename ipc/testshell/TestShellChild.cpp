/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestShellChild.h"

using mozilla::ipc::PTestShellCommandChild;
using mozilla::ipc::TestShellChild;
using mozilla::ipc::XPCShellEnvironment;

TestShellChild::TestShellChild()
    : mXPCShell(XPCShellEnvironment::CreateEnvironment()) {}

mozilla::ipc::IPCResult TestShellChild::RecvExecuteCommand(
    const nsAString& aCommand) {
  if (mXPCShell->IsQuitting()) {
    NS_WARNING("Commands sent after quit command issued!");
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mXPCShell->EvaluateString(aCommand)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PTestShellCommandChild* TestShellChild::AllocPTestShellCommandChild(
    const nsAString& aCommand) {
  return new PTestShellCommandChild();
}

bool TestShellChild::DeallocPTestShellCommandChild(
    PTestShellCommandChild* aCommand) {
  delete aCommand;
  return true;
}

mozilla::ipc::IPCResult TestShellChild::RecvPTestShellCommandConstructor(
    PTestShellCommandChild* aActor, const nsAString& aCommand) {
  if (mXPCShell->IsQuitting()) {
    NS_WARNING("Commands sent after quit command issued!");
    return IPC_FAIL_NO_REASON(this);
  }

  nsString response;
  if (!mXPCShell->EvaluateString(aCommand, &response)) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!PTestShellCommandChild::Send__delete__(aActor, response)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}
