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
	Eudora import module
*/

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsEudoraImport.h"
#include "nsCRT.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsEudoraStringBundle.h"
#include "EudoraDebugLog.h"

static NS_DEFINE_CID(kEudoraImportCID,    	NS_EUDORAIMPORT_CID);


///////////////////////////////////////////////////////////////////////

NS_METHOD EudoraRegister(nsIComponentManager *aCompMgr,
                         nsIFile *aPath, const char *registryLocation,
                         const char *componentType,
                         const nsModuleComponentInfo *info)
{	
	nsresult rv;

	nsCOMPtr<nsICategoryManager> catMan = do_GetService( NS_CATEGORYMANAGER_CONTRACTID, &rv);
	if (NS_SUCCEEDED( rv)) {
		nsXPIDLCString	replace;
		char *theCID = kEudoraImportCID.ToString();
		rv = catMan->AddCategoryEntry( "mailnewsimport", theCID, kEudoraSupportsString, PR_TRUE, PR_TRUE, getter_Copies( replace));
		nsCRT::free( theCID);
	}

	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** ERROR: Problem registering Eudora component in the category manager\n");
	}

	return( rv);
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsEudoraImport)

static const nsModuleComponentInfo components[] = {
    {	"Text Import Component", 
		NS_EUDORAIMPORT_CID,
		"@mozilla.org/import/import-eudora;1", 
		nsEudoraImportConstructor,
		EudoraRegister,
		nsnull
	}
};

PR_STATIC_CALLBACK(void)
eudoraModuleDtor(nsIModule* self)
{
	nsEudoraStringBundle::Cleanup();
}


NS_IMPL_NSGETMODULE_WITH_DTOR(nsEudoraImportModule, components, eudoraModuleDtor)

