/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsRepeatService.h"

#define INITAL_REPEAT_DELAY 250
#define REPEAT_DELAY        50

nsRepeatService* nsRepeatService::gInstance = nsnull;

nsRepeatService::nsRepeatService()
{
}

nsRepeatService::~nsRepeatService()
{
  mCallback = nsnull;
  Stop();
}

nsRepeatService* 
nsRepeatService::GetInstance()
{
  if (!gInstance) {
     gInstance = new nsRepeatService();
     gInstance->mRefCnt = 1;
  }

  return gInstance;
}

void nsRepeatService::Start(nsITimerCallback* aCallback)
{
  mCallback = aCallback;
  nsresult rv = NS_NewTimer(getter_AddRefs(mRepeatTimer));

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
  }
}

void nsRepeatService::Notify(nsITimer *timer)
{
   // if the repeat delay is the initial one reset it.
  if (mRepeatTimer) {
     mRepeatTimer->Cancel();
     nsresult rv = NS_NewTimer(getter_AddRefs(mRepeatTimer));
     mRepeatTimer->Init(this, REPEAT_DELAY);
  }

  mCallback->Notify(timer);
}

static NS_DEFINE_IID(kITimerCallbackIID, NS_ITIMERCALLBACK_IID);
NS_IMPL_ISUPPORTS(nsRepeatService, kITimerCallbackIID);

