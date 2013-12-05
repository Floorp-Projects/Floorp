/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inSearchLoop.h"

#include "nsITimer.h"
#include "nsIServiceManager.h"
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
  mTimer->InitWithFuncCallback(inSearchLoop::TimerCallback, (void*)this, 0, nsITimer::TYPE_REPEATING_SLACK);

  return NS_OK;
}

nsresult
inSearchLoop::Step()
{
  bool done = false;
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
