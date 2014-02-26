/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BeforeUnloadEvent_h_
#define mozilla_dom_BeforeUnloadEvent_h_

#include "nsIDOMBeforeUnloadEvent.h"
#include "nsDOMEvent.h"
#include "mozilla/dom/BeforeUnloadEventBinding.h"

namespace mozilla {
namespace dom {

class BeforeUnloadEvent : public nsDOMEvent,
                          public nsIDOMBeforeUnloadEvent
{
public:
  BeforeUnloadEvent(EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    WidgetEvent* aEvent)
    : nsDOMEvent(aOwner, aPresContext, aEvent)
  {
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return BeforeUnloadEventBinding::Wrap(aCx, aScope, this);
  }

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMBeforeUnloadEvent Interface
  NS_DECL_NSIDOMBEFOREUNLOADEVENT

protected:
  nsString mText;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BeforeUnloadEvent_h_
