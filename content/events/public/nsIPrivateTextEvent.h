/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIPrivateTextEvent_h__
#define nsIPrivateTextEvent_h__

#include "nsEvent.h"
#include "nsISupports.h"
#include "nsIPrivateTextRange.h"

#define NS_IPRIVATETEXTEVENT_IID \
{ 0xb6840e02, 0x9e56, 0x49d8, \
  { 0x84, 0xd, 0x5f, 0xc1, 0xcb, 0x6c, 0xff, 0xb3 } }

class nsIPrivateTextEvent : public nsISupports {

public:
	NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATETEXTEVENT_IID)

	NS_IMETHOD GetText(nsString& aText) = 0;
	NS_IMETHOD_(already_AddRefed<nsIPrivateTextRangeList>) GetInputRange() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrivateTextEvent, NS_IPRIVATETEXTEVENT_IID)

#endif // nsIPrivateTextEvent_h__

