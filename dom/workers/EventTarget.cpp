/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventTarget.h"
#include "mozilla/dom/EventListenerBinding.h"
#include "mozilla/dom/EventHandlerBinding.h"

USING_WORKERS_NAMESPACE
using mozilla::ErrorResult;
using mozilla::dom::EventListener;
using mozilla::dom::Nullable;
using mozilla::dom::EventHandlerNonNull;

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

already_AddRefed<EventHandlerNonNull>
EventTarget::GetEventListener(const nsAString& aType, ErrorResult& aRv) const
{
  JSContext* cx = GetJSContext();

  JS::RootedString type(cx,
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length()));
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  JS::RootedObject listener(
    cx, mListenerManager.GetEventListener(INTERNED_STRING_TO_JSID(cx, type)));
  if (!listener) {
    return nullptr;
  }

  nsRefPtr<EventHandlerNonNull> handler = new EventHandlerNonNull(listener);
  return handler.forget();
}

void
EventTarget::SetEventListener(const nsAString& aType,
                              EventHandlerNonNull* aListener,
                              ErrorResult& aRv)
{
  JSContext* cx = GetJSContext();

  JS::RootedString type(cx,
    JS_NewUCStringCopyN(cx, aType.BeginReading(), aType.Length()));
  if (!type || !(type = JS_InternJSString(cx, type))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  JS::RootedObject listener(cx);
  if (aListener) {
    listener = aListener->Callable();
  }
  mListenerManager.SetEventListener(cx, INTERNED_STRING_TO_JSID(cx, type),
                                    listener, aRv);
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
