/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ClipboardEvent_h_
#define mozilla_dom_ClipboardEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/ClipboardEventBinding.h"
#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {
class DataTransfer;

class ClipboardEvent : public Event
{
public:
  ClipboardEvent(EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 InternalClipboardEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ClipboardEvent, Event)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return ClipboardEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  static already_AddRefed<ClipboardEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const ClipboardEventInit& aParam,
              ErrorResult& aRv);

  DataTransfer* GetClipboardData();

  void InitClipboardEvent(const nsAString& aType, bool aCanBubble,
                          bool aCancelable,
                          DataTransfer* aClipboardData);

protected:
  ~ClipboardEvent() {}
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::ClipboardEvent>
NS_NewDOMClipboardEvent(mozilla::dom::EventTarget* aOwner,
                        nsPresContext* aPresContext,
                        mozilla::InternalClipboardEvent* aEvent);

#endif // mozilla_dom_ClipboardEvent_h_
