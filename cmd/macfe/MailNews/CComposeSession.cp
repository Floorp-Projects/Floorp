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



// CComposeSession.cp

#include "CComposeSession.h"
#include "CMailNewsContext.h"
#include "CEditView.h"

// PowerPlant
#include <LHandleStream.h>

// XP
#include "proto.h" // for XP_InterruptContext
#include "addrbook.h"
//#include "aberror.h"
#include "edt.h"
#include "mime.h"
#include "libi18n.h"

#include "msg_srch.h" // needed for priority 
// Mac FE
#include "uerrmgr.h"	// ErrorManager
#include "resgui.h"		// error string constants
#include "mailNewsgroupWindow_Defines.h" // Tmessages
int FE_QuoteCallback(void* closure, const char* data)
{
	if (data)
	{
		CComposeSession* session = (CComposeSession*)closure;
		// this causes a broadcast from session to compose window
		// how do I return an error?
		session->InsertQuoteText(data);
	}
	else
	{
		CComposeSession* session = (CComposeSession*)closure;
		session->AutoQuoteDone();
	}
	return 0;
}

//-----------------------------------
CComposeSession::CComposeSession(Boolean inOpeningAsDraft) 
//-----------------------------------
:	mCompositionContext(nil),
	mXPCompositionPane(nil),
	mDontIgnoreAllConnectionsComplete(false),
	mDownloadingAttachments(false),
	mCloseWindowAfterSavingDraft(false),
	mClosing(false),
	mAutoQuoting( false ),
	mOpeningAsDraft(inOpeningAsDraft)
{
}

//-----------------------------------
CComposeSession::~CComposeSession()
//-----------------------------------
{
	if (mClosing)
		return;
	mClosing = true;

	Stop();

	if (mXPCompositionPane)
		MSG_DestroyPane(mXPCompositionPane);

	mXPCompositionPane = nil;
	mCompositionContext->RemoveUser(this);
	mCompositionContext->RemoveListener(this);
}

// ---------------------------------------------------------------------------
MSG_Pane* CComposeSession::CreateBackendData(MWContext* inOldContext,
											 MSG_CompositionFields* inCompositionFields)
//	Create backend data associated with a compose session. This data consists
//	of the MWContext and the MSG_CompositionPane object.
// ---------------------------------------------------------------------------
{
	Try_ {
		// if we can't allocate CMailCompositionContext, this object does us no good
		mCompositionContext = new CMailCompositionContext();
		mCompositionContext->AddListener(this);
		mCompositionContext->CreateContextProgress();
		mCompositionContext->AddUser(this);
		mCompositionContext->WaitWhileBusy();
		mXPCompositionPane = MSG_CreateCompositionPane(
			*mCompositionContext,
			inOldContext,
			MSG_GetPrefsForMaster(CMailNewsContext::GetMailMaster()),
			inCompositionFields,
			CMailNewsContext::GetMailMaster());
		// while we're here, set from field using preference
#if 0	// FIX: no need to: 'From' field is already set
		if (mXPCompositionPane)
		{
			char * fromField = MIME_MakeFromField();
			// FIX ME -- what do we do if there is an error?
			int err = MSG_SetCompHeader(mXPCompositionPane,
										MSG_FROM_HEADER_MASK,
										fromField);
			XP_FREE(fromField);
		}
#endif
	}
	Catch_(e)
	{
		// There's a leak here: mCompositionPane will not be cleaned up.
		// I couldn't work out how to do it, unless it's to call:
		//		MSG_MailCompositionAllConnectionsComplete(mXPCompositionPane);

		Throw_(e);
	}
	EndCatch_
	// I assume that we call this method shortly after creating
	// the object itself, so mXPCompositionPane should be NULL before we
	// enter this method
	return mXPCompositionPane;
}

void CComposeSession::SetCompositionPaneFEData( void *data )
{
	if (mXPCompositionPane)
	{
		// let's set the window here so we can send it a close message after sending the mail
		MSG_SetFEData(mXPCompositionPane, data);
	}
}

void CComposeSession::ListenToMessage(MessageT inMessage, void* ioParam)
{
	if (mClosing)
		return;

	switch(inMessage)
	{
		case msg_NSCAllConnectionsComplete:
			if (mDownloadingAttachments)
			{
				// we're done downloading attachments, now do send command
				mDownloadingAttachments = false;
				switch (mMessage){
					case cmd_SaveDraft:
						SaveDraftOrTemplate(inMessage, mCloseWindowAfterSavingDraft );
						break;	
					case cmd_SaveTemplate:
						SaveDraftOrTemplate(inMessage, false );
						break;	
					default:	// Handles SendNow and SendLater
						SendMessage(mSendNow);
					break;
				}
				
			}	
			else if (mDontIgnoreAllConnectionsComplete) // MOAN!
			{
				if (mXPCompositionPane)
				{
					// We need to call this to let backend know to delete mXPCompositionPane
					// Backend will destroy mXPCompositionPane when ready, so we don't need to.
					// We'll come here several times for HTML parts messages, so don't do
					// anything final.  The backend will call FE_DestroyCompositionContext when
					// everything is REALLY complete.  We'll close the window then.
					MSG_MailCompositionAllConnectionsComplete(mXPCompositionPane);
				}
			}
			break;
		// pass on broadcast to listeners
		default:
			BroadcastMessage(inMessage, ioParam);
			break;
	}
}

void CComposeSession::SetMessageHeaderData(MSG_HEADER_SET inMessageHeaderType,
										   LHandleStream& inDataStream)
{
	if (mXPCompositionPane)
	{
		Handle data = inDataStream.GetDataHandle();
		StHandleLocker lock(data);
		int err = MSG_SetCompHeader(mXPCompositionPane,inMessageHeaderType, *data);
		// what do we do if there is an error?
	}
}

const char* CComposeSession::GetMessageHeaderData(MSG_HEADER_SET inMessageHeaderType)
{
	return MSG_GetCompHeader(mXPCompositionPane, inMessageHeaderType);
}

void CComposeSession::SetMessageBody(LHandleStream& inDataStream)
{
	if (mXPCompositionPane)
	{
		Handle data = inDataStream.GetDataHandle();
		StHandleLocker lock(data);
		MSG_SetHTMLMarkup(mXPCompositionPane, false);
		int err = MSG_SetCompBody(mXPCompositionPane, *data);
	}
}

void CComposeSession::SetHTMLMessageBody()
{
	if (mXPCompositionPane)
	{
		XP_HUGE_CHAR_PTR textp = nil;
		EDT_SaveToBuffer(*mCompositionContext, &textp);
		MSG_SetHTMLMarkup(mXPCompositionPane, true);
		int err = MSG_SetCompBody(mXPCompositionPane, textp);
		if (textp)
			delete textp;
	}
}

void CComposeSession::SendMessage(Boolean inSendNow)
{
	mDontIgnoreAllConnectionsComplete = true;
	mSendNow = inSendNow;
	MSG_Command(mXPCompositionPane, inSendNow ? MSG_SendMessage : MSG_SendMessageLater, nil, nil);
}

const char* CComposeSession::GetSubject()
{
	return MSG_GetCompHeader(mXPCompositionPane, MSG_SUBJECT_HEADER_MASK);
}

MSG_PRIORITY CComposeSession::GetPriority()
{
	// Oh, no!  msglib gives us a hard-coded string.  What sort of consciousness
	// can lead to this type of code.  Let's hope that the linker can pool strings for us,
	// so that this will only take up 20 extra bytes.
  	const char *priority = MSG_GetCompHeader( GetMSG_Pane(), MSG_PRIORITY_HEADER_MASK  );
	if( priority )
	{
		if (strcasestr(priority, "Normal") != NULL)
				return MSG_NormalPriority;
		else if (strcasestr(priority, "Lowest") != NULL)
				return MSG_LowestPriority;
		else if (strcasestr(priority, "Highest") != NULL)
				return MSG_HighestPriority;
		else if (strcasestr(priority, "High") != NULL || 
				 strcasestr(priority, "Urgent") != NULL)
				return MSG_HighPriority;
		else if (strcasestr(priority, "Low") != NULL ||
				 strcasestr(priority, "Non-urgent") != NULL)
				return MSG_LowPriority;
		else
				return MSG_NoPriority;
	}
	return MSG_NoPriority;
}

const struct MSG_AttachmentData*
CComposeSession::GetAttachmentData()
{
	return MSG_GetAttachmentList(mXPCompositionPane);
}

void CComposeSession::SetAttachmentList(MSG_AttachmentData* inAttachmentList)
{
	try
	{
		mDownloadingAttachments = true;
		int status = MSG_SetAttachmentList(mXPCompositionPane, inAttachmentList);
		if (status) throw status;
	}
	catch (...)
	{
		mDownloadingAttachments = false;
		throw;
	}
}


void CComposeSession::InsertQuoteText(const char* text)
{
	BroadcastMessage(msg_InsertQuoteText, (void*)text);
}

void CComposeSession::AutoQuoteDone()
{
	if( mAutoQuoting )
		BroadcastMessage(msg_AutoQuoteDone, NULL);
	mAutoQuoting = false;
}


void CComposeSession::CheckForAutoQuote()
{
	if (ShouldAutoQuote())
	{
		mAutoQuoting = true;
		QuoteMessage();
	}
}

void CComposeSession::QuoteMessage()
{
	MSG_QuoteMessage(mXPCompositionPane, FE_QuoteCallback, this);
}

#if 0
// As far as I can tell, this never gets called.
void CComposeSession::QuoteInHTMLMessage( const char *text )
{
	MWContext* context = (MWContext*)*mCompositionContext;
	EDT_BeginOfDocument( context, false ); // before signature, if any.
	// insert a blank line between message and quote
	EDT_ReturnKey( context );
	EDT_PasteQuote( context, (char *)text ); 
	// move the insertion caret to the beginning (where it should be)
	EDT_BeginOfDocument( context, false );
}
#endif

void CComposeSession::SetPriority(MSG_PRIORITY inPriority)
{
	char priorityString[50];
	MSG_GetUntranslatedPriorityName ( inPriority, priorityString, sizeof(priorityString));
	MSG_SetCompHeader(
		mXPCompositionPane,
		MSG_PRIORITY_HEADER_MASK,
		priorityString );
}

void CComposeSession::Stop()
{
	// ignore all connections complete message from backend
	mDontIgnoreAllConnectionsComplete = false;
	XP_InterruptContext(*mCompositionContext);
	mDownloadingAttachments = false;
}

void CComposeSession::SaveDraftOrTemplate(CommandT inCommand,  Boolean inCloseWindow )
{
	MSG_CommandType cmd;
	if ( inCloseWindow )
	{
		Assert_(inCommand == cmd_SaveDraft);
		cmd = MSG_SaveDraftThenClose;
		mDontIgnoreAllConnectionsComplete = true;
	}
	else if (inCommand == cmd_SaveTemplate)
		cmd = MSG_SaveTemplate;
	else
		cmd = MSG_SaveDraft;
	
	MSG_Command(mXPCompositionPane,  cmd  , nil, nil);
}

int	CComposeSession::SetCompBoolHeader(MSG_BOOL_HEADER_SET header,XP_Bool value)
{
	return  MSG_SetCompBoolHeader(mXPCompositionPane, header,value);
}


Uint32 GetAttachmentListLength(const MSG_AttachmentData* inAttachList);
Uint32 GetAttachmentListLength(const MSG_AttachmentData* inAttachList)
{
	UInt32 result = 0;
	if (inAttachList)
	{
		MSG_AttachmentData* iter = const_cast<MSG_AttachmentData*>(inAttachList);
		while (iter->url != nil)
		{
			++result;
			++iter;
		}
	}
	return result;
}

Int16 CComposeSession::GetDefaultCSID()
{	// Delegate to mCompositionContext
	if (mCompositionContext)
		return mCompositionContext->GetDefaultCSID();
	else 
		return 0;
}
void CComposeSession::SetDefaultCSID(Int16 default_csid)
{	// Delegate to mCompositionContext
	Assert_(mCompositionContext);
	if (mCompositionContext)
	{
		 mCompositionContext->SetDefaultCSID(default_csid);
		 mCompositionContext->SetDocCSID(default_csid);
		 mCompositionContext->SetWinCSID(INTL_DocToWinCharSetID(default_csid));
	}
}
int16 CComposeSession::GetWinCSID()
{
	Assert_(mCompositionContext);
	if (mCompositionContext)
		return mCompositionContext->GetWinCSID();
	else 
		return 0;
}

Boolean CComposeSession::NeedToSyncAttachmentList(MSG_AttachmentData* inAttachList)
{
	Boolean result = false;
	const MSG_AttachmentData* backendList = MSG_GetAttachmentList(mXPCompositionPane);
	if ( (inAttachList != nil && backendList == nil) ||
		 (inAttachList == nil && backendList != nil) )
	{ // easy case, one list is empty the other is not
		result = true;
	} else if (inAttachList != nil && backendList != nil)
	{ // both lists are non-empty
		// first check list lengths
		Uint32 inLength = GetAttachmentListLength(inAttachList),
			   backendLength = GetAttachmentListLength(backendList);
		if (inLength != backendLength)
			// lengths are different
			result = true;
		else
		{ // both same length, now check to see if both lists contain same data
			MSG_AttachmentData	*inIter = inAttachList,
								*backendIter = const_cast<MSG_AttachmentData*>(backendList);
			while (inIter->url != nil)
			{
				if (XP_STRCMP(inIter->url, backendIter->url))
				{ // URL's are different, need to sync up lists
					result = true;
					break;
				}
				++inIter;
				++backendIter;
			}
		}
	}
	return result;
}
