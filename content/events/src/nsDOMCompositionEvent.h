/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMCompositionEvent_h__
#define nsDOMCompositionEvent_h__

#include "nsDOMUIEvent.h"
#include "nsIDOMCompositionEvent.h"

class nsDOMCompositionEvent : public nsDOMUIEvent,
                              public nsIDOMCompositionEvent
{
public:
  nsDOMCompositionEvent(nsPresContext* aPresContext,
                        nsCompositionEvent* aEvent);
  virtual ~nsDOMCompositionEvent();

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMUIEVENT
  NS_DECL_NSIDOMCOMPOSITIONEVENT

protected:
  nsString mData;
  nsString mLocale;
};

#endif // nsDOMCompositionEvent_h__
