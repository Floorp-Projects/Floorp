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



// CNewsSubscriber.cp

#include "CNewsSubscriber.h"

#include "CMailNewsContext.h"
#include "CSubscribeWindow.h"

#include "UModalDialogs.h"
#include "LGAEditField.h"
#include "LGACheckbox.h"
#include "LGARadioButton.h"

#include "prefapi.h"
#include "msgcom.h"
#include "uprefd.h"
#include "uerrmgr.h"

extern "C"
{
	extern int MK_NNTP_SERVER_NOT_CONFIGURED;
}

MSG_Host*	CNewsSubscriber::mHost = nil;


//-----------------------------------
Boolean CNewsSubscriber::DoSubscribeNewsGroup(MSG_Host* host, Boolean inModal)
//-----------------------------------
{

	Boolean result = false;

	// When no host is specified, we try to use the previously
	// accessed host or the default host. If there isn't any,
	// we display an alert and ask the user to configure one.
	if (host == nil)
		host = GetHost();

	if (host == nil)
		host = (MSG_Host*)MSG_GetDefaultNewsHost(CMailNewsContext::GetMailMaster());

	if (host == nil)
	{
		FE_Alert(nil, XP_GetString(MK_NNTP_SERVER_NOT_CONFIGURED));
		FE_EditPreference(PREF_NewsHost);
	}
	else
	{
		SetHost(host);
		if (inModal)
		{
			result = CSubscribeWindow::DisplayDialog();
		}
		else
		{
			CSubscribeWindow::FindAndShow(true);
			result = true;
		}
	}
	return result;

} // CMessageFolderView::DoSubscribeNewsGroup


//-----------------------------------
Boolean CNewsSubscriber::DoAddNewsHost()
//-----------------------------------
{
	// Put up dialog
	StDialogHandler handler(14000, NULL);

	// Select the "Host" edit field
	LWindow* dialog = handler.GetDialog();
	LGAEditField *hostfield = (LGAEditField*)dialog->FindPaneByID('Host');
	SignalIf_(!hostfield);
	LGAEditField* portfield = (LGAEditField*)dialog->FindPaneByID('Port');
	SignalIf_(!portfield);
	LGACheckbox* securebox = (LGACheckbox*)dialog->FindPaneByID('Secu');
	SignalIf_(!securebox);
	if (!hostfield || ! portfield || !securebox)
		return false;
	dialog->SetLatentSub(hostfield);

	// Run the dialog
	MessageT message = 'Secu'; // so that the port is initialized correctly.
	CStr255 porttext;
	Int32 port = 0;
	Boolean userChangedPort = false;
	LControl* okButton = dynamic_cast<LControl*>(dialog->FindPaneByID ('Add_') );
	XP_ASSERT( okButton );
	do {
		if (message == 'Secu' && !userChangedPort)
		{
			port = securebox->GetValue() ? 563 : 119;
			NumToString(port, porttext);
			portfield->SetDescriptor(porttext);
		}
		message = handler.DoDialog();
		portfield->GetDescriptor(porttext);
		Int32 newport;
		StringToNum(porttext, &newport);
		userChangedPort = (newport != port);
		port = newport;
		
		CStr255 hosttext;
		hostfield->GetDescriptor(hosttext);
		if( hosttext.Length()  > 0 )
			okButton->Enable();
		else
			okButton->Disable();
	} while (message != msg_OK && message != msg_Cancel);
	
	// Use the result.
	if (message == msg_OK)
	{
		CStr255 hosttext;
		hostfield->GetDescriptor(hosttext);
		MSG_NewsHost* newHost = MSG_CreateNewsHost(CMailNewsContext::GetMailMaster(),
									hosttext, securebox->GetValue(), port);

		CNewsSubscriber::SetHost(MSG_GetMSGHostFromNewsHost(newHost));	// make it the default host

		// Be kind to the user: if there was no News server configured yet,
		// then use this one (otherwise, we'll throw the Prefs dialog
		// on the next common MSG_ call, such as MSG_SubscribeSetNewsHost())
		if (MSG_GetDefaultNewsHost(CMailNewsContext::GetMailMaster()) == NULL)
			(void)CPrefs::SetString(hosttext, CPrefs::NewsHost);
	}
	return (message == msg_OK);
} // CNewsSubscriber::DoAddNewsHost()	
				
//======================================
// FE_NewsDownloadPrompt
//======================================

// From listngst.cpp
extern "C" XP_Bool FE_NewsDownloadPrompt(
	MWContext *context,
	int32 numMessagesToDownload,
	XP_Bool *downloadAll);

//-----------------------------------
class StDownloadDialogHandler: public StDialogHandler
//-----------------------------------
{
	public:
        StDownloadDialogHandler(
        	MWContext* context,
			int32 numMessagesToDownload,
			XP_Bool& downloadAll);
        Boolean InitFields();
        void ReadFields();

        UInt32 mNumberOfMessages;
        XP_Bool mDownloadSome;
        Int32 mDownloadMax;
        XP_Bool mMarkRead;

		LGAEditField* mMaxHeadersField;
		LGACheckbox* mMarkReadCheckbox;
		LGARadioButton* mDownloadSomeRadio;
}; // class StDownloadDialogHandler

//-----------------------------------
StDownloadDialogHandler::StDownloadDialogHandler(
	MWContext* context,
	int32 numMessages,
	XP_Bool& downloadAll)
//-----------------------------------
:	StDialogHandler(14001, NULL)
,	mNumberOfMessages(numMessages)
,	mDownloadMax(0)
,	mDownloadSome(false)
,	mMarkRead(false)

,	mMaxHeadersField(NULL)
,	mMarkReadCheckbox(NULL)
,	mDownloadSomeRadio(NULL)
{
#pragma unused (context)
#pragma unused (downloadAll)
}

//-----------------------------------
void StDownloadDialogHandler::ReadFields()
//-----------------------------------
{
	mDownloadSome = mDownloadSomeRadio->GetValue();
	mMarkRead = mMarkReadCheckbox->GetValue();
	CStr255 headersText;
	mMaxHeadersField->GetDescriptor(headersText);
	StringToNum(headersText, &mDownloadMax);
	if (mDownloadSome)
	{
		PREF_SetBoolPref("news.mark_old_read", mMarkRead);
		PREF_SetIntPref("news.max_articles", mDownloadMax);
	}
}

//-----------------------------------
Boolean StDownloadDialogHandler::InitFields()
//-----------------------------------
{
	// Select the "Host" edit field
	LWindow* dialog = GetDialog();
	
	mMaxHeadersField = (LGAEditField*)dialog->FindPaneByID('maxH');
	SignalIf_(!mMaxHeadersField);
	
	mMarkReadCheckbox = (LGACheckbox*)dialog->FindPaneByID('MkUn');
	SignalIf_(!mMarkReadCheckbox);
	
	mDownloadSomeRadio = (LGARadioButton*)dialog->FindPaneByID('DnSm');
	SignalIf_(!mDownloadSomeRadio);

	LCaption* messagefield = (LCaption*)dialog->FindPaneByID('Mesg');
	SignalIf_(!messagefield);

	if (!messagefield || !mMaxHeadersField || !mMarkReadCheckbox || !mDownloadSomeRadio)
		return false;
	
	// The caption has the message format string with %d in it...
	CStr255 messageText;
	messagefield->GetDescriptor(messageText);
	char messageString[255];
	sprintf(messageString, messageText, mNumberOfMessages);
	messageText = messageString;
	messagefield->SetDescriptor(messageText);
	
	PREF_GetBoolPref( "news.mark_old_read", &mMarkRead );
	mMarkReadCheckbox->SetValue(mMarkRead);
	
	PREF_GetIntPref("news.max_articles", &mDownloadMax);
	CStr255 downloadString;
	NumToString(mDownloadMax, downloadString);
	mMaxHeadersField->SetDescriptor(downloadString);
	
	return true;
}

//-----------------------------------
XP_Bool FE_NewsDownloadPrompt(
	MWContext* context,
	int32 numMessagesToDownload,
	XP_Bool* downloadAll)
//-----------------------------------
{
	// Put up dialog
	StDownloadDialogHandler handler(context, numMessagesToDownload, *downloadAll);

	// Set up the dialog
	if (!handler.InitFields())
		return false;
	
	// Run the dialog
	MessageT message = msg_Nothing;
	do {
		message = handler.DoDialog();
	} while (message != msg_Cancel && message != msg_OK);
	
	// Use the result.
	if (message == msg_OK)
	{
		handler.ReadFields();
        *downloadAll = !handler.mDownloadSome;
		return true;
	}
	return false;
} // FE_NewsDownloadPrompt


//-----------------------------------
XP_Bool FE_CreateSubscribePaneOnHost(
	MSG_Master*		master,
	MWContext*		parentContext,
	MSG_Host*		host)
//-----------------------------------
{
#pragma unused (master)
#pragma unused (parentContext)
	XP_Bool result;
	result = CNewsSubscriber::DoSubscribeNewsGroup(host, false);
				// modeless, as required by API spec for this call.
	return result;
}
