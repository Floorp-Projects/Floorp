/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/ScrollAreaEvent.h"
#include "mozilla/ContentEvents.h"

namespace mozilla::dom {

ScrollAreaEvent::ScrollAreaEvent(EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 InternalScrollAreaEvent* aEvent)
    : UIEvent(aOwner, aPresContext, aEvent), mClientArea(new DOMRect(nullptr)) {
  mClientArea->SetLayoutRect(aEvent ? aEvent->mArea : nsRect());
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ScrollAreaEvent, UIEvent, mClientArea)

NS_IMPL_ADDREF_INHERITED(ScrollAreaEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(ScrollAreaEvent, UIEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScrollAreaEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

void ScrollAreaEvent::InitScrollAreaEvent(const nsAString& aEventType,
                                          bool aCanBubble, bool aCancelable,
                                          nsGlobalWindowInner* aView,
                                          int32_t aDetail, float aX, float aY,
                                          float aWidth, float aHeight) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  UIEvent::InitUIEvent(aEventType, aCanBubble, aCancelable, aView, aDetail);
  mClientArea->SetRect(aX, aY, aWidth, aHeight);
}

void ScrollAreaEvent::Serialize(IPC::MessageWriter* aWriter,
                                bool aSerializeInterfaceType) {
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aWriter, u"scrollareaevent"_ns);
  }

  Event::Serialize(aWriter, false);

  IPC::WriteParam(aWriter, X());
  IPC::WriteParam(aWriter, Y());
  IPC::WriteParam(aWriter, Width());
  IPC::WriteParam(aWriter, Height());
}

bool ScrollAreaEvent::Deserialize(IPC::MessageReader* aReader) {
  NS_ENSURE_TRUE(Event::Deserialize(aReader), false);

  float x, y, width, height;
  NS_ENSURE_TRUE(IPC::ReadParam(aReader, &x), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aReader, &y), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aReader, &width), false);
  NS_ENSURE_TRUE(IPC::ReadParam(aReader, &height), false);
  mClientArea->SetRect(x, y, width, height);

  return true;
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<ScrollAreaEvent> NS_NewDOMScrollAreaEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    InternalScrollAreaEvent* aEvent) {
  RefPtr<ScrollAreaEvent> ev =
      new ScrollAreaEvent(aOwner, aPresContext, aEvent);
  return ev.forget();
}
