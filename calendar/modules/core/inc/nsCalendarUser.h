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

#ifndef nsCalendarUser_h___
#define nsCalendarUser_h___

#include "nscalexport.h"
#include "nsICalendarUser.h"
#include "nsIUser.h"
#include "nsILayer.h"

class NS_CALENDAR nsCalendarUser : public nsICalendarUser 
{

public:
  nsCalendarUser(nsISupports* outer);
  ~nsCalendarUser();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  NS_IMETHOD GetLayer(nsILayer *& aLayer);
  NS_IMETHOD SetLayer(nsILayer* aLayer);

  /**
   * Check to see whether or not the user has a particular layer
   * in the list of layers that make up their calendar.
   * @param aCurl     the curl containing the host and csid
   * @param aContains return value set to PR_TRUE if the user's 
   *                  calendar list contains the specified curl
   *                  or PR_FALSE otherwise.
   * @return NS_OK on success.
   */
  NS_IMETHOD HasLayer(const JulianString& aCurl,PRBool& aContains);

protected:
  nsISupports * mpUserSupports;
  nsILayer    * mpLayer;

};

#endif //nsCalendarUser_h___
