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



// CComposeSession.h

#pragma once

// PowerPlant
#include <LListener.h>

// XP
#include "msgcom.h"
#include "structs.h" // mother of all structs (i.e. MWContext)

// MacFE
#include "CProgressBroadcaster.h"
#include "CMailComposeWindow.h" // for CMailCompositionContext

// quote function passed into MSG_QuoteMessage
extern "C" 
{
int FE_QuoteCallback(void* closeure, const char* data);
}

class CComposeSession	:	public CProgressBroadcaster,
							public LListener
{
public:
	enum {	msg_AllConnectionsComplete	= 'ACnC',
			msg_InsertQuoteText			= 'InsQ',
			msg_AutoQuoteDone				= 'AQDn' };

						CComposeSession(Boolean inOpeningAsDraft);
	virtual				~CComposeSession();

	virtual void		ListenToMessage(MessageT inMessage,void *ioParam);

	MSG_Pane*			CreateBackendData(MWContext* inOldContext,
										  MSG_CompositionFields* inCompositionFields);
	
// get data
	const char*			GetSubject();
	MSG_PRIORITY				GetPriority();
	const char*			GetMessageHeaderData(MSG_HEADER_SET inMessageHeaderType);
	const struct MSG_AttachmentData*
						GetAttachmentData();
	Boolean				GetDownloadingAttachments() const { return mDownloadingAttachments; }
	Boolean				GetDeliveryInProgress() const { if ( mXPCompositionPane ) 
															 return MSG_DeliveryInProgress( mXPCompositionPane );
														else
															return false;
													 }
	MSG_Pane*			GetMSG_Pane()	{return mXPCompositionPane;}
	void				MarkMSGPaneDestroyed()	{mXPCompositionPane = nil;}
	CMailCompositionContext* GetCompositionContext() {return mCompositionContext;}
	Boolean				GetAutoQuoting() { return mAutoQuoting; }
// set data
	void				SetMessageHeaderData(MSG_HEADER_SET inMessageHeaderType,
											 LHandleStream& inDataStream);
	void				SetAttachmentList(MSG_AttachmentData* inAttachmentList);
	void				SetPriority(MSG_PRIORITY inPriority);
	void				SetMessageBody(LHandleStream& inHandleStream);
	void				SetHTMLMessageBody();
	void				SetCompositionPaneFEData(void *data);
	void				SetMessage(MessageT command) {mMessage=command;};
	int					SetCompBoolHeader(MSG_BOOL_HEADER_SET header,XP_Bool value);
	XP_Bool				GetCompBoolHeader(MSG_BOOL_HEADER_SET header) 
				{ return  MSG_GetCompBoolHeader(mXPCompositionPane, header);}
// quoting
	void				InsertQuoteText(const char* text);
	void 				AutoQuoteDone();
	Boolean				ShouldAutoQuote() const
							{	return !mOpeningAsDraft
								&& ::MSG_ShouldAutoQuote(mXPCompositionPane);
							}
	void				CheckForAutoQuote();
	void				QuoteInHTMLMessage(const char* text);

// actions
	void				WaitWhileContextBusy();
	void				SendMessage(Boolean inSendNow);
	void				SetSendNow(Boolean inSendNow) { mSendNow = inSendNow; }
	void				SetCloseWindow( Boolean inClose) { mCloseWindowAfterSavingDraft = inClose; }
	void				QuoteMessage();
	void				Stop();
	void				SaveDraftOrTemplate(CommandT inCommand, Boolean inCloseWindow);

// potpourri
	Boolean				NeedToSyncAttachmentList(MSG_AttachmentData* inAttachList);

	// I18N stuff
	virtual Int16		GetDefaultCSID(void);
	virtual void 		SetDefaultCSID(Int16 defaultcsid);
	int16				GetWinCSID();
protected:

	CMailCompositionContext*	mCompositionContext;
	MSG_Pane*					mXPCompositionPane;
	// I must say, the reason why I have to do this...
	Boolean						mDontIgnoreAllConnectionsComplete;
	// Double SIGH!
	Boolean						mDownloadingAttachments;
	Boolean						mSendNow;
	MessageT					mMessage;
	Boolean						mCloseWindowAfterSavingDraft;
	Boolean						mClosing;
	Boolean						mAutoQuoting;
	Boolean						mOpeningAsDraft;
};


