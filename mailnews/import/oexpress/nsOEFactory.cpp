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
  
/*
	Outlook Express (Win32) import module
*/

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsOEImport.h"
#include "nsCRT.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsOEStringBundle.h"
#include "OEDebugLog.h"

static NS_DEFINE_CID(kOEImportCID,       	NS_OEIMPORT_CID);

////////////////////////////////////////////////////////////////////

NS_METHOD OERegister(nsIComponentManager *aCompMgr,
                     nsIFile *aPath,
                     const char *registryLocation,
                     const char *componentType)
{
	nsresult rv;
	
	nsCOMPtr<nsICategoryManager> catMan = do_GetService( NS_CATEGORYMANAGER_CONTRACTID, &rv);
	if (NS_SUCCEEDED( rv)) {
		nsXPIDLCString	replace;
		char *theCID = kOEImportCID.ToString();
		rv = catMan->AddCategoryEntry( "mailnewsimport", theCID, kOESupportsString, PR_TRUE, PR_TRUE, getter_Copies( replace));
		nsCRT::free( theCID);
	}

	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** ERROR: Problem registering Outlook Express component in the category manager\n");
	}

	return( rv);
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsOEImport)

static nsModuleComponentInfo components[] = {
    {	"Outlook Express Import Component", 
		NS_OEIMPORT_CID,
		"@mozilla.org/import/import-oe;1", 
		nsOEImportConstructor,
		&OERegister,
		nsnull
	}
};

PR_STATIC_CALLBACK(void)
oeModuleDtor(nsIModule* self)
{
	nsOEStringBundle::Cleanup();
}

NS_IMPL_NSGETMODULE_WITH_DTOR("nsOEImport", components, oeModuleDtor)





