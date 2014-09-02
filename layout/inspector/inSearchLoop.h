/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inSearchLoop_h__
#define __inSearchLoop_h__

#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "inISearchProcess.h"

class inSearchLoop
{
public:
  explicit inSearchLoop(inISearchProcess* aSearchProcess);
  virtual ~inSearchLoop();

  nsresult Start();
  nsresult Step();
  nsresult Stop();
  static void TimerCallback(nsITimer *aTimer, void *aClosure);

protected:
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<inISearchProcess> mSearchProcess;
};

#endif
