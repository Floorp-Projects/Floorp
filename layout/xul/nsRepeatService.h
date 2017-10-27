/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

class nsIDocument;
class nsITimer;

class nsRepeatService final
{
public:
  typedef void (* Callback)(void* aData);

  ~nsRepeatService();

  // Start dispatching timer events to the callback. There is no memory
  // management of aData here; it is the caller's responsibility to call
  // Stop() before aData's memory is released.
  //
  // aCallbackName is the label of the callback, used to pass to
  // InitWithNamedCallbackFunc.
  //
  // aDocument is used to get the event target in Start(). We need an event
  // target to init mRepeatTimer.
  void Start(Callback aCallback, void* aCallbackData,
             nsIDocument* aDocument, const nsACString& aCallbackName,
             uint32_t aInitialDelay = INITAL_REPEAT_DELAY);
  // Stop dispatching timer events to the callback. If the repeat service
  // is not currently configured with the given callback and data, this
  // is just ignored.
  void Stop(Callback aCallback, void* aData);

  static nsRepeatService* GetInstance();
  static void Shutdown();

protected:
  nsRepeatService();

private:
  // helper function to initialize callback function to mRepeatTimer
  void InitTimerCallback(uint32_t aInitialDelay);

  Callback           mCallback;
  void*              mCallbackData;
  nsCString          mCallbackName;
  nsCOMPtr<nsITimer> mRepeatTimer;

}; // class nsRepeatService

#endif
