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
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIImportService.h"
#include "nsImportMimeEncode.h"
#include "nsCRT.h"
#include "nsImportStringBundle.h"
#include "ImportDebug.h"


extern NS_METHOD NS_NewImportService(nsISupports* aOuter, REFNSIID aIID, void **aResult);


//----------------------------------------------------------------------

//----------------------------------------
static nsModuleComponentInfo components[] = {
	{	
		"Import Service Component", NS_IMPORTSERVICE_CID,
		"@mozilla.org/import/import-service;1", NS_NewImportService
	},
	{
		"Import Mime Encoder", NS_IMPORTMIMEENCODE_CID,
		"@mozilla.org/import/import-mimeencode;1", nsIImportMimeEncodeImpl::Create
	}
};


PR_STATIC_CALLBACK(void)
importModuleDtor(nsIModule* self)
{
	nsImportStringBundle::Cleanup();
}

NS_IMPL_NSGETMODULE_WITH_DTOR( "nsImportServiceModule", components, importModuleDtor)


