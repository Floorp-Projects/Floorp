/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// CSecureAttachment.cp

#pragma once

#include "CSecureAttachment.h"
#include "xp_core.h"
#include "secnav.h"
#include "secrng.h"

CSecureAttachment::CSecureAttachment()
	:	LAttachment(msg_Event)
{
	mTickCounter = 0;
}


void CSecureAttachment::ExecuteSelf(
	MessageT		inMessage,
	void			*ioParam)
{
	if (mTickCounter < 500)
		{
		EventRecord* theEvent = (EventRecord*)ioParam;
		RNG_RandomUpdate(theEvent, sizeof(EventRecord));
		long ticks = ::TickCount();
		RNG_RandomUpdate(&ticks, sizeof(long));
		mTickCounter++;
		}
	else
		delete this;
}

