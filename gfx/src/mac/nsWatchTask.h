/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef WatchTask_h__
#define WatchTask_h__


#include <Retrace.h>
#include <Quickdraw.h>
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
  Cursor mWatchCursor;      // the watch cursor
  PRPackedBool mBusy;       // are we currently spinning the cursor?
  PRPackedBool mSuspended;  // set if we've temporarily suspended operation
  PRPackedBool mInstallSucceeded;     // did we succeed in installing the task? (used in dtor)
  short mAnimation;         // stage of animation
  
};


#endif
