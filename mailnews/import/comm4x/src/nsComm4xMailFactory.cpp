/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Srilatha Moturi <srilatha@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
    Comm4xMail import module
*/

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIStringBundle.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsComm4xMailImport.h"
#include "nsCRT.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsComm4xMailStringBundle.h"
#include "Comm4xMailDebugLog.h"
#include "nsComm4xProfile.h"

static NS_DEFINE_CID(kComm4xMailImportCID,      NS_COMM4XMAILIMPORT_CID);



NS_METHOD Comm4xMailRegister(nsIComponentManager *aCompMgr,
                       nsIFile *aPath, const char *registryLocation,
                       const char *componentType,
                       const nsModuleComponentInfo *info)
{   
    nsresult rv;

    nsCOMPtr<nsICategoryManager> catMan = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsXPIDLCString  replace;
        char *theCID = kComm4xMailImportCID.ToString();
        rv = catMan->AddCategoryEntry("mailnewsimport", theCID, kComm4xMailSupportsString, PR_TRUE, PR_TRUE, getter_Copies(replace));
        nsCRT::free(theCID);
    }

    if (NS_FAILED(rv)) {
        IMPORT_LOG0("*** ERROR: Problem registering Comm4xMail Import component in the category manager\n");
    }

    return rv;
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsComm4xMailImport)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(ImportComm4xMailImpl, Initialize)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsComm4xProfile)

static nsModuleComponentInfo components[] = {
    {   "Comm4xMail Import Component", 
        NS_COMM4XMAILIMPORT_CID,
        "@mozilla.org/import/import-comm4xMail;1", 
        nsComm4xMailImportConstructor,
        Comm4xMailRegister,
        nsnull
    },
    {
       "Comm4xMail Import Mail Implementation",
       NS_COMM4XMAILIMPL_CID,
       NS_COMM4XMAILIMPL_CONTRACTID,
       ImportComm4xMailImplConstructor
    },
    {   NS_ICOMM4XPROFILE_CLASSNAME, 
        NS_ICOMM4XPROFILE_CID,
        NS_ICOMM4XPROFILE_CONTRACTID, 
        nsComm4xProfileConstructor
    }
};

PR_STATIC_CALLBACK(void)
comm4xMailModuleDtor(nsIModule* self)
{

}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsComm4xMailImportModule, components, comm4xMailModuleDtor)

