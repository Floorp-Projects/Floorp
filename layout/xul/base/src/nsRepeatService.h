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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

  NS_IMETHOD_(void) Notify(nsITimer *timer);

  void Start(nsITimerCallback* aCallback);
  void Stop();

  static nsRepeatService* GetInstance();
  static void Shutdown();

  NS_DECL_ISUPPORTS
  virtual ~nsRepeatService();

protected:
  nsRepeatService();

private:
  nsCOMPtr<nsITimerCallback> mCallback;
  nsCOMPtr<nsITimer>         mRepeatTimer;
  static nsRepeatService* gInstance;

}; // class nsRepeatService

#endif
