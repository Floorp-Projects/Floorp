/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// UOffline.cp

#include "UOffline.h"

#include "MailNewsgroupWindow_Defines.h"
#include "CMailNewsContext.h"
#include "CMailProgressWindow.h"
#include "COfflinePicker.h"
#include "UModalDialogs.h"
#include "MPreference.h"
#include "macutil.h"
#include "uapp.h"

#include "prefapi.h"
#include "msgcom.h"
#include "PascalString.h"
#include "xp_help.h"

//------------------------------------------------------------------------------
//		¥ ObeySynchronizeCommand
//------------------------------------------------------------------------------
//	Run the "Download Offline Items" dialog where the user can check what kind
//	of items should be downloaded: Mail Folders, Discussion Groups or Directories.
//	Since we are using the magic CPrefCheckbox(es) in that dialog, we have to
//	delete the dialog and its handler for the check-boxes to actually write their
//	new state on disk and update the Prefs file.
//
Boolean UOffline::ObeySynchronizeCommand(Boolean stayModal)
{

	Str255	windowTitle; // Get it from the offline dialog, to use in the progress dialog.

	{ // <-- start of scope for StDialogHandler

		// Save and restore the prefs write state
		MPreferenceBase::StWriteOnDestroy stateSetter(true);

		// Put up dialog.
		StDialogHandler aHandler(20002, NULL);

		LWindow* dialog = aHandler.GetDialog();
		dialog->GetDescriptor(windowTitle);

		// Run the dialog
		MessageT message = msg_OK;	// first pass message
		do
		{
			switch (message)
			{
				case 'Sele':		// Select button
					COfflinePickerWindow::DisplayDialog();
					break;

				case 'Help':
					ShowHelp(HELP_MAILNEWS_SYNCHRONIZE);
					break;

				case 'GetM':		// check-boxes
				case 'GetN':
				case 'GetD':
				case 'SndM':
				case msg_OK:		// first pass
					LControl *getmailBox = (LControl*)dialog->FindPaneByID('GetM');
					LControl *getnewsBox = (LControl*)dialog->FindPaneByID('GetN');
					LControl *getdirBox = (LControl*)dialog->FindPaneByID('GetD');
					LControl *sendBox = (LControl*)dialog->FindPaneByID('SndM');
					LControl *flaggedBox = (LControl*)dialog->FindPaneByID('GetF');
					LControl *syncButton = (LControl*)dialog->FindPaneByID('Sync');
					SignalIf_(!((getmailBox && getnewsBox && getdirBox && sendBox && flaggedBox && syncButton)));

					if ((getmailBox->GetValue() == 0)
						&& (getnewsBox->GetValue() == 0))
						{
							flaggedBox->Disable();
						}
						else
						{
							flaggedBox->Enable();
						}

					if ((getmailBox->GetValue() == 0)
						&& (getnewsBox->GetValue() == 0)
						&& (getdirBox->GetValue() == 0)
						&& (sendBox->GetValue() == 0))
						{
							syncButton->Disable();
						}
						else
						{
							syncButton->Enable();
						}
					break;
			}

			message = aHandler.DoDialog();

		} while (message != msg_OK && message != msg_Cancel);
		
		// Use the result.
		if (message != msg_OK)
			return false;
	} // <-- End of scope for StDialogHandler (need to save prefs before calling SynchronizeForOffline)

	// Start the download, using the prefs
	ResIDT resID = (stayModal ? CMailProgressWindow::res_ID_modal : CMailProgressWindow::res_ID_modeless);
	CMailProgressWindow* progressWindow = CMailProgressWindow::SynchronizeForOffline(windowTitle, resID);
	if (progressWindow && stayModal)
	{
		CNSContext* cnsContext = progressWindow->GetWindowContext();
		if (cnsContext)
		{
			do
			{
				CFrontApp::GetApplication()->ProcessNextEvent();
			} while (XP_IsContextBusy(*cnsContext));
		}
	}

	return true;
}

//------------------------------------------------------------------------------
//		¥ ObeyToggleOfflineCommand
//------------------------------------------------------------------------------
//
Boolean UOffline::ObeyToggleOfflineCommand()
{
	CMailProgressWindow::ToggleOffline();
	return true;
}


//------------------------------------------------------------------------------
//		¥ FindOfflineCommandStatus
//------------------------------------------------------------------------------
//
void UOffline::FindOfflineCommandStatus(CommandT inCommand, Boolean& enabled, Str255 name)
{
	enabled = true;
	switch (inCommand)
	{
		case cmd_ToggleOffline:
			::GetIndString(name, 1037, (UOffline::AreCurrentlyOnline() ? 4 : 3));
			break;

		case cmd_SynchronizeForOffline:
			enabled = UOffline::AreCurrentlyOnline();
			break;
	}
}


//------------------------------------------------------------------------------
//		¥ AreCurrentlyOnline
//------------------------------------------------------------------------------
//
Boolean UOffline::AreCurrentlyOnline()
{
	return !NET_IsOffline();
}
