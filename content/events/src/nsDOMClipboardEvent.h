/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMClipboardEvent_h_
#define nsDOMClipboardEvent_h_

#include "nsIDOMClipboardEvent.h"
#include "nsDOMEvent.h"
#include "mozilla/dom/ClipboardEventBinding.h"

class nsDOMClipboardEvent : public nsDOMEvent,
                            public nsIDOMClipboardEvent
{
public:
  nsDOMClipboardEvent(mozilla::dom::EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      nsClipboardEvent* aEvent);
  virtual ~nsDOMClipboardEvent();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMCLIPBOARDEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  nsresult InitFromCtor(const nsAString& aType,
                        JSContext* aCx, jsval* aVal);

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope)
  {
    return mozilla::dom::ClipboardEventBinding::Wrap(aCx, aScope, this);
  }

  static already_AddRefed<nsDOMClipboardEvent>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              const nsAString& aType,
              const mozilla::dom::ClipboardEventInit& aParam,
              mozilla::ErrorResult& aRv);

  nsIDOMDataTransfer* GetClipboardData();
};

#endif // nsDOMClipboardEvent_h_
