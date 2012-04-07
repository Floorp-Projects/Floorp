/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMBindingBase.h"

#include "nsIJSContextStack.h"

#include "jsfriendapi.h"
#include "mozilla/dom/bindings/DOMJSClass.h"
#include "nsContentUtils.h"
#include "nsWrapperCacheInlines.h"

USING_WORKERS_NAMESPACE

DOMBindingBase::DOMBindingBase(JSContext* aCx)
: mJSContext(aCx)
{
  if (!aCx) {
    AssertIsOnMainThread();
  }
}

DOMBindingBase::~DOMBindingBase()
{
  if (!mJSContext) {
    AssertIsOnMainThread();
  }
}

void
DOMBindingBase::_Trace(JSTracer* aTrc)
{
  JSObject* obj = GetJSObject();
  if (obj) {
    JS_CALL_OBJECT_TRACER(aTrc, obj, "cached wrapper");
  }
}

void
DOMBindingBase::_Finalize(JSFreeOp* aFop)
{
  ClearWrapper();
  NS_RELEASE_THIS();
}

JSContext*
DOMBindingBase::GetJSContextFromContextStack() const
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!mJSContext);

  if (!mContextStack) {
    mContextStack = nsContentUtils::ThreadJSContextStack();
    MOZ_ASSERT(mContextStack);
  }

  JSContext* cx;
  if (NS_FAILED(mContextStack->Peek(&cx))) {
    MOZ_NOT_REACHED("This should never fail!");
  }

  MOZ_ASSERT(cx);
  return cx;
}

#ifdef DEBUG
JSObject*
DOMBindingBase::GetJSObject() const
{
  // Make sure that the public method results in the same bits as our private
  // method.
  MOZ_ASSERT(GetJSObjectFromBits() == GetWrapperPreserveColor());
  return GetJSObjectFromBits();
}

void
DOMBindingBase::SetJSObject(JSObject* aObject)
{
  // Make sure that the public method results in the same bits as our private
  // method.
  SetWrapper(aObject);

  PtrBits oldWrapperPtrBits = mWrapperPtrBits;

  SetWrapperBits(aObject);

  MOZ_ASSERT(oldWrapperPtrBits == mWrapperPtrBits);
}
#endif
