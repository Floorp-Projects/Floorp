/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_callevent_h
#define mozilla_dom_telephony_callevent_h

#include "TelephonyCommon.h"

#include "nsDOMEvent.h"

namespace mozilla {
namespace dom {
struct CallEventInit;
}
}

BEGIN_TELEPHONY_NAMESPACE

class CallEvent MOZ_FINAL : public nsDOMEvent
{
  nsRefPtr<TelephonyCall> mCall;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CallEvent, nsDOMEvent)
  NS_FORWARD_TO_NSDOMEVENT

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  static already_AddRefed<CallEvent>
  Constructor(const GlobalObject& aGlobal, const nsAString& aType,
              const CallEventInit& aOptions, ErrorResult& aRv);

  already_AddRefed<TelephonyCall>
  GetCall() const;

  static already_AddRefed<CallEvent>
  Create(EventTarget* aOwner, const nsAString& aType, TelephonyCall* aCall,
         bool aCanBubble, bool aCancelable);

private:
  CallEvent(EventTarget* aOwner,
            nsPresContext* aPresContext,
            nsEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent)
  {
    SetIsDOMBinding();
  }

  virtual ~CallEvent()
  { }
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_callevent_h
