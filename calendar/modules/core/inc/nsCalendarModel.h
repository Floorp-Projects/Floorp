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

#ifndef nsCalendarModel_h___
#define nsCalendarModel_h___

#include "nscalexport.h"
#include "nsIModel.h"
#include "nsICalendarModel.h"
#include "nsICalendarUser.h"
#include "nsIXPFCObserver.h"
#include "nsIXPFCCommandReceiver.h"
#include "nsIXPFCSubject.h"

class nsXPFCNotificationStateCommand;
class nsCalFetchEventsCommand;

class nsCalendarModel : public nsICalendarModel,
                        public nsIModel,
                        public nsIXPFCObserver,
                        public nsIXPFCSubject,
                        public nsIXPFCCommandReceiver
{

public:
  nsCalendarModel(nsISupports* outer);
  ~nsCalendarModel();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  NS_IMETHOD GetCalendarUser(nsICalendarUser *& aCalendarUser);
  NS_IMETHOD SetCalendarUser(nsICalendarUser* aCalendarUser);

  // nsIXPFCObserver methods
  NS_IMETHOD_(nsEventStatus) Update(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand);

  // nsIXPFCCommandReceiver methods
  NS_IMETHOD_(nsEventStatus) Action(nsIXPFCCommand * aCommand);

  // nsIXPFCSubject methods
  NS_IMETHOD Attach(nsIXPFCObserver * aObserver);
  NS_IMETHOD Detach(nsIXPFCObserver * aObserver);
  NS_IMETHOD Notify(nsIXPFCCommand * aCommand);

private:
  NS_IMETHOD HandleNotificationCommand(nsXPFCNotificationStateCommand * aCommand);
  NS_IMETHOD HandleFetchCommand(nsCalFetchEventsCommand * aCommand);
  NS_IMETHOD SendModelUpdateCommand();

protected:
  nsICalendarUser    * mCalendarUser;

};

#endif //nsCalendarModel_h___
