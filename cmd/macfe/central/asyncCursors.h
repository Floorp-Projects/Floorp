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

#include <Errors.h>
#include <LowMem.h>
#include <Memory.h>
#include <OSUtils.h>
#include <Quickdraw.h>
#include <Resources.h>
#include <Retrace.h>


#define	ACUR_RESID			128
#define	SPIN_CYCLE			8L		// spin every 8/60ths of a second
#define	MAX_NUM_SPINS			(8L*120L)	// after two minutes, stop spinning


#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif


typedef	struct	{
		short			numCursors;
		short			index;
		CursHandle		cursors[1];
} **acurHandle;


typedef	struct	{
		VBLTask			theTask;
		long			theA5;
		acurHandle		cursorsH;
		long			numSpins;
} VBLTaskWithA5, *VBLTaskWithA5Ptr;


#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif


// prototypes


		void			startAsyncCursors();
		void			stopAsyncCursors();
