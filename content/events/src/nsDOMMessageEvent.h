/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMessageEvent_h__
#define nsDOMMessageEvent_h__

#include "nsIDOMMessageEvent.h"
#include "nsDOMEvent.h"
#include "nsCycleCollectionParticipant.h"
#include "jsapi.h"
#include "mozilla/dom/MessageEventBinding.h"

/**
 * Implements the MessageEvent event, used for cross-document messaging and
 * server-sent events.
 *
 * See http://www.whatwg.org/specs/web-apps/current-work/#messageevent for
 * further details.
 */
class nsDOMMessageEvent : public nsDOMEvent,
                          public nsIDOMMessageEvent
{
public:
  nsDOMMessageEvent(mozilla::dom::EventTarget* aOwner,
                    nsPresContext* aPresContext, nsEvent* aEvent);
  ~nsDOMMessageEvent();
                     
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsDOMMessageEvent,
                                                         nsDOMEvent)

  NS_DECL_NSIDOMMESSAGEEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope)
  {
    return mozilla::dom::MessageEventBinding::Wrap(aCx, aScope, this);
  }

  JS::Value GetData(JSContext* aCx, mozilla::ErrorResult& aRv)
  {
    JS::Value data;
    aRv = GetData(aCx, &data);
    return data;
  }

  already_AddRefed<nsIDOMWindow> GetSource()
  {
    nsCOMPtr<nsIDOMWindow> ret = mSource;
    return ret.forget();
  }

  void InitMessageEvent(JSContext* aCx,
                        const nsAString& aType,
                        bool aCanBubble,
                        bool aCancelable,
                        JS::Value& aData,
                        const nsAString& aOrigin,
                        const nsAString& aLastEventId,
                        nsIDOMWindow* aSource,
                        mozilla::ErrorResult& aRv)
  {
    aRv = InitMessageEvent(aType, aCanBubble, aCancelable, aData,
                           aOrigin, aLastEventId, aSource);
  }

private:
  JS::Value mData;
  nsString mOrigin;
  nsString mLastEventId;
  nsCOMPtr<nsIDOMWindow> mSource;
};

#endif // nsDOMMessageEvent_h__
