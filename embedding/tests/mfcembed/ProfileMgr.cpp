/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Local Includes
#include "stdafx.h"
#include "mfcembed.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"

// Mozilla Includes
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIRegistry.h"
#include "nsIProfile.h"

// Constants
#define kRegistryGlobalPrefsSubtreeString (NS_LITERAL_STRING("global-prefs"))
#define kRegistryShowProfilesAtStartup "start-show-dialog"

//*****************************************************************************
//***    CProfileMgr: Object Management
//*****************************************************************************

CProfileMgr::CProfileMgr()
{
}

CProfileMgr::~CProfileMgr()
{
}


//*****************************************************************************
//***    CProfileMgr: Public Methods
//*****************************************************************************
    
nsresult CProfileMgr::StartUp()
{
    nsresult rv;
         
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
        
    PRInt32 profileCount;
    rv = profileService->GetProfileCount(&profileCount);
    if (NS_FAILED(rv)) return rv;
    if (profileCount == 0)
    {
        // Make a new default profile
        NS_NAMED_LITERAL_STRING(newProfileName, "default");

        rv = profileService->CreateNewProfile(newProfileName.get(), nsnull, nsnull, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
        rv = profileService->SetCurrentProfile(newProfileName.get());
        if (NS_FAILED(rv)) return rv;
    }
    else
    {
        // Use our flag here to check for whether to show profile mgr UI. If the flag
        // says don't show it, just start with the last used profile.
        
        PRBool showIt;
        rv = GetShowDialogOnStart(&showIt);
                        
        if (NS_FAILED(rv) || (profileCount > 1 && showIt))
        {
            DoManageProfilesDialog(TRUE);
        }
        else
        {
            // GetCurrentProfile returns the profile which was last used but is not nescesarily
            // active. Call SetCurrentProfile to make it installed and active.
            
            nsXPIDLString   currProfileName;
            rv = profileService->GetCurrentProfile(getter_Copies(currProfileName));
            if (NS_FAILED(rv)) return rv;
            rv = profileService->SetCurrentProfile(currProfileName);
            if (NS_FAILED(rv)) return rv;
        }    
    }

    return NS_OK;
}

nsresult CProfileMgr::DoManageProfilesDialog(PRBool bAtStartUp)
{
    CProfilesDlg    dialog;
    nsresult        rv;
    PRBool          showIt;

    rv = GetShowDialogOnStart(&showIt);
    dialog.m_bAtStartUp = bAtStartUp;
    dialog.m_bAskAtStartUp = NS_SUCCEEDED(rv) ? showIt : TRUE;

    if (dialog.DoModal() == IDOK)
    {
        SetShowDialogOnStart(dialog.m_bAskAtStartUp);
         
        nsCOMPtr<nsIProfile> profileService = 
                 do_GetService(NS_PROFILE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
               rv = profileService->SetCurrentProfile(dialog.m_SelectedProfile.get());
    }
    return NS_OK;
}
    
 
//*****************************************************************************
//***    CProfileMgr: Protected Methods
//*****************************************************************************
     
nsresult CProfileMgr::GetShowDialogOnStart(PRBool* showIt)
{
    nsresult rv = NS_OK;
        
    *showIt = PR_TRUE;
                
    nsCOMPtr<nsIRegistry> registry(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationRegistry);
    if (NS_FAILED(rv)) return rv;

    nsRegistryKey profilesTreeKey;
    
    rv = registry->GetKey(nsIRegistry::Common, 
                          kRegistryGlobalPrefsSubtreeString.get(), 
                          &profilesTreeKey);

    if (NS_SUCCEEDED(rv)) 
    {
        PRInt32 flagValue;
        rv = registry->GetInt(profilesTreeKey, 
                              kRegistryShowProfilesAtStartup, 
                              &flagValue);
         
        if (NS_SUCCEEDED(rv))
            *showIt = (flagValue != 0);
    }
    return rv;        
}

nsresult CProfileMgr::SetShowDialogOnStart(PRBool showIt)
{
    nsresult rv = NS_OK;
                        
    nsCOMPtr<nsIRegistry> registry(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationRegistry);
    if (NS_FAILED(rv)) return rv;

    nsRegistryKey profilesTreeKey;
    
    rv = registry->GetKey(nsIRegistry::Common, 
                          kRegistryGlobalPrefsSubtreeString.get(), 
                          &profilesTreeKey);

    if (NS_FAILED(rv)) 
    {
        rv = registry->AddKey(nsIRegistry::Common, 
                              kRegistryGlobalPrefsSubtreeString.get(), 
                              &profilesTreeKey);
    }
    if (NS_SUCCEEDED(rv))
    {
    
        rv = registry->SetInt(profilesTreeKey, 
                              kRegistryShowProfilesAtStartup, 
                              showIt);
    }
    
    return rv;        
}

