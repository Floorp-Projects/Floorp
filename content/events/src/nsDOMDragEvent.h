/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDragEvent_h__
#define nsDOMDragEvent_h__

#include "nsIDOMDragEvent.h"
#include "nsDOMMouseEvent.h"
#include "mozilla/dom/DragEventBinding.h"

class nsEvent;

class nsDOMDragEvent : public nsDOMMouseEvent,
                       public nsIDOMDragEvent
{
public:
  nsDOMDragEvent(mozilla::dom::EventTarget* aOwner,
                 nsPresContext* aPresContext, nsInputEvent* aEvent);
  virtual ~nsDOMDragEvent();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMDRAGEVENT
  
  NS_FORWARD_TO_NSDOMMOUSEEVENT

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope)
  {
    return mozilla::dom::DragEventBinding::Wrap(aCx, aScope, this);
  }

  nsIDOMDataTransfer* GetDataTransfer();

  void InitDragEvent(const nsAString& aType,
                     bool aCanBubble, bool aCancelable,
                     nsIDOMWindow* aView, int32_t aDetail,
                     int32_t aScreenX, int32_t aScreenY,
                     int32_t aClientX, int32_t aClientY, 
                     bool aCtrlKey, bool aAltKey, bool aShiftKey,
                     bool aMetaKey, uint16_t aButton,
                     mozilla::dom::EventTarget* aRelatedTarget,
                     nsIDOMDataTransfer* aDataTransfer,
                     mozilla::ErrorResult& aRv)
  {
    aRv = InitDragEvent(aType, aCanBubble, aCancelable,
                        aView, aDetail, aScreenX, aScreenY, aClientX, aClientY,
                        aCtrlKey, aAltKey, aShiftKey, aMetaKey, aButton,
                        aRelatedTarget, aDataTransfer);
  }
};

nsresult NS_NewDOMDragEvent(nsIDOMEvent** aInstancePtrResult,
                            mozilla::dom::EventTarget* aOwner,
                            nsPresContext* aPresContext,
                            nsDragEvent* aEvent);

#endif // nsDOMDragEvent_h__
