/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MutationEvent_h_
#define mozilla_dom_MutationEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsINode.h"

namespace mozilla {
namespace dom {

class MutationEvent : public Event
{
public:
  MutationEvent(EventTarget* aOwner,
                nsPresContext* aPresContext,
                InternalMutationEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(MutationEvent, Event)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return MutationEventBinding::Wrap(aCx, this, aGivenProto);
  }

  void GetPrevValue(nsAString& aPrevValue) const;
  void GetNewValue(nsAString& aNewValue) const;
  void GetAttrName(nsAString& aAttrName) const;

  nsINode* GetRelatedNode();

  uint16_t AttrChange();

  void InitMutationEvent(const nsAString& aType,
                         bool aCanBubble, bool aCancelable,
                         nsINode* aRelatedNode,
                         const nsAString& aPrevValue,
                         const nsAString& aNewValue,
                         const nsAString& aAttrName,
                         uint16_t& aAttrChange,
                         ErrorResult& aRv);

protected:
  ~MutationEvent() {}
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::MutationEvent>
NS_NewDOMMutationEvent(mozilla::dom::EventTarget* aOwner,
                       nsPresContext* aPresContext,
                       mozilla::InternalMutationEvent* aEvent);

#endif // mozilla_dom_MutationEvent_h_
