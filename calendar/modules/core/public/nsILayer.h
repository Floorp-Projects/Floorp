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
#ifndef nsILayer_h___
#define nsILayer_h___

#include "nsISupports.h"
#include "jdefines.h"
#include "julnstr.h"
#include "vevent.h"

//5482d0d0-4cca-11d2-924a-00805f8a7ab6
#define NS_ILAYER_IID   \
{ 0x5482d0d0, 0x4cca, 0x11d2,    \
{ 0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class NSCalendar;
class DateTime;
class JulianPtrArray;
class nsCalendarShell;

class nsILayer : public nsISupports
{

public:
  NS_IMETHOD Init() = 0;

  /**
   * Set the curl that points to the cal store
   * @param  s  the curl
   * @return NS_OK on success
   */
  NS_IMETHOD SetCurl(const JulianString& s) = 0;
  
  /**
   * Set the shell pointer
   * @param  s  the curl
   * @return NS_OK on success
   */
  NS_IMETHOD SetShell(nsCalendarShell* aShell) = 0;
  
  /**
   * Get the curl that points to the cal store
   * @param  s  the curl
   * @return NS_OK on success
   */
  NS_IMETHOD GetCurl(JulianString& s) = 0;

  /**
   * Check to see if the layer matches the supplied curl.
   * In this case, matching means that the host and CSID 
   * values are equal.
   * @param  s  the curl
   * @return NS_OK on success
   */
  NS_IMETHOD URLMatch(const JulianString& aCurl, PRBool& aMatch)  = 0;

  NS_IMETHOD SetCal(NSCalendar* aCal) = 0;
  NS_IMETHOD GetCal(NSCalendar*& aCal) = 0;
  NS_IMETHOD FetchEventsByRange(
                      DateTime* aStart, 
                      DateTime* aStop,
                      JulianPtrArray* anArray
                      ) = 0;

  /**
   * Save an event in this layer
   * @param  addEvent  the new event to save
   * @return NS_OK on success
   */
   NS_IMETHOD StoreEvent(VEvent& addEvent) = 0;

};


#endif /* nsILayer */
