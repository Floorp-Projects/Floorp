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
#ifndef nsICalendarUser_h___
#define nsICalendarUser_h___

#include "nsISupports.h"
#include "nsILayer.h"

//2fa12310-4cd8-11d2-924a-00805f8a7ab6
#define NS_ICALENDAR_USER_IID   \
{ 0x2fa12310, 0x4cd8, 0x11d2,    \
{ 0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsICalendarUser : public nsISupports
{

public:
  NS_IMETHOD Init() = 0;

  NS_IMETHOD GetLayer(nsILayer *& aLayer) = 0;
  NS_IMETHOD SetLayer(nsILayer* aLayer) = 0;
  NS_IMETHOD HasLayer(const JulianString& aCurl,PRBool& aContains) = 0;

};


#endif /* nsICalendarUser */
