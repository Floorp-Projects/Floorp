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
	Text import module
*/

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsTextImport.h"
#include "nsCRT.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsTextStringBundle.h"

#include "TextDebugLog.h"

static NS_DEFINE_CID(kTextImportCID,    	NS_TEXTIMPORT_CID);



NS_METHOD TextRegister(nsIComponentManager *aCompMgr,
                       nsIFile *aPath, const char *registryLocation,
                       const char *componentType,
                       const nsModuleComponentInfo *info)
{	
	nsresult rv;

	nsCOMPtr<nsICategoryManager> catMan = do_GetService( NS_CATEGORYMANAGER_CONTRACTID, &rv);
	if (NS_SUCCEEDED( rv)) {
		nsXPIDLCString	replace;
		char *theCID = kTextImportCID.ToString();
		rv = catMan->AddCategoryEntry( "mailnewsimport", theCID, kTextSupportsString, PR_TRUE, PR_TRUE, getter_Copies( replace));
		nsCRT::free( theCID);
	}

	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** ERROR: Problem registering Text Import component in the category manager\n");
	}

	return( rv);
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextImport)

static const nsModuleComponentInfo components[] = {
    {	"Text Import Component", 
		NS_TEXTIMPORT_CID,
		"@mozilla.org/import/import-text;1", 
		nsTextImportConstructor,
		TextRegister,
		nsnull
	}
};

PR_STATIC_CALLBACK(void)
textModuleDtor(nsIModule* self)
{
	nsTextStringBundle::Cleanup();
}


NS_IMPL_NSGETMODULE_WITH_DTOR(nsTextImportModule, components, textModuleDtor)


