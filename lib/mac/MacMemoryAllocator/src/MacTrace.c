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

/* 
 * we can't ship the Library Manager, so only turn it on if people have the right libs,
 * etc. To turn this back on, define DEBUG and set MAC_USE_LIBRARY_MANAGER to 1. Also,
 * you need to add the LibraryManagerPPC.o file to the MemAllocator.mcp project so
 * it will link.
 */
#define MAC_USE_LIBRARY_MANAGER 0

#if MAC_USE_LIBRARY_MANAGER
	#include "LibraryManager.h"
	#include "LibraryManagerUtilities.h"
#endif

#include "xp_trace.h"

void FE_Trace(const char *message) {
#ifdef DEBUG
	#if MAC_USE_LIBRARY_MANAGER
								// This whole routine should be ifdef’ed out, but 
								// we don't know who depends on it & it’s too late 
								// for that foolishness in 4.x.  bjones
		Trace("%s", message);
	#else
		#pragma unused(message)
	#endif
#else
#pragma unused(message)
#endif
}

void XP_TraceInit(void) {
#ifdef DEBUG
#if MAC_USE_LIBRARY_MANAGER
	// Needed by ASLM.  Will be called during startup.  Must be supplied, see LibraryMangaerUtilities.h
	OSErr	err;

	err = InitLibraryManager(0, kTempZone, kNormalMemory);
	if (err == noErr) {
		Trace("Here's XP_Trace!\n\n");
	}
#endif
#endif
}

