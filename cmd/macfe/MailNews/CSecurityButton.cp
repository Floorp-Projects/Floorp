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



// CSecurityButton.cp

#include "CSecurityButton.h"
/*  #include "CMailComposeWindow.h" */
/*  #include "msgcom.h" */
#include "CComposeSession.h"
/*  #include "CBrowserContext.h" */
#include "CButton.h"
#include "LGAIconSuiteControl.h"

#include "CMessageView.h"
#include "ssl.h"

void USecurityIconHelpers::UpdateMailWindow( CMessageView *messageView )
{
	if ( messageView )
	{
		CMailNewsWindow* window = dynamic_cast<CMailNewsWindow*>(LWindow::FetchWindowObject(
											messageView->GetMacPort()));
		if ( !window )
			return;
		MWContext* context = NULL;
		context = (MWContext*)*messageView->GetContext();
		uint32 folderFlags = messageView->GetFolderFlags();
		XP_Bool isEncrypted = 0;
		XP_Bool isSigned = 0;

		MIME_GetMessageCryptoState( context , 0 ,0, &isSigned, &isEncrypted );
		
		Boolean	isNewsGroup	= ((folderFlags & MSG_FOLDER_FLAG_NEWSGROUP)!=0) ;
		if ( isNewsGroup )
		{
			// news message encryption depends if the News group is secure	
			int secLevel = XP_GetSecurityStatus( context );
			isEncrypted = (secLevel != SSL_SECURITY_STATUS_OFF);
		}
		if ( isSigned >1 || isSigned<0)
			isSigned = 0;
		if ( isEncrypted>1 || isEncrypted<0 )
			isEncrypted = 0;
		
		USecurityIconHelpers::ChangeToSecurityState( window, isEncrypted, isSigned );	
	}
}

void USecurityIconHelpers::UpdateComposeButton( CMailComposeWindow *window )
{
	Assert_( window !=NULL );
	CComposeSession * session = window->GetComposeSession();
	XP_Bool isEncrypted = 0;
	XP_Bool isSigned = 0;
	isEncrypted = session->GetCompBoolHeader( MSG_ENCRYPTED_BOOL_HEADER_MASK );
	isSigned = session->GetCompBoolHeader( MSG_SIGNED_BOOL_HEADER_MASK );
	USecurityIconHelpers::ChangeToSecurityState( window, isEncrypted, isSigned );	
}

void USecurityIconHelpers::SetNoMessageLoadedSecurityState( LWindow * window )
{
	Assert_( window != NULL  );
	USecurityIconHelpers::ChangeToSecurityState( window, false, false );	
}

void USecurityIconHelpers::AddListenerToSmallButton(LWindow* inWindow, LListener* inListener)
{
	LGAIconSuiteControl* button = dynamic_cast<LGAIconSuiteControl*>(inWindow->FindPaneByID( eSmallEncryptedButton ));
	if (button)
		button->AddListener(inListener);
	button = dynamic_cast<LGAIconSuiteControl*>(inWindow->FindPaneByID( eSmallSignedButton ));
	if (button)
		button->AddListener(inListener);
}

void USecurityIconHelpers::ChangeToSecurityState(
	LWindow* inWindow,
	Boolean inEncrypted,
	Boolean inSigned )
{
	if (!inWindow)
		return;
	CButton* button = dynamic_cast<CButton*>(inWindow->FindPaneByID( eBigSecurityButton ));
	if (button)
	{
		static const ResIDT bigSecurityIcons[4]={ 15439, 15435, 15447, 15443 };
		ResIDT newIconID = bigSecurityIcons[ inSigned + 2*inEncrypted ];
		if (button->GetGraphicID() != newIconID )
		{
			button->SetGraphicID( newIconID);
			button->Refresh();
		}
	}
	LGAIconSuiteControl* smallButton
		= dynamic_cast<LGAIconSuiteControl*>(inWindow->FindPaneByID( eSmallEncryptedButton ));
	if (smallButton)
	{
		ResIDT newIconID =  inEncrypted ? 15336 : 15335;
		if (smallButton->GetIconResourceID() != newIconID )
		{
			smallButton->SetIconResourceID( newIconID);
			smallButton->Refresh();
		}
	}
	smallButton
		= dynamic_cast<LGAIconSuiteControl*>(inWindow->FindPaneByID( eSmallSignedButton ));
	if (smallButton)
	{
		if (inSigned)
			smallButton->Show();
		else
			smallButton->Hide();
	}
}

void CMailSecurityListener::ListenToMessage(MessageT inMessage, void* ioParam)
{
#pragma unused (ioParam)
	switch( inMessage )
	{
		case  msg_NSCAllConnectionsComplete:
			USecurityIconHelpers::UpdateMailWindow( mMessageView);
			break;
		
		default:
			break;
	}
}
