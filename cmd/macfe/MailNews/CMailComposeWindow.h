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



// CMailComposeWindow.h

#pragma once

// C/C++ headers
//#include <yvals.h>

// PowerPlant
#include <LListener.h>
#include <LBroadcaster.h>
#include <LEditField.h>
#include <LCommander.h>
#include <LHandleStream.h>

// Mail/News MacFE stuff
#include "CBrowserContext.h"
#include "CComposeAddressTableView.h"
//#include "VEditField.h"
#include "CTSMEditField.h"

// UI elements

#include "CMailNewsWindow.h"
#include "CPatternBevelView.h"
#include "CTabSwitcher.h"
// #include "CGrayBevelView.h"

// Netscape stuff
#include "msgcom.h"
#include "mattach.h"
#include "PascalString.h"

class CSimpleTextView;
extern "C" void FE_SecurityOptionsChanged( MWContext* context);
 

const MessageT msg_DeleteCompositionContext = 'DCmC';

const MessageT msg_FinalizeAddrCellEdit		= 'FAdC';

const MessageT msg_RefreshAddrCellEdit		= 'RfAC';

const MessageT msg_ActivatingAttachTab		= 'AcAT';
const MessageT msg_DeactivatingAttachTab	= 'DeAT';
const MessageT msg_ActivatingAddressTab		= 'AcAD';
const MessageT msg_DeactivatingAddressTab	= 'DeAD';
const MessageT msg_ActivatingOptionTab		= 'AcOT';
const MessageT msg_DeactivatingOptionTab	= 'DeOT';

const PaneIDT cSendButtonPaneID				= 'Send';
const PaneIDT cSendLaterButtonPaneID		= 'Sl8r';
const PaneIDT cQuoteButtonPaneID			= 'Quot';
const PaneIDT cStopButtonPaneID				= 'Stop';
const PaneIDT cAddressTab					= 10611;
const PaneIDT cAttachTab					= 10612;
const PaneIDT cOptionTab					= 10617;
const PaneIDT cFormattingToolbar			= 'HTbr';

const CommandT cmd_Attach					= 'Batc';
const MessageT msg_ReturnRecipt				= 'OtRe';
const MessageT msg_Garbled					= 'OtEn';
// const MessageT msg_8BitEncoding			= 'Ot8b';
const MessageT msg_Signed					= 'OtSi';
const MessageT msg_UUEncode					= 'OtUU';
const MessageT msg_HTMLAction				= 'OtHa';
const CommandT cmd_AttachMyAddressBookCard 	= 'AtMA';
class CMailComposeWindow;
class CProgressListener;
class CComposeSession;

//======================================
class UComposeUtilities
//======================================
{
public:
	static void				WordWrap(Handle& inText,
									 Uint32 inTextLength,
									 LHandleStream& outTextStream);

	static MSG_HEADER_SET	GetMsgHeaderMaskFromAddressType(EAddressType inAddressType);
	
	static MSG_PRIORITY		GetMsgPriorityFromMenuItem(Int32 inMenuItem);

	static void				RegisterComposeClasses();
};

//======================================
class CMailCompositionContext	:	 public CBrowserContext
//======================================
{
public:
						CMailCompositionContext();
	virtual				~CMailCompositionContext() {}

	void				Cleanup() { BroadcastMessage(msg_DeleteCompositionContext); }

	virtual void		AllConnectionsComplete();	// call this method
	
	void				CreateContextProgress();
};



//======================================
class CTabContainer	:	public CPatternBevelView
//======================================
{
private: typedef CPatternBevelView Inherited;
public:
	enum { class_ID = 'C_TC' };
						CTabContainer(LStream* inStream);

	virtual				~CTabContainer() { };
	
	virtual void		DrawSelf();
	virtual void		DrawBeveledFrame();
};

//======================================
class CMailTabContainer : public CTabContainer
//=========================================
{
public:
						CMailTabContainer(LStream* inStream);

	virtual				~CMailTabContainer() { };
	virtual void		DrawBeveledFrame();
	virtual void		DrawSelf();
};

//======================================
class CMailComposeTabContainer :	public CMailTabContainer
//======================================
{
private: typedef CTabContainer Inherited;

public:
	enum { class_ID = 'CmTC' };
	
						CMailComposeTabContainer(LStream* inStream);
	virtual				~CMailComposeTabContainer() { };
	virtual void		FinishCreateSelf();
};

//======================================
class CMailAttachmentTabContainer :	public CMailTabContainer
//======================================
{
private: typedef CTabContainer Inherited;
public:
	enum { class_ID = 'AtTC' };
	
						CMailAttachmentTabContainer(LStream* inStream);
	virtual				~CMailAttachmentTabContainer() { };

	virtual void		FinishCreateSelf();
};



typedef struct TERec **TEHandle;
class CMailEditView;

//======================================
class CMailComposeWindow :	public CMailNewsWindow,
							public LBroadcaster,
							public LListener,
							public LPeriodical
//======================================
{
private:
	typedef CMailNewsWindow Inherited;
public:
	enum { class_ID = 'mail', res_ID = 10610, text_res_ID = 10614 };	// this is same class_ID as old compose window
	// Index into mToolbarShown for tracking visibility of toolbars
	// Start at 2 because CMailNewsWindow uses 0 and 1
	enum { FORMATTING_TOOLBAR = 2};
	enum { COMPOSE_BUTTON_BAR_ID	= 10618};
						CMailComposeWindow(LStream* inStream);
	virtual				~CMailComposeWindow();
	virtual void		FinishCreateSelf();
	
	MSG_Pane*			CreateSession(MWContext* old_context,
									  MSG_CompositionFields* inCompositionFields,
									  const char* initialText,
									  Boolean inOpeningAsDraft);
		
	virtual void		ListenToMessage(MessageT inMessage, void* ioParam);
	virtual Boolean 	ObeyCommand(CommandT inCommand, void *ioParam);
	virtual void		FindCommandStatus(
								CommandT			inCommand,
								Boolean				&outEnabled,
								Boolean				&outUsesMark,
								Char16				&outMark,
								Str255				outName);

	virtual void		AttemptClose();
	virtual Boolean		AttemptQuitSelf(Int32 inSaveOption);
	CComposeSession*	GetComposeSession() { return mComposeSession; }
	void				HandleUpdateCompToolbar(); // from FE_UpdateCompToolbar.
	
	virtual Boolean		HandleTabKey(const EventRecord &inKeyEvent);
	virtual Boolean 	HandleKeyPress(const EventRecord &inKeyEvent);

	void				DoCloseLater();
	virtual void		SpendTime(const EventRecord &inMacEvent);

	// I18N stuff
	virtual Int16			DefaultCSIDForNewWindow(void);
	virtual void			SetDefaultCSID(Int16 default_csid);

protected:
	void				SetDefaultWebAttachmentURL();
	void				SendMessage(Boolean inSendNow);
	void 				UpdateSendButton();
	void				SaveDraftOrTemplate(CommandT inCommand, Boolean inCloseWindow = false );
	Boolean				PrepareMessage(Boolean isDraft = false);
	void 				SyncAddressLists();
	void				GetSubjectFromBackend();
	void				GetPriorityFromBackend();
	void				GetAttachmentsFromBackend();
	void				InitializeHTMLEditor(CMailEditView* inEditorView);
	void				GetAllCompHeaders();
	void				SetCompHeader(EAddressType inAddressType);
	void				GetCompHeader(EAddressType inAddressType);
	void				EnsureAtLeastOneAddressee();
	void				SetSensibleTarget();
	char*				GetSubject();

	ExceptionCode		InsertMessageCompositionText(const char* text, Boolean leaveCursorinFront = false);
	void				TargetMessageCompositionText();
	Boolean				NeedToSave();
	Boolean 			AskIfUserWantsToClose();
	virtual ResIDT		GetStatusResID(void) const;
	virtual UInt16		GetValidStatusVersion(void) const { return 0x0115; }
	
	CComposeSession*	mComposeSession;
	CComposeAddressTableView*	mAddressTableView;
	CAttachmentList*	mAttachmentList;
	CAttachmentView*	mAttachmentView;
	CProgressListener*	mProgressListener;
	CMailEditView*		mHTMLEditView; // nil unless HTML mode!
	CSimpleTextView*	mPlainEditView; // nil unless in PlainTextMode
	Boolean				mHeadersDirty; // The address, attachment
	Boolean				mHaveInitializedAttachmentsFromBE;
	enum EInitializeState {
		eUninitialized,
		eComposeSessionIsSet,
		eAboutURLLoading,
		eDone };
	EInitializeState	mInitializeState;
	Boolean				mOnlineLastFindCommandStatus;
	
	CStr255				mDefaultWebAttachmentURL;
	CommandT			mCurrentSaveCommand; // cmd_SaveDraft, cmd_SaveTemplate
}; // class CMailComposeWindow

//======================================
class CComposeTabSwitcher	:	public CTabSwitcher,
								public LBroadcaster
//======================================
{
public:
	enum { class_ID = 'CmTb' };
						CComposeTabSwitcher(LStream* inStream);
	virtual				~CComposeTabSwitcher() { };
	virtual	void		ManuallySwitchToTab( int32 tabID);
	virtual void		DoPostLoad(LView* inLoadedPage, Boolean inWillCache);
	virtual void		DoPreDispose(LView* inLeavingPage, Boolean inWillCache);
};

//======================================
class CMailOptionTabContainer :	public CMailTabContainer
//======================================
{
private: typedef CTabContainer Inherited;

public:
	enum { class_ID = 'OpTC' };
	
						CMailOptionTabContainer(LStream* inStream);
	virtual				~CMailOptionTabContainer() { };
	virtual void		FinishCreateSelf();
};

