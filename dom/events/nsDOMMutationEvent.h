/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMutationEvent_h__
#define nsDOMMutationEvent_h__

#include "nsIDOMMutationEvent.h"
#include "nsINode.h"
#include "nsDOMEvent.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/EventForwards.h"

class nsDOMMutationEvent : public nsDOMEvent,
                           public nsIDOMMutationEvent
{
public:
  nsDOMMutationEvent(mozilla::dom::EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     mozilla::InternalMutationEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMMUTATIONEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::MutationEventBinding::Wrap(aCx, aScope, this);
  }

  // xpidl implementation
  // GetPrevValue(nsAString& aPrevValue);
  // GetNewValue(nsAString& aNewValue);
  // GetAttrName(nsAString& aAttrName);

  already_AddRefed<nsINode> GetRelatedNode();

  uint16_t AttrChange();

  void InitMutationEvent(const nsAString& aType,
                         bool& aCanBubble, bool& aCancelable,
                         nsINode* aRelatedNode,
                         const nsAString& aPrevValue,
                         const nsAString& aNewValue,
                         const nsAString& aAttrName,
                         uint16_t& aAttrChange, mozilla::ErrorResult& aRv)
  {
    aRv = InitMutationEvent(aType, aCanBubble, aCancelable,
                            aRelatedNode ? aRelatedNode->AsDOMNode() : nullptr,
                            aPrevValue, aNewValue, aAttrName, aAttrChange);
  }
};

#endif // nsDOMMutationEvent_h__
