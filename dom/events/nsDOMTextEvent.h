/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMTextEvent_h__
#define nsDOMTextEvent_h__

#include "mozilla/dom/UIEvent.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsIPrivateTextEvent.h"
#include "nsPrivateTextRange.h"

class nsDOMTextEvent : public mozilla::dom::UIEvent,
                       public nsIPrivateTextEvent
{
  typedef mozilla::dom::UIEvent UIEvent;

public:
  nsDOMTextEvent(mozilla::dom::EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 mozilla::WidgetTextEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to base class
  NS_FORWARD_TO_UIEVENT

  // nsIPrivateTextEvent interface
  NS_IMETHOD GetText(nsString& aText) MOZ_OVERRIDE;
  NS_IMETHOD_(already_AddRefed<nsIPrivateTextRangeList>) GetInputRange() MOZ_OVERRIDE;
  
protected:
  nsString mText;
  nsRefPtr<nsPrivateTextRangeList> mTextRange;
};

#endif // nsDOMTextEvent_h__
