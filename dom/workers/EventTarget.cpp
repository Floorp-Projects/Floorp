/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventTarget.h"
#include "mozilla/dom/EventListenerBinding.h"

USING_WORKERS_NAMESPACE
using mozilla::ErrorResult;
using mozilla::dom::EventListener;
using mozilla::dom::Nullable;

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

  JS::RootedString type(cx,
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length()));
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  return mListenerManager.GetEventListener(INTERNED_STRING_TO_JSID(cx, type));
}

void
EventTarget::SetEventListener(const nsAString& aType,
                              JS::Handle<JSObject*> aListener,
                              ErrorResult& aRv)
{
  JSContext* cx = GetJSContext();

  JS::RootedString type(cx,
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length()));
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  mListenerManager.SetEventListener(cx, INTERNED_STRING_TO_JSID(cx, type),
                                    aListener, aRv);
}

void
EventTarget::AddEventListener(const nsAString& aType,
                              EventListener* aListener,
                              bool aCapturing, Nullable<bool> aWantsUntrusted,
                              ErrorResult& aRv)
{
  if (!aListener) {
    return;
  }

  JSContext* cx = GetJSContext();

  JS::RootedString type(cx,
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length()));
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  bool wantsUntrusted = !aWantsUntrusted.IsNull() && aWantsUntrusted.Value();
  mListenerManager.AddEventListener(cx, INTERNED_STRING_TO_JSID(cx, type),
                                    aListener->Callback(), aCapturing,
                                    wantsUntrusted, aRv);
}

void
EventTarget::RemoveEventListener(const nsAString& aType,
                                 EventListener* aListener,
                                 bool aCapturing, ErrorResult& aRv)
{
  if (!aListener) {
    return;
  }

  JSContext* cx = GetJSContext();

  JS::RootedString type(cx,
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length()));
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  mListenerManager.RemoveEventListener(cx, INTERNED_STRING_TO_JSID(cx, type),
                                       aListener->Callback(), aCapturing);
}
