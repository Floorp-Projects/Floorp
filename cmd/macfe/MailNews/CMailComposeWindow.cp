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



// CMailComposeWindow.cp

#include "CMailComposeWindow.h"
#include "rosetta.h"

#include "CSimpleTextView.h"
#include "CEditView.h"
#include "CThreadWindow.h"
#include "CBrowserWindow.h"
#include "CMessageWindow.h"

//#include <LTextModel.h>
//#include <LTextSelection.h>
//#include <LTextEngine.h>
#include <LUndoer.h>
#include <UDrawingState.h>
#include <LPane.h>
#include <LView.h>
#include <LWindow.h>
#include <LArrayIterator.h>
#include <LString.h>
#include <UTextTraits.h>
#include <UMemoryMgr.h>
#include <UReanimator.h>
#include <LDragAndDrop.h>
#include <LSharable.h>

#include <PP_KeyCodes.h>
#include <LGAIconSuiteControl.h>

#include "UOffline.h"
#include "macutil.h"
#include "resgui.h"
#include "uapp.h"
#include "MailNewsgroupWindow_Defines.h"
#include "CComposeSession.h"
#include "UStdDialogs.h"
#include "URobustCreateWindow.h"
#include "LGACheckbox.h"
#include "edt.h"
#include "macgui.h" // MakeLOColor
#include "xpgetstr.h" // for XP_GetString
#include "secnav.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
#include "CTabControl.h"
#include "meditor.h"	// HandleModalDialog
#include "UGraphicGizmos.h"
#include "CPatternButtonPopup.h"
#include "CMailNewsContext.h"
#include "CLargeEditField.h"
#include "CPasteSnooper.h"
#include "UDeferredTask.h"

#include "MailNewsAddressBook.h"
#include "ABcom.h"
#ifdef MOZ_NEWADDR
#include "CAddressPickerWindow.h"
#endif

#define cmd_CheckSpelling					'ChSp'

const Uint32 	cDefaultLineWrapLength 		= 78;
const ResIDT 	COMPOSEDLG_SAVE_BEFORE_QUIT	= 10621; 
const MessageT 	msg_NoSave 					= 3;

#include "msg_srch.h" // needed for priority 
#define UPDATE_WINDOW_TITLE_ON_IDLE 1


#pragma mark -- libmsg callbacks --

#include "prefapi.h"	// temporarily include this until MacFE prefs for ComposeHTML are added

//-----------------------------------
MSG_Pane* FE_CreateCompositionPane(
	MWContext* old_context,
	MSG_CompositionFields* fields,
	const char* initialText,
	MSG_EditorType editorType)
//-----------------------------------
{
	MSG_Pane* result = nil;
	CMailComposeWindow *win = nil;
	try
	{
		CMailNewsContext::ThrowUnlessPrefsSet(MWContextMessageComposition);
		XP_Bool useHtmlEditor;
		PREF_GetBoolPref( "mail.html_compose", &useHtmlEditor );
		// override if
		if (MSG_HTML_EDITOR == editorType)
			useHtmlEditor = TRUE;
		else if (MSG_PLAINTEXT_EDITOR == editorType)
			useHtmlEditor = FALSE;
		else if (IsThisKeyDown(kCtlKey))
			useHtmlEditor = !useHtmlEditor;
			
		ResIDT id = useHtmlEditor ?
						CMailComposeWindow::res_ID
					:	CMailComposeWindow::text_res_ID;
		
		win = dynamic_cast<CMailComposeWindow*>(
			URobustCreateWindow::CreateWindow(
				id,
				LCommander::GetTopCommander()));
		if (win)
		{
			// Now here's a hidden meaning I bet you didn't know.  Opening from the sent or
			// drafts folder is indicated by the value editorType != MSG_DEFAULT!
			Boolean openingAsDraft = editorType != MSG_DEFAULT;
			result = win->CreateSession(old_context, fields, initialText, openingAsDraft);
		}
	}
	catch (...)
	{
		if (win)
			win->DoClose();
	}
	return result;
} // FE_CreateCompositionPane

void FE_UpdateCompToolbar(MSG_Pane* msgpane) // in
{
	if (msgpane)
	{
		void *fedata = MSG_GetFEData(msgpane);
		if (fedata)
		{
			CMailComposeWindow *window = (CMailComposeWindow *)fedata;
			window->HandleUpdateCompToolbar();
		}
	}
}

void FE_DestroyMailCompositionContext(MWContext* context)
{
	MSG_Pane* msgpane;
	msgpane = MSG_FindPane(context, MSG_COMPOSITIONPANE);
	if (msgpane)
	{
		void *fedata = MSG_GetFEData(msgpane);
		if (fedata)
		{
			CMailComposeWindow *window = (CMailComposeWindow *)fedata;
			if (window->IsVisible())
			{
				if (window->GetComposeSession())
					window->GetComposeSession()->MarkMSGPaneDestroyed();
//				window->BroadcastMessage(msg_NSCAllConnectionsComplete, nil);
				window->DoCloseLater();
			}
		}
	}
}

void FE_SecurityOptionsChanged( MWContext* context)
{
	MSG_Pane* msgpane  = MSG_FindPane(context, MSG_COMPOSITIONPANE);
	if (msgpane)
	{
		void *fedata = MSG_GetFEData( msgpane );
		if (fedata)
		{
			CMailComposeWindow *window = (CMailComposeWindow *)fedata;
			USecurityIconHelpers::UpdateComposeButton( window );
			CMailOptionTabContainer *optionTab= dynamic_cast <CMailOptionTabContainer*> (window->FindPaneByID('OpTC') );
			if ( optionTab ) // if the option tab is visible update it
				window-> ListenToMessage( msg_ActivatingOptionTab, optionTab );
		}
	}

}

#pragma mark -

void UComposeUtilities::WordWrap(Handle& inText,
								 Uint32 inTextLength,
								 LHandleStream& outTextStream)
{
	Int32		lineWrapLength = cDefaultLineWrapLength;
	
	PREF_GetIntPref("mailnews.wraplength", &lineWrapLength);
	
	try {
		StHandleLocker lock(inText);
		char	*scanner = *inText,
				*startOfLine = *inText,
				*lastSpace = *inText,
				*end = *inText + inTextLength;
		while (startOfLine < end)
		{
			// if line starts with "> " skip to next line
			if (*startOfLine == '>' && *(startOfLine + 1) == ' ')
			{	// move to next line
				while (scanner < end && *scanner != '\r')
					++scanner;
				// move past '\r'
				++scanner;
				// copy text into stream
				outTextStream.WriteBlock(startOfLine, scanner - startOfLine);
				// set startOfLine to scanner
				startOfLine = scanner;
			}
			else
			{
				// find ' ' in text stream
				while (scanner < end && *scanner != '\r' && *scanner != ' ')
					++scanner;
				// we spit out a line of text if a) we hit the end of the text,
				// b) have enough text to fill a line, or c) hit a carriage return
				if (scanner - startOfLine > lineWrapLength ||
					scanner == end ||
					*scanner == '\r')
				{
					if (*scanner == '\r' || scanner == end)
						outTextStream.WriteBlock(startOfLine, scanner - startOfLine);
					else
						outTextStream.WriteBlock(startOfLine, lastSpace - startOfLine);
					if (scanner < end)
					{
						outTextStream << (unsigned char) '\r';
						if (*scanner == '\r')
							startOfLine = scanner + 1;
						else
							startOfLine = lastSpace + 1;
					}
					else
						startOfLine = scanner;
				}
				lastSpace = scanner;
				++scanner;
			}
		}
	} catch (...) {
	}
}

MSG_HEADER_SET UComposeUtilities::GetMsgHeaderMaskFromAddressType(EAddressType inAddressType)
{
	MSG_HEADER_SET result = 0;
	switch (inAddressType)
	{
		case eToType:
			result = MSG_TO_HEADER_MASK;
			break;
		case eCcType:
			result = MSG_CC_HEADER_MASK;
			break;
		case eBccType:
			result = MSG_BCC_HEADER_MASK;
			break;
		case eReplyType:
			result = MSG_REPLY_TO_HEADER_MASK;
			break;
		case eNewsgroupType:
			result = MSG_NEWSGROUPS_HEADER_MASK;
			break;
		case eFollowupType:
			result = MSG_FOLLOWUP_TO_HEADER_MASK;
			break;
	}
	return result;
}

MSG_PRIORITY UComposeUtilities::GetMsgPriorityFromMenuItem(Int32 inMenuItem)
{
	// These are mapped to MSG_LowPriority, MSG_NormalPriority, MSG_HighPriority,
	// and MSG_HighestPriority, respectively.
	return (MSG_PRIORITY)(inMenuItem - 1 + MSG_LowestPriority);
}


#pragma mark -

//#define REGISTER_(letter,root) \
//	RegisterClass_(letter##root::class_ID, \
//		(ClassCreatorFunc)letter##root::Create##root##Stream);

#define REGISTER_(letter,root) \
	RegisterClass_(letter##root);
		
#define REGISTERC(root) REGISTER_(C,root)
#define REGISTERL(root) REGISTER_(L,root)

//-----------------------------------
void UComposeUtilities::RegisterComposeClasses()
//-----------------------------------
{
	REGISTERC(MailComposeWindow)
	REGISTERC(MailEditView)	
	REGISTERC(MailComposeTabContainer)
	REGISTERC(MailOptionTabContainer)
	REGISTERC(MailAttachmentTabContainer)
	REGISTERC(ComposeAddressTableView)
	REGISTERC(ComposeTabSwitcher)
	REGISTERC(MailAddressEditField)
	#ifdef MOZ_NEWADDR
	REGISTERC(AddressPickerWindow);
	#endif
}

//-----------------------------------
CMailEditView::CMailEditView(LStream * inStream) : CEditView(inStream)
//-----------------------------------
{
	mEditorDoneLoading = false;
	mHasAutoQuoted = false;
	mHasInsertSignature =false;
	mCursorSet = false;
	mComposeSession = nil;
	mInitialText = nil;
	mStartQuoteOffset = 0;
	mEndQuoteOffset = 0;
}

//-----------------------------------
void CMailEditView::InstallBackgroundColor()
// Overridden to use a white background
//-----------------------------------
{
	memset(&mBackgroundColor, 0xFF, sizeof(mBackgroundColor)); // white is default
}

void CMailEditView::GetDefaultBackgroundColor(LO_Color* outColor) const
{
	// if the editor is not done initializing then set the color to mBackgroundColor (white)
	// we do this to keep the window from changing colors (potentially)
	if ( !IsDoneLoading() )
		*outColor = UGraphics::MakeLOColor(mBackgroundColor);
	// else
	// if the editor has completed initialization then we don't want to change outColor
}

//-----------------------------------
void CMailEditView::SetInitialText( const char *textp )
//-----------------------------------
{
	if ( mInitialText )
	{
		XP_FREE( mInitialText );
		mInitialText = nil;
	}
	
	if ( textp )
	{
		mInitialText = XP_STRDUP( textp );
	}
}


//-----------------------------------
void CMailEditView::InitMailCompose()
//-----------------------------------
{
	MWContext *context;
	if (GetContext())
		context = *(GetContext());
	else
		return;
	
	XP_Bool		composeDirty = EDT_DirtyFlag(context);
	
	MSG_Pane *msgpane = MSG_FindPane( context, MSG_COMPOSITIONPANE );

	if (!mEditorDoneLoading)
	{
		mEditorDoneLoading = true;
		MSG_SetHTMLMarkup( msgpane, true );

		// do InstallBackgroundColor again to set the editor background color
		// we need to do this one last time to set the backend editor data to
		// have the correct (white) background color
		InstallBackgroundColor();
		// if mInitalText is nonNull it means that the message is a draft/Edit Message
		// and there fore quoting should not be done.
		if (mComposeSession->ShouldAutoQuote() && !mInitialText)
		{
			mHasAutoQuoted = true;
			mStartQuoteOffset = EDT_GetInsertPointOffset( context );
			EDT_EndOfDocument( context, false );
			mComposeSession->QuoteMessage();
			// Then eventually CComposeSession::QuoteInHTMLMessage will be called.
		}
		else
		{
			EDT_EndOfDocument( context, false );
			mHasInsertSignature = true;
			/* insert signature here!!!!  and Drafts too*/
			DisplayDefaultTextBody();
			EDT_BeginOfDocument( context, false );
			//mCursorSet = true;
		}
		
	}
	else
	{
		
		mEndQuoteOffset =  EDT_GetInsertPointOffset( context );
		if ( !mHasInsertSignature )
		{
			mHasInsertSignature = true;
			EDT_EndOfDocument( context, false );
			
			/* insert signature here!!!!  and Drafts too*/
			DisplayDefaultTextBody();
		//	EDT_BeginOfDocument( context, false );
		//	mCursorSet = true;
		}
	
		if ( !mCursorSet )
		{
			mCursorSet = true;
			if( mHasAutoQuoted )
			{
				int32 eReplyOnTop = 1;
				if ( PREF_NOERROR == PREF_GetIntPref("mailnews.reply_on_top", &eReplyOnTop) )
				{
					switch (eReplyOnTop) 
					{
						case 0:
							EDT_SetInsertPointToOffset( context, mEndQuoteOffset, 0 );
							break;
						case 1:
						default:
							EDT_SetInsertPointToOffset( context, mStartQuoteOffset, 0 );
							break;
						case 2:
						case 3:
							EDT_SetInsertPointToOffset( context, mStartQuoteOffset, 
													mEndQuoteOffset - mStartQuoteOffset );
							break;				
					}
				}
			}
			else
			{		
				EDT_BeginOfDocument( context, false );
				mCursorSet = true;
			}
		}
		
	}
	
	if (!composeDirty)
		EDT_SetDirtyFlag( context , false );
	
} // CMailEditView::InitMailCompose


// Taken from X11
void CMailEditView::InsertMessageCompositionText(const char* text, 
					     XP_Bool leaveCursorBeginning, XP_Bool isHTML)
{

		Boolean  noTagsP = false;
		MWContext *context;
		if (GetContext())
			context = *(GetContext());
		else
			return;
	
		ED_BufferOffset ins = -1;

		if ( leaveCursorBeginning ){
			ins = EDT_GetInsertPointOffset( context );
		}

		if (isHTML) {
			//
			// NOTE:  let's try to see if they really have any HTML tags 
			//        in the text they've given us... [ poor man's parser ]
			//
			if (XP_STRCASESTR(text, "<HTML>") ||
				XP_STRCASESTR(text, "</A>")   ||
				XP_STRCASESTR(text, "<PRE>")  ||
				XP_STRCASESTR(text, "<HR")    ||
				XP_STRCASESTR(text, "<IMG")   ||
				XP_STRCASESTR(text, "<TABLE")
				) {
				noTagsP = false;
			}
			else {		
				noTagsP = true;
			}

			EDT_PasteQuoteBegin( context, isHTML );
			if (noTagsP)  
				EDT_PasteQuote( context, "<PRE>\n");

			EDT_PasteQuote( context, (char *) text );

			if (noTagsP)  
				EDT_PasteQuote( context, "</PRE>\n" );
			EDT_PasteQuoteEnd( context );
		}
		else {
			EDT_PasteText( context, (char *) text );
		}

		if ( leaveCursorBeginning && ins != -1 ) {
			EDT_SetInsertPointToOffset( context, ins, 0 );
		}
}

void	CMailEditView::DisplayDefaultTextBody()
{
     
	const char *pBody = MSG_GetCompBody(mComposeSession->GetMSG_Pane());

	if ( mInitialText && strlen(mInitialText) ) {
		 InsertMessageCompositionText( mInitialText, false, true);
		 InsertMessageCompositionText( "\n", false, true);

		 MSG_SetCompBody(mComposeSession->GetMSG_Pane(), "");
	
		 XP_FREEIF(mInitialText);
	 }
	 else if ( pBody && strlen(pBody) ) {
	 	XP_Bool isHTML =XP_STRCASESTR( pBody,"<HTML>") ? true:false;
	  	// To prevent quoting the SIG. Is there a more correct way to do this?
	  	EDT_ReturnKey( *GetContext() );
		InsertMessageCompositionText( pBody, true, isHTML );
	 }
}

#pragma mark -

//-----------------------------------
CMailComposeWindow::CMailComposeWindow(LStream* inStream) :
//-----------------------------------
	CMailNewsWindow(inStream, WindowType_Compose),
	mComposeSession(nil),
	mAddressTableView(nil),
	mProgressListener(nil),
	mAttachmentList(nil),
	mAttachmentView(nil),
	mHTMLEditView(nil),
	mPlainEditView(nil),
	mInitializeState(eUninitialized),
	mHeadersDirty( false ),
	mOnlineLastFindCommandStatus( true ),
	mDefaultWebAttachmentURL("\p"),
	mCurrentSaveCommand(cmd_SaveDraft)
{
} // CMailComposeWindow::CMailComposeWindow

//-----------------------------------
CMailComposeWindow::~CMailComposeWindow()
//-----------------------------------
{
	StopListening();
	if (mPlainEditView && mPlainEditView->IsOnDuty() )	// for a TSM problem, we have deactivate it first
		SwitchTarget(this);
	
	delete mComposeSession;
	
	if (mAttachmentView)
		mAttachmentView->SetAttachList(nil);
	
	delete mAttachmentList;
	
	delete mProgressListener;
	RemoveAllAttachments();
		// Kludgy, but prevents crash in LUndoer caused by view being destroyed before
		// attachments.
} // CMailComposeWindow::~CMailComposeWindow

//-----------------------------------
ResIDT CMailComposeWindow::GetStatusResID() const
//-----------------------------------
{
	return mHTMLEditView ? 
			CMailComposeWindow::res_ID
		:	CMailComposeWindow::text_res_ID;
} // CMailComposeWindow::GetStatusResID

//-----------------------------------
void CMailComposeWindow::FinishCreateSelf()
//-----------------------------------
{
	mPlainEditView = dynamic_cast<CSimpleTextView*>(FindPaneByID('text'));
	if (mPlainEditView)
	{

//	mPlainEditView->BuildTextObjects(this); // sets the super model
	
//	SetDefaultSubModel(mPlainEditView->GetTextModel());
		
//		LTextSelection* theSelection = mPlainEditView->GetTextSelection();
//		if (theSelection)
//			theSelection->SetSelectionRange(LTextEngine::sTextStart);

		mPlainEditView->SetSelection( 0, 0 );
		mPlainEditView->SetTabSelectsAll( false );
	}
	else
		mHTMLEditView = (CMailEditView *)FindPaneByID(CMailEditView::pane_ID);
		
	try {
		// make the window the undoer.  typing actions end up there.
		LUndoer* theUndoAttachment = new LUndoer;
		AddAttachment(theUndoAttachment); // attach to window
		// create attachment list
		mAttachmentList = new CAttachmentList;
		mHaveInitializedAttachmentsFromBE = false;
	} catch (...) {
	}
	
	// link up compose window to addressing table view and vice versa
	CComposeAddressTableView* tableView = dynamic_cast<CComposeAddressTableView*>(FindPaneByID('Addr'));
	mAddressTableView = tableView;
	mAddressTableView->AddListener( this );
	
	// add an attachment to the subject edit field to strip out CRs
	LEditField* subjectField = dynamic_cast<LEditField*>(FindPaneByID('Subj'));
	Assert_( subjectField );
	subjectField->AddAttachment(new CPasteSnooper(MAKE_RETURNS_SPACES));


//	// set subject as latent sub
//	jrm 97/02/04 removed this, now done more subtly in CreateSession.
//	LEditField* subjectField = dynamic_cast<LEditField*>(FindPaneByID('Subj'));
//	if (subjectField)
//		SetLatentSub(subjectField);

	UReanimator::LinkListenerToControls(this, this, COMPOSE_BUTTON_BAR_ID);
	
	CComposeTabSwitcher* tabSwitcher =
		dynamic_cast<CComposeTabSwitcher*>(FindPaneByID('TbSw'));
	if (tabSwitcher)
		tabSwitcher->AddListener(this);

	USecurityIconHelpers::AddListenerToSmallButton(
		this /*LWindow**/,
		this /*LListener**/);

	LGAIconSuiteControl* offlineButton
		= dynamic_cast<LGAIconSuiteControl*>(FindPaneByID(kOfflineButtonPaneID));
	if (offlineButton)
		offlineButton->AddListener(CFrontApp::GetApplication());

#if UPDATE_WINDOW_TITLE_ON_IDLE
	StartIdling();
#endif
	CSaveWindowStatus::FinishCreateWindow();
	// Build the priority Menu
	LMenu* priorityMenu = new LMenu(10620,"\pPriority");
	for( MSG_PRIORITY priorityToInsert = MSG_LowestPriority; priorityToInsert <= MSG_HighestPriority; priorityToInsert=MSG_PRIORITY(priorityToInsert+1) )
	{
	  char	priorityString[255];
	  MSG_GetPriorityName ( priorityToInsert, priorityString, sizeof( priorityString ) );
	  priorityMenu->InsertCommand(CStr255(priorityString ),0, Int16(priorityToInsert)-2   );
	}
	 CPatternButtonPopup* button = dynamic_cast<CPatternButtonPopup*>(FindPaneByID('Prio')); 
	 if( button )
	 	button->AdoptMenu( priorityMenu );
	 	
	 SetDefaultWebAttachmentURL();
	 
} // CMailComposeWindow::FinishCreateSelf

//-----------------------------------
void CMailComposeWindow::SetDefaultWebAttachmentURL (void)
//-----------------------------------
{
	CWindowIterator	lookForIt(WindowType_Any);
	CMediatedWindow	*parentWindow;
	do {
		lookForIt.Next(parentWindow);	// skip over invisible progress dialog and compose window itself
	} while (
		parentWindow &&
		(parentWindow->GetWindowType() == WindowType_Progress) ||
		(parentWindow->GetWindowType() == WindowType_Compose));
	
	cstring	defaultURL = "\p";
				
	if (parentWindow) {
		switch (parentWindow->GetWindowType()) {
			case WindowType_MailThread:
			{
				CThreadWindow *threadParent = dynamic_cast<CThreadWindow *>(parentWindow);
				if (threadParent)
					defaultURL = threadParent->GetCurrentURL();
			}
			case WindowType_Message:
			{
				CMessageWindow *messageParent = dynamic_cast<CMessageWindow *>(parentWindow);
				if (messageParent)
					defaultURL = messageParent->GetCurrentURL();
			}
			case WindowType_Browser:
			{
				CBrowserWindow *browserParent = dynamic_cast<CBrowserWindow *>(parentWindow);
				if (browserParent)
					defaultURL = browserParent->GetWindowContext()->GetCurrentURL();
			}	
		}
	}
	mDefaultWebAttachmentURL = (char *)defaultURL;
}

//-----------------------------------
MSG_Pane* CMailComposeWindow::CreateSession(
	MWContext* old_context,
	MSG_CompositionFields* inCompositionFields,
	const char* initialText,
	Boolean inOpeningDraft)
//-----------------------------------
{
	MSG_Pane* result = nil;
	try {
		mComposeSession = new CComposeSession(inOpeningDraft);
		mComposeSession->AddListener(this);
		mProgressListener = new CProgressListener(this, mComposeSession);
		
		result = mComposeSession->CreateBackendData(old_context, inCompositionFields);
		if (result)
		{		
		
			// now pull data out from backend
			// mHTMLEditView will be the flag if we are doing HTML mail or plain text
			// if we have an editView we have an HTML mail window
			// if we have an editor window, we need to init/load a blank page
			// we do this after obtaining the subject and addressees so those 
			// fields are properly updated
			SetSensibleTarget();
			if ( !mHTMLEditView )
			{
			//-----------------------------------
			// JRM: NOTE THE ORDERING.  EACH ONE OF THESE CALLS WILL SET ITSELF AS
			// THE TARGET IF IT IS EMPTY.  SO THE ORDERING SHOULD BE MESSAGE, SUBJECT,
			// ADDRESSEES.
			//-----------------------------------
			// check to see if we need to quote message at startup
				mComposeSession->CheckForAutoQuote();
				const char* body =nil;
				body = initialText? initialText:MSG_GetCompBody(mComposeSession->GetMSG_Pane());
				InsertMessageCompositionText( body, true );
			}
			else
			{
				mHTMLEditView->SetInitialText(initialText);
			//	SwitchTarget(mHTMLEditView);
			}
			
			// get subject
			GetSubjectFromBackend();
			// get addresses
			GetAllCompHeaders();
			
			// Get priority
			GetPriorityFromBackend();
			// Set up security button
			USecurityIconHelpers::UpdateComposeButton( this );
			
			// set fe data
			mComposeSession->SetCompositionPaneFEData(this);
			// make sure there is at least an empty addressee
			EnsureAtLeastOneAddressee();
			
			// we initialize the editor which will take care of calling the necessary
			// functions for quoting, signatures, etc. (once the editor is done loading)
			if ( mHTMLEditView )
				this->InitializeHTMLEditor( mHTMLEditView );
			// Set the default encodings
			SetDefaultCSID( DefaultCSIDForNewWindow() );
		}
	} catch (...) {
		// what do we do if we can't create compose session or backend data?
		if (mComposeSession)
			mComposeSession->Stop();
	}
	
	// now that everything is set up, show the window
	Show();
	
	return result;
} // CMailComposeWindow::CreateSession

//-----------------------------------
void CMailComposeWindow::FindCommandStatus(
	CommandT inCommand,
	Boolean &outEnabled,
	Boolean &outUsesMark,
	Char16 &outMark,
	Str255 outName)
//-----------------------------------
{
	switch(inCommand)
	{
		case cmd_Toggle_Paragraph_Toolbar:
			if (mHTMLEditView != nil) {
				outEnabled = true;
				outUsesMark = false;
				if (mToolbarShown[CMailComposeWindow::FORMATTING_TOOLBAR])
					::GetIndString(outName, CEditView::STRPOUND_EDITOR_MENUS,
						CEditView::EDITOR_MENU_HIDE_FORMAT_TOOLBAR);
				else
					::GetIndString(outName, CEditView::STRPOUND_EDITOR_MENUS,
						CEditView:: EDITOR_MENU_SHOW_FORMAT_TOOLBAR);
			} else {
				outEnabled = false;
				outUsesMark = false;
			}
			break;
		case cmd_ToggleToolbar:
			outEnabled = true;
			outUsesMark = false;
			if (mToolbarShown[CMailComposeWindow::MESSAGE_TOOLBAR])
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, HIDE_MESSAGE_TOOLBAR_STRING);
			else
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, SHOW_MESSAGE_TOOLBAR_STRING);
			break;
		case cmd_SendMessage:
		case cmd_SendMessageLater:
			outEnabled = (
				(!mHTMLEditView || mHTMLEditView->IsDoneLoading()) // HTML edit case
				&& !mComposeSession->GetDeliveryInProgress()
				);
			outUsesMark = false;
			if ( mOnlineLastFindCommandStatus != UOffline::AreCurrentlyOnline(  ) )
				UpdateSendButton();
			break;
		case cmd_Save:
			if (mCurrentSaveCommand == cmd_SaveDraft)
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID,
					SAVE_DRAFT_STRING);
			else
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID,
					SAVE_TEMPLATE_STRING);
			// and fall through
		case cmd_SaveDraft:
		case cmd_SaveTemplate:
		case cmd_QuoteMessage:
		case cmd_Attach:
		case msg_AttachWeb:
		case msg_AttachFile:
		case cmd_SecurityInfo:
		case cmd_AddressBookWindow:
			outEnabled = (
				(!mHTMLEditView || mHTMLEditView->IsDoneLoading()) // HTML edit case
				&& !mComposeSession->GetDeliveryInProgress()
				);
			outUsesMark = false;
			break;
		case  cmd_AttachMyAddressBookCard:
			outEnabled = (
				(!mHTMLEditView || mHTMLEditView->IsDoneLoading()) // HTML edit case
				&& !mComposeSession->GetDeliveryInProgress() );
			outUsesMark = true;
			outMark = mComposeSession->GetCompBoolHeader( MSG_ATTACH_VCARD_BOOL_HEADER_MASK)
					? checkMark: noMark;
			break;
		case cmd_PasteQuote:
			Int32	offset;		
			outEnabled = (
				(!mHTMLEditView || mHTMLEditView->IsDoneLoading()) // HTML edit case
				&& !mComposeSession->GetDeliveryInProgress()
				&& ::GetScrap(nil, 'TEXT', &offset) > 0);
			break;
		case cmd_SaveAs:
			if( !mHTMLEditView )
				outEnabled = true;
			else
				Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
		default:
			if(inCommand >= ENCODING_BASE && inCommand < ENCODING_CEILING)
			{
				outEnabled = true;
				outUsesMark = true;
				
				int16 csid = CPrefs::CmdNumToDocCsid( inCommand );
				outMark = (csid == mComposeSession->GetDefaultCSID()) ? checkMark : ' ';
			} else {
				Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			}
			break;
	}
} // CMailComposeWindow::FindCommandStatus

//-----------------------------------
void CMailComposeWindow::HandleUpdateCompToolbar()
// called from FE_UpdateCompToolbar.
//-----------------------------------
{
	if (mInitializeState == eComposeSessionIsSet)
	{
		CMailCompositionContext* context = mComposeSession->GetCompositionContext();
		if (context)
		{
			URL_Struct* url = NET_CreateURLStruct( "about:editfilenew", NET_NORMAL_RELOAD );
			mInitializeState = eAboutURLLoading;
			context->SwitchLoadURL( url, FO_CACHE_AND_PRESENT );
		}
	}
} // CMailComposeWindow::HandleUpdateCompToolbar

//-----------------------------------
Boolean CMailComposeWindow::HandleTabKey(const EventRecord &inKeyEvent)
//-----------------------------------
{
	Boolean		keyHandled = true;
	LCommander	*body = NULL;
	
	if ( mHTMLEditView )
		body = dynamic_cast<LCommander*>( mHTMLEditView );
	else if ( mPlainEditView )
		body = dynamic_cast<LCommander*>( mPlainEditView );
	
	Assert_( body != NULL );
	
	Boolean 	tabBack = (inKeyEvent.modifiers & shiftKey) != 0;
	
	LEditField 	*subjectBar = dynamic_cast<LEditField*>( FindPaneByID('Subj') );
	
	Assert_( subjectBar != NULL );
	Assert_( mAddressTableView != NULL );
	
	LCommander 	*curTarget = NULL, *tempTarget = nil;
	
	// we have to handle the following things here:
	// 1. Collapsed drag bars
	// 2. Reordering of drag bars
	Boolean		addressVisible = true, subjectVisible = true;
	
	// find out ordering in window of subject bar and address panel
	SPoint32	subjectLocation, addressLocation;
	
	subjectBar->GetFrameLocation( subjectLocation );
	mAddressTableView->GetFrameLocation( addressLocation );

	Int32		curItem = 1;
	Int32		curTargetIndex = 1;			//assume first item has focus for now
	Int32		newTargetIndex, numTargets = 0;
	LArray		*targetList = new LArray();

	if (addressVisible )
	{
		tempTarget = dynamic_cast<LCommander*>( mAddressTableView );
		targetList->InsertItemsAt(1, LArray::index_Last, &tempTarget, sizeof( LCommander *) );
		if (tempTarget->IsOnDuty())
			curTarget = tempTarget;
	}
	
	if ( subjectVisible )
	{
		tempTarget = dynamic_cast<LCommander*>( subjectBar );
		targetList->InsertItemsAt(1, LArray::index_Last, &tempTarget, sizeof( LCommander *) );
		if (tempTarget->IsOnDuty())
			curTarget = tempTarget;
	}
	
	if (addressVisible && subjectVisible)
	{
		if ( addressLocation.v >= subjectLocation.v )		// address below subject
			targetList->SwapItems(1, 2);
	}
	
	targetList->InsertItemsAt(1, LArray::index_Last, &body, sizeof( LCommander *) );
	if (body->IsOnDuty())
		curTarget = body;
	
	Assert_(curTarget != NULL);		// something should always have focus.
	
	// now let's work out who is on duty, and get forward and backward		
	curTargetIndex = targetList->FetchIndexOf(&curTarget, sizeof(LCommander *));
	Assert_( curTargetIndex != LArray::index_Bad);
	
	// not so stupid as it seems, when we have collapsed bars
	numTargets = targetList->GetCount();
	
	// choose which one to go to
	if ( tabBack ) {
		newTargetIndex = curTargetIndex - 1;
		if (newTargetIndex < 1)	 				//arrays are 1-based
			newTargetIndex += numTargets;
	} else {
		newTargetIndex = curTargetIndex + 1;
		if (newTargetIndex > numTargets) 		//arrays are 1-based
			newTargetIndex -= numTargets;
	}
	
	LCommander 	*newTarget = NULL;
	
	if ( targetList->FetchItemAt( newTargetIndex, &newTarget) && (newTarget != curTarget) )
	{
		if ( newTarget->ProcessCommand(msg_TabSelect) )
		{
			// ProcessCommand sets the correct target for the mAddressTAbleView so Don't do it here
			if( newTarget != mAddressTableView )  
				SwitchTarget( newTarget );		
		}
		else
			SwitchTarget( body );
	}
	
	delete targetList;
	
	return keyHandled;		// always true for now
}

//-----------------------------------
Boolean CMailComposeWindow::HandleKeyPress(const EventRecord &inKeyEvent)
//-----------------------------------
{
	Boolean		keyHandled = true;
	Char16		theKey = inKeyEvent.message & charCodeMask;
	
	if ( theKey == char_Tab )
		keyHandled = HandleTabKey( inKeyEvent );
	else
		 keyHandled = LCommander::HandleKeyPress(inKeyEvent);
		 
	return keyHandled;
}

//-----------------------------------
void CMailComposeWindow::InitializeHTMLEditor( CMailEditView* inEditorView )
//-----------------------------------
{
	// initialize editor by loading url:  "about:editfilenew"
	if ( inEditorView )
	{
	
		CMailCompositionContext* context = mComposeSession->GetCompositionContext();
		inEditorView->SetContext( context );
		
		// we need to set the compose session for auto-quoting (later)
		inEditorView->SetComposeSession( mComposeSession );
		mInitializeState = eComposeSessionIsSet;

		if (!XP_IsContextBusy(*context))
		{
			// we won't get an FE_UpdateCompToolbar call, so do it ourselves!
			if (mInitializeState == eComposeSessionIsSet)
			{
				HandleUpdateCompToolbar();
			}
		}
	}

}

//-----------------------------------
void CMailComposeWindow::ListenToMessage(MessageT inMessage, void* ioParam)
//-----------------------------------
{
	CAttachmentView* attachView;
	MSG_HTMLComposeAction action;
	switch (inMessage)
	{
		// cmd's sent from toolbar buttons
		case cmd_SendMessage:
			SetSensibleTarget();
			XP_Bool sendNow;
			PREF_GetBoolPref("network.online" , &sendNow);
			SendMessage( sendNow );
			break;
		case cmd_SendMessageLater:
			SetSensibleTarget();
			SendMessage(false);
			break;
		case cmd_Save:
			// This does a "save as template/draft according to last command.
			inMessage = mCurrentSaveCommand;
			// FALL THROUGH
		case cmd_SaveDraft:
		case cmd_SaveTemplate:
			mCurrentSaveCommand = inMessage;
			mComposeSession->SetMessage(inMessage); //needed for a relatively clean Drafts imp.
			SetSensibleTarget();
			SaveDraftOrTemplate(inMessage);
			break;
		case cmd_QuoteMessage:
			SetSensibleTarget();
			mComposeSession->QuoteMessage();
			break;
		case cmd_Stop:
			SetSensibleTarget();
			mComposeSession->Stop();
			// EnableButtons();
			break;
		// messages from CComposeSession
		case CComposeSession::msg_InsertQuoteText:
			//SetSensibleTarget();
			InsertMessageCompositionText(reinterpret_cast<const char*>(ioParam));
			break;
		case CComposeSession::msg_AutoQuoteDone:
			int32 eReplyOnTop = 1;

//		LTextEngine* theTextEngine = mPlainEditView->GetTextEngine();
//		LTextSelection* theSelection = mPlainEditView->GetTextSelection();
//		TextRangeT theBeforeRange = theSelection->GetSelectionRange();		

			SInt32	selStart = 0;
			SInt32	selEnd = 0;
			
			mPlainEditView->GetSelection( &selStart, &selEnd );
			
			if ( PREF_NOERROR ==PREF_GetIntPref("mailnews.reply_on_top", &eReplyOnTop) )
			{
				switch (eReplyOnTop) 
				{
					case 0:
						// Cursor is in the right spot
						break;
					case 1:
					default:
					
//						theBeforeRange.SetStart( 0 );
//						theSelection->SetSelectionRange(theBeforeRange);

							selEnd = selEnd - selStart;
							selStart = 0;
							
							mPlainEditView->SetSelection( selStart, selEnd );
							
						break;
					case 2:
					case 3:

//					long end = theBeforeRange.Start();
//					theBeforeRange.SetStart( 0 );
//					theBeforeRange.SetEnd( end );
//					theSelection->SetSelectionRange(theBeforeRange);

						selEnd = selStart;
						selStart = 0;
						
						mPlainEditView->SetSelection( selStart, selEnd );						
						
						break;				
				}
			}
			break;
			
		case cmd_CheckSpelling:
			if( mHTMLEditView )
				mHTMLEditView->ObeyCommand( inMessage, ioParam );
			else
			{
				if( mPlainEditView )
					mPlainEditView->ObeyCommand( inMessage, ioParam );
			}
			break;
		// message from CComposeTabSwitcher
		// This is the only semi-clean way that I know of to synchronize
		// the attachment list with the CAttachmentView.
		case msg_ActivatingAttachTab:
			SetSensibleTarget();
			attachView = reinterpret_cast<CAttachmentView*>(ioParam);
			if (mAttachmentList && !attachView->GetAttachList())
			{	
				GetAttachmentsFromBackend();
				mHaveInitializedAttachmentsFromBE = true;
				attachView->SetAttachList(mAttachmentList);
				mAttachmentView = attachView;
				mAttachmentView->AddListener( this );
			}
			attachView->AddDropAreaToWindow(this);
			break;
		case msg_DeactivatingAttachTab:
			SetSensibleTarget();
			attachView = reinterpret_cast<CAttachmentView*>(ioParam);
			attachView->RemoveDropAreaFromWindow(this);
			break;
		case msg_ActivatingAddressTab:
			SetSensibleTarget();
			mAddressTableView->AddDropAreaToWindow(this);
			break;
		case msg_DeactivatingAddressTab:
			SetSensibleTarget();
			mAddressTableView->RemoveDropAreaFromWindow(this);
			break;
		case msg_ActivatingOptionTab:
			SetSensibleTarget();
			CMailOptionTabContainer* optionView = reinterpret_cast<CMailOptionTabContainer*>(ioParam);
			
			LGACheckbox *control=dynamic_cast <LGACheckbox *>  (optionView-> FindPaneByID( 'OtRe' ));
			Assert_(control);
			control->SetValue( mComposeSession->GetCompBoolHeader( MSG_RETURN_RECEIPT_BOOL_HEADER_MASK) );
			
			HG43287
			
			LGAPopup *popup = dynamic_cast<LGAPopup*>( optionView->FindPaneByID('OtHa') );
			Assert_( popup != nil );
			if( mHTMLEditView )
			{
				action = MSG_GetHTMLAction( mComposeSession->GetMSG_Pane() );
				Int32 popupValue = 0;
				switch( action)
				{
					case MSG_HTMLAskUser:
						popupValue = 1;
						break;
					case MSG_HTMLConvertToPlaintext:
						popupValue = 2;
						break;
					case  MSG_HTMLSendAsHTML: 
						popupValue = 3;
						break;
					case  MSG_HTMLUseMultipartAlternative:
						popupValue = 4;
						break;
				}
				popup->SetValue( popupValue );
			}
			else
				popup->Disable(); // Not used for plain text editing
			break;
		case cmd_SecurityInfo:
			SetSensibleTarget();
			// Update Header fields
			try
			{
				SyncAddressLists();
				SECNAV_SecurityAdvisor((MWContext*)*(mComposeSession->GetCompositionContext()),
							(URL_Struct_*)nil);
			} catch (...) {
				// couldn't allocate memory for buffer
			}
				break;
		case cmd_AddressBookWindow:
			SetSensibleTarget();
			#ifdef MOZ_NEWADDR
			CAddressPickerWindow::DoPickerDialog( mAddressTableView );
			#else
				CAddressBookManager::ShowAddressBookWindow(  );
			#endif
			break;
	// Option Tab
		case msg_ReturnRecipt:
				mComposeSession->SetCompBoolHeader( MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, *(Int32*)ioParam);
			break;
		case msg_Garbled:
			HG43288
			break;
		case msg_Signed:
			mComposeSession->SetCompBoolHeader( MSG_SIGNED_BOOL_HEADER_MASK, *(Int32*)ioParam);
			USecurityIconHelpers::UpdateComposeButton( this);
			break;
		case msg_UUEncode:
			mComposeSession->SetCompBoolHeader( MSG_UUENCODE_BINARY_BOOL_HEADER_MASK, *(Int32*)ioParam);
			break;
		#if 0
		case msg_8BitEncoding:
			MIME_ConformToStandard(  value ? 0 : 1  );
			break;
		#endif //0
		case msg_HTMLAction:
			switch( *(Int32*)ioParam )
			{
				case 1:
					action = MSG_HTMLAskUser;
					break;
				case 2:
					action = MSG_HTMLConvertToPlaintext;
					break;
				case 3:
					action =  MSG_HTMLSendAsHTML; 
					break;
				case 4:
					action =  MSG_HTMLUseMultipartAlternative;
					break;				
			}
			MSG_SetHTMLAction( mComposeSession->GetMSG_Pane(), action );
		break;
		// Attachments	
		case msg_AttachFile:
		case msg_AttachWeb:
				// Create an attachment view by switching to it and then switch back
				CComposeTabSwitcher* tabSwitcher =
					dynamic_cast<CComposeTabSwitcher*>(FindPaneByID('TbSw'));
				tabSwitcher->ManuallySwitchToTab( 2 );	
				mAttachmentView->ListenToMessage( inMessage, (void *)((unsigned char *)mDefaultWebAttachmentURL));
			break;

		case cmd_AttachMyAddressBookCard:
			mComposeSession->SetCompBoolHeader(MSG_ATTACH_VCARD_BOOL_HEADER_MASK ,
					 !mComposeSession->GetCompBoolHeader( MSG_ATTACH_VCARD_BOOL_HEADER_MASK));
			break;
		// mDirtyHeaders
		case msg_AddressChanged:
		case CAttachmentView::msg_AttachmentsAdded:
		case CAttachmentView::msg_AttachmentsRemoved:
			mHeadersDirty = true;
		break;
	}
}

void CMailComposeWindow::UpdateSendButton()
{
	mOnlineLastFindCommandStatus = UOffline::AreCurrentlyOnline(  );
	 	
	CPatternButton	*sendNowButton = dynamic_cast<CPatternButton *>(FindPaneByID(cSendButtonPaneID));
	const ResIDT kSendNowGraphicID = 15319, kSendLaterGraphicID = 15323;
	sendNowButton->SetGraphicID(
			mOnlineLastFindCommandStatus
		?	kSendNowGraphicID
		:	kSendLaterGraphicID);
	sendNowButton->Refresh();
}

Boolean CMailComposeWindow::ObeyCommand(CommandT inCommand, void *ioParam)
{
	Boolean		cmdHandled = true;
	
	switch (inCommand)
	{
		case cmd_ToggleToolbar:
			ToggleDragBar(cMessageToolbar, CMailNewsWindow::MESSAGE_TOOLBAR);
			cmdHandled = true;
			break;
			
		case cmd_Toggle_Paragraph_Toolbar:
			ToggleDragBar(cFormattingToolbar, CMailComposeWindow::FORMATTING_TOOLBAR);
			cmdHandled = true;
			break;
		
		case cmd_PasteQuote:
			SetSensibleTarget();
			int32 textLen;
			textLen = LClipboard::GetClipboard()->GetData( 'TEXT', nil );
			if (textLen > 0)
			{
				Handle	textHandle = ::NewHandle(textLen + 1);
				if (textHandle)
				{
					LClipboard::GetClipboard()->GetData( 'TEXT', textHandle );
					::HLock(textHandle);
					(*textHandle)[textLen] = '\0';
					if( mHTMLEditView )
						MSG_PastePlaintextQuotation(mComposeSession->GetMSG_Pane(), *textHandle);
					else
					{ //plain text case
						char *quotedText = MSG_ConvertToQuotation (*textHandle);
						if( quotedText )
						{
							InsertMessageCompositionText( quotedText );
							XP_FREE( quotedText );
						}
					}
					::DisposeHandle(textHandle);
				}
			}
		break;
		case cmd_SaveAs:
			if( mHTMLEditView )
				cmdHandled =false;
			else
			{
				StandardFileReply reply;
				short format;
				CStr31 defaultFileName;
				GetDescriptor( defaultFileName );
				UStdDialogs::AskSaveAsSource( reply, defaultFileName , format );
				if ( reply.sfGood ) 
					mPlainEditView->Save( reply.sfFile );
			}	
			break;
		// Commands that have button bar equivalents
		case cmd_SendMessage:
		case cmd_SendMessageLater:
		case cmd_QuoteMessage:
		case cmd_SaveDraft:
		case cmd_SaveTemplate:
		case msg_AttachWeb:
		case msg_AttachFile:
		case cmd_AttachMyAddressBookCard:
			ListenToMessage( inCommand, ioParam);
			break;
		default:
			if(inCommand >= ENCODING_BASE && inCommand < ENCODING_CEILING)
			{
				SetDefaultCSID(CPrefs::CmdNumToDocCsid(inCommand));
				cmdHandled = true;
			} else {
				cmdHandled = Inherited::ObeyCommand(inCommand, ioParam);
			}
			break;
	}
	return cmdHandled;
}
void CMailComposeWindow::SetDefaultCSID(Int16 defaultcsid)
{
	Assert_(mComposeSession);
	if(mComposeSession)
	{
		
		int16 wincsid = INTL_DocToWinCharSetID(defaultcsid);						
		ResIDT textTraitID = CPrefs::GetTextFieldTextResIDs(wincsid);

		// Set the HTML editor, if present
		if (mHTMLEditView)
		{	
			if( !mHTMLEditView->SetDefaultCSID(defaultcsid) )
				return;
		}
		// Set the plain text editor, if present
		if ( mPlainEditView ) {
			mPlainEditView->SetInitialTraits(textTraitID);
			mPlainEditView->ResetModCount();			// fix for bug # 104805 ask save on close
		}

		// Set The Subject Font
		if(CTSMEditField* subjectField = dynamic_cast<CTSMEditField*>(FindPaneByID('Subj')))
		{
			subjectField->SetTextTraitsID(textTraitID);
			subjectField->Refresh();
		}
		
		// Set The Address Font
		if( mAddressTableView )
			mAddressTableView->SetTextTraits( textTraitID );

		// Set the Session
		mComposeSession->SetDefaultCSID(defaultcsid);

#if 0			
		// To Work Around Our TSMTE problem
		Int32 dummyVer;
		if( (defaultcsid & MULTIBYTE ) && 
			(noErr == ::Gestalt(gestaltTSMTEAttr, &dummyVer)) &&
			((dummyVer & (1 << gestaltTSMTEPresent) ) != 0) 
		) {
			this->SetSensibleTarget();
		}
#endif //0
	}
}


void CMailComposeWindow::SyncAddressLists()
{
	
		// Clear the comp headers in case entries were deleted from the
		// address view 
		// Save & Restore the (Reply To) and (Followup To) fields
		char* replyto = nil;
		replyto = XP_STRDUP(
				 mComposeSession->GetMessageHeaderData( MSG_REPLY_TO_HEADER_MASK ) );
		FailNIL_(replyto);
		char* followupto = nil;
		followupto = XP_STRDUP(
					 mComposeSession->GetMessageHeaderData( MSG_FOLLOWUP_TO_HEADER_MASK ) );
		FailNIL_(followupto);
		MSG_ClearComposeHeaders ( mComposeSession->GetMSG_Pane() );
		MSG_SetCompHeader( mComposeSession->GetMSG_Pane(),  
							 MSG_REPLY_TO_HEADER_MASK, replyto );
		MSG_SetCompHeader( mComposeSession->GetMSG_Pane(), 
							 MSG_FOLLOWUP_TO_HEADER_MASK, followupto );
		XP_FREE( replyto );
		XP_FREE( followupto );

		// set composition headers
		for (int type = eToType; type <= eFollowupType; type++)
			SetCompHeader((EAddressType)type);
}

Boolean CMailComposeWindow::PrepareMessage( Boolean isDraft)
{
	// disable all toolbar buttons except stop
	//DisableAllButtonsExceptStop();
	if (mComposeSession)
	{
		SyncAddressLists();
		// set subject
		char* subject = GetSubject();
		// if (**teHandle).teLength == 0, we should probably ask the user
		// if they want to enter a subject right about here
		if (subject && XP_STRLEN( subject ))
		{
			// Copy subject out of TEHandle in edit field and pass it to
			// msglib. Don't forget to dispose of buffer.
			MSG_SetCompHeader(mComposeSession->GetMSG_Pane(),MSG_SUBJECT_HEADER_MASK, subject);
			
			XP_FREE( subject );
		}
#if 1 // This is a temporary hack till we really fix bug #42878.
		else if( !isDraft )
		{
			CTSMEditField* subjectField = dynamic_cast<CTSMEditField*>(FindPaneByID('Subj'));	// use CTSMEditField to support Asian Inline input
			SysBeep(1);
			SwitchTarget(subjectField);
			CStr255 errorSubject(XP_GetString(MK_MIME_NO_SUBJECT));
			subjectField->SetDescriptor(errorSubject);
			subjectField->SelectAll();
			return false;
		}
#endif		
		// set priority
		// pkc -- 10/8/96 priority isn't ready in backend
		LControl* priorityMenu = dynamic_cast<LControl*>(FindPaneByID('Prio'));
		Int32 priority = priorityMenu->GetValue();
		mComposeSession->SetPriority(UComposeUtilities::GetMsgPriorityFromMenuItem(priority));		
		
		// set body
		if ( mPlainEditView )
		{
//		LTextEngine* textEngine = dynamic_cast<LTextEngine*>(mPlainEditView->GetTextEngine());
	
			Handle msgBody = mPlainEditView->GetTextHandle();
			if (msgBody)
			{
				try {
					LHandleStream handleStream;
					// preallocate handle
					handleStream.SetLength(1024);
					
//				TextRangeT textRange = textEngine->GetTotalRange();
					UInt32	textLength = mPlainEditView->GetTextLength();
					
					UComposeUtilities::WordWrap(msgBody, textLength, handleStream);
					handleStream << (unsigned char) '\0';
					mComposeSession->SetMessageBody(handleStream);
				} catch (...) {
					// couldn't allocate memory for LHandleStream
				}
			}
		}
		else
		{
			// send HTML text
			// all of the HTML is gotten from the MWContext
			mComposeSession->SetHTMLMessageBody();
		}	
	}
	if( !mHaveInitializedAttachmentsFromBE )
	{
		GetAttachmentsFromBackend();
		mHaveInitializedAttachmentsFromBE = true;
	}
	
	return true;
} // CMailComposeWindow::PrepareMessage

extern "C" void MSG_ResetUUEncode(MSG_Pane *pane);

void CMailComposeWindow::SaveDraftOrTemplate(CommandT inCommand, Boolean inCloseWindow)
{
		mComposeSession->SetMessage( inCommand );
		MSG_AttachmentData* attachList = nil;
		try {

			// ### mwelch Reset the uuencode UI state so the 
			//     dialog pops up once per send attempt.
			MSG_ResetUUEncode(mComposeSession->GetMSG_Pane());
			
			PrepareMessage( true );
			// set attachments
			attachList = mAttachmentList->NewXPAttachmentList();
			if (mComposeSession->NeedToSyncAttachmentList(attachList))
			{	// send attachments
				// there's no need to call mComposeSession->SaveDraftOrTemplate()
				// because the compose session will catch the FE_AllConnectionsComplete
				// callback when loading the attachments and will SaveDraft then
				mComposeSession->SetAttachmentList(attachList);
				mComposeSession->SetCloseWindow( inCloseWindow );
				XP_FREE(attachList);
			}
			else
			{
				// just save the Draft
				mComposeSession->SaveDraftOrTemplate(inCommand, inCloseWindow);
			}
		} catch (...) {
			XP_FREEIF(attachList);
			// most likely, couldn't allocate memory for attachList
		}
		mHeadersDirty = false;
		if( mPlainEditView )
		{
		
//		LTextEngine* theTextEngine = mPlainEditView->GetTextEngine();
//		theTextEngine->SetTextChanged( false );

			mPlainEditView->ResetModCount();
			
		}
}

void CMailComposeWindow::SendMessage(Boolean inSendNow)
{
		// See if we have to spell check
		XP_Bool needToSpellCheck = false;
		PREF_GetBoolPref("mail.SpellCheckBeforeSend", &needToSpellCheck);
		if ( needToSpellCheck )
		{
			ListenToMessage( cmd_CheckSpelling, NULL );
			ObeyCommand( cmd_CheckSpelling,  NULL );
		}
		
		MSG_AttachmentData* attachList = nil;
		//Boolean needToSave = NeedToSave();
		if( PrepareMessage( false ) )
		{
			// set attachments
			try {
				// ### mwelch. Reset the uuencode UI state so the 
				//     dialog pops up once per send attempt.
				MSG_ResetUUEncode(mComposeSession->GetMSG_Pane());
				
				attachList = mAttachmentList->NewXPAttachmentList();
				if (mComposeSession->NeedToSyncAttachmentList(attachList))
				{	// send attachments
				// there's no need to call mComposeSession->SendMessage()
				// because the compose session will catch the FE_AllConnectionsComplete
				// callback when loading the attachments and will call SendMessage then
					mComposeSession->SetAttachmentList(attachList);
					XP_FREE(attachList);
					mComposeSession->SetSendNow(inSendNow);
				}
				else
				{
					// just send message
					mComposeSession->SendMessage(inSendNow);
				}
			} catch (...) {
				XP_FREEIF(attachList);
				// ### mwelch. We can get exceptions by running out of memory
				//     or if the user hit "Cancel" to a uuencode confirmation
				//     dialog. (Yes, the back end throws an exception here.)
				//     See MSG_DeliverMimeAttachment::SnarfAttachment in
				//     ns/lib/libmsg/msgsend.cpp.
			}
		}
		//mHeadersDirty = needToSave;		// if send fails, dirty flag is still preserved
}

void CMailComposeWindow::SetCompHeader(EAddressType inAddressType)
{
	if (mComposeSession && mAddressTableView)
	{
		try {
			LHandleStream stream;
			// preallocate handle
			stream.SetLength(512);
			stream.SetMarker(0, streamFrom_Start);
			mAddressTableView->CreateCompHeader(inAddressType, stream);
			if (stream.GetMarker() > 0)
				mComposeSession->SetMessageHeaderData(
					UComposeUtilities::GetMsgHeaderMaskFromAddressType(inAddressType),
					stream);
		} catch (...) {
		}
	}
}


//-----------------------------------
void CMailComposeWindow::GetCompHeader(EAddressType inAddressType)
//-----------------------------------
{
	Assert_(mAddressTableView && mComposeSession);
	char* scanner = (char *)mComposeSession->GetMessageHeaderData(
							UComposeUtilities::GetMsgHeaderMaskFromAddressType(inAddressType));
	MSG_HeaderEntry *returnList=nil;	
	Int32 numberItems= MSG_ExplodeHeaderField( inAddressType, scanner, &returnList );
	if ( numberItems!=-1 )
	{
		for(Int32 currentItem=0; currentItem<numberItems; currentItem++ )
		{
			mAddressTableView->InsertNewRow(EAddressType(returnList[currentItem].header_type), 
										returnList[currentItem].header_value );
			XP_FREE( returnList[currentItem].header_value );
		}
		XP_FREE ( returnList );
	}
	
	#if 0 // MSGComh. has a better function
	if (scanner)
	{
		// now parse comma delimited string
		char *addr = scanner;
		while (*scanner != nil)
		{
			// find end of address
			while (*scanner != nil && *scanner != ',')
				++scanner;
			try {
				char* addrStr = new char[scanner - addr + 1];
				::BlockMoveData(addr, addrStr, scanner - addr);
				addrStr[scanner - addr] = '\0';
				mAddressTableView->InsertNewRow(inAddressType, addrStr);
				delete [] addrStr;
			} catch (...) {
			}
			while (*scanner != nil && (*scanner == ',' || *scanner == ' ' || *scanner == '\t'))
				++scanner;
			addr = scanner;
		}
	}	
	#endif //0
} // CMailComposeWindow::GetCompHeader

//-----------------------------------
Int16 	CMailComposeWindow::DefaultCSIDForNewWindow()
//-----------------------------------
{
	if(mComposeSession)
		return mComposeSession->GetDefaultCSID();	// Delgate to  mComposeSession
	else
		return 0;
} // CMailComposeWindow::DefaultCSIDForNewWindow()

//-----------------------------------
void CMailComposeWindow::GetAllCompHeaders()
//-----------------------------------
{
	for (int i = eToType; i <= eFollowupType; i++)
		if (i != eReplyType)
			GetCompHeader((EAddressType)i);
}

//-----------------------------------
char* CMailComposeWindow::GetSubject()
// Caller responsible for freeing return value
//-----------------------------------
{
	char* result = nil;
	CLargeEditField* subjectField = dynamic_cast<CLargeEditField*>(FindPaneByID('Subj'));	// use CTSMEditField to support Asian Inline input
	if (subjectField)
		result = subjectField->GetLongDescriptor();
	Assert_(result);
	return result;
} // CMailComposeWindow::GetSubjectTEHandle

//-----------------------------------
void CMailComposeWindow::GetSubjectFromBackend()
//-----------------------------------
{
	Assert_(mComposeSession);
	const char* subject = mComposeSession->GetSubject();
	
	// We need to set up the TextTrait of Subject according to the csid.
	CLargeEditField* subjectField = dynamic_cast<CLargeEditField*>(FindPaneByID('Subj'));
	
	if (subject && *subject)
		subjectField->SetLongDescriptor( subject );
	else
	{
		SwitchTarget( subjectField );
	}
} //  CMailComposeWindow::GetSubjectFromBackend


void  CMailComposeWindow::GetPriorityFromBackend()
{
	LControl *priority = dynamic_cast<LControl*>( FindPaneByID('Prio') );
	if( priority )
	{
		// Need to do this to ensure that we refresh the menu with new value
		priority->SetValue( 1 ); 
		MSG_PRIORITY backendPriority = mComposeSession->GetPriority();
		if( backendPriority == MSG_NoPriority )
			backendPriority = MSG_NormalPriority;
		// MSG_NoPriority isn't a popup item so all numbers off by 1
		Int32 value = Int32( backendPriority) - Int32( MSG_NoPriority ); 
		priority->SetValue( value );
	}
}
//-----------------------------------
void CMailComposeWindow::GetAttachmentsFromBackend()
//-----------------------------------
{
	Assert_(mComposeSession);
	const struct MSG_AttachmentData* attachData =
		mComposeSession->GetAttachmentData();
	if (attachData)
	{
		Assert_(mAttachmentList);
		mAttachmentList->InitializeFromXPAttachmentList(attachData);
	}
}

//-----------------------------------
void CMailComposeWindow::EnsureAtLeastOneAddressee()
//-----------------------------------
{
	Assert_(mAddressTableView);
	TableIndexT numRows;
	mAddressTableView->GetNumRows(numRows);
	Boolean insertNewRow = true;
	while ( numRows )
	{
		EAddressType addressType = mAddressTableView->GetRowAddressType( numRows );
		if( addressType != eBccType)
		{
			insertNewRow = false;
			break;
		}
		numRows--;
	}
	
	if ( insertNewRow )
	{
		mAddressTableView->InsertNewRow( true, true);
	}
	
}

//-----------------------------------
void CMailComposeWindow::SetSensibleTarget()
//-----------------------------------
{	

	if (mPlainEditView)
	{
		SwitchTarget(mPlainEditView);
	}
	if (mHTMLEditView)
		SwitchTarget(mHTMLEditView);	
}

//-----------------------------------
ExceptionCode CMailComposeWindow::InsertMessageCompositionText(const char* text, Boolean leaveCursorinFront)
// this code taken straight from mailmac.cp
// Actually seems to be called only for plain-text quoting.
//-----------------------------------
{
#define SUCCESS 0	
	ExceptionCode err = SUCCESS;
//	if (text == nil) // Don't do this.
//		Even if text is nil, this function must set the target to the appropriate text view.
//		return err;

	StWatchCursor	watchCursor;

	try	// TextReplaceByPtr can throw
	{
		
		Assert_(mPlainEditView); // seems to be called only for simple text.
		if (mPlainEditView)
		{
		//	SwitchTarget(mPlainEditView);
			if (!text || !*text)
				return SUCCESS;
	
			Boolean textChanged = (mPlainEditView->GetModCount() > 0);
			

			//
			// Blast the text into the text field.
			//
			// We must first ensure that the field is not marked
			// read-only (which is likely since we've probably been
			// asked by XP to disable toolbar buttons, at which
			// point we also disable the body field).
			//
			// Save the field's attributes, mark it writable,
			// blast the text, restore the attributes.
			//

			Boolean	isReadOnly = mPlainEditView->IsReadOnly();
			
			if ( isReadOnly )
				mPlainEditView->SetReadOnly( false );
							
			SInt32	oldSelStart = 0;
			SInt32	oldSelEnd = 0;
			
			mPlainEditView->GetSelection( &oldSelStart, &oldSelEnd );
			
			SInt32		textLen = text ? XP_STRLEN(text) : 0;
			
			if (text != NULL)
				mPlainEditView->InsertPtr( const_cast<char *>(text), textLen, NULL, NULL, false, false );
			
			// We need to move the selection range manually
			
			if ( !leaveCursorinFront )
			{
				oldSelStart = oldSelStart + textLen;
				
				if ( oldSelEnd < oldSelStart )
					oldSelEnd = oldSelStart;
			}
				
			mPlainEditView->SetSelection( oldSelStart, oldSelEnd );
			
			if (! textChanged)
				mPlainEditView->ResetModCount();
		}

	}
	catch (ExceptionCode inErr)
	{
		err = inErr;
	}
	
	return err;
} // CMailComposeWindow::InsertMessageCompositionText

Boolean CMailComposeWindow::NeedToSave()
{
	
	// attachments and addresses broadcast when changed
	SetSensibleTarget();
	if( mHeadersDirty )
		return true;
	Boolean dirty = true;
	
	// Subject
	CStr255  subject( mComposeSession->GetSubject() );
	CTSMEditField* subjectField = dynamic_cast<CTSMEditField*>(FindPaneByID('Subj'));
	CStr255  currentText;
	if (subjectField)
		subjectField->GetDescriptor(currentText);
	dirty = (currentText != subject);
	if (dirty)
		return dirty;
	//Body
	
	if (mHTMLEditView)
		dirty = EDT_DirtyFlag( mComposeSession->GetCompositionContext() ->operator MWContext*() );
	else
	{
	
//	LTextEngine* theTextEngine = mPlainEditView->GetTextEngine();
		
		dirty = (mPlainEditView->GetModCount() > 0);

	}
	return dirty;
	
} // CMailComposeWindow::NeedToSave

Boolean CMailComposeWindow::AskIfUserWantsToClose()
{
	MessageT itemHit = HandleModalDialog( COMPOSEDLG_SAVE_BEFORE_QUIT, nil, nil );
		if (itemHit == cancel)
			return false;
					
		if (itemHit == ok)
		{		
			SaveDraftOrTemplate(cmd_SaveDraft, true );
			return false ; // Drafts will close the window Clean up for 4.02
		}
		return true;
}

void CMailComposeWindow::AttemptClose()
{
	Select(); // This helps for "close all"
	if (mComposeSession->GetDownloadingAttachments()
			|| XP_IsContextBusy(*mComposeSession->GetCompositionContext()))
	{
		mComposeSession->Stop();
		return;					// do we really want to return here? See bug #73544
								// need this return to cancel close of aborted send
								// but it might leave open a window with partially
								// loaded attachments, which could cause us to send
								// bad data.
								// see also bug 107078 that prompted this comment.
	}
	else
	{
		if (NeedToSave() && !AskIfUserWantsToClose())
			return;
	}
	Inherited::AttemptClose();	
} // CMailComposeWindow::AttemptClose()

//----------------------------------------------------------------------------------------
Boolean CMailComposeWindow::AttemptQuitSelf(Int32 inSaveOption)
//----------------------------------------------------------------------------------------
{
	if (mComposeSession->GetDownloadingAttachments()
			|| XP_IsContextBusy(*mComposeSession->GetCompositionContext()))
	{
		mComposeSession->Stop();
		return false;
	}

	if (NeedToSave() && !AskIfUserWantsToClose())
		return false;
	return Inherited::AttemptQuitSelf(inSaveOption);	
} // CMailComposeWindow::AttemptQuitSelf

//----------------------------------------------------------------------------------------
void CMailComposeWindow::DoCloseLater()
//----------------------------------------------------------------------------------------
{
	CDeferredCloseTask::DeferredClose(this);
} // CMailComposeWindow::DoCloseLater

//----------------------------------------------------------------------------------------
void CMailComposeWindow::SpendTime(const EventRecord &/*inMacEvent*/)
//----------------------------------------------------------------------------------------
{
	// Update the window title to match the subject
	CTSMEditField* subjectField = dynamic_cast<CTSMEditField*>(FindPaneByID('Subj'));
	CStr255 currentSubject;
	if (subjectField)
	{
		subjectField->GetDescriptor(currentSubject);
		// Leave the default window title until they've typed something.
		if (currentSubject != CStr255::sEmptyString)
		{
			CStr255 windowTitle;
			this->GetDescriptor(windowTitle);
			// Test for equality to avoid flickering.
			if (windowTitle != currentSubject)
				this->SetDescriptor(currentSubject);
		}
	}
} // CMailComposeWindow::SpendTime

#pragma mark -

CTabContainer::CTabContainer(LStream* inStream) :
CPatternBevelView(inStream)
{
}

void CTabContainer::DrawBeveledFrame(void)
{
	// Overridden to draw a partial-rect border
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	SBooleanRect theBevelSides = { true, false, true, true };	
	UGraphicGizmos::BevelTintPartialRect(theFrame, mMainBevel, 0x4000, 0x4000, theBevelSides);
}

void CTabContainer::DrawSelf()
{
#if 0
	Inherited::DrawSelf();
	Rect theFrame;
	if (CalcLocalFrameRect(theFrame))
		{
		StColorPenState theSaver;
		theSaver.Normalize();
		
		SBevelColorDesc theDesc;
		UGraphicGizmos::LoadBevelTraits(5000, theDesc);
	
		StClipRgnState theClipSaver(theFrame);
		StColorState::Normalize();
		
		theFrame.left-=5; 
		::FrameRect(&theFrame);

		SBooleanRect theBevelSides = { false, true, true, true };	
		UGraphicGizmos::BevelPartialRect(theFrame, 1, eStdGrayBlack, eStdGrayBlack, theBevelSides);
		::InsetRect(&theFrame, 1, 1);
		UGraphicGizmos::BevelPartialRect(theFrame, 2, theDesc.topBevelColor, theDesc.bottomBevelColor, theBevelSides);
		}
#endif
	Inherited::DrawSelf();
	Rect theFrame;
	if (CalcLocalFrameRect(theFrame))
		{
		StColorPenState theSaver;
		theSaver.Normalize();
		
		StClipRgnState theClipSaver(theFrame);
		StColorState::Normalize();

		theFrame.top -= 5; // to hide the top
		::FrameRect(&theFrame);
		}
}

CMailTabContainer::CMailTabContainer(LStream* inStream) : CTabContainer( inStream )
{
}

void CMailTabContainer::DrawBeveledFrame(void)
{
	// Overridden to draw a partial-rect border
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	SBooleanRect theBevelSides = { false, true, true, true };	
	UGraphicGizmos::BevelTintPartialRect(theFrame, mMainBevel, 0x4000, 0x4000, theBevelSides);
}

void CMailTabContainer::DrawSelf()
{
	Inherited::DrawSelf();
	Rect theFrame;
	if (CalcLocalFrameRect(theFrame))
		{
		StColorPenState theSaver;
		theSaver.Normalize();
		
		StClipRgnState theClipSaver(theFrame);
		StColorState::Normalize();

		theFrame.left -= 5; // to hide the left
		::FrameRect(&theFrame);
		}
}

#pragma mark -

CMailComposeTabContainer::CMailComposeTabContainer(LStream* inStream) :
CMailTabContainer(inStream)
{
}

void CMailComposeTabContainer::FinishCreateSelf()
{
	//UReanimator::LinkListenerToControls(dynamic_cast<CComposeAddressTableView*>(FindPaneByID('Addr')),this,10611);
}


#pragma mark -

CMailAttachmentTabContainer::CMailAttachmentTabContainer(LStream* inStream) :
CMailTabContainer(inStream)
{
}

void CMailAttachmentTabContainer::FinishCreateSelf()
{
//	CAttachmentView* attachView = dynamic_cast<CAttachmentView*>(FindPaneByID('Attv'));
//	try {
//		attachView->SetAttachList(new CAttachmentList);
//	} catch (...) {
//	}
	UReanimator::LinkListenerToControls(dynamic_cast<CAttachmentView*>(FindPaneByID('Attv')),this,10612);
}

#pragma mark -

CMailCompositionContext::CMailCompositionContext() :
CBrowserContext(MWContextMessageComposition)
{
	MWContext *mwcontext = operator MWContext*();
	if ( mwcontext )
	{
		mwcontext->is_editor = true;
		mwcontext->bIsComposeWindow = true;
	}
}

void CMailCompositionContext::AllConnectionsComplete()
{
	StSharer share(this);
	CBrowserContext::AllConnectionsComplete();
}

void CMailCompositionContext::CreateContextProgress()
{
	try {
		mProgress = new CContextProgress;
		mProgress->AddUser(this);
	} catch (...) {
	}
}

#pragma mark -

CComposeTabSwitcher::CComposeTabSwitcher(LStream* inStream) :
CTabSwitcher(inStream)
{
}

void CComposeTabSwitcher::ManuallySwitchToTab( int32 tabID)
{
	CTabControl* theTabControl = (CTabControl*)FindPaneByID(mTabControlID);
	theTabControl->SetValue( tabID );
}
void CComposeTabSwitcher::DoPostLoad(LView* inLoadedPage, Boolean /*inWillCache*/)
{
	
	LView* view = (LView*)inLoadedPage->FindPaneByID('Attv');
	if (view)
	{
		CAttachmentView* attachView = dynamic_cast<CAttachmentView*>(view);
		BroadcastMessage(msg_ActivatingAttachTab, attachView);
	}
	else if ((LView*)inLoadedPage->FindPaneByID('Addr'))
	{
		BroadcastMessage(msg_ActivatingAddressTab,nil);
	} 
	
	view = ( LView*)inLoadedPage->FindPaneByID('OpTC');
	if( view )
	{
		CMailOptionTabContainer* optionView = dynamic_cast<CMailOptionTabContainer*>(view);
		BroadcastMessage( msg_ActivatingOptionTab, optionView );
	}
	
}

void CComposeTabSwitcher::DoPreDispose(LView* inLeavingPage, Boolean /*inWillCache*/)
{
	LView* view = (LView*)inLeavingPage->FindPaneByID('Attv');
	if (view)
		BroadcastMessage(msg_DeactivatingAttachTab, view);
	else if ((LView*)inLeavingPage->FindPaneByID('Addr'))
	{
		BroadcastMessage(msg_DeactivatingAddressTab, nil);
	}
}
	
//======================================
// class CMailOptionTabContainer :	public CTabContainer
//======================================
CMailOptionTabContainer::CMailOptionTabContainer(LStream* inStream):CMailTabContainer(inStream)
{
	
}

void CMailOptionTabContainer::FinishCreateSelf()
{
	CMailComposeWindow* window
			= dynamic_cast<CMailComposeWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	if( window )
	{
		UReanimator::LinkListenerToControls( window, this, 10617);	
	}
}
