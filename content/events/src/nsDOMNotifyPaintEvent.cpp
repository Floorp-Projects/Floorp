/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"
#include "nsDOMNotifyPaintEvent.h"
#include "nsContentUtils.h"
#include "nsClientRect.h"
#include "nsPaintRequest.h"
#include "nsIFrame.h"
#include "nsDOMClassInfoID.h"

nsDOMNotifyPaintEvent::nsDOMNotifyPaintEvent(nsPresContext* aPresContext,
                                             nsEvent* aEvent,
                                             uint32_t aEventType,
                                             nsInvalidateRequestList* aInvalidateRequests)
: nsDOMEvent(aPresContext, aEvent)
{
  if (mEvent) {
    mEvent->message = aEventType;
  }
  if (aInvalidateRequests) {
    mInvalidateRequests.MoveElementsFrom(aInvalidateRequests->mRequests);
  }
}

DOMCI_DATA(NotifyPaintEvent, nsDOMNotifyPaintEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMNotifyPaintEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNotifyPaintEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(NotifyPaintEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMNotifyPaintEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMNotifyPaintEvent, nsDOMEvent)

nsRegion
nsDOMNotifyPaintEvent::GetRegion()
{
  nsRegion r;
  if (!nsContentUtils::IsCallerChrome()) {
    return r;
  }
  for (uint32_t i = 0; i < mInvalidateRequests.Length(); ++i) {
    r.Or(r, mInvalidateRequests[i].mRect);
    r.SimplifyOutward(10);
  }
  return r;
}

NS_IMETHODIMP
nsDOMNotifyPaintEvent::GetBoundingClientRect(nsIDOMClientRect** aResult)
{
  // Weak ref, since we addref it below
  nsClientRect* rect = new nsClientRect();
  if (!rect)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult = rect);
  if (!mPresContext)
    return NS_OK;

  rect->SetLayoutRect(GetRegion().GetBounds());
  return NS_OK;
}

NS_IMETHODIMP
nsDOMNotifyPaintEvent::GetClientRects(nsIDOMClientRectList** aResult)
{
  nsRefPtr<nsClientRectList> rectList =
    new nsClientRectList(static_cast<nsIDOMEvent*>(static_cast<nsDOMEvent*>(this)));
  if (!rectList)
    return NS_ERROR_OUT_OF_MEMORY;

  nsRegion r = GetRegion();
  nsRegionRectIterator iter(r);
  for (const nsRect* rgnRect = iter.Next(); rgnRect; rgnRect = iter.Next()) {
    nsRefPtr<nsClientRect> rect = new nsClientRect();
    if (!rect)
      return NS_ERROR_OUT_OF_MEMORY;
    
    rect->SetLayoutRect(*rgnRect);
    rectList->Append(rect);
  }

  rectList.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMNotifyPaintEvent::GetPaintRequests(nsIDOMPaintRequestList** aResult)
{
  nsRefPtr<nsPaintRequestList> requests = PaintRequests();
  requests.forget(aResult);
  return NS_OK;
}

already_AddRefed<nsPaintRequestList>
nsDOMNotifyPaintEvent::PaintRequests()
{
  nsDOMEvent* parent = this;
  nsRefPtr<nsPaintRequestList> requests = new nsPaintRequestList(parent);

  if (nsContentUtils::IsCallerChrome()) {
    for (uint32_t i = 0; i < mInvalidateRequests.Length(); ++i) {
      nsRefPtr<nsPaintRequest> r = new nsPaintRequest(parent);
      r->SetRequest(mInvalidateRequests[i]);
      requests->Append(r);
    }
  }

  return requests.forget();
}

NS_IMETHODIMP_(void)
nsDOMNotifyPaintEvent::Serialize(IPC::Message* aMsg,
                                 bool aSerializeInterfaceType)
{
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aMsg, NS_LITERAL_STRING("notifypaintevent"));
  }

  nsDOMEvent::Serialize(aMsg, false);

  uint32_t length = mInvalidateRequests.Length();
  IPC::WriteParam(aMsg, length);
  for (uint32_t i = 0; i < length; ++i) {
    IPC::WriteParam(aMsg, mInvalidateRequests[i].mRect.x);
    IPC::WriteParam(aMsg, mInvalidateRequests[i].mRect.y);
    IPC::WriteParam(aMsg, mInvalidateRequests[i].mRect.width);
    IPC::WriteParam(aMsg, mInvalidateRequests[i].mRect.height);
    IPC::WriteParam(aMsg, mInvalidateRequests[i].mFlags);
  }
}

NS_IMETHODIMP_(bool)
nsDOMNotifyPaintEvent::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  NS_ENSURE_TRUE(nsDOMEvent::Deserialize(aMsg, aIter), false);

  uint32_t length = 0;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &length), false);
  mInvalidateRequests.SetCapacity(length);
  for (uint32_t i = 0; i < length; ++i) {
    nsInvalidateRequestList::Request req;
    NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &req.mRect.x), false);
    NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &req.mRect.y), false);
    NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &req.mRect.width), false);
    NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &req.mRect.height), false);
    NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &req.mFlags), false);
    mInvalidateRequests.AppendElement(req);
  }

  return true;
}

nsresult NS_NewDOMNotifyPaintEvent(nsIDOMEvent** aInstancePtrResult,
                                   nsPresContext* aPresContext,
                                   nsEvent *aEvent,
                                   uint32_t aEventType,
                                   nsInvalidateRequestList* aInvalidateRequests) 
{
  nsDOMNotifyPaintEvent* it =
    new nsDOMNotifyPaintEvent(aPresContext, aEvent, aEventType,
                              aInvalidateRequests);
  if (nullptr == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
