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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


//	This file implements a callback declared in msgcom.h, for presenting the IMAP
//	subscription upgrade dialog.

#include "msgcom.h" // For the prototype, and the enum for the return value.

#include "PascalString.h"
#include "macutil.h"
#include "UStdDialogs.h"
#include "resgui.h" // for msg_Help
#include "xp_help.h"
#include <LGARadioButton.h>
#include <Sound.h> // for SysBeep()

//----------------------------------------------------------------------------------------
MSG_IMAPUpgradeType FE_PromptIMAPSubscriptionUpgrade(MWContext*, const char* hostName)
//----------------------------------------------------------------------------------------
{
	StDialogHandler handler(14007, NULL);
	LWindow* dialog = handler.GetDialog(); // initially invisible.

	// Find the radio button for the "Automatic" choice.
	LGARadioButton* autoButton = (LGARadioButton*)dialog->FindPaneByID('AUTO');
	SignalIf_(!autoButton);
	if (!autoButton)
		return MSG_IMAPUpgradeDont;
	
	// Replace ^0 in the radio button string with the host name that was given to us
	CStr255 workString;
	autoButton->GetDescriptor(workString);
	StringParamText(workString, hostName);
	autoButton->SetDescriptor(workString);
	
	// Run the dialog
	MSG_IMAPUpgradeType result = MSG_IMAPUpgradeAutomatic;
	MessageT message = msg_Nothing;
	if (UStdDialogs::TryToInteract())
	{
		dialog->Show();
		do {
			message = handler.DoDialog();
			switch (message)
			{
				case msg_Help:
					ShowHelp( HELP_IMAP_UPGRADE );
					break;
#if 0
				// This doesn't work, and here's why.  When one of the radio buttons is
				// clicked, first the value message gets broadcast (good), and then
				// the radio group broadcasts "msg_ControlClicked".  Unfortunately,
				// StDialogHandler merely stores the last message it receives!
				case 'AUTO':
					result = MSG_IMAPUpgradeAutomatic;
					break;
				case 'PICK':
					result = MSG_IMAPUpgradeCustom;
					break;
#endif
			}
		} while (message != msg_OK && message != msg_Cancel);
	}
	// Use the OK/Cancel message together with the value of the the radio button for
	// the automatic choice to determine the result.
	if (message != msg_OK)
		result = MSG_IMAPUpgradeDont;
	else if (autoButton->GetValue() == Button_On)
		result = MSG_IMAPUpgradeAutomatic;
	else
		result = MSG_IMAPUpgradeCustom;
	return result;
} // FE_PromptIMAPSubscriptionUpgrade
