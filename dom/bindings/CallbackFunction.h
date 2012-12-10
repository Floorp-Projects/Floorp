/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A common base class for representing WebIDL callback function types in C++.
 *
 * This class implements common functionality like lifetime
 * management, initialization with the callable, and setup of the call
 * environment.  Subclasses corresponding to particular callback
 * function types should provide a Call() method that actually does
 * the call.
 */

#pragma once

#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsCycleCollectionParticipant.h"
#include "jsapi.h"
#include "jswrapper.h"
#include "mozilla/Assertions.h"
#include "mozilla/Util.h"
#include "nsContentUtils.h"
#include "nsWrapperCache.h"
#include "nsJSEnvironment.h"
#include "xpcpublic.h"
#include "nsLayoutStatics.h"

namespace mozilla {
namespace dom {

class CallbackFunction : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CallbackFunction)

  /**
   * Create a CallbackFunction.  aCallable is the callable we're wrapping.
   * aOwner is the object that will be receiving this CallbackFunction as a
   * method argument, if any.  We need this so we can store our callable in the
   * same compartment as our owner.  If *aInited is set to false, an exception
   * has been thrown.
   */
  CallbackFunction(JSContext* cx, JSObject* aOwner, JSObject* aCallable,
                   bool* aInited)
    : mCallable(nullptr)
  {
    MOZ_ASSERT(JS_ObjectIsCallable(cx, aCallable));
    // If aOwner is not null, enter the compartment of aOwner's
    // underlying object.
    if (aOwner) {
      aOwner = js::UnwrapObject(aOwner);
      JSAutoCompartment ac(cx, aOwner);
      if (!JS_WrapObject(cx, &aCallable)) {
        *aInited = false;
        return;
      }
    }

    // Set mCallable before we hold, on the off chance that a GC could somehow
    // happen in there... (which would be pretty odd, granted).
    mCallable = aCallable;
    // Make sure we'll be able to drop as needed
    nsLayoutStatics::AddRef();
    NS_HOLD_JS_OBJECTS(this, CallbackFunction);
    *aInited = true;
  }

  virtual ~CallbackFunction()
  {
    DropCallback();
  }

  JSObject* Callable() const
  {
    xpc_UnmarkGrayObject(mCallable);
    return mCallable;
  }

  bool HasGrayCallable() const
  {
    // Play it safe in case this gets called after unlink.
    return mCallable && xpc_IsGrayGCThing(mCallable);
  }

protected:
  explicit CallbackFunction(CallbackFunction* aCallbackFunction)
    : mCallable(aCallbackFunction->mCallable)
  {
    // Set mCallable before we hold, on the off chance that a GC could somehow
    // happen in there... (which would be pretty odd, granted).
    // Make sure we'll be able to drop as needed
    nsLayoutStatics::AddRef();
    NS_HOLD_JS_OBJECTS(this, CallbackFunction);
  }

  void DropCallback()
  {
    if (mCallable) {
      mCallable = nullptr;
      NS_DROP_JS_OBJECTS(this, CallbackFunction);
      nsLayoutStatics::Release();
    }
  }

  JSObject* mCallable;

  class NS_STACK_CLASS CallSetup
  {
    /**
     * A class that performs whatever setup we need to safely make a
     * call while this class is on the stack, After the constructor
     * returns, the call is safe to make if GetContext() returns
     * non-null.
     */
  public:
    CallSetup(JSObject* const aCallable);
    ~CallSetup();

    JSContext* GetContext() const
    {
      return mCx;
    }

  private:
    // We better not get copy-constructed
    CallSetup(const CallSetup&) MOZ_DELETE;

    // Members which can go away whenever
    JSContext* mCx;
    nsCOMPtr<nsIScriptContext> mCtx;

    // And now members whose construction/destruction order we need to control.

    // Put our nsAutoMicrotask first, so it gets destroyed after everything else
    // is gone
    nsAutoMicroTask mMt;

    // Can't construct an XPCAutoRequest until we have a JSContext, so
    // this needs to be a Maybe.
    Maybe<XPCAutoRequest> mAr;

    // Can't construct a TerminationFuncHolder without an nsJSContext.  But we
    // generally want its destructor to come after the destructor of mCxPusher.
    Maybe<nsJSContext::TerminationFuncHolder> mTerminationFuncHolder;

    nsCxPusher mCxPusher;

    // Can't construct a JSAutoCompartment without a JSContext either.  Also,
    // Put mAc after mCxPusher so that we exit the compartment before we pop the
    // JSContext.  Though in practice we'll often manually order those two
    // things.
    Maybe<JSAutoCompartment> mAc;
  };
};

} // namespace dom
} // namespace mozilla
