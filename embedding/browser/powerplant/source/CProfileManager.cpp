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

// PPBrowser
#include "CProfileManager.h"
#include "ApplIDs.h"
#include "UMacUnicode.h"

// PowerPlant
#include <LEditText.h>
#include <LTextTableView.h>
#include <LPushButton.h>
#include <LTableMonoGeometry.h>
#include <LTableArrayStorage.h>
#include <LTableSingleSelector.h>
#include <LCheckBox.h>

// Carbon
#include <CFPreferences.h>

// Mozilla
#include "nsIProfile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"

// ANSI
#include <climits>

// Constants

const MessageT    msg_OnNewProfile 	        = 2000;
const MessageT    msg_OnDeleteProfile 	    = 2001;
const MessageT    msg_OnRenameProfile 	    = 2002;

#define kPrefShowProfilesAtStartup "start-show-dialog"

//*****************************************************************************
//***    CProfileManager
//*****************************************************************************

CProfileManager::CProfileManager() :
    LAttachment(msg_AnyMessage,true)
{
}

CProfileManager::~CProfileManager()
{
}

void CProfileManager::StartUp()
{
    nsresult rv;
         
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    ThrowIfNil_(profileService);
        
    PRInt32 profileCount;
    rv = profileService->GetProfileCount(&profileCount);
    ThrowIfError_(rv);
    if (profileCount == 0)
    {
        // Make a new default profile
        NS_NAMED_LITERAL_STRING(newProfileName, "default");

        rv = profileService->CreateNewProfile(newProfileName.get(), nsnull, nsnull, PR_FALSE);
        ThrowIfError_(rv);
        rv = profileService->SetCurrentProfile(newProfileName.get());
        ThrowIfError_(rv);
    }
    else
    {
        // Use our flag here to check for whether to show profile mgr UI. If the flag
        // says don't show it, just start with the last used profile.
        
        PRBool showIt;
        rv = GetShowDialogOnStart(&showIt);
                        
        if (NS_FAILED(rv) || (profileCount > 1 && showIt))
        {
            DoManageProfilesDialog();
        }
        else
        {
            // GetCurrentProfile returns the profile which was last used but is not nescesarily
            // active. Call SetCurrentProfile to make it installed and active.
            
            nsXPIDLString   currProfileName;
            rv = profileService->GetCurrentProfile(getter_Copies(currProfileName));
            ThrowIfError_(rv);
            rv = profileService->SetCurrentProfile(currProfileName);
            ThrowIfError_(rv);
        }    
    }
}

Boolean CProfileManager::DoNewProfileDialog(char *outName, UInt32 bufSize)
{
    Boolean confirmed;
    StDialogHandler	theHandler(dlog_NewProfile, LCommander::GetTopCommander());
    LWindow			 *theDialog = theHandler.GetDialog();
    
    ThrowIfNil_(theDialog);
    LEditText *responseText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Name'));
    ThrowIfNil_(responseText);
    theDialog->SetLatentSub(responseText);

    theDialog->Show();
    theDialog->Select();
	
  	while (true)  // This is our modal dialog event loop
  	{				
  		MessageT	hitMessage = theHandler.DoDialog();
  		
  		if (hitMessage == msg_OK)
  		{
  		    Str255 pStr;
  		    UInt32 outLen;
  		    
 		    responseText->GetDescriptor(pStr);
 		    outLen = pStr[0] >= bufSize ? bufSize - 1 : pStr[0];
 		    memcpy(outName, &pStr[1], outLen);
 		    outName[outLen] = '\0'; 
            confirmed = PR_TRUE;
     		break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    confirmed = PR_FALSE;
   		    break;
   		}
  	}
  	return confirmed;
}


void CProfileManager::DoManageProfilesDialog()
{
    nsresult rv;
    StDialogHandler	theHandler(dlog_ManageProfiles, LCommander::GetTopCommander());
    LWindow			 *theDialog = theHandler.GetDialog();

    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    ThrowIfNil_(profileService);
        
    // Set up the dialog by filling the list of current profiles
    LTextTableView *table = (LTextTableView*) theDialog->FindPaneByID('List');    
    ThrowIfNil_(table);
    LPushButton *deleteButton = (LPushButton *) theDialog->FindPaneByID('Dele');
    ThrowIfNil_(deleteButton);
    
    //Str255 pascalStr;
    nsAutoString unicodeStr;
    nsCAutoString cStr;
    char dataBuf[256];
    UInt32 dataSize;
    
    // PowerPlant stuff to set up the list view
    STableCell selectedCell(1, 1);
    SDimension16 tableSize;
    TableIndexT rows, cols;

    table->GetFrameSize(tableSize);
	table->SetTableGeometry(new LTableMonoGeometry(table, tableSize.width, 16));
	table->SetTableStorage(new LTableArrayStorage(table, 0UL));
	table->SetTableSelector(new LTableSingleSelector(table));
	table->InsertCols(1, 0);

    // Get the name of the current profile so we can select it
    nsXPIDLString   currProfileName;
    profileService->GetCurrentProfile(getter_Copies(currProfileName));
    
    // Get the list of profile names and add them to the list
    PRUint32 listLen;
    PRUnichar **profileList;
    rv = profileService->GetProfileList(&listLen, &profileList);
    ThrowIfError_(rv);
    
    for (PRUint32 index = 0; index < listLen; index++)
    {
          CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(profileList[index]), cStr);
          table->InsertRows(1, LONG_MAX, cStr.get(), cStr.Length(), true);
          
          if (nsCRT::strcmp(profileList[index], currProfileName.get()) == 0)
            selectedCell.row = index + 1;
    }
    
    PRInt32 numProfiles;
    rv = profileService->GetProfileCount(&numProfiles);
    ThrowIfError_(rv);    
    (numProfiles > 1) ? deleteButton->Enable() : deleteButton->Disable();
    table->SelectCell(selectedCell);


    // Handle the "Ask At StartUp" checkbox    
    LCheckBox *showAtStartCheck = (LCheckBox*) theDialog->FindPaneByID('Show');
    ThrowIfNil_(showAtStartCheck);
    PRBool showIt;
    rv = GetShowDialogOnStart(&showIt);
    if (NS_FAILED(rv))
        showIt = PR_TRUE;
    showAtStartCheck->SetValue(showIt);

    
    theDialog->Show();
    theDialog->Select();
	
  	while (true)  // This is our modal dialog event loop
  	{				
  		MessageT	hitMessage = theHandler.DoDialog();
  		
  		if (hitMessage == msg_OK)
  		{
  		    theDialog->Hide();
            SetShowDialogOnStart(showAtStartCheck->GetValue());
   		    selectedCell = table->GetFirstSelectedCell();
   		    if (selectedCell.row > 0)
   		    {
   		        dataSize = sizeof(dataBuf) - 1;
   		        table->GetCellData(selectedCell, dataBuf, dataSize);
   		        dataBuf[dataSize] = '\0';
                CPlatformUCSConversion::GetInstance()->PlatformToUCS(nsDependentCString(dataBuf), unicodeStr);
   		        rv = profileService->SetCurrentProfile(unicodeStr.get());
            }
  		    break;
  		}
        else if (hitMessage == msg_Cancel)
        {
           	break;
        }
        else if (hitMessage == msg_OnNewProfile)
   		{
   		    if (DoNewProfileDialog(dataBuf, sizeof(dataBuf)))
   		    {
   		        CPlatformUCSConversion::GetInstance()->PlatformToUCS(nsDependentCString(dataBuf), unicodeStr);
   		        rv = profileService->CreateNewProfile(unicodeStr.get(), nsnull, nsnull, PR_FALSE);
   		        if (NS_FAILED(rv))
   		            break;
   		        
                table->InsertRows(1, LONG_MAX, dataBuf, strlen(dataBuf), true);
                table->GetTableSize(rows, cols);
                table->SelectCell(STableCell(rows, cols));
                
                rv = profileService->GetProfileCount(&numProfiles);
                (NS_SUCCEEDED(rv) && numProfiles > 1) ? deleteButton->Enable() : deleteButton->Disable();
   		    }    
   		}
   		else if (hitMessage == msg_OnDeleteProfile)
   		{
   		    selectedCell = table->GetFirstSelectedCell();
   		    if (selectedCell.row > 0)
   		    {
   		        dataSize = sizeof(dataBuf) - 1;
   		        table->GetCellData(selectedCell, dataBuf, dataSize);
   		        dataBuf[dataSize] = '\0';
   		        CPlatformUCSConversion::GetInstance()->PlatformToUCS(nsDependentCString(dataBuf), unicodeStr);
   		        
   		        rv = profileService->DeleteProfile(unicodeStr.get(), PR_TRUE);
   		        if (NS_FAILED(rv))
   		            break;
   		        
   		        table->RemoveRows(1, selectedCell.row, true);
   		        table->GetTableSize(rows, cols);    
   		        if (selectedCell.row >= rows)
   		            selectedCell.row = rows - 1;
   		        table->SelectCell(selectedCell);
   		        
                rv = profileService->GetProfileCount(&numProfiles);
                (NS_SUCCEEDED(rv) && numProfiles > 1) ? deleteButton->Enable() : deleteButton->Disable();
   		    }
   		}
   		else if (hitMessage == msg_OnRenameProfile)
   		{
	        nsAutoString oldName;

   		    selectedCell = table->GetFirstSelectedCell();
	        dataSize = sizeof(dataBuf) - 1;
	        table->GetCellData(selectedCell, dataBuf, dataSize);
	        dataBuf[dataSize] = '\0';
	        CPlatformUCSConversion::GetInstance()->PlatformToUCS(nsDependentCString(dataBuf), oldName);

   		    if (DoNewProfileDialog(dataBuf, sizeof(dataBuf)))
   		    {
   		        CPlatformUCSConversion::GetInstance()->PlatformToUCS(nsDependentCString(dataBuf), unicodeStr);
                profileService->RenameProfile(oldName.get(), unicodeStr.get());
                table->SetCellData(selectedCell, dataBuf, strlen(dataBuf)); 		        
   		    }
   		}
  	}  
}


void CProfileManager::DoLogout()
{
    enum { iPersist = 1, iCancel, iCleanse };

    nsresult rv;
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    ThrowIfNil_(profileService);
    
    nsXPIDLString currentProfile;
    Str255 pStr;
    profileService->GetCurrentProfile(getter_Copies(currentProfile));
   	CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(currentProfile.get()), pStr);
    ::ParamText(pStr, "\p", "\p", "\p");
    
    DialogItemIndex item = UModalAlerts::StopAlert(alrt_ConfirmLogout);
    if (item == iCancel)
      return;

    if (item == iPersist)
        rv = profileService->ShutDownCurrentProfile(nsIProfile::SHUTDOWN_PERSIST);
    else    
        rv = profileService->ShutDownCurrentProfile(nsIProfile::SHUTDOWN_CLEANSE);
    if (NS_SUCCEEDED(rv)) {
        // Just put this up modally until they pick a new profile
        DoManageProfilesDialog();
    }
}


/*
    The following three methods have nothing to do with profile management per se.
    They use the registry to store a flag which allows the user to choose whether
    to show the profile manager dialog at startup. After all, since we can switch
    it at any time - why must we deal with the dialog every time we start the app?
*/

nsresult CProfileManager::GetShowDialogOnStart(PRBool* showIt)
{
    *showIt = PR_TRUE;

    CFStringRef showDialogKey = CFSTR(kPrefShowProfilesAtStartup);
    Boolean keyExistsAndIsValid, value;
    
    value = CFPreferencesGetAppBooleanValue(showDialogKey,
                kCFPreferencesCurrentApplication,
                &keyExistsAndIsValid);
            
    if (!keyExistsAndIsValid)
      return NS_ERROR_FAILURE;
    *showIt = value;
    
    return NS_OK;
}

nsresult CProfileManager::SetShowDialogOnStart(PRBool showIt)
{
    CFStringRef showDialogKey = CFSTR(kPrefShowProfilesAtStartup);
    
    CFPreferencesSetAppValue(showDialogKey,
      showIt ? kCFBooleanTrue : kCFBooleanFalse,
      kCFPreferencesCurrentApplication);
    
    // Write out the preference data - this is our 1 and only use of it.
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
    
    return NS_OK;
}

//*****************************************************************************
//***    CProfileManager::LAttachment
//*****************************************************************************

void CProfileManager::ExecuteSelf(MessageT inMessage, void *ioParam)
{
	mExecuteHost = true;
	// update status
	if (inMessage == msg_CommandStatus) {
		SCommandStatus	*status = (SCommandStatus *)ioParam;
		if (status->command == cmd_ManageProfiles) {
			*status->enabled = true;
			*status->usesMark = false;
			mExecuteHost = false; // we handled it
		}
		else if (status->command == cmd_Logout) {
			*status->enabled = true;
			*status->usesMark = false;
			mExecuteHost = false; // we handled it
		}
	}
	else if (inMessage == cmd_ManageProfiles) {
	    DoManageProfilesDialog();
	    mExecuteHost = false; // we handled it
	}
	else if (inMessage == cmd_Logout) {
	    DoLogout();
	    mExecuteHost = false; // we handled it
	}
}
