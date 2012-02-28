/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPCShell.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TestShellParent.h"

/* This must occur *after* TestShellParent.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/jsipc/ContextWrapperParent.h"

#include "nsAutoPtr.h"

using namespace mozilla;
using mozilla::ipc::TestShellParent;
using mozilla::ipc::TestShellCommandParent;
using mozilla::ipc::PTestShellCommandParent;
using mozilla::dom::ContentParent;
using mozilla::jsipc::PContextWrapperParent;
using mozilla::jsipc::ContextWrapperParent;

PTestShellCommandParent*
TestShellParent::AllocPTestShellCommand(const nsString& aCommand)
{
  return new TestShellCommandParent();
}

bool
TestShellParent::DeallocPTestShellCommand(PTestShellCommandParent* aActor)
{
  delete aActor;
  return true;
}

bool
TestShellParent::CommandDone(TestShellCommandParent* command,
                             const nsString& aResponse)
{
  // XXX what should happen if the callback fails?
  /*JSBool ok = */command->RunCallback(aResponse);
  command->ReleaseCallback();

  return true;
}

PContextWrapperParent*
TestShellParent::AllocPContextWrapper()
{
    ContentParent* cpp = static_cast<ContentParent*>(Manager());
    return new ContextWrapperParent(cpp);
}

bool
TestShellParent::DeallocPContextWrapper(PContextWrapperParent* actor)
{
    delete actor;
    return true;
}

JSBool
TestShellParent::GetGlobalJSObject(JSContext* cx, JSObject** globalp)
{
    // TODO Unify this code with TabParent::GetGlobalJSObject.
    InfallibleTArray<PContextWrapperParent*> cwps(1);
    ManagedPContextWrapperParent(cwps);
    if (cwps.Length() < 1)
        return JS_FALSE;
    NS_ASSERTION(cwps.Length() == 1, "More than one PContextWrapper?");
    ContextWrapperParent* cwp = static_cast<ContextWrapperParent*>(cwps[0]);
    return cwp->GetGlobalJSObject(cx, globalp);
}

JSBool
TestShellCommandParent::SetCallback(JSContext* aCx,
                                    jsval aCallback)
{
  if (!mCallback.Hold(aCx)) {
    return JS_FALSE;
  }

  mCallback = aCallback;
  mCx = aCx;

  return JS_TRUE;
}

JSBool
TestShellCommandParent::RunCallback(const nsString& aResponse)
{
  NS_ENSURE_TRUE(*mCallback.ToJSValPtr() != JSVAL_NULL && mCx, JS_FALSE);

  JSAutoRequest ar(mCx);

  JSObject* global = JS_GetGlobalObject(mCx);
  NS_ENSURE_TRUE(global, JS_FALSE);

  JSAutoEnterCompartment ac;
  if (!ac.enter(mCx, global)) {
    NS_ERROR("Failed to enter compartment!");
    return false;
  }

  JSString* str = JS_NewUCStringCopyN(mCx, aResponse.get(), aResponse.Length());
  NS_ENSURE_TRUE(str, JS_FALSE);

  jsval argv[] = { STRING_TO_JSVAL(str) };
  unsigned argc = ArrayLength(argv);

  jsval rval;
  JSBool ok = JS_CallFunctionValue(mCx, global, mCallback, argc, argv, &rval);
  NS_ENSURE_TRUE(ok, JS_FALSE);

  return JS_TRUE;
}

void
TestShellCommandParent::ReleaseCallback()
{
  mCallback.Release();
}

bool
TestShellCommandParent::ExecuteCallback(const nsString& aResponse)
{
  return static_cast<TestShellParent*>(Manager())->CommandDone(
      this, aResponse);
}

void
TestShellCommandParent::ActorDestroy(ActorDestroyReason why)
{
  if (why == AbnormalShutdown) {
    ExecuteCallback(EmptyString());
  }
}
