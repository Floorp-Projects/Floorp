/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMessageEvent.h"
#include "nsContentUtils.h" // for NS_HOLD_JS_OBJECTS, NS_DROP_JS_OBJECTS
#include "jsapi.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  tmp->mData = JSVAL_VOID;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMMessageEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMessageEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMessageEvent, nsDOMEvent)

nsDOMMessageEvent::nsDOMMessageEvent(mozilla::dom::EventTarget* aOwner,
                                     nsPresContext* aPresContext,
                                     nsEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent),
    mData(JSVAL_VOID)
{
}

nsDOMMessageEvent::~nsDOMMessageEvent()
{
  mData = JSVAL_VOID;
  NS_DROP_JS_OBJECTS(this, nsDOMMessageEvent);
}

NS_IMETHODIMP
nsDOMMessageEvent::GetData(JSContext* aCx, JS::Value* aData)
{
  ErrorResult rv;
  *aData = GetData(aCx, rv);
  return rv.ErrorCode();
}

JS::Value
nsDOMMessageEvent::GetData(JSContext* aCx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> data(aCx, mData);
  if (!JS_WrapValue(aCx, data.address())) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
  return data;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetOrigin(nsAString& aOrigin)
{
  aOrigin = mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetLastEventId(nsAString& aLastEventId)
{
  aLastEventId = mLastEventId;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetSource(nsIDOMWindow** aSource)
{
  NS_IF_ADDREF(*aSource = mSource);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::InitMessageEvent(const nsAString& aType,
                                    bool aCanBubble,
                                    bool aCancelable,
                                    const JS::Value& aData,
                                    const nsAString& aOrigin,
                                    const nsAString& aLastEventId,
                                    nsIDOMWindow* aSource)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mData = aData;
  NS_HOLD_JS_OBJECTS(this, nsDOMMessageEvent);
  mOrigin = aOrigin;
  mLastEventId = aLastEventId;
  mSource = aSource;

  return NS_OK;
}

nsresult
NS_NewDOMMessageEvent(nsIDOMEvent** aInstancePtrResult,
                      mozilla::dom::EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      nsEvent* aEvent) 
{
  nsDOMMessageEvent* it = new nsDOMMessageEvent(aOwner, aPresContext, aEvent);

  return CallQueryInterface(it, aInstancePtrResult);
}
