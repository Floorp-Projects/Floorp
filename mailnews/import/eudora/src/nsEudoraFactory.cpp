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
 * The Original Code is mozilla.org Code.
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

