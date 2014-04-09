/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DragEvent_h_
#define mozilla_dom_DragEvent_h_

#include "nsIDOMDragEvent.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/DragEventBinding.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace dom {

class DataTransfer;

class DragEvent : public MouseEvent,
                  public nsIDOMDragEvent
{
public:
  DragEvent(EventTarget* aOwner,
            nsPresContext* aPresContext,
            WidgetDragEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMDRAGEVENT

  NS_FORWARD_TO_MOUSEEVENT

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return DragEventBinding::Wrap(aCx, this);
  }

  DataTransfer* GetDataTransfer();

  void InitDragEvent(const nsAString& aType,
                     bool aCanBubble, bool aCancelable,
                     nsIDOMWindow* aView, int32_t aDetail,
                     int32_t aScreenX, int32_t aScreenY,
                     int32_t aClientX, int32_t aClientY,
                     bool aCtrlKey, bool aAltKey, bool aShiftKey,
                     bool aMetaKey, uint16_t aButton,
                     EventTarget* aRelatedTarget,
                     DataTransfer* aDataTransfer,
                     ErrorResult& aError);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DragEvent_h_
