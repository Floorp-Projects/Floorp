/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIPrivateTextEvent_h__
#define nsIPrivateTextEvent_h__

#include "nsGUIEvent.h"
#include "nsISupports.h"
#include "nsIPrivateTextRange.h"

#define NS_IPRIVATETEXTEVENT_IID \
{ /* 37B69251-4ACE-11d3-9EA6-0060089FE59B */ \
0x37b69251, 0x4ace, 0x11d3, \
{0x9e, 0xa6, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b} }

class nsIPrivateTextEvent : public nsISupports {

public:
	static const nsIID& GetIID() { static nsIID iid = NS_IPRIVATETEXTEVENT_IID; return iid; }

	NS_IMETHOD GetText(nsString& aText) = 0;
	NS_IMETHOD GetInputRange(nsIPrivateTextRangeList** aInputRange) = 0;
	NS_IMETHOD GetEventReply(struct nsTextEventReply** aReply) = 0;
};

#endif // nsIPrivateTextEvent_h__

