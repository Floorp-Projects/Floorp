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

#include "CConfigActiveScroller.h"
#include "uprefd.h"

CConfigActiveScroller::CConfigActiveScroller(LStream* inStream)
	:	LActiveScroller(inStream)
{
}
	
void CConfigActiveScroller::HandleThumbScroll(LStdControl *inWhichControl)
{
	if (CPrefs::GetBoolean(CPrefs::EnableActiveScrolling))
		LActiveScroller::HandleThumbScroll(inWhichControl);
}

void CConfigActiveScroller::ListenToMessage(MessageT inMessage, void *ioParam)
{
	if (inMessage == msg_ThumbDragged)
		{
		// NOTE: we call the LScroller implemetation if active scrolling
		// is _not_ enabled.  We do not call the LActiveScroller implementation
		// because it ignores thumb dragged messages, assuming that the image is
		// already at the right place after tracking
		if (!CPrefs::GetBoolean(CPrefs::EnableActiveScrolling))
			LScroller::ListenToMessage(inMessage, ioParam);
		}
	else
		LActiveScroller::ListenToMessage(inMessage, ioParam);
}
