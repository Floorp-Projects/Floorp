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
 * The Original Code is Mozilla
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *           Rajiv Dayal <rdayal@netscape.com>
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
#include "nsCOMPtr.h"
#include "objbase.h"
#include "nsISupports.h"

#include "nsIGenericFactory.h"
#include "nsIObserverService.h"
#include "nsIAppStartupNotifier.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsICategoryManager.h"
#include "nsCRT.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"

#include "PalmSyncImp.h"
#include "nsPalmSyncSupport.h"
#include "Registry.h"

/** Implementation of the IPalmSyncSupport interface.
 *  Use standard implementation of nsISupports stuff.
 *  This also implements the Observors for App Startup
 *  at which time it will register the MSCOM interface
 *  for PalmSync used for IPC with the Palm Conduits
 *  
 */

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPalmSyncSupport, nsIPalmSyncSupport, nsIObserver);

static NS_METHOD nsPalmSyncRegistrationProc(nsIComponentManager *aCompMgr,
                   nsIFile *aPath, const char *registryLocation, const char *componentType,
                   const nsModuleComponentInfo *info)
{
    
  nsresult rv;

  nsCOMPtr<nsICategoryManager> categoryManager(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) 
      rv = categoryManager->AddCategoryEntry(APPSTARTUP_CATEGORY, "PalmSync Support", 
                  "service," NS_IPALMSYNCSUPPORT_CONTRACTID, PR_TRUE, PR_TRUE, nsnull);
 
  return rv ;

}

NS_IMETHODIMP
nsPalmSyncSupport::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
    nsresult rv = NS_OK ;

    if (!strcmp(aTopic, "profile-after-change"))
        return InitializePalmSyncSupport();

    if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
        return ShutdownPalmSyncSupport();

    nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1", &rv));
    if (NS_FAILED(rv)) return rv;
 
    rv = observerService->AddObserver(this,"profile-after-change", PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
}


nsPalmSyncSupport::nsPalmSyncSupport()
: m_dwRegister(0),
  m_nsPalmSyncFactory(nsnull)
{
   NS_INIT_ISUPPORTS();
}

nsPalmSyncSupport::~nsPalmSyncSupport()
{
}

NS_IMETHODIMP
nsPalmSyncSupport::InitializePalmSyncSupport()
{
    ::CoInitialize(nsnull) ;

    if (m_nsPalmSyncFactory == nsnull)    // No Registering if already done.  Sanity Check!!
    {
        m_nsPalmSyncFactory = new CPalmSyncFactory();

        if (m_nsPalmSyncFactory != nsnull)
        {
            HRESULT hr = ::CoRegisterClassObject(CLSID_CPalmSyncImp, \
                                                 m_nsPalmSyncFactory, \
                                                 CLSCTX_LOCAL_SERVER, \
                                                 REGCLS_MULTIPLEUSE, \
                                                 &m_dwRegister);

            if (FAILED(hr))
            {
                m_nsPalmSyncFactory->Release() ;
                m_nsPalmSyncFactory = nsnull;
                return NS_ERROR_FAILURE;
            }
	    RegisterPalmSync();
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsPalmSyncSupport::ShutdownPalmSyncSupport()
{
    if (m_dwRegister != 0)
        ::CoRevokeClassObject(m_dwRegister);

    if (m_nsPalmSyncFactory != nsnull)
    {
        m_nsPalmSyncFactory->Release();
        m_nsPalmSyncFactory = nsnull;
    }

    ::CoUninitialize();

    return NS_OK ;
}

NS_IMETHODIMP
nsPalmSyncSupport::RegisterPalmSync()
{
    return RegisterServerForPalmSync(CLSID_CPalmSyncImp, "Mozilla PalmSync", "mozMapi", "mozMapi.1");
}

NS_IMETHODIMP
nsPalmSyncSupport::UnRegisterPalmSync()
{
    return UnregisterServerForPalmSync(CLSID_CPalmSyncImp, "mozMapi", "mozMapi.1");
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsPalmSyncSupport);

// The list of components we register
static const nsModuleComponentInfo components[] = 
{
    {
        NS_IPALMSYNCSUPPORT_CLASSNAME,
        NS_IPALMSYNCSUPPORT_CID,
        NS_IPALMSYNCSUPPORT_CONTRACTID,
        nsPalmSyncSupportConstructor,
        nsPalmSyncRegistrationProc,
        nsnull
    }
};

NS_IMPL_NSGETMODULE(PalmSyncModule, components);

