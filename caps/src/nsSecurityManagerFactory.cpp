/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
/*Factory for internal browser security resource managers*/

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIScriptSecurityManager.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsCodebasePrincipal.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCodebasePrincipal)

static NS_IMETHODIMP
Construct_nsIScriptSecurityManager(nsISupports *aOuter, REFNSIID aIID, 
								   void **aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;
	*aResult = nsnull;
	if (aOuter)
		return NS_ERROR_NO_AGGREGATION;
	nsScriptSecurityManager *obj = nsScriptSecurityManager::GetScriptSecurityManager();
	if (!obj) 
		return NS_ERROR_OUT_OF_MEMORY;
	if (NS_FAILED(obj->QueryInterface(aIID, aResult)))
		return NS_ERROR_FAILURE;
	return NS_OK;
}

static nsModuleComponentInfo components[] =
{
    { NS_SCRIPTSECURITYMANAGER_CLASSNAME, 
      NS_SCRIPTSECURITYMANAGER_CID, 
      NS_SCRIPTSECURITYMANAGER_CONTRACTID,
      Construct_nsIScriptSecurityManager 
    },

    { NS_CODEBASEPRINCIPAL_CLASSNAME, 
      NS_CODEBASEPRINCIPAL_CID, 
      NS_CODEBASEPRINCIPAL_CONTRACTID,
      nsCodebasePrincipalConstructor
    }
};


NS_IMPL_NSGETMODULE("nsSecurityManagerModule", components);


