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

#ifndef nsLayer_h___
#define nsLayer_h___

#include "nscalexport.h"
#include "nsILayer.h"

class nsCalendarShell;

class NS_CALENDAR nsLayer : public nsILayer 
{

  JulianString msCurl;
  NSCalendar* mpCal;
  nsCalendarShell* mpShell;

public:
  nsLayer(nsISupports* outer);
  ~nsLayer();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD SetCurl(const JulianString& s);
  NS_IMETHOD GetCurl(JulianString& s);

  /**
   * Check to see if the layer matches the supplied curl.
   * In this case, matching means that the host and CSID 
   * values are equal.
   * @param  s  the curl
   * @return NS_OK on success
   */
  NS_IMETHOD URLMatch(const JulianString& aCurl, PRBool& aMatch);
  
  NS_IMETHOD SetCal(NSCalendar* aCal);
  NS_IMETHOD GetCal(NSCalendar*& aCal);
  NS_IMETHOD SetShell(nsCalendarShell* aShell) {mpShell = aShell; return NS_OK;}
  NS_IMETHOD FetchEventsByRange(
                      DateTime* aStart, 
                      DateTime* aStop,
                      JulianPtrArray* anArray
                      );

  NS_IMETHOD StoreEvent(VEvent& addEvent);

private:
  nsresult FetchEventsByRange();
};

#endif //nsLayer_h___
