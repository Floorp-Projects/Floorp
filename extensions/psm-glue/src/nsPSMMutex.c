/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
*/

#include "prmon.h"
#include "prtypes.h"

#include "nsPSMMutex.h"

static PRMonitor *_nsPSMMutexVar;

PRStatus
nsPSMMutexInit()
{
  if (!_nsPSMMutexVar)
    _nsPSMMutexVar = PR_NewMonitor();
  else
    printf("PSMMutex warning got called twice\n");

  return _nsPSMMutexVar ? PR_SUCCESS : PR_FAILURE;
}

PRStatus
nsPSMMutexDestroy()
{
    if (!_nsPSMMutexVar)
        return PR_FAILURE;
    
    PR_Wait(_nsPSMMutexVar, PR_INTERVAL_NO_TIMEOUT);

    PR_DestroyMonitor(_nsPSMMutexVar);
    return PR_SUCCESS;
}

static void
nsPSMMutexLock(CMTMutexPointer *p)
{
  PR_EnterMonitor(*(PRMonitor **)p);
  return;
}

static void
nsPSMMutexUnlock(CMTMutexPointer *p)
{
  PR_ExitMonitor(*(PRMonitor **)p);
  return;
}

CMT_MUTEX nsPSMMutexTbl = 
{
  &_nsPSMMutexVar,
  (CMTMutexFunction)nsPSMMutexLock,
  (CMTMutexFunction)nsPSMMutexUnlock
};
