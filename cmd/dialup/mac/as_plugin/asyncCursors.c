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

#include "pluginIncludes.h"



		Boolean			taskInstalled = FALSE;
		acurHandle		cursorsH = NULL;
		VBLTaskWithA5Ptr	cursorTask = NULL;

extern		short			pluginResFile;



OSErr
initAsyncCursors()
{
		CursHandle		*workPtr;
		OSErr			err = noErr;
		short			cursorCount,saveResFile;

	if (cursorsH == NULL)	{						// only initialize once
		saveResFile = CurResFile();
		UseResFile(pluginResFile);
		cursorsH = (acurHandle)Get1Resource('acur', ACUR_RESID);
		if (cursorsH != NULL)	{
			HNoPurge((Handle)cursorsH);
			HUnlock((Handle)cursorsH);
			HLockHi((Handle)cursorsH);
			DetachResource((Handle)cursorsH);
			cursorCount=(**cursorsH).numCursors;
			(**cursorsH).index=0;
			workPtr=(**cursorsH).cursors;
			while(cursorCount--)	{
				*workPtr = (CursHandle)Get1Resource('CURS',*(short *)workPtr);
				if (*workPtr != NULL)	{
					HNoPurge((Handle)*workPtr);
					HUnlock((Handle)*workPtr);
					HLockHi((Handle)*workPtr);
					DetachResource((Handle)*workPtr);
					}
				++workPtr;
				}
			}
		else	{
			err = (err=ResError()) ? err:resNotFound;
			}
		UseResFile(saveResFile);
		}
	return(err);
}



void
spinCursor(acurHandle cursorsH)
{
		short		nextIndex,theIndex;

	if (cursorsH != NULL)	{
		theIndex = (**cursorsH).index;
		nextIndex = theIndex + 1;
		if (nextIndex >= (**cursorsH).numCursors)	{
			nextIndex = 0;
			}
		(**cursorsH).index = nextIndex;
		if ((**cursorsH).cursors[theIndex])	{
			SetCursor(*(**cursorsH).cursors[theIndex]);
			}
		}
}



#if defined(powerc) || defined(__powerc)

void
asyncCursorTask(VBLTaskWithA5Ptr theTaskPtr)

#else

void
asyncCursorTask(VBLTaskWithA5Ptr theTaskPtr:__A0)
#endif

{
#if defined(powerc) || defined(__powerc)
#else
		long		oldA5;
#endif
	if (LMGetCrsrBusy() == 0)	{

#if defined(powerc) || defined(__powerc)
#else
		oldA5 = SetA5(theTaskPtr->theA5);
#endif
		spinCursor(theTaskPtr->cursorsH);
		theTaskPtr->theTask.vblCount = SPIN_CYCLE;
		if (--cursorTask->numSpins <= 0)	{		// max spin time expired, stop spinning
			(void)VRemove((QElemPtr)cursorTask);
			taskInstalled = FALSE;
			}

#if defined(powerc) || defined(__powerc)
#else
		(void)SetA5(oldA5);
#endif
		
		}
}



void
startAsyncCursors()
{
	if (initAsyncCursors() != noErr)	return;
//	if (LJ_GetJavaStatus() != LJJavaStatus_Enabled)	return;		// java enabled but not running?

	if (cursorTask == NULL)	{
		cursorTask = (VBLTaskWithA5Ptr)NewPtrClear(sizeof(VBLTaskWithA5));
		if (cursorTask != NULL)	{
			cursorTask->theTask.qType = vType;
			cursorTask->theTask.vblAddr = (void *)NewVBLProc(asyncCursorTask);
			cursorTask->theTask.vblCount = SPIN_CYCLE;
			cursorTask->theTask.vblPhase = 0;
			cursorTask->theA5 = (long)LMGetCurrentA5();
			cursorTask->cursorsH = cursorsH;
			cursorTask->numSpins = MAX_NUM_SPINS;
			
			if (VInstall((QElemPtr)cursorTask) == noErr)	{
				taskInstalled = TRUE;
				}
			else	{
				taskInstalled = FALSE;
				if (cursorTask->theTask.vblAddr)	{
					DisposeRoutineDescriptor(cursorTask->theTask.vblAddr);
					}
				DisposePtr((Ptr)cursorTask);
				cursorTask = NULL;
				}
			}
		}
}



void
stopAsyncCursors()
{
	if (cursorTask)	{
		if (taskInstalled == TRUE)	{
			(void)VRemove((QElemPtr)cursorTask);
			taskInstalled = FALSE;
			}
		if (cursorTask->theTask.vblAddr)	{
			DisposeRoutineDescriptor(cursorTask->theTask.vblAddr);
			}
		DisposePtr((Ptr)cursorTask);
		cursorTask = NULL;
		InitCursor();
		}
}
