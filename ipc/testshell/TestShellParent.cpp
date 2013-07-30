/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestShellParent.h"

/* This must occur *after* TestShellParent.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "mozilla/dom/ContentParent.h"

#include "nsAutoPtr.h"

using namespace mozilla;
using mozilla::ipc::TestShellParent;
using mozilla::ipc::TestShellCommandParent;
using mozilla::ipc::PTestShellCommandParent;
using mozilla::dom::ContentParent;

PTestShellCommandParent*
TestShellParent::AllocPTestShellCommandParent(const nsString& aCommand)
{
  return new TestShellCommandParent();
}

bool
TestShellParent::DeallocPTestShellCommandParent(PTestShellCommandParent* aActor)
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

JSBool
TestShellCommandParent::SetCallback(JSContext* aCx,
                                    JS::Value aCallback)
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
  NS_ENSURE_TRUE(mCallback.ToJSObject(), JS_FALSE);
  JSAutoCompartment ac(mCx, mCallback.ToJSObject());
  JS::Rooted<JSObject*> global(mCx, JS::CurrentGlobalOrNull(mCx));

  JSString* str = JS_NewUCStringCopyN(mCx, aResponse.get(), aResponse.Length());
  NS_ENSURE_TRUE(str, JS_FALSE);

  JS::Rooted<JS::Value> strVal(mCx, JS::StringValue(str));

  JS::Rooted<JS::Value> rval(mCx);
  JSBool ok = JS_CallFunctionValue(mCx, global, mCallback, 1, strVal.address(),
                                   rval.address());
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
