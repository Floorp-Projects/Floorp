/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsRepeatService.h"

#if defined(XP_MAC) || defined(XP_MACOSX)
#define INITAL_REPEAT_DELAY 250
#define REPEAT_DELAY        10
#else
#define INITAL_REPEAT_DELAY 250
#define REPEAT_DELAY        50
#endif

nsRepeatService* nsRepeatService::gInstance = nsnull;

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

