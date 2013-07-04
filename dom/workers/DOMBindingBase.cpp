/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMBindingBase.h"

#include "jsfriendapi.h"
#include "mozilla/dom/DOMJSClass.h"
#include "nsContentUtils.h"
#include "nsWrapperCacheInlines.h"

using namespace mozilla;
using namespace mozilla::dom;
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

NS_IMPL_ADDREF(DOMBindingBase)
NS_IMPL_RELEASE(DOMBindingBase)
NS_INTERFACE_MAP_BEGIN(DOMBindingBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

void
DOMBindingBase::_trace(JSTracer* aTrc)
{
  TraceJSObject(aTrc, "cached wrapper");
}

void
DOMBindingBase::_finalize(JSFreeOp* aFop)
{
  ClearWrapper();
  NS_RELEASE_THIS();
}

JSContext*
DOMBindingBase::GetJSContext() const {
  return mJSContext ? mJSContext : nsContentUtils::GetCurrentJSContext();
}

#ifdef DEBUG
JSObject*
DOMBindingBase::GetJSObject() const
{
  // Make sure that the public method results in the same bits as our private
  // method.
  MOZ_ASSERT(GetWrapperJSObject() == GetWrapperPreserveColor());
  return GetWrapperJSObject();
}

void
DOMBindingBase::SetJSObject(JSObject* aObject)
{
  // Make sure that the public method results in the same bits as our private
  // method.
  SetWrapper(aObject);

  uint8_t oldFlags = mFlags;

  SetWrapperJSObject(aObject);

  MOZ_ASSERT(oldFlags == mFlags && aObject == mWrapper);
}
#endif
