/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsWatchTask.h"
#include <LowMem.h>
#include <Appearance.h>


// our global, as soon as the gfx code fragment loads, this will setup
// the VBL task automagically. 
nsWatchTask gWatchTask;


//
// GetTask
//
// A nice little getter to avoid exposing the global variable
nsWatchTask&
nsWatchTask :: GetTask ( )
{
  return gWatchTask;
}


nsWatchTask :: nsWatchTask ( )
  : mChecksum('mozz'), mSelf(this), mTicks(::TickCount()), mBusy(PR_FALSE), mSuspended(PR_FALSE),
     mInstallSucceeded(PR_FALSE), mAnimation(0)
{
  // setup the task
  mTask.qType = vType;
	mTask.vblAddr = NewVBLProc((VBLProcPtr)DoWatchTask);
  mTask.vblCount = kRepeatInterval;
  mTask.vblPhase = 0;
  
  // install it
  mInstallSucceeded = ::VInstall((QElemPtr)&mTask) == noErr;
}


nsWatchTask :: ~nsWatchTask ( ) 
{
  if ( mInstallSucceeded )
    ::VRemove ( (QElemPtr)&mTask );
  InitCursor();
}


//
// DoWatchTask
//
// Called at vertical retrace. If we haven't been to the event loop for
// |kTicksToShowWatch| ticks, animate the cursor.
//
// Note: assumes that the VBLTask, mTask, is the first member variable, so
// that we can piggy-back off the pointer to get to the rest of our data.
//
// (Do we still need the check for LMGetCrsrBusy()? It's not in carbon...)
//
pascal void
nsWatchTask :: DoWatchTask ( nsWatchTask* inSelf )
{
  if ( inSelf->mChecksum == 'mozz' ) {
    if ( !inSelf->mSuspended  ) {
      if ( !inSelf->mBusy && !LMGetCrsrBusy() ) {
        if ( ::TickCount() - inSelf->mTicks > kTicksToShowWatch ) {
          ::SetAnimatedThemeCursor(kThemeWatchCursor, inSelf->mAnimation);
          inSelf->mBusy = PR_TRUE;
        }
      }
      else
        ::SetAnimatedThemeCursor(kThemeWatchCursor, inSelf->mAnimation);
      
      // next frame in cursor animation    
      ++inSelf->mAnimation;
    }
    
    // reset the task to fire again
    inSelf->mTask.vblCount = kRepeatInterval;
    
  } // if valid checksum
  
} // DoWatchTask


//
// EventLoopReached
//
// Called every time we reach the event loop (or an event loop), this tickles
// our internal tick count to reset the time since our last visit to WNE and
// if we were busy, we're not any more.
//
void
nsWatchTask :: EventLoopReached ( )
{
  // reset the cursor if we were animating it
  if ( mBusy )
    ::InitCursor();
 
  mBusy = PR_FALSE;
  mTicks = ::TickCount();
  mAnimation = 0;

}

