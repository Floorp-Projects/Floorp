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

#include "CReceiptsMediator.h"

#include "macutil.h"

#include "prefapi.h"

#include <LGACaption.h>
#include <UModalDialogs.h>

//-----------------------------------
CReceiptsMediator::CReceiptsMediator(LStream*)
//-----------------------------------
:	CPrefsMediator(class_ID)
,	mCustomDialogHandler(nil)
{
} // CReceiptsMediator::CReceiptsMediator

//-----------------------------------
CReceiptsMediator::~CReceiptsMediator()
//-----------------------------------
{
	FinalizeCustomDialog();
} // CReceiptsMediator::~CReceiptsMediator

//-----------------------------------
void CReceiptsMediator::WritePrefs()
//-----------------------------------
{
	FinalizeCustomDialog();
} // CReceiptsMediator::WritePrefs

//-----------------------------------
void CReceiptsMediator::FinalizeCustomDialog()
//-----------------------------------
{
	delete mCustomDialogHandler; // this causes controls to write themselves.
	mCustomDialogHandler = nil;
} // CReceiptsMediator::FinalizeCustomDialog

//-----------------------------------
void CReceiptsMediator::DoCustomDialog()
//-----------------------------------
{
	LWindow* dialog = nil;
	try
	{
		Boolean firstTime = (mCustomDialogHandler == nil);
		if (firstTime)
			mCustomDialogHandler = new StDialogHandler(12010, nil);
		dialog = mCustomDialogHandler->GetDialog();
		

		if (firstTime)
		{
			// Set up the domain name in the caption.
			char buf[256];
			const char* domainName = buf;
			int bufLength = sizeof(buf);
			PREF_GetCharPref("mail.identity.defaultdomain", buf, &bufLength);
			if (!domainName[0])
			{
				bufLength = sizeof(buf);
				PREF_GetCharPref("mail.identity.useremail", buf, &bufLength);
				char* cp = strchr(buf, '@');
				if (cp)
					domainName = cp + 1;
			}
			LGACaption* domainField = (LGACaption*)dialog->FindPaneByID('Domn');
			ThrowIfNil_(domainField);
			CStr255 captionText;
			
			domainField->GetDescriptor(captionText);
			StringParamText(captionText, domainName);
			domainField->SetDescriptor(captionText);
		}
		dialog->Show();
		dialog->Select();
		Int32 popupValue[3];
		// Store the dialog popup values in case cancel is hit
		for (int32 i =0; i<3; i++ )
		{
			LGAPopup* popup = dynamic_cast<LGAPopup*>( dialog->FindPaneByID( i+ 1) );
			XP_ASSERT( popup );
			popupValue[i] = popup->GetValue();
		}
		
		MessageT message = msg_Nothing;
		do {
			message = mCustomDialogHandler->DoDialog();
		} while (message != msg_OK && message != msg_Cancel); 
		
		// Use the result.
		if (message == msg_OK)
		{
			// Nothing to do, the prefs are written out when the dialog is destroyed.
		}
		else if ( message == msg_Cancel )
		{
			// Restore values from when the dialog was first put up
			for ( int32 i =0; i<3; i++ )
			{
				LGAPopup* popup = dynamic_cast<LGAPopup*>( dialog->FindPaneByID( i+ 1) );
				XP_ASSERT( popup );
				popup->SetValue(popupValue[i]);
			}
		}
	}
	catch(...)
	{
	}
	if (dialog)
		dialog->Hide(); // don't delete, we delete when we want to write the prefs.
} // CReceiptsMediator::DoCustomDialog

//-----------------------------------
void CReceiptsMediator::ListenToMessage(MessageT inMessage, void *ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
		case 'Cust':
			DoCustomDialog();
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
} // CReceiptsMediator::ListenToMessage
