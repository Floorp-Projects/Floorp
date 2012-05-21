/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsRepeatService
//
#ifndef nsRepeatService_h__
#define nsRepeatService_h__

#include "nsCOMPtr.h"
#include "nsITimer.h"

#define INITAL_REPEAT_DELAY 250

#ifdef XP_MACOSX
#define REPEAT_DELAY        25
#else
#define REPEAT_DELAY        50
#endif

class nsITimer;

class nsRepeatService : public nsITimerCallback
{
public:

  typedef void (* Callback)(void* aData);
    
  NS_DECL_NSITIMERCALLBACK

  // Start dispatching timer events to the callback. There is no memory
  // management of aData here; it is the caller's responsibility to call
  // Stop() before aData's memory is released.
  void Start(Callback aCallback, void* aData,
             PRUint32 aInitialDelay = INITAL_REPEAT_DELAY);
  // Stop dispatching timer events to the callback. If the repeat service
  // is not currently configured with the given callback and data, this
  // is just ignored.
  void Stop(Callback aCallback, void* aData);

  static nsRepeatService* GetInstance();
  static void Shutdown();

  NS_DECL_ISUPPORTS
  virtual ~nsRepeatService();

protected:
  nsRepeatService();

private:
  Callback           mCallback;
  void*              mCallbackData;
  nsCOMPtr<nsITimer> mRepeatTimer;
  static nsRepeatService* gInstance;

}; // class nsRepeatService

#endif
