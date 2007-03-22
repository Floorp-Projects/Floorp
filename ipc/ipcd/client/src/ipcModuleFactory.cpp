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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsICategoryManager.h"
#include "ipcdclient.h"
#include "ipcService.h"
#include "ipcConfig.h"
#include "ipcCID.h"

//-----------------------------------------------------------------------------
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//-----------------------------------------------------------------------------
NS_GENERIC_FACTORY_CONSTRUCTOR(ipcService)

// enable this code to make the IPC service auto-start.
#if 0
NS_METHOD
ipcServiceRegisterProc(nsIComponentManager *aCompMgr,
                       nsIFile *aPath,
                       const char *registryLocation,
                       const char *componentType,
                       const nsModuleComponentInfo *info)
{
    //
    // add ipcService to the XPCOM startup category
    //
    nsCOMPtr<nsICategoryManager> catman(do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
    if (catman) {
        nsXPIDLCString prevEntry;
        catman->AddCategoryEntry(NS_XPCOM_STARTUP_OBSERVER_ID, "ipcService",
                                 IPC_SERVICE_CONTRACTID, PR_TRUE, PR_TRUE,
                                 getter_Copies(prevEntry));
    }
    return NS_OK;
}

NS_METHOD
ipcServiceUnregisterProc(nsIComponentManager *aCompMgr,
                         nsIFile *aPath,
                         const char *registryLocation,
                         const nsModuleComponentInfo *info)
{
    nsCOMPtr<nsICategoryManager> catman(do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
    if (catman)
        catman->DeleteCategoryEntry(NS_XPCOM_STARTUP_OBSERVER_ID, 
                                    IPC_SERVICE_CONTRACTID, PR_TRUE);
    return NS_OK;
}
#endif

//-----------------------------------------------------------------------------
// extensions

#include "ipcLockService.h"
#include "ipcLockCID.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(ipcLockService, Init)

#include "tmTransactionService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(tmTransactionService)

#ifdef BUILD_DCONNECT

#include "ipcDConnectService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(ipcDConnectService, Init)

// enable this code to make the IPC DCONNECT service auto-start.
NS_METHOD
ipcDConnectServiceRegisterProc(nsIComponentManager *aCompMgr,
                               nsIFile *aPath,
                               const char *registryLocation,
                               const char *componentType,
                               const nsModuleComponentInfo *info)
{
    //
    // add ipcService to the XPCOM startup category
    //
    nsCOMPtr<nsICategoryManager> catman(do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
    if (catman) {
        nsXPIDLCString prevEntry;
        catman->AddCategoryEntry(NS_XPCOM_STARTUP_OBSERVER_ID, "ipcDConnectService",
                                 IPC_DCONNECTSERVICE_CONTRACTID, PR_TRUE, PR_TRUE,
                                 getter_Copies(prevEntry));
    }
    return NS_OK;
}

NS_METHOD
ipcDConnectServiceUnregisterProc(nsIComponentManager *aCompMgr,
                                 nsIFile *aPath,
                                 const char *registryLocation,
                                 const nsModuleComponentInfo *info)
{
    nsCOMPtr<nsICategoryManager> catman(do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
    if (catman)
        catman->DeleteCategoryEntry(NS_XPCOM_STARTUP_OBSERVER_ID, 
                                    IPC_DCONNECTSERVICE_CONTRACTID, PR_TRUE);
    return NS_OK;
}

#endif // BUILD_DCONNECT

//-----------------------------------------------------------------------------
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//-----------------------------------------------------------------------------
static const nsModuleComponentInfo components[] = {
  { IPC_SERVICE_CLASSNAME,
    IPC_SERVICE_CID,
    IPC_SERVICE_CONTRACTID,
    ipcServiceConstructor },
    /*
    ipcServiceRegisterProc,
    ipcServiceUnregisterProc },
    */
  //
  // extensions go here:
  //
  { IPC_LOCKSERVICE_CLASSNAME,
    IPC_LOCKSERVICE_CID,
    IPC_LOCKSERVICE_CONTRACTID,
    ipcLockServiceConstructor },
  { IPC_TRANSACTIONSERVICE_CLASSNAME,
    IPC_TRANSACTIONSERVICE_CID,
    IPC_TRANSACTIONSERVICE_CONTRACTID,
    tmTransactionServiceConstructor },

#ifdef BUILD_DCONNECT
  { IPC_DCONNECTSERVICE_CLASSNAME,
    IPC_DCONNECTSERVICE_CID,
    IPC_DCONNECTSERVICE_CONTRACTID,
    ipcDConnectServiceConstructor,
    ipcDConnectServiceRegisterProc,
    ipcDConnectServiceUnregisterProc },
#endif
};

//-----------------------------------------------------------------------------

PR_STATIC_CALLBACK(nsresult)
ipcdclient_init(nsIModule *module)
{
  return IPC_Init();
}

PR_STATIC_CALLBACK(void)
ipcdclient_shutdown(nsIModule *module)
{
  IPC_Shutdown();
}

//-----------------------------------------------------------------------------
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//-----------------------------------------------------------------------------
NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(ipcdclient, components,
                                   ipcdclient_init,
                                   ipcdclient_shutdown)
