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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// prefw.cp
// Preferences dialog box
// Contains all the views/commands for the preferences dialog box
// Created by atotic, July 30th, 1994

#include "prefw.h"

#include "InternetConfig.h"
#include "CPrefsDialog.h"

//-----------------------------------
void FE_EditPreference( PREF_Enum which )
//-----------------------------------
{
	if (CInternetConfigInterface::CurrentlyUsingIC())
	{
		switch ( which )
		{
			case PREF_EmailAddress:
				CInternetConfigInterface::LaunchInternetConfigApplication(kICEmail);
				break;
	#ifdef MOZ_MAIL_NEWS
			case PREF_Pop3ID:
				CInternetConfigInterface::LaunchInternetConfigApplication(kICMailAccount);
				break;
			case PREF_SMTPHost:
				CInternetConfigInterface::LaunchInternetConfigApplication(kICSMTPHost);
				break;
			case PREF_PopHost:
				CInternetConfigInterface::LaunchInternetConfigApplication(kICMailAccount);
				break;
			case PREF_NewsHost:
				CInternetConfigInterface::LaunchInternetConfigApplication(kICNNTPHost);
				break;
	#endif // MOZ_MAIL_NEWS
			default:
				XP_ASSERT(FALSE);
		}
	}
	else
	{
		switch ( which )
		{
			case PREF_EmailAddress:
				CPrefsDialog::EditPrefs(	CPrefsDialog::eExpandMailNews,
												PrefPaneID::eMailNews_Identity,
												CPrefsDialog::eEmailAddress);
				break;
	#ifdef MOZ_MAIL_NEWS
			case PREF_Pop3ID:
				CPrefsDialog::EditPrefs(	CPrefsDialog::eExpandMailNews,
												PrefPaneID::eMailNews_MailServer,
												CPrefsDialog::ePOPUserID);
				break;
			case PREF_SMTPHost:
				CPrefsDialog::EditPrefs(	CPrefsDialog::eExpandMailNews,
												PrefPaneID::eMailNews_MailServer,
												CPrefsDialog::eSMTPHost);
				break;
			case PREF_PopHost:
				CPrefsDialog::EditPrefs(	CPrefsDialog::eExpandMailNews,
												PrefPaneID::eMailNews_MailServer,
												CPrefsDialog::ePOPHost);
				break;
			case PREF_NewsHost:
				CPrefsDialog::EditPrefs(	CPrefsDialog::eExpandMailNews,
												PrefPaneID::eMailNews_NewsServer,
												CPrefsDialog::eNewsHost);
				break;
	#endif // MOZ_MAIL_NEWS
			default:
				XP_ASSERT(FALSE);
		}
	}
}
