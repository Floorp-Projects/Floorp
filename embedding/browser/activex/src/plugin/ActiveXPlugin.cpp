/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "stdafx.h"

static CActiveXPlugin *gpFactory = NULL;

extern "C" NS_EXPORT nsresult 
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aContractID,
             nsIFactory **aFactory)
{
    if (aClass.Equals(kIPluginIID))
	{
        if (gpFactory)
		{
			gpFactory->AddRef();
            *aFactory = (nsIFactory *) gpFactory;
            return NS_OK;
        }

        CActiveXPlugin *pFactory = new CActiveXPlugin();
        if (pFactory == NULL)
		{
            return NS_ERROR_OUT_OF_MEMORY;
		}
        pFactory->AddRef();
        gpFactory = pFactory;
        *aFactory = pFactory;
        return NS_OK;
    }
    
	return NS_ERROR_FAILURE;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr)
{
    return (_Module.GetLockCount() == 0);
}

///////////////////////////////////////////////////////////////////////////////
	
CActiveXPlugin::CActiveXPlugin()
{
	NS_INIT_REFCNT();
}


CActiveXPlugin::~CActiveXPlugin()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation


NS_IMPL_ADDREF(CActiveXPlugin)
NS_IMPL_RELEASE(CActiveXPlugin)

nsresult CActiveXPlugin::QueryInterface(const nsIID& aIID, void** aInstancePtrResult)
{
	NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
	if (nsnull == aInstancePtrResult)
	{
		return NS_ERROR_NULL_POINTER;
	}

	*aInstancePtrResult = NULL;

	if (aIID.Equals(kISupportsIID))
	{
		*aInstancePtrResult = (void*) ((nsIPlugin*)this);
		AddRef();
		return NS_OK;
	}

	if (aIID.Equals(kIFactoryIID))
	{
		*aInstancePtrResult = (void*) ((nsIPlugin*)this);
		AddRef();
		return NS_OK;
	}

	if (aIID.Equals(kIPluginIID))
	{
		*aInstancePtrResult = (void*) ((nsIPlugin*)this);
		AddRef();
		return NS_OK;
	}

	return NS_NOINTERFACE;
}

///////////////////////////////////////////////////////////////////////////////
// nsIFactory overrides

NS_IMETHODIMP CActiveXPlugin::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    CActiveXPluginInstance *pInst = new CActiveXPluginInstance();
    if (pInst == NULL) 
	{
        return NS_ERROR_OUT_OF_MEMORY;
	}
    pInst->AddRef();
    *aResult = pInst;
	return NS_OK;
}

NS_IMETHODIMP CActiveXPlugin::LockFactory(PRBool aLock)
{
	if (aLock)
	{
		_Module.Lock();
	}
	else
	{
		_Module.Unlock();
	}
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIPlugin overrides

static const char *gpszMime = "application/x-oleobject:smp:Mozilla ActiveX Control Plug-in";
static const char *gpszPluginName = "Mozilla ActiveX Control Plug-in";
static const char *gpszPluginDesc = "ActiveX control host";

NS_IMETHODIMP CActiveXPlugin::CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, const char* aPluginMIMEType, void **aResult)
{
	return CreateInstance(aOuter, aIID, aResult);
}

NS_IMETHODIMP CActiveXPlugin::Initialize()
{
	return NS_OK;
}


NS_IMETHODIMP CActiveXPlugin::Shutdown(void)
{
	return NS_OK;
}


NS_IMETHODIMP CActiveXPlugin::GetMIMEDescription(const char* *resultingDesc)
{
    *resultingDesc = gpszMime;
    return NS_OK;
}


NS_IMETHODIMP CActiveXPlugin::GetValue(nsPluginVariable variable, void *value)
{
    nsresult err = NS_OK;
    if (variable == nsPluginVariable_NameString)
	{
        *((char **)value) = const_cast<char *>(gpszPluginName);
	}
    else if (variable == nsPluginVariable_DescriptionString)
	{
        *((char **)value) = const_cast<char *>(gpszPluginDesc);
	}
    else
	{
        err = NS_ERROR_FAILURE;
	}
    return err;
}
