/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMessageEvent.h"
#include "nsContentUtils.h"
#include "jsapi.h"
#include "nsDOMClassInfoID.h"

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  if (tmp->mDataRooted) {
    tmp->UnrootData();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCI_DATA(MessageEvent, nsDOMMessageEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMMessageEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMessageEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MessageEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMessageEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMessageEvent, nsDOMEvent)

nsDOMMessageEvent::nsDOMMessageEvent(mozilla::dom::EventTarget* aOwner,
                                     nsPresContext* aPresContext,
                                     nsEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent),
    mData(JSVAL_VOID),
    mDataRooted(false)
{
  SetIsDOMBinding();
}

nsDOMMessageEvent::~nsDOMMessageEvent()
{
  if (mDataRooted)
    UnrootData();
}

void
nsDOMMessageEvent::RootData()
{
  NS_ASSERTION(!mDataRooted, "...");
  NS_HOLD_JS_OBJECTS(this, nsDOMMessageEvent);
  mDataRooted = true;
}

void
nsDOMMessageEvent::UnrootData()
{
  NS_ASSERTION(mDataRooted, "...");
  mDataRooted = false;
  mData = JSVAL_VOID;
  NS_DROP_JS_OBJECTS(this, nsDOMMessageEvent);
}

NS_IMETHODIMP
nsDOMMessageEvent::GetData(JSContext* aCx, jsval* aData)
{
  *aData = mData;
  if (!JS_WrapValue(aCx, aData))
    return NS_ERROR_FAILURE;
  return NS_OK;
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
                                    const jsval& aData,
                                    const nsAString& aOrigin,
                                    const nsAString& aLastEventId,
                                    nsIDOMWindow* aSource)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Allowing double-initialization seems a little silly, but we have a test
  // for it so it might be important ...
  if (mDataRooted)
    UnrootData();
  mData = aData;
  RootData();
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
