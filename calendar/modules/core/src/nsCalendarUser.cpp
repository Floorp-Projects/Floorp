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

#include <stdio.h>
#include "nsCalendarUser.h"
#include "nsCoreCIID.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kICalendarUserIID, NS_ICALENDAR_USER_IID);
static NS_DEFINE_IID(kIUserIID,         NS_IUSER_IID);
static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCUserCID,         NS_USER_CID);

nsCalendarUser::nsCalendarUser(nsISupports* outer)
{
  NS_INIT_REFCNT();
  mUserSupports = nsnull;
  mUser = nsnull;
}

NS_IMPL_QUERY_INTERFACE(nsCalendarUser, kICalendarUserIID)
//  if (nsnull != mWidgetSupports)
//    return mWidgetSupports->QueryInterface(aIID, aInstancePtr);

NS_IMPL_ADDREF(nsCalendarUser)
NS_IMPL_RELEASE(nsCalendarUser)

nsCalendarUser::~nsCalendarUser()
{
  NS_IF_RELEASE(mUserSupports); 
  mUser = nsnull; // Do Not Release
}

nsresult nsCalendarUser::Init()
{
  nsresult res;

  nsISupports * supports ;

  res = QueryInterface(kISupportsIID, (void **) &supports);

  if (NS_OK != res)
    return res;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  res = nsRepository::CreateInstance(kCUserCID, 
                                     supports, 
                                     kISupportsIID, 
                                     (void**)&mUserSupports);

  if (NS_OK == res) 
  {
    res = mUserSupports->QueryInterface(kIUserIID, (void**)&mUser);
    if (NS_OK != res) 
    {
	    mUserSupports->Release();
	    mUserSupports = NULL;
    }
    else 
    {
      mUser->Release();
    }
  }


  return res;

}


NS_IMETHODIMP nsCalendarUser :: GetLayer(nsILayer *& aLayer)
{
  aLayer = mLayer;
  return NS_OK;
}

NS_IMETHODIMP nsCalendarUser :: SetLayer(nsILayer* aLayer)
{
  mLayer = aLayer;
  return NS_OK;
}

