/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/NotifyPaintEvent.h"
#include "mozilla/dom/PaintRequest.h"
#include "mozilla/GfxMessageUtils.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

NotifyPaintEvent::NotifyPaintEvent(EventTarget* aOwner,
                                   nsPresContext* aPresContext,
                                   WidgetEvent* aEvent,
                                   EventMessage aEventMessage,
                                   nsTArray<nsRect>* aInvalidateRequests,
                                   uint64_t aTransactionId,
                                   DOMHighResTimeStamp aTimeStamp)
  : Event(aOwner, aPresContext, aEvent)
{
  if (mEvent) {
    mEvent->mMessage = aEventMessage;
  }
  if (aInvalidateRequests) {
    mInvalidateRequests.SwapElements(*aInvalidateRequests);
  }

  mTransactionId = aTransactionId;
  mTimeStamp = aTimeStamp;
}

nsRegion
NotifyPaintEvent::GetRegion(SystemCallerGuarantee)
{
  nsRegion r;
  for (uint32_t i = 0; i < mInvalidateRequests.Length(); ++i) {
    r.Or(r, mInvalidateRequests[i]);
    r.SimplifyOutward(10);
  }
  return r;
}

already_AddRefed<DOMRect>
NotifyPaintEvent::BoundingClientRect(SystemCallerGuarantee aGuarantee)
{
  RefPtr<DOMRect> rect = new DOMRect(ToSupports(this));

  if (mPresContext) {
    rect->SetLayoutRect(GetRegion(aGuarantee).GetBounds());
  }

  return rect.forget();
}

already_AddRefed<DOMRectList>
NotifyPaintEvent::ClientRects(SystemCallerGuarantee aGuarantee)
{
  nsISupports* parent = ToSupports(this);
  RefPtr<DOMRectList> rectList = new DOMRectList(parent);

  nsRegion r = GetRegion(aGuarantee);
  for (auto iter = r.RectIter(); !iter.Done(); iter.Next()) {
    RefPtr<DOMRect> rect = new DOMRect(parent);
    rect->SetLayoutRect(iter.Get());
    rectList->Append(rect);
  }

  return rectList.forget();
}

already_AddRefed<PaintRequestList>
NotifyPaintEvent::PaintRequests(SystemCallerGuarantee)
{
  Event* parent = this;
  RefPtr<PaintRequestList> requests = new PaintRequestList(parent);

  for (uint32_t i = 0; i < mInvalidateRequests.Length(); ++i) {
    RefPtr<PaintRequest> r = new PaintRequest(parent);
    r->SetRequest(mInvalidateRequests[i]);
    requests->Append(r);
  }

  return requests.forget();
}

void
NotifyPaintEvent::Serialize(IPC::Message* aMsg,
                            bool aSerializeInterfaceType)
{
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aMsg, NS_LITERAL_STRING("notifypaintevent"));
  }

  Event::Serialize(aMsg, false);

  uint32_t length = mInvalidateRequests.Length();
  IPC::WriteParam(aMsg, length);
  for (uint32_t i = 0; i < length; ++i) {
    IPC::WriteParam(aMsg, mInvalidateRequests[i]);
  }
}

bool
NotifyPaintEvent::Deserialize(const IPC::Message* aMsg, PickleIterator* aIter)
{
  NS_ENSURE_TRUE(Event::Deserialize(aMsg, aIter), false);

  uint32_t length = 0;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &length), false);
  mInvalidateRequests.SetCapacity(length);
  for (uint32_t i = 0; i < length; ++i) {
    nsRect req;
    NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &req), false);
    mInvalidateRequests.AppendElement(req);
  }

  return true;
}

uint64_t
NotifyPaintEvent::TransactionId(SystemCallerGuarantee)
{
  return mTransactionId;
}

DOMHighResTimeStamp
NotifyPaintEvent::PaintTimeStamp(SystemCallerGuarantee)
{
  return mTimeStamp;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<NotifyPaintEvent>
NS_NewDOMNotifyPaintEvent(EventTarget* aOwner,
                          nsPresContext* aPresContext,
                          WidgetEvent* aEvent,
                          EventMessage aEventMessage,
                          nsTArray<nsRect>* aInvalidateRequests,
                          uint64_t aTransactionId,
                          DOMHighResTimeStamp aTimeStamp)
{
  RefPtr<NotifyPaintEvent> it =
    new NotifyPaintEvent(aOwner, aPresContext, aEvent, aEventMessage,
                         aInvalidateRequests, aTransactionId, aTimeStamp);
  return it.forget();
}
