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

#ifndef nsCalNetFetchCollector_h__
#define nsCalNetFetchCollector_h__

#include "nscalexport.h"
#include "nsCalNetFetchVals.h"
#include "nsICalNetFetchCollector.h"
#include "nsIArray.h"


class NS_CALENDAR nsCalNetFetchCollector : public nsICalNetFetchCollector 
{

  nsIArray * mpFetchList ;

public:
  nsCalNetFetchCollector(nsISupports* outer);
  ~nsCalNetFetchCollector();

  NS_DECL_ISUPPORTS

  /**
   *  Register a fetch by range, but don't perform it yet.
   *  @param nsIUser user making the request
   *  @param d1  start date/time of range
   *  @param d2  end date/time of range
   */
  NS_IMETHOD QueueFetchByRange(nsIUser* pUser, nsILayer* pLayer, DateTime d1, DateTime d2);

  /**
   *  Perform all the queued fetches.
   *  @param pID the returned ID number associated with this set of fetches.
   */
  NS_IMETHOD FlushFetchByRange(PRInt32* pID);

  /**
   *  Set the priority of the fetches belonging to the supplied ID number
   *  @param id    the ID number of the fetches as returned by FlushFetchByRange.
   *  @param iPri  the priority for their threads
   */
  NS_IMETHOD SetPriority(PRInt32 id, PRInt32 iPri);

  /**
   *  Get the state associated with the ID'd fetches
   *  @param id  the id of the fetch
   *  @param d1  its current state
   */
  NS_IMETHOD GetState(PRInt32 ID, eCalNetFetchState *pState);

  /**
   *  Cancel everything associated with the fetch
   *  @param nsIUser user making the request
   *  @param d1  start date/time of range
   *  @param d2  end date/time of range
   */
  NS_IMETHOD Cancel(nsILayer * aLayer);
};

#endif //nsCalNetFetchCollector_h__
