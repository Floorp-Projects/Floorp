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

#ifndef nsIPrivateCompositionEvent_h__
#define nsIPrivateCompositionEvent_h__

#include "nsGUIEvent.h"
#include "nsISupports.h"

// {ECF6BEF1-5F0C-11d3-9EB3-0060089FE59B}
#define NS_IPRIVATECOMPOSITIONEVENT_IID	\
{ 0xecf6bef1, 0x5f0c, 0x11d3, \
{ 0x9e, 0xb3, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}

class nsIPrivateCompositionEvent : public nsISupports {

public:
	static const nsIID& GetIID() { static nsIID iid = NS_IPRIVATECOMPOSITIONEVENT_IID; return iid; }

	NS_IMETHOD GetCompositionReply(struct nsTextEventReply** aReply) = 0;
};

#endif // nsIPrivateCompositionEvent_h__

