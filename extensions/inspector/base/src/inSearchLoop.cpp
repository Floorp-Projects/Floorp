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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#include "inSearchLoop.h"

#include "nsITimer.h"

///////////////////////////////////////////////////////////////////////////////

inSearchLoop::inSearchLoop(inISearchProcess* aSearchProcess)
{
  mSearchProcess = aSearchProcess;
  nsresult rv;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
}

inSearchLoop::~inSearchLoop()
{
}

///////////////////////////////////////////////////////////////////////////////
// inSearchLoop

nsresult
inSearchLoop::Start()
{
  mTimer->Init(inSearchLoop::TimerCallback, (void*)this, 0, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);

  return NS_OK;
}

nsresult
inSearchLoop::Step()
{
  PRBool done = PR_FALSE;
  mSearchProcess->SearchStep(&done);

  if (done)
    Stop();

  return NS_OK;
}

nsresult
inSearchLoop::Stop()
{
  mTimer->Cancel();
  
  return NS_OK;
}

void 
inSearchLoop::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  inSearchLoop* loop = (inSearchLoop*) aClosure;
  loop->Step();
}
