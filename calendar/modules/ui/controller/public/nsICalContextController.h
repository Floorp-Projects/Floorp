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

#ifndef nsICalContextController_h___
#define nsICalContextController_h___

#include "nsISupports.h"

#include "nsDateTime.h"
#include "nsDuration.h"

#include "nsCalPeriodFormat.h"

//aefb24f0-e9e5-11d1-9244-00805f8a7ab6
#define NS_ICAL_CONTEXT_CONTROLLER_IID   \
{ 0xaefb24f0, 0xe9e5, 0x11d1,    \
{ 0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

// ContextController Orientation
enum nsContextControllerOrientation
{
  nsContextControllerOrientation_north,
  nsContextControllerOrientation_south,
  nsContextControllerOrientation_east,
  nsContextControllerOrientation_west,
  nsContextControllerOrientation_default
};

class nsICalContextController : public nsISupports
{

public:
  
  NS_IMETHOD_(void)                           SetOrientation(nsContextControllerOrientation eOrientation) = 0;
  NS_IMETHOD_(nsContextControllerOrientation) GetOrientation() = 0;

  NS_IMETHOD_(nsDuration *) GetDuration() = 0 ;
  NS_IMETHOD SetDuration(nsDuration * aDuration) = 0;

  NS_IMETHOD_(nsCalPeriodFormat) GetPeriodFormat() = 0;
  NS_IMETHOD SetPeriodFormat(nsCalPeriodFormat aPeriodFormat) = 0;

};

#endif /* nsICalContextController_h___ */
