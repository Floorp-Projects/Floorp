/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//
// nsRepeatService
//
#ifndef nsRepeatService_h__
#define nsRepeatService_h__

#include "nsCOMPtr.h"
#include "nsITimerCallback.h"
#include "nsITimer.h"

class nsITimer;

class nsRepeatService : public nsITimerCallback
{
public:

  virtual void Notify(nsITimer *timer);

  void Start(nsITimerCallback* aCallback);
  void Stop();

  static nsRepeatService* GetInstance();

  NS_DECL_ISUPPORTS
  virtual ~nsRepeatService();

protected:
  nsRepeatService();

private:
  nsCOMPtr<nsITimerCallback> mCallback;
  nsCOMPtr<nsITimer>         mRepeatTimer;
  static nsRepeatService*    gInstance;

}; // class nsRepeatService

#endif
