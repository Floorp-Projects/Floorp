/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventTarget.h"

USING_WORKERS_NAMESPACE
using mozilla::ErrorResult;
using namespace mozilla::dom;

void
EventTarget::_trace(JSTracer* aTrc)
{
  mListenerManager._trace(aTrc);
  DOMBindingBase::_trace(aTrc);
}

void
EventTarget::_finalize(JSFreeOp* aFop)
{
  mListenerManager._finalize(aFop);
  DOMBindingBase::_finalize(aFop);
}

JSObject*
EventTarget::GetEventListener(const nsAString& aType, ErrorResult& aRv) const
{
  JSContext* cx = GetJSContext();

  JSString* type =
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length());
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  return mListenerManager.GetEventListener(INTERNED_STRING_TO_JSID(cx, type));
}

void
EventTarget::SetEventListener(const nsAString& aType, JSObject* aListener_,
                              ErrorResult& aRv)
{
  JSContext* cx = GetJSContext();
  JS::Rooted<JSObject*> aListener(cx, aListener_);

  JSString* type =
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length());
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  mListenerManager.SetEventListener(cx, INTERNED_STRING_TO_JSID(cx, type),
                                    aListener, aRv);
}

void
EventTarget::AddEventListener(const nsAString& aType, JSObject* aListener_,
                              bool aCapturing, Nullable<bool> aWantsUntrusted,
                              ErrorResult& aRv)
{
  if (!aListener_) {
    return;
  }

  JSContext* cx = GetJSContext();
  JS::Rooted<JSObject*> aListener(cx, aListener_);

  JSString* type =
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length());
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  bool wantsUntrusted = !aWantsUntrusted.IsNull() && aWantsUntrusted.Value();
  mListenerManager.AddEventListener(cx, INTERNED_STRING_TO_JSID(cx, type),
                                    aListener, aCapturing, wantsUntrusted,
                                    aRv);
}

void
EventTarget::RemoveEventListener(const nsAString& aType, JSObject* aListener_,
                                 bool aCapturing, ErrorResult& aRv)
{
  if (!aListener_) {
    return;
  }

  JSContext* cx = GetJSContext();
  JS::Rooted<JSObject*> aListener(cx, aListener_);

  JSString* type =
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length());
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  mListenerManager.RemoveEventListener(cx, INTERNED_STRING_TO_JSID(cx, type),
                                       aListener, aCapturing);
}
