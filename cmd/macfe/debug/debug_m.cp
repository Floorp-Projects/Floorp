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

#include <errno.h>

#include <Errors.h>

#include "xp.h"
#include "xp_mem.h"
#include "xpassert.h"
//#include "sys/errno.h"

#include "xp_mac.h"
#include "xp_debug.h"

#include <LHandleStream.h>
#include <LCaption.h>
#include <UDrawingState.h>

/*-----------------------------------------------------------------------------
	xp_MacToUnixErr
	Maps Mac system errors to unix-stype errno errors.
-----------------------------------------------------------------------------*/

int xp_MacToUnixErr (OSErr err)
{
	if (!err)
		return 0;
	
	switch (err) {
		case noErr: return 0;
		case memLockedErr: return -1;
		/* some error messages have no standard counterpart; how to deal? */
		case resNotFound:
			XP_Abort ("Macintosh-specific error, should not be mapped to unix.");
		case rfNumErr:
			return EBADF;
	};
#ifdef DEBUG
	XP_Abort ("Can't figure out what to return");
#endif
	return -1; 
}

/*-----------------------------------------------------------------------------
	Global Debug Variables
-----------------------------------------------------------------------------*/

#ifdef DEBUG

int Debug = 1;

#endif /* DEBUG */

