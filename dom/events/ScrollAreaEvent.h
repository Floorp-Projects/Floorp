/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScrollAreaEvent_h_
#define mozilla_dom_ScrollAreaEvent_h_

#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/ScrollAreaEventBinding.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace dom {

class ScrollAreaEvent : public UIEvent
{
public:
  ScrollAreaEvent(EventTarget* aOwner,
                  nsPresContext* aPresContext,
                  InternalScrollAreaEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ScrollAreaEvent, UIEvent)

  void Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) override;
  bool Deserialize(const IPC::Message* aMsg, PickleIterator* aIter) override;

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return ScrollAreaEventBinding::Wrap(aCx, this, aGivenProto);
  }

  float X() const
  {
    return mClientArea->Left();
  }

  float Y() const
  {
    return mClientArea->Top();
  }

  float Width() const
  {
    return mClientArea->Width();
  }

  float Height() const
  {
    return mClientArea->Height();
  }

  void InitScrollAreaEvent(const nsAString& aType,
                           bool aCanBubble,
                           bool aCancelable,
                           nsGlobalWindowInner* aView,
                           int32_t aDetail,
                           float aX, float aY,
                           float aWidth, float aHeight);

protected:
  ~ScrollAreaEvent() {}

  RefPtr<DOMRect> mClientArea;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::ScrollAreaEvent>
NS_NewDOMScrollAreaEvent(mozilla::dom::EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         mozilla::InternalScrollAreaEvent* aEvent);

#endif // mozilla_dom_ScrollAreaEvent_h_
