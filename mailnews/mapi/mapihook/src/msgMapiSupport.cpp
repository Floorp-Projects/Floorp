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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Krishna Mohan Khandrika (kkhandrika@netscape.com)
 *                 Rajiv Dayal <rdayal@netscape.com>
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

#include "msgMapiSupport.h"
#include "nsMapiRegistryUtils.h" 
#include "nsMapiRegistry.h"
#include "msgMapiImp.h"

/** Implementation of the nsIMapiSupport interface.
 *  Use standard implementation of nsISupports stuff.
 */

NS_IMPL_THREADSAFE_ISUPPORTS2(nsMapiSupport, nsIMapiSupport, nsIObserver);

static NS_METHOD nsMapiRegistrationProc(nsIComponentManager *aCompMgr,
                   nsIFile *aPath, const char *registryLocation, const char *componentType,
                   const nsModuleComponentInfo *info)
{
    
  nsresult rv;

  nsCOMPtr<nsICategoryManager> categoryManager(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) 
      rv = categoryManager->AddCategoryEntry(APPSTARTUP_CATEGORY, "Mapi Support", 
                  "service," NS_IMAPISUPPORT_CONTRACTID, PR_TRUE, PR_TRUE, nsnull);
 
  return rv ;

}

NS_IMETHODIMP
nsMapiSupport::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
    nsresult rv = NS_OK ;

    if (!nsCRT::strcmp(aTopic, "profile-after-change"))
        return InitializeMAPISupport();

    if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
        return ShutdownMAPISupport();

    nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1", &rv));
    if (NS_FAILED(rv)) return rv;
 
    rv = observerService->AddObserver(this,"profile-after-change", PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
    if (NS_FAILED(rv))  return rv;

    return rv;
}


nsMapiSupport::nsMapiSupport()
: m_dwRegister(0),
  m_nsMapiFactory(nsnull)
{
}

nsMapiSupport::~nsMapiSupport()
{
}

NS_IMETHODIMP
nsMapiSupport::InitializeMAPISupport()
{
    ::CoInitialize(nsnull) ;

    if (m_nsMapiFactory == nsnull)    // No Registering if already done.  Sanity Check!!
    {
        m_nsMapiFactory = new CMapiFactory();

        if (m_nsMapiFactory != nsnull)
        {
            HRESULT hr = ::CoRegisterClassObject(CLSID_CMapiImp, \
                                                 m_nsMapiFactory, \
                                                 CLSCTX_LOCAL_SERVER, \
                                                 REGCLS_MULTIPLEUSE, \
                                                 &m_dwRegister);

            if (FAILED(hr))
            {
                m_nsMapiFactory->Release() ;
                m_nsMapiFactory = nsnull;
                return NS_ERROR_FAILURE;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsMapiSupport::ShutdownMAPISupport()
{
    if (m_dwRegister != 0)
        ::CoRevokeClassObject(m_dwRegister);

    if (m_nsMapiFactory != nsnull)
    {
        m_nsMapiFactory->Release();
        m_nsMapiFactory = nsnull;
    }

    ::CoUninitialize();

    return NS_OK ;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMapiRegistry);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMapiSupport);

// The list of components we register
static const nsModuleComponentInfo components[] = 
{
  {
    NS_IMAPIREGISTRY_CLASSNAME, 
    NS_IMAPIREGISTRY_CID,
    NS_IMAPIREGISTRY_CONTRACTID, 
    nsMapiRegistryConstructor
  },

{
    NS_IMAPISUPPORT_CLASSNAME,
    NS_IMAPISUPPORT_CID,
    NS_IMAPISUPPORT_CONTRACTID,
    nsMapiSupportConstructor,
    nsMapiRegistrationProc,
    nsnull
}
};

NS_IMPL_NSGETMODULE(msgMapiModule, components);

