/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsRepeatService.h"

#if XP_MAC
#define INITAL_REPEAT_DELAY 250
#define REPEAT_DELAY        10
#else
#define INITAL_REPEAT_DELAY 250
#define REPEAT_DELAY        50
#endif

nsCOMPtr<nsITimerCallback> nsRepeatService::gInstance = new nsRepeatService();

nsRepeatService::nsRepeatService()
{
  NS_INIT_REFCNT();
}

nsRepeatService::~nsRepeatService()
{
  Stop();
}

nsRepeatService* 
nsRepeatService::GetInstance()
{
  return (nsRepeatService*)gInstance.get();
}

void nsRepeatService::Start(nsITimerCallback* aCallback)
{
  NS_PRECONDITION(aCallback != nsnull, "null ptr");
  if (! aCallback)
    return;

  mCallback = aCallback;
  nsresult rv;
  mRepeatTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);

  if (NS_OK == rv)  {
    mRepeatTimer->Init(this, INITAL_REPEAT_DELAY);
  }

}

void nsRepeatService::Stop()
{
  //printf("Stopping repeat timer\n");
  if (mRepeatTimer) {
     mRepeatTimer->Cancel();
     mRepeatTimer = nsnull;
     mCallback = nsnull;
  }
}

NS_IMETHODIMP_(void) nsRepeatService::Notify(nsITimer *timer)
{
   // if the repeat delay is the initial one reset it.
  if (mRepeatTimer) {
     mRepeatTimer->Cancel();
  }

  // do callback
  if (mCallback)
    mCallback->Notify(timer);

  // start timer again.
  if (mRepeatTimer) {
     mRepeatTimer = do_CreateInstance("@mozilla.org/timer;1");
     mRepeatTimer->Init(this, REPEAT_DELAY);
  }

}

NS_IMPL_ISUPPORTS1(nsRepeatService, nsITimerCallback)

