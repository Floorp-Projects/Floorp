/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// File Overview....
//
// Test cases for the nsISelection Interface

#include "stdafx.h"
#include "QaUtils.h"
#include <stdio.h>
#include "nsProfile.h"

CProfile::CProfile(nsIWebBrowser* mWebBrowser)
{
	qaWebBrowser = mWebBrowser;
}


CProfile::~CProfile()
{
}


void CProfile::OnStartTests(UINT nMenuID)
{
	// Calls  all or indivdual test cases on the basis of the 
	// option selected from menu.

	switch(nMenuID)
	{
		case ID_INTERFACES_NSIPROFILE_RUNALLTESTS	:
			RunAllTests();
			break ;
		case ID_INTERFACES_NSIPROFILE_GETPROFILECOUNT :
			GetProfileCount();
			break ;
		case ID_INTERFACES_NSIPROFILE_GETCURRENTPROFILE :
			GetCurrentProfile();
			break ;
		case ID_INTERFACES_NSIPROFILE_SETCURRENTPROFILE :
			SetCurrentProfile();
			break ;
		case ID_INTERFACES_NSIPROFILE_GETPROFILELIST :
			GetProfileList();
			break ;
		case ID_INTERFACES_NSIPROFILE_PROFILEEXISTS :
			ProfileExists();
			break ;
		case ID_INTERFACES_NSIPROFILE_CREATENEWPROFILE :
			CreateNewProfile();
			break ;
		case ID_INTERFACES_NSIPROFILE_RENAMEPROFILE :
			RenameProfile();
			break ;
		case ID_INTERFACES_NSIPROFILE_DELETEPROFILE :
			DeleteProfile();
			break ;
		case ID_INTERFACES_NSIPROFILE_CLONEPROFILE :
			CloneProfile();
			break ;
		case ID_INTERFACES_NSIPROFILE_SHUTDOWNCURRENTPROFILE :
			ShutDownCurrentProfile();
			break ;
	}

}

void CProfile::RunAllTests()
{
    
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);

	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	PRInt32 profileCount;
    rv = oNsProfile->GetProfileCount(&profileCount);
    RvTestResultDlg(rv, "nsIProfile::GetProfileCount() ");

    nsXPIDLString   currProfileName;
    rv = oNsProfile->GetCurrentProfile(getter_Copies(currProfileName));
    RvTestResultDlg(rv, "nsIProfile::GetCurrentProfile() ");

    rv = oNsProfile->SetCurrentProfile(currProfileName);
    RvTestResultDlg(rv, "nsIProfile::SetCurrentProfile() ");


    PRUint32    listLen;
    PRUnichar   **profileList;

    rv = oNsProfile->GetProfileList(&listLen, &profileList);
    RvTestResultDlg(rv, "oNsProfile->GetProfileList");

    for (PRUint32 index = 0; index < listLen; index++)
    {
        CString tmpStr(profileList[index]);
	    RvTestResultDlg(rv, tmpStr);
	}

    PRBool exists = FALSE;
    rv = oNsProfile->ProfileExists(currProfileName, &exists);
    RvTestResultDlg(rv, "oNsProfile->ProfileExists");

	USES_CONVERSION ;

	NS_NAMED_LITERAL_STRING(newProfileName, "New Test");

    rv = oNsProfile->CreateNewProfile(newProfileName.get(), nsnull, nsnull, PR_TRUE);
    RvTestResultDlg(rv, "oNsProfile->CreateNewProfile");

    rv = oNsProfile->RenameProfile(currProfileName, T2W("New default"));
    RvTestResultDlg(rv, "oNsProfile->RenameProfile");

    rv = oNsProfile->DeleteProfile(currProfileName, PR_TRUE);
    RvTestResultDlg(rv, "oNsProfile->DeleteProfile");

    rv = oNsProfile->CloneProfile(currProfileName);
    RvTestResultDlg(rv, "oNsProfile->CloneProfile");

}

void CProfile::GetProfileCount()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);

	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	PRInt32 profileCount;
    rv = oNsProfile->GetProfileCount(&profileCount);
    RvTestResultDlg(rv, "nsIProfile::GetProfileCount() ");
}

void CProfile::GetCurrentProfile()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	nsXPIDLString   currProfileName;
    rv = oNsProfile->GetCurrentProfile(getter_Copies(currProfileName));
    RvTestResultDlg(rv, "nsIProfile::GetCurrentProfile() ");
}

void CProfile::SetCurrentProfile()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	nsXPIDLString   currProfileName;
    rv = oNsProfile->GetCurrentProfile(getter_Copies(currProfileName));
    RvTestResultDlg(rv, "nsIProfile::GetCurrentProfile() ");

    rv = oNsProfile->SetCurrentProfile(currProfileName);
    RvTestResultDlg(rv, "nsIProfile::SetCurrentProfile() ");
}

void CProfile::GetProfileList()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	PRUint32    listLen;
    PRUnichar   **profileList;

    rv = oNsProfile->GetProfileList(&listLen, &profileList);
    RvTestResultDlg(rv, "oNsProfile->GetProfileList");

    for (PRUint32 index = 0; index < listLen; index++)
    {
        CString tmpStr(profileList[index]);
	    RvTestResultDlg(rv, tmpStr);
	}
}

void CProfile::ProfileExists()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	nsXPIDLString   currProfileName;
    rv = oNsProfile->GetCurrentProfile(getter_Copies(currProfileName));
    RvTestResultDlg(rv, "nsIProfile::GetCurrentProfile() ");

	PRBool exists = FALSE;
    rv = oNsProfile->ProfileExists(currProfileName, &exists);
    RvTestResultDlg(rv, "oNsProfile->ProfileExists");
}

void CProfile::CreateNewProfile()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	USES_CONVERSION ;

	rv = oNsProfile->CreateNewProfile(T2W("New Test"), nsnull, nsnull, PR_TRUE);
    RvTestResultDlg(rv, "oNsProfile->CreateNewProfile");
   
}

void CProfile::RenameProfile()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	USES_CONVERSION ;

	nsXPIDLString   currProfileName;
    rv = oNsProfile->GetCurrentProfile(getter_Copies(currProfileName));
    RvTestResultDlg(rv, "nsIProfile::GetCurrentProfile() ");

	rv = oNsProfile->RenameProfile(currProfileName, T2W("New default"));
    RvTestResultDlg(rv, "oNsProfile->RenameProfile");

 

}

void CProfile::DeleteProfile()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}

	USES_CONVERSION ;

	nsXPIDLString   currProfileName;
    rv = oNsProfile->GetCurrentProfile(getter_Copies(currProfileName));
    RvTestResultDlg(rv, "nsIProfile::GetCurrentProfile() ");

    rv = oNsProfile->DeleteProfile(currProfileName, PR_TRUE);
    RvTestResultDlg(rv, "oNsProfile->DeleteProfile");


}

void CProfile::CloneProfile()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}
 
	USES_CONVERSION ;

	nsXPIDLString   currProfileName;
    rv = oNsProfile->GetCurrentProfile(getter_Copies(currProfileName));
    RvTestResultDlg(rv, "nsIProfile::GetCurrentProfile() ");

    rv = oNsProfile->CloneProfile(currProfileName);
    RvTestResultDlg(rv, "oNsProfile->CloneProfile");
}

void CProfile::ShutDownCurrentProfile()
{
	nsCOMPtr<nsIProfile> oNsProfile (do_GetService(NS_PROFILE_CONTRACTID,&rv));

    RvTestResultDlg(rv, "do_GetService",true);
	if (!oNsProfile)
	{
	    RvTestResultDlg(rv, "Cannot get the nsIprofile object");
	    return ;
	}


}

