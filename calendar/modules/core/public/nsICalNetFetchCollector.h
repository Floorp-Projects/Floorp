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
#ifndef nsICalNetFetchCollector_h___
#define nsICalNetFetchCollector_h___

#include "nsISupports.h"
#include "nsCalNetFetchVals.h"

//09176270-638f-11d2-b566-0060088a4b1d 
#define NS_ICAL_NET_FETCH_COLLECTOR_IID   \
  { 0x09176270, 0x638f, 0x11d2,    \
  { 0xb5, 0x66, 0x00, 0x60, 0x08, 0x8a, 0x4b, 0x1d } }

class nsICalNetFetchCollector : public nsISupports
{

public:
  NS_IMETHOD Init() = 0;

  /**
   *  Register a fetch by range, but don't perform it yet.
   *  @param nsIUser user making the request
   *  @param pLayer
   *  @param d1  start date/time of range
   *  @param d2  end date/time of range
   */
  NS_IMETHOD QueueFetchEventsByRange(nsILayer* pLayer, DateTime d1, DateTime d2) = 0;

  /**
   *  Perform all the queued fetches.
   *  @param pID the returned ID number associated with this set of fetches.
   */
  NS_IMETHOD FlushFetchEventsByRange(PRInt32* pID) = 0;

  /**
   *  Set the priority of the fetches belonging to the supplied ID number
   *  @param id    the ID number of the fetches as returned by FlushFetchByRange.
   *  @param iPri  the priority for their threads
   */
  NS_IMETHOD SetPriority(PRInt32 id, PRInt32 iPri) = 0;

  /**
   *  Get the state associated with the ID'd fetches
   *  @param id  the id of the fetch
   *  @param d1  its current state
   */
  NS_IMETHOD GetState(PRInt32 id, eCalNetFetchState *pState) = 0;

  /**
   *  Cancel everything associated with the fetch
   *  @param nsIUser user making the request
   *  @param d1  start date/time of range
   *  @param d2  end date/time of range
   */
  NS_IMETHOD Cancel(PRInt32 id) = 0;

private:
  PRInt32 GetNextID();
};


#endif /* nsICalNetFetchCollector */