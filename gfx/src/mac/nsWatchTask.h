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


#ifndef WatchTask_h__
#define WatchTask_h__


#include <Retrace.h>
#include "PRTypes.h"
#include "nscore.h"


//
// class nsWatchTask
//
// A nice little class that installs/removes a VBL to set the cursor to
// the watch if we're away from the event loop for a while. Will also
// animate the watch cursor.
//

class nsWatchTask
{
public:
  nsWatchTask ( ) ;
  ~nsWatchTask ( ) ;

    // Registers the VBL task and does other various init tasks to begin
    // watching for time away from the event loop. It is ok to call other
    // methods on this object w/out calling Start().
  NS_GFX void Start ( ) ;
  
    // call from the main event loop
  NS_GFX void EventLoopReached ( ) ;
  
    // turn off when we know we're going into an area where it's ok
    // that WNE is not called (eg, the menu code)
  void Suspend ( ) { mSuspended = PR_TRUE; };
  void Resume ( ) { mSuspended = PR_FALSE; };
  
  static NS_GFX nsWatchTask& GetTask ( ) ;
  
private:

  enum { 
    kRepeatInterval = 10,       // check every 1/6 of a second if we should show watch (10/60)
    kTicksToShowWatch = 45,     // show watch if haven't seen WNE for 3/4 second (45/60)
    kStepsInAnimation = 12
  };
  
    // the VBL task
  static pascal void DoWatchTask(nsWatchTask* theTaskPtr) ;
  
  VBLTask mTask;            // this must be first!!
  long mChecksum;           // 'mozz' to validate we have real data at interrupt time (not needed?)
  void* mSelf;              // so we can get back to |this| from the static routine
  long mTicks;              // last time the event loop was hit
  PRPackedBool mBusy;       // are we currently spinning the cursor?
  PRPackedBool mSuspended;  // set if we've temporarily suspended operation
  PRPackedBool mInstallSucceeded;     // did we succeed in installing the task? (used in dtor)
  short mAnimation;         // stage of animation
  
};


#endif
