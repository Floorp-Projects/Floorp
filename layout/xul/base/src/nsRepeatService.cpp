/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsRepeatService.h"
#include "nsIServiceManager.h"

nsRepeatService* nsRepeatService::gInstance = nsnull;

nsRepeatService::nsRepeatService()
: mCallback(nsnull), mCallbackData(nsnull)
{
}

nsRepeatService::~nsRepeatService()
{
  NS_ASSERTION(!mCallback && !mCallbackData, "Callback was not removed before shutdown");
}

nsRepeatService* 
nsRepeatService::GetInstance()
{
  if (!gInstance) {
    gInstance = new nsRepeatService();
    NS_IF_ADDREF(gInstance);
  }
  return gInstance;
}

/*static*/ void
nsRepeatService::Shutdown()
{
  NS_IF_RELEASE(gInstance);
}

void nsRepeatService::Start(Callback aCallback, void* aCallbackData,
                            PRUint32 aInitialDelay)
{
  NS_PRECONDITION(aCallback != nsnull, "null ptr");

  mCallback = aCallback;
  mCallbackData = aCallbackData;
  nsresult rv;
  mRepeatTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);

  if (NS_SUCCEEDED(rv))  {
    mRepeatTimer->InitWithCallback(this, aInitialDelay, nsITimer::TYPE_ONE_SHOT);
  }
}

void nsRepeatService::Stop(Callback aCallback, void* aCallbackData)
{
  if (mCallback != aCallback || mCallbackData != aCallbackData)
    return;

  //printf("Stopping repeat timer\n");
  if (mRepeatTimer) {
     mRepeatTimer->Cancel();
     mRepeatTimer = nsnull;
  }
  mCallback = nsnull;
  mCallbackData = nsnull;
}

NS_IMETHODIMP nsRepeatService::Notify(nsITimer *timer)
{
  // do callback
  if (mCallback)
    mCallback(mCallbackData);

  // start timer again.
  if (mRepeatTimer) {
    mRepeatTimer->InitWithCallback(this, REPEAT_DELAY, nsITimer::TYPE_ONE_SHOT);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsRepeatService, nsITimerCallback)
