/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*-----------------------------------------------------------------------------
	Position String Formatting
	%ns		argument n is a string
	%ni		argument n is an integer
	%%		literal %
	
	n must be from 1 to 9
	
	XP_MessageLen returns the length of the formatted message, including 
	the terminating NULL. 
	
	XP_Message formats the message into the given buffer, not exceeding
	bufferLen (which includes the terminating NULL). If there isn't enough 
	space, XP_Message will truncate the text and terminate it (unlike
	strncpy, which will truncate but not terminate).
	
	XP_StaticMessage is like XP_Message but maintains a private buffer 
	which it resizes as necessary.
-----------------------------------------------------------------------------*/

#ifndef _XP_Message_
#define _XP_Message_

#include "xp_core.h"

XP_BEGIN_PROTOS

int
XP_MessageLen (const char * format, ...);

void
XP_Message (char * buffer, int bufferLen, const char * format, ...);

const char *
XP_StaticMessage (const char * format, ...);

XP_END_PROTOS

#endif /* _XP_Message_ */

