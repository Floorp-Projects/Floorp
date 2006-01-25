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
#ifdef MOZ_THUNDERBIRD
#include "nsIExtensionManager.h"
#endif
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include "PalmSyncImp.h"
#include "nsPalmSyncSupport.h"
#include "Registry.h"
#include <shellapi.h> // needed for ShellExecute

#define EXTENSION_ID "p@m" // would ideally be palmsync@mozilla.org, but palm can't handle the long file path
#define EXECUTABLE_FILENAME "PalmSyncInstall.exe"
#define PREF_CONDUIT_REGISTERED "extensions.palmsync.conduitRegistered"


/** Implementation of the IPalmSyncSupport interface.
 *  Use standard implementation of nsISupports stuff.
 *  This also implements the Observors for App Startup
 *  at which time it will register the MSCOM interface
 *  for PalmSync used for IPC with the Palm Conduits
 *  
 */

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPalmSyncSupport, nsIPalmSyncSupport, nsIObserver)

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

static NS_METHOD nsPalmSyncUnRegistrationProc(nsIComponentManager *aCompMgr,
                   nsIFile *aPath, const char *registryLocation, const nsModuleComponentInfo *info)
{    
    nsresult rv;
    nsCOMPtr<nsICategoryManager> categoryManager(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) 
        rv = categoryManager->DeleteCategoryEntry(APPSTARTUP_CATEGORY, "PalmSync Support", PR_TRUE);
 
    return rv;
}

NS_IMETHODIMP
nsPalmSyncSupport::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
    nsresult rv = NS_OK ;

    // If nsIAppStartupNotifer tells us the app is starting up, then register
    // our observer topics and return.
    if (!strcmp(aTopic, "app-startup"))
    {
    nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1", &rv));
      NS_ENSURE_SUCCESS(rv, rv);
 
    rv = observerService->AddObserver(this,"profile-after-change", PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = observerService->AddObserver(this, "em-action-requested", PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);      

      rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
}
    // otherwise, take the appropriate action based on the topic
    else if (!strcmp(aTopic, "profile-after-change"))
    {
#ifdef MOZ_THUNDERBIRD
        // we can't call installPalmSync in app-startup because the extension manager hasn't been initialized yet. 
        // so we need to wait until the profile-after-change notification has fired. 
        rv = LaunchPalmSyncInstallExe(); 
#endif
        rv |= InitializePalmSyncSupport();
    } 
    else if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
        rv = ShutdownPalmSyncSupport();
#ifdef MOZ_THUNDERBIRD
    else if (aSubject && !strcmp(aTopic, "em-action-requested") && !nsCRT::strcmp(aData, NS_LITERAL_STRING("item-uninstalled").get()))
    {
        // make sure the subject is our extension.
        nsCOMPtr<nsIUpdateItem> updateItem (do_QueryInterface(aSubject, &rv));
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString extensionName;
        updateItem->GetId(extensionName);

        if (extensionName.EqualsLiteral(EXTENSION_ID))
        {
            nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
            NS_ENSURE_SUCCESS(rv,rv);
           
            // clear the conduit pref so we'll re-run PalmSyncInstall.exe the next time the extension is installed.
            rv = prefBranch->ClearUserPref(PREF_CONDUIT_REGISTERED); 

            nsCOMPtr<nsILocalFile> palmSyncInstall;
            rv = GetPalmSyncInstall(getter_AddRefs(palmSyncInstall));
            NS_ENSURE_SUCCESS(rv, rv);

            nsCAutoString nativePath;
            palmSyncInstall->GetNativePath(nativePath);
        
            LONG r = (LONG) ::ShellExecuteA( NULL, NULL, nativePath.get(), "/u", NULL, SW_SHOWNORMAL);  // silent uninstall
        }
    }
#endif

    return rv;
}

nsPalmSyncSupport::nsPalmSyncSupport()
: m_dwRegister(0),
  m_nsPalmSyncFactory(nsnull)
{
}

nsPalmSyncSupport::~nsPalmSyncSupport()
{
}

#ifdef MOZ_THUNDERBIRD
nsresult nsPalmSyncSupport::GetPalmSyncInstall(nsILocalFile ** aLocalFile)
{
    nsresult rv;
    nsCOMPtr<nsIExtensionManager> em(do_GetService("@mozilla.org/extensions/manager;1"));
    NS_ENSURE_TRUE(em, NS_ERROR_FAILURE);

    nsCOMPtr<nsIInstallLocation> installLocation;
    rv = em->GetInstallLocation(NS_LITERAL_STRING(EXTENSION_ID), getter_AddRefs(installLocation));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> palmSyncInstallExe;
    rv = installLocation->GetItemFile(NS_LITERAL_STRING(EXTENSION_ID), NS_LITERAL_STRING(EXECUTABLE_FILENAME), getter_AddRefs(palmSyncInstallExe));
    NS_ENSURE_SUCCESS(rv, rv);

    palmSyncInstallExe->QueryInterface(NS_GET_IID(nsILocalFile), (void **) aLocalFile);
    return rv;
}

nsresult nsPalmSyncSupport::LaunchPalmSyncInstallExe()
{
    // we only want to call PalmSyncInstall.exe the first time the app is run after installing the
    // palm sync extension. We use PREF_CONDUIT_REGISTERED to keep track of that
    // information.

    nsresult rv;    
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    
    PRBool conduitIsRegistered = PR_FALSE;
    rv = prefBranch->GetBoolPref(PREF_CONDUIT_REGISTERED, &conduitIsRegistered);
    NS_ENSURE_SUCCESS(rv,rv);

    if (!conduitIsRegistered)
    {        
        rv = prefBranch->SetBoolPref(PREF_CONDUIT_REGISTERED, PR_TRUE); 
        nsCOMPtr<nsILocalFile> palmSyncInstall;
        rv = GetPalmSyncInstall(getter_AddRefs(palmSyncInstall));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = palmSyncInstall->Launch();             
    }

    return rv;
}
#endif

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
    return RegisterServerForPalmSync(CLSID_CPalmSyncImp, "Mozilla PalmSync", "MozillaPalmSync", "MozillaPalmSync.1");
}

NS_IMETHODIMP
nsPalmSyncSupport::UnRegisterPalmSync()
{
    return UnregisterServerForPalmSync(CLSID_CPalmSyncImp, "MozillaPalmSync", "MozillaPalmSync.1");
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsPalmSyncSupport)

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

NS_IMPL_NSGETMODULE(PalmSyncModule, components)

