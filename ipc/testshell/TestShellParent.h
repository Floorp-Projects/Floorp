/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_testshell_TestShellParent_h
#define ipc_testshell_TestShellParent_h 1

#include "mozilla/ipc/PTestShellParent.h"
#include "mozilla/ipc/PTestShellCommandParent.h"

#include "jsapi.h"
#include "nsAutoJSValHolder.h"
#include "nsStringGlue.h"

struct JSContext;
struct JSObject;

namespace mozilla {

namespace jsipc {
class PContextWrapperParent;
}

namespace ipc {

class TestShellCommandParent;

class TestShellParent : public PTestShellParent
{
public:
  PTestShellCommandParent*
  AllocPTestShellCommand(const nsString& aCommand);

  bool
  DeallocPTestShellCommand(PTestShellCommandParent* aActor);

  bool
  CommandDone(TestShellCommandParent* aActor, const nsString& aResponse);

  PContextWrapperParent* AllocPContextWrapper();
  bool DeallocPContextWrapper(PContextWrapperParent* actor);

  JSBool GetGlobalJSObject(JSContext* cx, JSObject** globalp);
};


class TestShellCommandParent : public PTestShellCommandParent
{
public:
  TestShellCommandParent() : mCx(NULL) { }

  JSBool SetCallback(JSContext* aCx,
                     jsval aCallback);

  JSBool RunCallback(const nsString& aResponse);

  void ReleaseCallback();

protected:
  bool ExecuteCallback(const nsString& aResponse);

  void ActorDestroy(ActorDestroyReason why);
  
  bool Recv__delete__(const nsString& aResponse) {
    return ExecuteCallback(aResponse);
  }

private:
  JSContext* mCx;
  nsAutoJSValHolder mCallback;
};


} /* namespace ipc */
} /* namespace mozilla */

#endif /* ipc_testshell_TestShellParent_h */
