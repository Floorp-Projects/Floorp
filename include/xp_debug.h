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
	XPUtil.h
	Cross-Platform Debugging
	
	These routines are NOT for handling expected error conditions! They are
	for detecting *program logic* errors. Error conditions (such as running
	out of memory) cannot be predicted at compile-time and must be handled
	gracefully at run-time.
-----------------------------------------------------------------------------*/
#ifndef _XPDebug_
#define _XPDebug_

#include "xp_core.h"
#include "xpassert.h"
#include "xp_trace.h"

/*-----------------------------------------------------------------------------
DEBUG (macro)
-----------------------------------------------------------------------------*/

#ifdef DEBUG
/* 
 * MSVC seems to have a problem with Debug as an int (it was probably
 * previously defined as a macro).
 */
#ifdef __cplusplus
#ifndef Debug
#define Debug 1
#endif
#else
	extern int Debug;
#endif
#else
#define Debug 0
#endif

#endif /* _XPDebug_ */

