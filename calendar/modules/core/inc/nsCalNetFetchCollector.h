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
#include "nsIVector.h"


class NS_CALENDAR nsCalNetFetchCollector : public nsICalNetFetchCollector 
{

  nsIVector * mpFetchList ;

public:
  nsCalNetFetchCollector(nsISupports* outer);
  ~nsCalNetFetchCollector();

  NS_DECL_ISUPPORTS

  NS_IMETHOD QueueFetchByRange(nsIUser* pUser, nsILayer* pLayer, DateTime d1, DateTime d2);
  NS_IMETHOD FlushFetchByRange(PRInt32* pID);
  NS_IMETHOD SetPriority(PRInt32 id, PRInt32 iPri);
  NS_IMETHOD GetState(PRInt32 ID, eCalNetFetchState *pState);
  NS_IMETHOD Cancel(nsILayer * aLayer);
};

#endif //nsCalNetFetchCollector_h__
