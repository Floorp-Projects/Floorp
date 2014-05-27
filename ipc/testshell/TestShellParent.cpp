/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestShellParent.h"

/* This must occur *after* TestShellParent.h to avoid typedefs conflicts. */
#include "mozilla/ArrayUtils.h"

#include "mozilla/dom/ContentParent.h"

#include "nsAutoPtr.h"
#include "nsCxPusher.h"

using namespace mozilla;
using mozilla::ipc::TestShellParent;
using mozilla::ipc::TestShellCommandParent;
using mozilla::ipc::PTestShellCommandParent;
using mozilla::dom::ContentParent;

void
TestShellParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005177
}

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
  /*bool ok = */command->RunCallback(aResponse);
  command->ReleaseCallback();

  return true;
}

bool
TestShellCommandParent::SetCallback(JSContext* aCx,
                                    JS::Value aCallback)
{
  if (!mCallback.Hold(aCx)) {
    return false;
  }

  mCallback = aCallback;
  mCx = aCx;

  return true;
}

bool
TestShellCommandParent::RunCallback(const nsString& aResponse)
{
  NS_ENSURE_TRUE(!mCallback.get().isNull() && mCx, false);

  // We're pulling a cx off the heap, so make sure it's stack-top.
  AutoCxPusher pusher(mCx);
  NS_ENSURE_TRUE(mCallback.ToJSObject(), false);
  JSAutoCompartment ac(mCx, mCallback.ToJSObject());
  JS::Rooted<JSObject*> global(mCx, JS::CurrentGlobalOrNull(mCx));

  JSString* str = JS_NewUCStringCopyN(mCx, aResponse.get(), aResponse.Length());
  NS_ENSURE_TRUE(str, false);

  JS::Rooted<JS::Value> strVal(mCx, JS::StringValue(str));

  JS::Rooted<JS::Value> rval(mCx);
  JS::Rooted<JS::Value> callback(mCx, mCallback);
  bool ok = JS_CallFunctionValue(mCx, global, callback, JS::HandleValueArray(strVal), &rval);
  NS_ENSURE_TRUE(ok, false);

  return true;
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
