/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_eventtarget_h__
#define mozilla_dom_workers_eventtarget_h__

#include "mozilla/dom/workers/bindings/DOMBindingBase.h"

// I hate having to export this...
#include "mozilla/dom/workers/bindings/EventListenerManager.h"

#include "mozilla/dom/bindings/Nullable.h"

using namespace mozilla::dom::bindings;

BEGIN_WORKERS_NAMESPACE

class EventTarget : public DOMBindingBase
{
  EventListenerManager mListenerManager;

protected:
  EventTarget(JSContext* aCx)
  : DOMBindingBase(aCx)
  { }

  virtual ~EventTarget()
  { }

public:
  virtual void
  _Trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _Finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  void
  AddEventListener(const nsAString& aType, JSObject* aListener,
                   bool aCapture, Nullable<bool> aWantsUntrusted, nsresult& aRv);

  void
  RemoveEventListener(const nsAString& aType, JSObject* aListener,
                      bool aCapture, nsresult& aRv);

  bool
  DispatchEvent(JSObject* aEvent, nsresult& aRv) const
  {
    return mListenerManager.DispatchEvent(GetJSContext(), *this, aEvent, aRv);
  }

  JSObject*
  GetEventListener(const nsAString& aType, nsresult& aRv) const;

  void
  SetEventListener(const nsAString& aType, JSObject* aListener,
                   nsresult& aRv);

  bool
  HasListeners() const
  {
    return mListenerManager.HasListeners();
  }
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_eventtarget_h__
