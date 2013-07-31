/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestShellChild.h"

using mozilla::ipc::TestShellChild;
using mozilla::ipc::PTestShellCommandChild;
using mozilla::ipc::XPCShellEnvironment;

TestShellChild::TestShellChild()
: mXPCShell(XPCShellEnvironment::CreateEnvironment())
{
}

bool
TestShellChild::RecvExecuteCommand(const nsString& aCommand)
{
  if (mXPCShell->IsQuitting()) {
    NS_WARNING("Commands sent after quit command issued!");
    return false;
  }

  return mXPCShell->EvaluateString(aCommand);
}

PTestShellCommandChild*
TestShellChild::AllocPTestShellCommand(const nsString& aCommand)
{
  return new PTestShellCommandChild();
}

bool
TestShellChild::DeallocPTestShellCommand(PTestShellCommandChild* aCommand)
{
  delete aCommand;
  return true;
}

bool
TestShellChild::RecvPTestShellCommandConstructor(PTestShellCommandChild* aActor,
                                                 const nsString& aCommand)
{
  if (mXPCShell->IsQuitting()) {
    NS_WARNING("Commands sent after quit command issued!");
    return false;
  }

  nsString response;
  if (!mXPCShell->EvaluateString(aCommand, &response)) {
    return false;
  }

  return PTestShellCommandChild::Send__delete__(aActor, response);
}

