/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#define NS_IMPL_IDS
#include "pratom.h"
//#include "nsIFactory.h"
//#include "nsIServiceManager.h"
#include "nsRepository.h"
#include "nsIObserverList.h"
#include "nsObserverList.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsAutoLock.h"


#define NS_AUTOLOCK(__monitor) nsAutoLock __lock(__monitor)

static NS_DEFINE_IID(kIObserverListIID, NS_IOBSERVERLIST_IID);
static NS_DEFINE_IID(kObserverListCID, NS_OBSERVERLIST_CID);


////////////////////////////////////////////////////////////////////////////////
// nsObserverList Implementation


NS_IMPL_ISUPPORTS(nsObserverList, kIObserverListIID);

NS_BASE nsresult NS_NewObserverList(nsIObserverList** anObserverList)
{

    if (anObserverList == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }  
    nsObserverList* it = new nsObserverList();

    if (it == 0) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    return it->QueryInterface(kIObserverListIID, (void **) anObserverList);
}

nsObserverList::nsObserverList()
    : mObserverList(NULL),
	  mLock(nsnull)
{
    NS_INIT_REFCNT();
    mLock = PR_NewLock();
}

nsObserverList::~nsObserverList(void)
{
    PR_DestroyLock(mLock);
}


nsresult nsObserverList::AddObserver(nsIObserver** anObserver)
{
	nsresult rv;
    
	NS_AUTOLOCK(mLock);

    if (anObserver == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }  

	if(!mObserverList) {
        rv = NS_NewISupportsArray(&mObserverList);
		if (NS_FAILED(rv)) return rv;
    }

	if(*anObserver) {
		mObserverList->AppendElement(*anObserver);      
    }

	return NS_OK;
}

nsresult nsObserverList::RemoveObserver(nsIObserver** anObserver)
{
 	NS_AUTOLOCK(mLock);

    if (anObserver == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }  

	if(*anObserver) {
		mObserverList->RemoveElement(*anObserver);      
    }

    return NS_OK;

}

NS_IMETHODIMP nsObserverList::EnumerateObserverList(nsIEnumerator** anEnumerator)
{
	NS_AUTOLOCK(mLock);

    if (anEnumerator == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }
    
 	return mObserverList->Enumerate(anEnumerator);
}

