/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"

#include "nsDOMScrollAreaEvent.h"
#include "nsGUIEvent.h"
#include "nsClientRect.h"

nsDOMScrollAreaEvent::nsDOMScrollAreaEvent(mozilla::dom::EventTarget* aOwner,
                                           nsPresContext *aPresContext,
                                           nsScrollAreaEvent *aEvent)
  : nsDOMUIEvent(aOwner, aPresContext, aEvent)
  , mClientArea(nullptr)
{
  mClientArea.SetLayoutRect(aEvent ? aEvent->mArea : nsRect());
}

nsDOMScrollAreaEvent::~nsDOMScrollAreaEvent()
{
  if (mEventIsInternal && mEvent) {
    if (mEvent->eventStructType == NS_SCROLLAREA_EVENT) {
      delete static_cast<nsScrollAreaEvent *>(mEvent);
      mEvent = nullptr;
    }
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMScrollAreaEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMScrollAreaEvent, nsDOMUIEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMScrollAreaEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMScrollAreaEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)


#define FORWARD_GETTER(_name)                                                   \
  NS_IMETHODIMP                                                                 \
  nsDOMScrollAreaEvent::Get ## _name(float* aResult)                            \
  {                                                                             \
    *aResult = _name();                                                         \
    return NS_OK;                                                               \
  }

FORWARD_GETTER(X)
FORWARD_GETTER(Y)
FORWARD_GETTER(Width)
FORWARD_GETTER(Height)

NS_IMETHODIMP
nsDOMScrollAreaEvent::InitScrollAreaEvent(const nsAString &aEventType,
                                          bool aCanBubble,
                                          bool aCancelable,
                                          nsIDOMWindow *aView,
                                          int32_t aDetail,
                                          float aX, float aY,
                                          float aWidth, float aHeight)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aEventType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  mClientArea.SetRect(aX, aY, aWidth, aHeight);

  return NS_OK;
}

NS_IMETHODIMP_(void)
nsDOMScrollAreaEvent::Serialize(IPC::Message* aMsg,
                                bool aSerializeInterfaceType)
{
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aMsg, NS_LITERAL_STRING("scrollareaevent"));
  }

  nsDOMEvent::Serialize(aMsg, false);

  IPC::WriteParam(aMsg, X());
  IPC::WriteParam(aMsg, Y());
  IPC::WriteParam(aMsg, Width());
  IPC::WriteParam(aMsg, Height());
}

NS_IMETHODIMP_(bool)
nsDOMScrollAreaEvent::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  NS_ENSURE_TRUE(nsDOMEvent::Deserialize(aMsg, aIter), false);

  float x, y, width, height;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &x), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &y), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &width), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &height), false);
  mClientArea.SetRect(x, y, width, height);

  return true;
}

nsresult
NS_NewDOMScrollAreaEvent(nsIDOMEvent **aInstancePtrResult,
                         mozilla::dom::EventTarget* aOwner,
                         nsPresContext *aPresContext,
                         nsScrollAreaEvent *aEvent)
{
  nsDOMScrollAreaEvent* ev =
    new nsDOMScrollAreaEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(ev, aInstancePtrResult);
}
