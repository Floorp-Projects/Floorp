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
#include "nsIRegistry.h"
#include "nsIImportService.h"
#include "nsEudoraImport.h"

#include "EudoraDebugLog.h"

static NS_DEFINE_CID(kEudoraImportCID,    	NS_EUDORAIMPORT_CID);
static NS_DEFINE_CID(kImportServiceCID,		NS_IMPORTSERVICE_CID);
static NS_DEFINE_CID(kRegistryCID,			NS_REGISTRY_CID);


///////////////////////////////////////////////////////////////////////
nsresult GetImportModulesRegKey( nsIRegistry *reg, nsRegistryKey *pKey)
{
	nsRegistryKey	nScapeKey;

	nsresult rv = reg->GetSubtree( nsIRegistry::Common, "Netscape", &nScapeKey);
	if (NS_FAILED(rv)) {
		rv = reg->AddSubtree( nsIRegistry::Common, "Netscape", &nScapeKey);
	}
	if (NS_FAILED( rv))
		return( rv);

	nsRegistryKey	iKey;
	rv = reg->GetSubtree( nScapeKey, "Import", &iKey);
	if (NS_FAILED( rv)) {
		rv = reg->AddSubtree( nScapeKey, "Import", &iKey);
	}
	
	if (NS_FAILED( rv))
		return( rv);

	rv = reg->GetSubtree( iKey, "Modules", pKey);
	if (NS_FAILED( rv)) {
		rv = reg->AddSubtree( iKey, "Modules", pKey);
	}

	return( rv);
}

NS_METHOD EudoraRegister(nsIComponentManager *aCompMgr,
                                            nsIFileSpec *aPath,
                                            const char *registryLocation,
                                            const char *componentType)
{	
	nsresult rv;

    NS_WITH_SERVICE( nsIRegistry, reg, kRegistryCID, &rv);
    if (NS_FAILED(rv)) {
	    IMPORT_LOG0( "*** Import Eudora, ERROR GETTING THE Registry\n");
    	return rv;
    }
    
    rv = reg->OpenDefault();
    if (NS_FAILED(rv)) {
	    IMPORT_LOG0( "*** Import Eudora, ERROR OPENING THE REGISTRY\n");
    	return( rv);
    }
    
	nsRegistryKey	importKey;
	
	rv = GetImportModulesRegKey( reg, &importKey);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Import Eudora, ERROR getting Netscape/Import registry key\n");
		return( rv);
	}		
		    
	nsRegistryKey	key;
    rv = reg->AddSubtree( importKey, "Eudora", &key);
    if (NS_FAILED(rv)) return( rv);
    
    rv = reg->SetString( key, "Supports", kEudoraSupportsString);
    if (NS_FAILED(rv)) return( rv);
    char *myCID = kEudoraImportCID.ToString();
    rv = reg->SetString( key, "CLSID", myCID);
    delete [] myCID;
    if (NS_FAILED(rv)) return( rv);  	

	return( rv);
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsEudoraImport)

static nsModuleComponentInfo components[] = {
    {	"Text Import Component", 
		NS_EUDORAIMPORT_CID,
		"component://mozilla/import/import-eudora", 
		nsEudoraImportConstructor,
		&EudoraRegister,
		nsnull
	}
};

NS_IMPL_NSGETMODULE("nsEudoraImportModule", components)


