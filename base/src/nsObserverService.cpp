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
#include "prlock.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsRepository.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsIObserverList.h"
#include "nsObserverList.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"
#include "nsString.h"

static NS_DEFINE_IID(kIObserverServiceIID, NS_IOBSERVERSERVICE_IID);
static NS_DEFINE_IID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);


////////////////////////////////////////////////////////////////////////////////

class nsObserverService : public nsIObserverService {
public:
    static nsresult GetObserverService(nsIObserverService** anObserverService);

    NS_IMETHOD    GetObserverList(nsString* aTopic, nsIObserverList** anObserverList);
   
    nsObserverService();
    ~nsObserverService(void);
     
    NS_DECL_ISUPPORTS

   
private:

    nsHashtable   *mObserverTopicTable;

};

static nsObserverService* gObserverService = nsnull; // The one-and-only ObserverService

////////////////////////////////////////////////////////////////////////////////
// nsObserverService Implementation


NS_IMPL_ISUPPORTS(nsObserverService, kIObserverServiceIID);

NS_BASE nsresult NS_NewObserverService(nsIObserverService** anObserverService)
{
    return nsObserverService::GetObserverService(anObserverService);
}

nsObserverService::nsObserverService()
    : mObserverTopicTable(NULL)
{
    NS_INIT_REFCNT();
    mObserverTopicTable = nsnull;
}

nsObserverService::~nsObserverService(void)
{
    if(mObserverTopicTable)
        delete mObserverTopicTable;
    gObserverService = nsnull;
}


nsresult nsObserverService::GetObserverService(nsIObserverService** anObserverService)
{
    if (! gObserverService) {
        nsObserverService* it = new nsObserverService();
        if (! it)
            return NS_ERROR_OUT_OF_MEMORY;
        gObserverService = it;
    }

    NS_ADDREF(gObserverService);
    *anObserverService = gObserverService;
    return NS_OK;
}

nsresult nsObserverService::GetObserverList(nsString* aTopic, nsIObserverList** anObserverList)
{
    if (anObserverList == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }
	
	if(mObserverTopicTable == NULL) {
        mObserverTopicTable = new nsHashtable(256, PR_TRUE);
        if (mObserverTopicTable == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }


	// Safely convert to a C-string 
    char buf[128];
    char* topic = buf;
	
    if ((*aTopic).Length() >= sizeof(buf))
        topic = new char[(*aTopic).Length() + 1];

    if (topic == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    (*aTopic).ToCString(topic, (*aTopic).Length() + 1);

	nsCStringKey key(topic);

    nsIObserverList *topicObservers = nsnull;
    if(mObserverTopicTable->Exists(&key)) {
        topicObservers = (nsIObserverList *) mObserverTopicTable->Get(&key);
        if (topicObservers != NULL) {
	        *anObserverList = topicObservers;	
        } else {
            NS_NewObserverList(&topicObservers);
            mObserverTopicTable->Put(&key, topicObservers);
        }
    } else {
        NS_NewObserverList(&topicObservers);
        *anObserverList = topicObservers;
        mObserverTopicTable->Put(&key, topicObservers);

    }

	return NS_OK;
}



////////////////////////////////////////////////////////////////////////////////
// nsObserverServiceFactory Implementation

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsObserverServiceFactory, kIFactoryIID);

nsObserverServiceFactory::nsObserverServiceFactory(void)
{
    NS_INIT_REFCNT();
}

nsObserverServiceFactory::~nsObserverServiceFactory(void)
{
  
}

nsresult
nsObserverServiceFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;
    
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;
    nsIObserverService* inst = nsnull;

    
    if (NS_FAILED(rv = NS_NewObserverService(&inst)))
        return rv;
   

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = inst->QueryInterface(aIID, aResult);

    if (NS_FAILED(rv)) {
        *aResult = NULL;
    }
    return rv;
}

nsresult
nsObserverServiceFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////////////

