/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the implementation providing nsILiveconnect XP-COM interface.
 *
 */
#include "prtypes.h"
#include "nspr.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"

#include "nsCLiveconnect.h"
#include "nsCLiveconnectFactory.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,     NS_IFACTORY_IID);
static NS_DEFINE_CID(kCLiveconnectCID, NS_CLIVECONNECT_CID);

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NSGetFactory:
 * Provides entry point to liveconnect dll.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    
    if (!aClass.Equals(kCLiveconnectCID)) {
        return NS_ERROR_FACTORY_NOT_LOADED;     // XXX right error?
    }
    nsCLiveconnectFactory* factory = new nsCLiveconnectFactory();
    if (factory == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    factory->AddRef();
    *aFactory = factory;
    return NS_OK;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
    return PR_FALSE;
}


////////////////////////////////////////////////////////////////////////////
// from nsISupports 

NS_METHOD
nsCLiveconnectFactory::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    PR_ASSERT(NULL != aInstancePtr); 
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (aIID.Equals(kIFactoryIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    }
    return NS_NOINTERFACE; 
}

NS_IMPL_ADDREF(nsCLiveconnectFactory)
NS_IMPL_RELEASE(nsCLiveconnectFactory)

////////////////////////////////////////////////////////////////////////////
// from nsIFactory:

NS_METHOD
nsCLiveconnectFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
	if (!aResult)
		return NS_ERROR_INVALID_POINTER;

	*aResult  = NULL;

	if (aOuter && !aIID.Equals(kISupportsIID))
		return NS_ERROR_INVALID_ARG;

	nsCLiveconnect* liveconnect = new nsCLiveconnect(aOuter);
	if (liveconnect == NULL)
		return NS_ERROR_OUT_OF_MEMORY;
		
	nsresult result = liveconnect->AggregatedQueryInterface(aIID, aResult);
	if (NS_FAILED(result))
		delete liveconnect;

	return result;
}

NS_METHOD
nsCLiveconnectFactory::LockFactory(PRBool aLock)
{
   return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// from nsCLiveconnectFactory:

nsCLiveconnectFactory::nsCLiveconnectFactory(void)
{
      NS_INIT_REFCNT();
}

nsCLiveconnectFactory::~nsCLiveconnectFactory()
{
}
