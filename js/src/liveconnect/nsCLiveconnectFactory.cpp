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
#include "nsRepository.h"

static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,     NS_IFACTORY_IID);
static NS_DEFINE_CID(kCLiveconnectCID, NS_CLIVECONNECT_CID);
static NS_DEFINE_IID(kILiveconnectIID, NS_ILIVECONNECT_IID);

nsIFactory      *nsCLiveconnectFactory::m_pNSIFactory     = NULL;
nsCLiveconnect  *nsCLiveconnectFactory::m_pNSCLiveconnect = NULL;



/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NSGetFactory:
 * Provides entry point to liveconnect dll.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

extern "C" NS_EXPORT nsresult
NSGetFactory(const nsCID &aClass, nsISupports* servMgr, nsIFactory **aFactory)
{
    
    if (!aClass.Equals(kCLiveconnectCID)) {
        return NS_ERROR_FACTORY_NOT_LOADED;     // XXX right error?
    }
    nsCLiveconnectFactory* pCLiveConnectFactory = new nsCLiveconnectFactory();
    if (pCLiveConnectFactory == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    pCLiveConnectFactory->AddRef();
    *aFactory = pCLiveConnectFactory;
    return NS_OK;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(void)
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
   *aResult  = NULL;
   
   if (aOuter && !aIID.Equals(kISupportsIID))
       return NS_NOINTERFACE;   // XXX right error?
   if (m_pNSCLiveconnect == NULL)
   {
      m_pNSCLiveconnect = new nsCLiveconnect(aOuter);
   }
   if (m_pNSCLiveconnect == NULL)
   {
         return NS_ERROR_FAILURE;
   }
   if (m_pNSCLiveconnect->QueryInterface(aIID,
                            (void**)aResult) != NS_OK) {
       // then we're trying get a interface other than nsISupports and
       // nsICapsManager
       return NS_ERROR_FAILURE;
   }
   return NS_OK;
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
      if( m_pNSIFactory != NULL)
      {
         return;
      }

      NS_INIT_REFCNT();
      nsresult     err         = NS_OK;
      NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

      err = this->QueryInterface(kIFactoryIID, (void**)&m_pNSIFactory);
      if ( (err == NS_OK) && (m_pNSIFactory != NULL) )
      {
         NS_DEFINE_CID(kCLiveconnectCID, NS_CLIVECONNECT_CID);
         nsRepository::RegisterFactory(kCLiveconnectCID, m_pNSIFactory,
                                     PR_FALSE);
      }
}

nsCLiveconnectFactory::~nsCLiveconnectFactory()
{
    if(mRefCnt == 0)
    {
      NS_DEFINE_CID(kCLiveconnectCID, NS_CLIVECONNECT_CID);
      nsRepository::UnregisterFactory(kCLiveconnectCID, (nsIFactory *)m_pNSIFactory);
      
    }
}


