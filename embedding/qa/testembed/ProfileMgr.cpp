/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

// Local Includes
#include "stdafx.h"
#include "Testembed.h"
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
         
    //NS_WITH_SERVICE(nsIProfile, profileService, NS_PROFILE_CONTRACTID, &rv);
    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID,&rv));
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
        // Use our flag here to chek for whether to show profile mgr UI. Ifc the flag
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
         
        //NS_WITH_SERVICE(nsIProfile, profileService, NS_PROFILE_CONTRACTID, &rv);
	    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID,&rv));
        if (NS_FAILED(rv))
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

