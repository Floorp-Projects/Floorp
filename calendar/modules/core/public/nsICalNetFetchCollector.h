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

//ea7225c0-6313-11d2-b564-0060088a4b1d
#define NS_ICAL_NET_FETCH_COLLECTOR_IID   \
{ 0xea7225c0, 0x6313, 0x11d2,    \
{ 0xb5, 0x64, 0x00, 0x60, 0x08, 0x8a, 0x4b, 0x1d } }

class nsICalNetFetchCollector : public nsISupports
{
  enum EState { ePAUSED, eCOLLECTING, eEXECUTING, eCOMPLETED };

public:
  NS_IMETHOD Init() = 0;

  NS_IMETHOD QueueFetchByRange(nsIUser* pUser, nsILayer* pLayer, DateTime d1, DateTime d2) = 0;
  NS_IMETHOD FlushFetchByRange(PRInt32* pID) = 0;
  NS_IMETHOD SetPriority(PRInt32 id, PRInt32 iPri) = 0;
  NS_IMETHOD GetState(PRInt32 ID, EState *pState) = 0;
  NS_IMETHOD Cancel(nsILayer * aLayer) = 0;
};


#endif /* nsICalNetFetchCollector */