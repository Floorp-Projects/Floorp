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

#pragma once
#include <stdio.h>
#include <Types.h>
#include "poof.h"

/*-----------------------------------------------------------------------------
	xp_MacToUnixErr
	Module function. Maps Mac system errors to unix-stype errno errors.
-----------------------------------------------------------------------------*/

extern int xp_MacToUnixErr (OSErr err);

/*-----------------------------------------------------------------------------
	Debuging Setup
-----------------------------------------------------------------------------*/

/*
#ifdef DEBUG

class DebugMgr: public Manager {
public:
					DebugMgr ();
					~DebugMgr ();
	void			Inspect (OSType item, int value);
			// Delegation
	Boolean 		ObeyCommand (CommandT command, void *param);
	void 			FindCommandStatus (CommandT command, Boolean &enabled, 
						Boolean &usesMark, Char16 &mark, Str255 name);
	void			ExecuteSelf (MessageT message, void *ioParam);
			// Printing
	void 			Trace (char *message, int length);
private:	// Module
	void			LoadState ();
	void			SaveState ();
	void 			EnterSelf ();
	void 			ExitSelf ();
public:
	FILE *			fTraceFile;
private:
	LWindow			*fInspector;
	OSErr			fTraceLog;
};

extern DebugMgr TheDebugMgr;

#endif // DEBUG
*/

