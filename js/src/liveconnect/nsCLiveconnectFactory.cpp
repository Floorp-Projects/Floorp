/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

#include "nsXPCOM.h"
#include "nsCLiveconnect.h"
#include "nsCLiveconnectFactory.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"

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
             const char *aContractID,
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

extern "C" NS_EXPORT nsresult
JSJ_RegisterLiveConnectFactory()
{
    nsCOMPtr<nsIComponentRegistrar> registrar;
    nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
    if (NS_FAILED(rv))
        return rv;
      
    nsCOMPtr<nsIFactory> factory = new nsCLiveconnectFactory;
    if (factory) {
        return registrar->RegisterFactory(kCLiveconnectCID, "LiveConnect",
                                          "@mozilla.org/liveconnect/liveconnect;1",
                                          factory);
    }
    return NS_ERROR_OUT_OF_MEMORY;
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
}

nsCLiveconnectFactory::~nsCLiveconnectFactory()
{
}
