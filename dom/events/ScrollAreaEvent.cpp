/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/ScrollAreaEvent.h"
#include "mozilla/ContentEvents.h"

namespace mozilla {
namespace dom {

ScrollAreaEvent::ScrollAreaEvent(EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 InternalScrollAreaEvent* aEvent)
  : UIEvent(aOwner, aPresContext, aEvent)
  , mClientArea(nullptr)
{
  mClientArea.SetLayoutRect(aEvent ? aEvent->mArea : nsRect());
}

NS_IMPL_ADDREF_INHERITED(ScrollAreaEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(ScrollAreaEvent, UIEvent)

NS_INTERFACE_MAP_BEGIN(ScrollAreaEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMScrollAreaEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)


#define FORWARD_GETTER(_name)                                                  \
  NS_IMETHODIMP                                                                \
  ScrollAreaEvent::Get ## _name(float* aResult)                                \
  {                                                                            \
    *aResult = _name();                                                        \
    return NS_OK;                                                              \
  }

FORWARD_GETTER(X)
FORWARD_GETTER(Y)
FORWARD_GETTER(Width)
FORWARD_GETTER(Height)

NS_IMETHODIMP
ScrollAreaEvent::InitScrollAreaEvent(const nsAString& aEventType,
                                     bool aCanBubble,
                                     bool aCancelable,
                                     nsIDOMWindow* aView,
                                     int32_t aDetail,
                                     float aX,
                                     float aY,
                                     float aWidth,
                                     float aHeight)
{
  nsresult rv =
    UIEvent::InitUIEvent(aEventType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  mClientArea.SetRect(aX, aY, aWidth, aHeight);

  return NS_OK;
}

NS_IMETHODIMP_(void)
ScrollAreaEvent::Serialize(IPC::Message* aMsg,
                           bool aSerializeInterfaceType)
{
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aMsg, NS_LITERAL_STRING("scrollareaevent"));
  }

  Event::Serialize(aMsg, false);

  IPC::WriteParam(aMsg, X());
  IPC::WriteParam(aMsg, Y());
  IPC::WriteParam(aMsg, Width());
  IPC::WriteParam(aMsg, Height());
}

NS_IMETHODIMP_(bool)
ScrollAreaEvent::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  NS_ENSURE_TRUE(Event::Deserialize(aMsg, aIter), false);

  float x, y, width, height;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &x), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &y), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &width), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &height), false);
  mClientArea.SetRect(x, y, width, height);

  return true;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMScrollAreaEvent(nsIDOMEvent** aInstancePtrResult,
                         EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         InternalScrollAreaEvent* aEvent)
{
  ScrollAreaEvent* ev = new ScrollAreaEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(ev, aInstancePtrResult);
}
