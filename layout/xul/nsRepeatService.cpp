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

using namespace mozilla;

static StaticRefPtr<nsRepeatService> gRepeatService;

nsRepeatService::nsRepeatService()
: mCallback(nullptr), mCallbackData(nullptr)
{
}

nsRepeatService::~nsRepeatService()
{
  NS_ASSERTION(!mCallback && !mCallbackData, "Callback was not removed before shutdown");
}

/* static */ nsRepeatService*
nsRepeatService::GetInstance()
{
  if (!gRepeatService) {
    gRepeatService = new nsRepeatService();
  }
  return gRepeatService;
}

/*static*/ void
nsRepeatService::Shutdown()
{
  gRepeatService = nullptr;
}

void nsRepeatService::Start(Callback aCallback, void* aCallbackData,
                            uint32_t aInitialDelay)
{
  NS_PRECONDITION(aCallback != nullptr, "null ptr");

  mCallback = aCallback;
  mCallbackData = aCallbackData;
  nsresult rv;
  mRepeatTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);

  if (NS_SUCCEEDED(rv))  {
    InitTimerCallback(aInitialDelay);
  }
}

void nsRepeatService::Stop(Callback aCallback, void* aCallbackData)
{
  if (mCallback != aCallback || mCallbackData != aCallbackData)
    return;

  //printf("Stopping repeat timer\n");
  if (mRepeatTimer) {
     mRepeatTimer->Cancel();
     mRepeatTimer = nullptr;
  }
  mCallback = nullptr;
  mCallbackData = nullptr;
}

NS_IMETHODIMP nsRepeatService::Notify(nsITimer *timer)
{
  // do callback
  if (mCallback)
    mCallback(mCallbackData);

  // start timer again.
  InitTimerCallback(REPEAT_DELAY);
  return NS_OK;
}

void
nsRepeatService::InitTimerCallback(uint32_t aInitialDelay)
{
  if (mRepeatTimer) {
    mRepeatTimer->InitWithCallback(this, aInitialDelay, nsITimer::TYPE_ONE_SHOT);
  }
}

NS_IMPL_ISUPPORTS(nsRepeatService, nsITimerCallback)
