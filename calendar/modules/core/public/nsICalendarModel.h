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
#ifndef nsICalendarModel_h___
#define nsICalendarModel_h___

#include "nsISupports.h"
#include "nsILayer.h"

//317353e0-4e66-11d2-924a-00805f8a7ab6
#define NS_ICALENDAR_MODEL_IID   \
{ 0x317353e0, 0x4e66, 0x11d2,    \
{ 0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsICalendarUser ;

class nsICalendarModel : public nsISupports
{

public:
  NS_IMETHOD Init() = 0;

  NS_IMETHOD GetCalendarUser(nsICalendarUser *& aCalendarUser) = 0;
  NS_IMETHOD SetCalendarUser(nsICalendarUser* aCalendarUser) = 0;

};


#endif /* nsICalendarModel */
