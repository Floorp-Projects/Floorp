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
 
#include "meditdlg.h"
#include "meditor.h" 	// HandleModalDialog
#include "shist.h"
#include "CEditView.h"
#include "edt.h"
#include "pa_tags.h"
#include "secnav.h"		// SECNAV_MungeString, SECNAV_UnMungeString (password munging calls for publishing)

// macfe
#include "ulaunch.h"	// StartDocInApp
#include "macgui.h"		// StPrepareForDialog, UGraphics
#include "resgui.h"		// msg_Help, msg_Apply, CLOSE_STR_RESID, EDITOR_PERCENT_PARENT_CELL
#include "uerrmgr.h"
#include "uapp.h"		// CFrontApp::DoGetURL
#include "macutil.h"
#include "ufilemgr.h"
#include "CPrefsDialog.h"
#include "prefapi.h"
#include "prefwutil.h"	// SetColor
#include "CNSContext.h"	// ExtractHyperView
#include "CLargeEditField.h"
#include "CPrefsMediator.h"
#include "URobustCreateWindow.h"
#include "UGraphicGizmos.h"

// powerplant
#include "CTabControl.h"
#include "LToggleButton.h"
#include "StBlockingDialogHandler.h"
#include "UMenuUtils.h"
#include "LGAEditField.h"
#include "LGAPopup.h"
#include "CTabSwitcher.h"
#include <LStdControl.h>
#include <UDesktop.h>
#include <UReanimator.h>
#include <URegistrar.h>
#include <UTextTraits.h>

#include "CCaption.h"	// new location for CCaptionDisable ( renamed to CCaption )
#include "xp_help.h"


extern char *XP_NEW_DOC_NAME;

static Boolean IsEditFieldWithinLimits(LGAEditField* editField, int minVal, int maxVal );


static void SetTextTraitsIDByCsid(LEditField* pane, int16 win_csid)
{
	ThrowIfNil_(pane);
	static int16 res_csid = -1;
	if(-1 == res_csid)	// un-initialized
		res_csid = INTL_CharSetNameToID(INTL_ResourceCharSet());
	if(win_csid != res_csid)
		pane->SetTextTraitsID(CPrefs::GetTextFieldTextResIDs(win_csid));
}


// input: c-string; output: c-string
// returns true if the user clicked ok
static Boolean GetExtraHTML( char *pExtra, char **newExtraHTML , int16 win_csid)
{
	StBlockingDialogHandler	handler(5114, LCommander::GetTopCommander());
	LWindow *dlog = (LWindow *)handler.GetDialog();
	
	LControl *okButton = (LControl *)dlog->FindPaneByID( 'OKOK' );
	if ( okButton == NULL )
		return false;
	
	CLargeEditField *editField = (CLargeEditField *)dlog->FindPaneByID( 'Xhtm' );
	if ( editField == NULL )
		return false;
	SetTextTraitsIDByCsid( editField, win_csid );
	
	dlog->SetLatentSub( editField );
	if ( pExtra )
	{
		editField->SetLongDescriptor( pExtra );
		editField->SelectAll();
	}
	
	MessageT theMessage;
	do
	{
		char *pCurrent = editField->GetLongDescriptor(); 
		if ( pCurrent == NULL || ( pExtra && XP_STRCMP( pExtra, pCurrent ) == 0 ) )
			okButton->Disable();
		else
			okButton->Enable();
		
		if ( pCurrent )
			XP_FREE( pCurrent );
		
		theMessage = handler.DoDialog();
//			case msg_Help:
//				newWindow->Help();
//				break;
	} while ( theMessage != msg_OK && theMessage != msg_Cancel );
	
	if ( theMessage == msg_OK )
		*newExtraHTML = editField->GetLongDescriptor();
	
	return theMessage == msg_OK;
}


void CChameleonView::SetColor(RGBColor textColor)
{
	fTextColor.red = textColor.red;
	fTextColor.green = textColor.green;
	fTextColor.blue = textColor.blue;
}

void CChameleonView::DrawSelf()
{
	Rect	frame;
	CalcLocalFrameRect(frame);
	
	ApplyForeAndBackColors();
	::RGBBackColor(&fTextColor);
	
	EraseRect(&frame);
}


void CChameleonCaption::SetColor(RGBColor textColor, RGBColor backColor)
{
	fTextColor.red = textColor.red;
	fTextColor.green = textColor.green;
	fTextColor.blue = textColor.blue;
	
	fBackColor.red = backColor.red;
	fBackColor.green = backColor.green;
	fBackColor.blue = backColor.blue;
}


void CChameleonCaption::DrawSelf()
{
	Rect	frame;
	CalcLocalFrameRect(frame);
	
	Int16	just = UTextTraits::SetPortTextTraits(mTxtrID);
	
	::RGBForeColor(&fTextColor);
	::RGBBackColor(&fBackColor);
	
	UTextDrawing::DrawWithJustification((Ptr)&mText[1], mText[0], frame, just);
}


void CEditorPrefContain::DrawSelf()
{
	Rect theFrame;
	if (CalcLocalFrameRect(theFrame))
		{
		StColorPenState theSaver;
		theSaver.Normalize();
		
		SBevelColorDesc theDesc;
		UGraphicGizmos::LoadBevelTraits(5000, theDesc);
	
		::PmForeColor(theDesc.fillColor);
		::PaintRect(&theFrame);
	
		StClipRgnState theClipSaver(theFrame);
		StColorState::Normalize();
		
		theFrame.top -= 5;
		::FrameRect(&theFrame);

		SBooleanRect theBevelSides = { true, true, true, true };	
		UGraphicGizmos::BevelTintPartialRect(theFrame, 3, 0x4000, 0x4000, theBevelSides);
		}
}


#pragma mark -
/**********************************************************/
// This is probably stupid. This just allows all the dialogs I'm creating
// to remember who created them. Also the creator can specify which tab the
// Tab control should start on, if there is a tab control. Also, if this
// is an image, link, or HRule dialog, the creator can specify whether the
// final action should be to insert or modify a selected element.

Boolean CEditDialog::Start(ResIDT inWindowID, MWContext * context, short initTabValue, Boolean insert)
{
	CEditDialog *dlog = (CEditDialog *)URobustCreateWindow::CreateWindow( inWindowID, LCommander::GetTopCommander() );
	if ( dlog == NULL )
		return false;

	UReanimator::LinkListenerToControls( dlog, dlog, inWindowID );		// the handler is listening; but we want a crack at the messages also...
	dlog->SetContext( context );
	dlog->SetInitTabValue( initTabValue );
	dlog->SetInWindowID( inWindowID );
	dlog->SetInsertFlag( insert );
	dlog->InitializeDialogControls();
	dlog->Show();
	
	return true;
}


void CEditDialog::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch ( inMessage )
	{
		case msg_Help:
			Help();
			break;
		
		case 'Xtra':
			char * newExtraHTML = NULL;
			INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo( GetContext() );
			Boolean canceled = !GetExtraHTML( pExtra, &newExtraHTML ,INTL_GetCSIWinCSID(csi) );
			if (!canceled)
			{
				if ( pExtra )
					XP_FREE( pExtra );
				pExtra = newExtraHTML;
			}
			break;

		case msg_OK:
		case msg_Apply:
			LCommander* theTarget = GetTarget();
			if ( theTarget == NULL || theTarget->SwitchTarget(this) )
			{
				if ( CommitChanges( true ) )
				{
					if ( inMessage == msg_OK )
					{
						if ( mUndoInited )
							EDT_EndBatchChanges( fContext );
						
						LDialogBox::ListenToMessage( cmd_Close, ioParam );
						return;
					}
					
					LStdControl* cancel = (LStdControl*)FindPaneByID( 'CANC' );
					if (cancel)
					{
						StringHandle CloseHandle = GetString(CLOSE_STR_RESID);
						if ( CloseHandle )
						{
							SInt8 hstate = HGetState( (Handle)CloseHandle );
							HLock( (Handle)CloseHandle );
							cancel->SetDescriptor( *CloseHandle );
							HSetState( (Handle)CloseHandle, hstate );
						}
					}
				}
			}
			break;
		
		case msg_Cancel:
			if ( mUndoInited )
				EDT_EndBatchChanges( fContext );
			
			LDialogBox::ListenToMessage( cmd_Close, ioParam );
			break;
		
		default:
			LDialogBox::ListenToMessage( inMessage, ioParam );
	}
}


Boolean CEditDialog::AllowSubRemoval( LCommander * /* inSub */ )
{
	// don't allow removal
	return false;
}


// copied from StDialogHandler
void CEditDialog::FindCommandStatus(
	CommandT	/* inCommand */,
	Boolean		&outEnabled,
	Boolean&	/* outUsesMark */,
	Char16&		/* outMark */,
	Str255		/* outName */)
{
		// Don't enable any commands except cmd_About, which will keep
		// the Apple menu enabled. This function purposely does not
		// call the inherited FindCommandStatus, thereby suppressing
		// commands that are handled by SuperCommanders. Only those
		// commands enabled by SubCommanders will be active.
		//
		// This is usually what you want for a moveable modal dialog.
		// Commands such as "New", "Open" and "Quit" that are handled
		// by the Applcation are disabled, but items within the dialog
		// can enable commands. For example, an EditField would enable
		// items in the "Edit" menu.
		
	outEnabled = false;
	// actually we don't want to enable "about" either as it might open
	// a browser window or ??? which would be very bad
}


void CEditDialog::ChooseImageFile(CLargeEditField* editField)
{
	if ( editField == NULL )
		return;
	
	StPrepareForDialog	preparer;	
	StandardFileReply	reply;
	Point				loc = { -1, -1 };
	OSType				types[ 4 ];

	types[ 0 ] = 'GIFf';
	types[ 1 ] = 'TEXT';
	types[ 2 ] = 'JPEG';
	types[ 3 ] = 'JFIF';

	::StandardGetFile( NULL, 4, types, &reply );

	if ( !reply.sfGood ) 
		return;

	char *fileLink = CFileMgr::GetURLFromFileSpec( reply.sfFile );
	if ( fileLink == NULL )
		return;
	
	editField->SetDescriptor( CtoPstr(/*NET_UnEscape*/(fileLink)) );

	XP_FREE(fileLink);
}


/**********************************************************/
#pragma mark -
#pragma mark CTarget


CTarget::CTarget( LStream* inStream ) : CEditDialog( inStream )
{
	fOriginalTarget = NULL;
	fTargetName = NULL;
	
	ErrorManager::PrepareToInteract();
	UDesktop::Deactivate();
}

CTarget::~CTarget()
{
	UDesktop::Activate();
	
	if ( fOriginalTarget )
		XP_FREE( fOriginalTarget );
	fOriginalTarget = NULL;
	fTargetName = NULL;
}


void CTarget::InitializeDialogControls()
{
	// Create Controls
	ThrowIfNil_(fTargetName = (CLargeEditField*)FindPaneByID( 'tgnm' ));
	
	if ( EDT_GetCurrentElementType(fContext) == ED_ELEMENT_TARGET )
		StrAllocCopy( fOriginalTarget, EDT_GetTargetData(fContext) );
	else
	{
		StrAllocCopy( fOriginalTarget, (char *)LO_GetSelectionText(fContext) );
		CleanUpTargetString( fOriginalTarget );
	}
	
	if ( fOriginalTarget )
		fTargetName->SetDescriptor( CStr255(fOriginalTarget) );

	this->SetLatentSub( fTargetName );
	fTargetName->SelectAll();
}

void CTarget::CleanUpTargetString(char *target)
{
	if ( target == NULL )
		return;
	
	// move temp past spaces and leading # and strcpy from new location over target
	// remove leading spaces
	char *temp = target;
	while ( *temp == ' ' )
		temp++;
	
	// remove a leading pound, if any
	if ( *temp == '#' )
		temp++;
	
	// strcpy now that we have temp correctly positioned
	XP_STRCPY( target, temp );
	
	// truncate at the first return character
	char *ret = XP_STRCHR( target, '\r' );
	if ( ret )
		*ret = '\0';
}


Boolean CTarget::AlreadyExistsInDocument(char *anchor)
{
	char *targs = EDT_GetAllDocumentTargets( fContext );
	if ( targs == NULL )
		return FALSE;
	
	char *parse = targs;
	if ( parse )
	{
		while ( *parse )
		{
			if ( XP_STRCMP( anchor, parse ) == 0 )
			{
				XP_FREE( targs );
				return TRUE;
			}
			parse += XP_STRLEN( parse ) + 1;
		}
	}
	
	XP_FREE(targs);
	return FALSE;
}


Boolean CTarget::CommitChanges( Boolean /* isAllPanes */ )
{
	char *anchor = fTargetName->GetLongDescriptor();
	if ( anchor == NULL )
		return true;
	
	CleanUpTargetString( anchor );
	
	if ( AlreadyExistsInDocument(anchor) && (fInsert || XP_STRCMP(anchor, fOriginalTarget)) )
	{
		ErrorManager::PlainAlert( DUPLICATE_TARGET );
		XP_FREE( anchor );
		return false;
	}
	
	if ( !mUndoInited )
	{
		EDT_BeginBatchChanges( fContext );
		mUndoInited = true;
	}
	
	if ( fInsert )
		EDT_InsertTarget( fContext, anchor );
	else
		EDT_SetTargetData( fContext, anchor );
		
	XP_FREE( anchor );
	return true;
}


/**********************************************************/
#pragma mark CUnknownTag


CUnknownTag::CUnknownTag( LStream* inStream ) : CEditDialog( inStream )
{
	ErrorManager::PrepareToInteract();
	UDesktop::Deactivate();
}


CUnknownTag::~CUnknownTag()
{
	UDesktop::Activate();
}


void CUnknownTag::FinishCreateSelf()
{
	fTargetName = NULL;
	
	CEditDialog::FinishCreateSelf();
}


void CUnknownTag::InitializeDialogControls()
{
	fTargetName = (CLargeEditField *)FindPaneByID( 'tgnm' );
	SetTextTraitsIDByCsid(fTargetName, GetWinCSID());
	
	if ( !fInsert )
	{
		char *tag = EDT_GetUnknownTagData( fContext );
		if ( tag )
		{
			fTargetName->SetLongDescriptor( tag );
			XP_FREE( tag );
		}
	}
	
	this->SetLatentSub( fTargetName );
	fTargetName->SelectAll();
}


Boolean CUnknownTag::CommitChanges( Boolean /* isAllPanes */ )
{
	// HACK!!!  CUnknownTag is used (unfortunately) by both "Insert HTML Tag" and "Extra HTMLÉ"
	// This is bad and should be corrected as soon as possible (after 4.0 ships)
	
	// in the case of extra HTML we can end up here but fTargetName will be NULL
	// let's just bail out and handle this in the extraHTML dlog code (by returning false)
	if ( fTargetName == NULL )
		return false;
	
	char *anchor = fTargetName->GetLongDescriptor();
	if ( anchor == NULL )
		return true;
		
	if ( EDT_ValidateTag(anchor, FALSE) != ED_TAG_OK )
	{
		ErrorManager::PlainAlert( BAD_TAG );
		XP_FREE( anchor );
		return false;
	}
	
	if ( mUndoInited )
	{
		EDT_BeginBatchChanges( fContext );
		mUndoInited = true;
	}
	
	if ( fInsert )
		EDT_InsertUnknownTag( fContext, anchor );
	else
		EDT_SetUnknownTagData( fContext, anchor );
		
	XP_FREE( anchor );
	return true;
}


void CUnknownTag::Help()
{
	ShowHelp( HELP_HTML_TAG );
}


void CUnknownTag::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch ( inMessage )
	{
		case 'vrfy':
			char *anchor = fTargetName->GetLongDescriptor();
			if (anchor)
			{
				if (EDT_ValidateTag( anchor, false ) != ED_TAG_OK)
					ErrorManager::PlainAlert( BAD_TAG );
				
				XP_FREE( anchor );
			}
		break;
		
		default:
			CEditDialog::ListenToMessage( inMessage, ioParam );
		break;
	}
}


/**********************************************************/
#pragma mark -
// Does PowerPlant now have a better class?  Can we replace this homebrew class?
// This is a single column LListBox which allows multiple things to be selected.

MultipleSelectionSingleColumn::MultipleSelectionSingleColumn( LStream* inStream ) : LListBox( inStream )
{
	FocusDraw();
	if((*mMacListH)->dataBounds.right == 0)
		::LAddColumn(1 , 0, mMacListH);
	(*mMacListH)->selFlags &= ~lOnlyOne;			// make sure this bit is cleared.
}


int16 MultipleSelectionSingleColumn::NumItems() 
{ 
	return ((**mMacListH).dataBounds.bottom);
}


void MultipleSelectionSingleColumn::DeselectAll() 
{ 
	Cell	theCell;
	int16	count = NumItems();
	theCell.h = 0;
	
	FocusDraw();

	for (theCell.v = 0; theCell.v < count; theCell.v++)
		LSetSelect(false, theCell, mMacListH);
}


void MultipleSelectionSingleColumn::SelectAll() 
{ 
	Cell	theCell;
	int16	count = NumItems();
	theCell.h = 0;
	
	FocusDraw();

	for (theCell.v = 0; theCell.v < count; theCell.v++)
		LSetSelect(true, theCell, mMacListH);
}


void MultipleSelectionSingleColumn::AddItem( char* data, Boolean isSelected )
{
	if(SwitchTarget(this))
	{
		int16	rowNum = 0;
		FocusDraw();
		Cell theCell;
		theCell.h = 0;
		rowNum = ::LAddRow(1 , (**mMacListH).dataBounds.bottom, mMacListH);
		theCell.v = rowNum;
		::LSetCell(data, strlen(data) + 1 ,theCell, mMacListH);
		::LSetSelect( isSelected, theCell, mMacListH );
	}
}


StringPtr MultipleSelectionSingleColumn::GetItem(Str255	outDescriptor, int32 rowNum) const
{
	outDescriptor[0] = 0;
	Cell	theCell = {0, 0};
	theCell.h = 0;
	theCell.v = rowNum;
	Int16	dataLen = 255;
	::LGetCell(outDescriptor + 1, &dataLen, theCell, mMacListH);
	outDescriptor[0] = dataLen;
	return outDescriptor;
}


Boolean MultipleSelectionSingleColumn::IsSelected(int32 rowNum)	// rowNum is zero based
{
	FocusDraw();
	Cell theCell;
	theCell.h = 0;
	theCell.v = rowNum;
	return ::LGetSelect(false, &theCell, mMacListH);		// false means to only look at this one item...
}


void MultipleSelectionSingleColumn::RemoveAllItems()
{
	FocusDraw();
	if ((**mMacListH).dataBounds.bottom)
		::LDelRow((**mMacListH).dataBounds.bottom, 0, mMacListH);
}


/**********************************************************/
#pragma mark CPublishHistory

char* CPublishHistory::GetPublishHistoryCharPtr(short whichone)
{
	switch (whichone) {
		case 0: return CPrefs::GetCharPtr(CPrefs::PublishHistory0);
		case 1: return CPrefs::GetCharPtr(CPrefs::PublishHistory1);
		case 2: return CPrefs::GetCharPtr(CPrefs::PublishHistory2);
		case 3: return CPrefs::GetCharPtr(CPrefs::PublishHistory3);
		case 4: return CPrefs::GetCharPtr(CPrefs::PublishHistory4);
		case 5: return CPrefs::GetCharPtr(CPrefs::PublishHistory5);
		case 6: return CPrefs::GetCharPtr(CPrefs::PublishHistory6);
		case 7: return CPrefs::GetCharPtr(CPrefs::PublishHistory7);
		case 8: return CPrefs::GetCharPtr(CPrefs::PublishHistory8);
		case 9: return CPrefs::GetCharPtr(CPrefs::PublishHistory9);
	}
	return CPrefs::GetCharPtr(CPrefs::PublishHistory0);
}


void CPublishHistory::SetPublishHistoryCharPtr(char* entry, short whichone)
{
	switch (whichone) {
		case 0:	CPrefs::SetString( entry, CPrefs::PublishHistory0); break;
		case 1:	CPrefs::SetString( entry, CPrefs::PublishHistory1); break;
		case 2:	CPrefs::SetString( entry, CPrefs::PublishHistory2); break;
		case 3:	CPrefs::SetString( entry, CPrefs::PublishHistory3); break;
		case 4:	CPrefs::SetString( entry, CPrefs::PublishHistory4); break;
		case 5:	CPrefs::SetString( entry, CPrefs::PublishHistory5); break;
		case 6:	CPrefs::SetString( entry, CPrefs::PublishHistory6); break;
		case 7:	CPrefs::SetString( entry, CPrefs::PublishHistory7); break;
		case 8:	CPrefs::SetString( entry, CPrefs::PublishHistory8); break;
		case 9:	CPrefs::SetString( entry, CPrefs::PublishHistory9); break;
	}
}


Boolean CPublishHistory::IsTherePublishHistory()
{
	char *lowest = GetPublishHistoryCharPtr(0);
	
	if (lowest == NULL) return FALSE;
	if (*lowest == '\0') return FALSE;
	return TRUE;
}


void CPublishHistory::AddPublishHistoryEntry(char *entry)
{
	if (entry == NULL) return;
	
	short highestcopy = 9;
	short i;
	
	for (i = 0; i < 9 ; i++)  {
		char * hist = GetPublishHistoryCharPtr(i);
		if (hist && strcmp(hist, entry) == NULL) highestcopy = i;
	}
	
	while (highestcopy > 0) {
		SetPublishHistoryCharPtr(GetPublishHistoryCharPtr(highestcopy - 1), highestcopy);
		highestcopy--;
	}
		
	SetPublishHistoryCharPtr(entry, 0);
}


#pragma mark CPublish

CPublish::CPublish( LStream* inStream ) : CEditDialog( inStream )
{
	ErrorManager::PrepareToInteract();
	UDesktop::Deactivate();
}


CPublish::~CPublish()
{
	UDesktop::Activate();
}


void CPublish::FinishCreateSelf()
{
	// Create Controls
	
	fLocalLocation = NULL;
	
	fImageFiles = NULL;
	fFolderFiles = NULL;
	fDefaultLocation = NULL;
	
	fFileList = NULL;
	
	fPublishLocation = NULL;
	fUserID = NULL;
	fPassword = NULL;

	fSavePassword = NULL;
	mHistoryList = NULL;

	CEditDialog::FinishCreateSelf();
}


void CPublish::InitializeDialogControls()
{
	fLocalLocation = (LCaption*)this->FindPaneByID( 'Pub0' );
	
	fImageFiles = (LControl*)this->FindPaneByID( 'Pub1' );
	fFolderFiles = (LControl*)this->FindPaneByID( 'Pub2' );
	
	History_entry * pEntry = SHIST_GetCurrent(&fContext->hist);
	if (pEntry && pEntry->address && NET_IsLocalFileURL(pEntry->address))
		fFolderFiles->Enable();
	else
		fFolderFiles->Disable();
	
	fFileList = (MultipleSelectionSingleColumn*)this->FindPaneByID( 'Pub3' );
		
	fPublishLocation = (LGAEditField*)this->FindPaneByID( 'Pub4' );
	fUserID = (LGAEditField*)this->FindPaneByID( 'user' );
	fPassword = (LGAEditField*)this->FindPaneByID( 'Pass' );
	
	fDefaultLocation = (LControl*)this->FindPaneByID( 'Dflt' );
	
	fSavePassword = (LControl*)this->FindPaneByID( 'Pub5' );
	mHistoryList = (LGAPopup *)this->FindPaneByID( 'Pub6' );
	
	MenuHandle menu = mHistoryList->GetMacMenuH();
	
	if (menu && CPublishHistory::IsTherePublishHistory()) {
	
		::SetMenuItemText(menu, 1, "\p ");
		mHistoryList->SetMaxValue(10);				// maximum of 10 entries.
		
		int i;
		
		for (i = 0; i < 10; i++) {
			char *entry = CPublishHistory::GetPublishHistoryCharPtr(i);
			if (entry == NULL || *entry == '\0') break;
			
			char *location = NULL;
			char *user = NULL;
			char *pass = NULL;

			if (NET_ParseUploadURL(entry, &location, &user, &pass)) {
				
				UMenuUtils::InsertMenuItem(menu, CtoPstr(location), i + 2);
				
				XP_FREE(location);
				XP_FREE(user);
				XP_FREE(pass);
			}
		}
	}

	char *location = EDT_GetDefaultPublishURL( fContext, NULL, NULL, NULL );
	if ( location )
	{
		fPublishLocation->SetDescriptor( CtoPstr(location) );
		XP_FREE( location );
	}
	this->SetLatentSub( fPublishLocation );
	
	char *localDir = DocName();
	if (localDir) {
	
		char *pSlash = strrchr(localDir, '/');
		if (pSlash)	{				// if there is a slash, only keep everything AFTER the last slash
			pSlash++;				// go past the slash
			XP_STRCPY(localDir, pSlash);
		}
		
		fLocalLocation->SetDescriptor( CtoPstr(localDir) );
		
		XP_FREE( localDir );
	}

	CLargeEditField *titleField = (CLargeEditField *)FindPaneByID( 'Titl' );
	if ( titleField )
	{
		int16 res_csid = INTL_CharSetNameToID(INTL_ResourceCharSet());
		int16 win_csid = GetWinCSID();
		if(win_csid != res_csid)
			titleField->SetTextTraitsID(CPrefs::GetTextFieldTextResIDs(win_csid));

		EDT_PageData *pageData = EDT_GetPageData( fContext );
		if ( pageData )
		{
			titleField->SetDescriptor( CtoPstr(pageData->pTitle) );
			EDT_FreePageData( pageData );
		}
	}
	
	CStr255 locationCStr = CPrefs::GetString(CPrefs::PublishLocation);
	if (locationCStr.Length())
		fDefaultLocation->Enable();
	else
		fDefaultLocation->Disable();
		
	TurnOn(fImageFiles);
	
}

char *CPublish::DocName()
{
	 History_entry * pEntry = SHIST_GetCurrent(&fContext->hist);

	if (pEntry == NULL || pEntry->address == NULL) return NULL;
	
	if (!NET_IsLocalFileURL(pEntry->address)) return NULL;
	
	char *filename = NULL;
	XP_ConvertUrlToLocalFile( pEntry->address, &filename );
	
	return filename;
}

Boolean CPublish::CommitChanges( Boolean /* isAllPanes */ )
{
	char *BrowseLoc = NULL;
	
	CStr255	outText;
	
	fPublishLocation->GetDescriptor( outText );
	
	int type = NET_URL_Type(outText);
    if (type != FTP_TYPE_URL && type != HTTP_TYPE_URL && type != SECURE_HTTP_TYPE_URL) {
		ErrorManager::PlainAlert( INVALID_PUBLISH_LOC );
    	return FALSE;
    }
	
        // Tell user the URL they are publishing to looks like it might be wrong.
        // e.g. ends in a slash or does not have a file extension.
        // Give the user the option of attempting to publish to the
        // specified URL even if it looks suspicious.
    if ( !EDT_CheckPublishURL( fContext, (char*)outText ) ) {
		// XP will put up alert
		return FALSE;
	}
	

	/* set the page title */
	EDT_PageData *pageData = EDT_GetPageData( fContext );
	if ( pageData )
	{
		CLargeEditField *titleField = (CLargeEditField *)FindPaneByID( 'Titl' );
		if ( titleField )
			pageData->pTitle = titleField->GetLongDescriptor();
		
		EDT_SetPageData( fContext, pageData );
		EDT_FreePageData( pageData );
	}

	/* get the userID and password to create the internal URL */
	CStr255		user;
	CStr255		pass;
	fUserID->GetDescriptor( user );
	fPassword->GetDescriptor( pass );

	if (!NET_MakeUploadURL(&BrowseLoc, outText, user, pass) || BrowseLoc == NULL)
		return TRUE;		// must be an out of memory problem, but we return TRUE so the user can try to recover

	/* look for related files to also publish */
	int count = 0;
	int i;
	for (i = fFileList->NumItems() - 1; i >= 0; i--)
		if (fFileList->IsSelected(i))
			count++;

	char **allFiles = (char **) XP_CALLOC((count + 2), sizeof(char *));
	if (allFiles == NULL) {
		XP_FREE(BrowseLoc);
		return TRUE;		// must be an out of memory problem, but we return TRUE so the user can try to recover
	}
	
	allFiles[count + 1] = NULL;
	allFiles[0] = XP_STRDUP(outText);	// we are actually copying the doc name as well as the localDir here

	int	allFilesIndex = 1;
	for (i = 0; i < count; i++)
	{
		if (fFileList->IsSelected(i))
		{
			CStr255 lookFor;
			fFileList->GetItem(lookFor, i);
			if (allFilesIndex < count + 1)		// sanity check
				allFiles[ allFilesIndex ] = XP_STRDUP( lookFor );
			else
				XP_ASSERT(0);
			
			allFilesIndex++;
		}
	}

	/* get preferences and current URL */
	XP_Bool bKeepImages;
	PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
	XP_Bool bKeepLinks;
	PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);

    char *pSrcURL;
    History_entry * pEntry = SHIST_GetCurrent(&fContext->hist);
    if (pEntry && pEntry->address && *pEntry->address) {
      pSrcURL = pEntry->address;
    }
    else {
      // no source name.
      pSrcURL = XP_NEW_DOC_NAME;
    }

	/* actually publish--finally! */
	EDT_PublishFile( fContext, ED_FINISHED_REVERT_BUFFER, pSrcURL, 
					allFiles, BrowseLoc, bKeepImages, bKeepLinks, false );	// this will eventually free allFiles, when we are done publishing....
    XP_FREE(BrowseLoc);
    BrowseLoc = NULL;
    
    // add this new location to our history...known bugs in this area
	if (fSavePassword->GetValue())
	{
		char *result;
		result = SECNAV_MungeString(pass);
		if ( result && (strcmp( result, pass ) == 0) )
		{
			// munging didn't do anything so we'll write out ""
			pass = "";
		}
		else
			pass = result;
	}
	else
		pass = "";
	
	if (NET_MakeUploadURL(&BrowseLoc, outText, user, pass) && BrowseLoc) {
		CPublishHistory::AddPublishHistoryEntry(BrowseLoc);
    	XP_FREE(BrowseLoc);
	}
			
	// finally, show them what we've done!
	CStr255		urlString = CPrefs::GetString( CPrefs::PublishBrowseLocation );
	if ( urlString != CStr255::sEmptyString )
		CFrontApp::DoGetURL( (unsigned char*)urlString );
	
	return TRUE;
}


void CPublish::Help()
{
	ShowHelp( HELP_PUBLISH_FILES );
}


void CPublish::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch ( inMessage )
	{
		case 'None':	// deselect everything in file list
			fFileList->DeselectAll();
			break;
		
		case 'SAll':	// select everything in file list
			fFileList->SelectAll();
			break;
		
		case 'Pub1':	// add all document images to list
			if (fImageFiles->GetValue()) {
				fFileList->RemoveAllItems();
				
				XP_Bool bKeepImages;
				PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
				XP_Bool *bSelectionArray = NULL;
				char *images = EDT_GetAllDocumentFilesSelected( fContext, 
											&bSelectionArray, bKeepImages );
				if (images) {
					char *next = images;
					
					int index = 0;
					while (*next) {
						fFileList->AddItem(next, bSelectionArray[ index++ ]);
						next += strlen(next) + 1;
					}
					
					XP_FREE(images);
				}
				if ( bSelectionArray )
					XP_FREE( bSelectionArray );
			}
			break;
		
		case 'Pub2':	// add all images in folder to list
		/* this should not be enabled if remote document or new document (file:///Untitled) */
			if (fFolderFiles->GetValue())
			{
				fFileList->RemoveAllItems();
			
			    History_entry * pEntry = SHIST_GetCurrent(&fContext->hist);
				if (pEntry && pEntry->address && NET_IsLocalFileURL(pEntry->address))
				{
					char *URLDir = XP_STRDUP( pEntry->address );
					if ( URLDir )
					{
						char *pSlash = strrchr(URLDir, '/');
						if (pSlash)		// is there is a slash, chop off everything after the last slash
							*++pSlash = '\0';
							
					    char **images = NET_AssembleAllFilesInDirectory( fContext, URLDir );
						if (images)
						{
							char **next = images;				// go through the list and add them all
							while (*next)
							{
								if (strcmp(*next, pEntry->address))	// don't add current file
								{
//									char *path = strstr(*next, URLDir);
//									if (path) 
//										fFileList->AddItem( path + strlen(URLDir), false );
//									else
//										fFileList->AddItem( *next, false );
					
//									char *absURL = NET_MakeAbsoluteURL( pEntry->address, *next );
									fFileList->AddItem( *next, false );	
								}
									
								next++;
							}
							
							next = images;						// now free them all... what a waste
							while (*next) XP_FREE(*next++);
							XP_FREE(images);
					
						}
							
						XP_FREE( URLDir );
					}
				}
			}
		break;
		
		case 'Pub6':	// extract one of the history entries and put in publish location
			CStr255 text = CPublishHistory::GetPublishHistoryCharPtr(mHistoryList->GetValue() - 2);
			
			// reset it now, because we don't know when the user might modify the textedit 
			// field making the selected menu item no longer valid.
			mHistoryList->SetValue(1);
						
			if ((char *) text && *(char *)text) {
				
				char *location = NULL;
				char *user = NULL;
				char *pass = NULL;

				if (NET_ParseUploadURL(text, &location, &user, &pass)) {
					if (pass && *pass) {
						char *newpass = SECNAV_UnMungeString(pass);
						XP_FREE(pass);
						pass = newpass;
					}
					
					fPublishLocation->SetDescriptor( CtoPstr(location) );
					fUserID->SetDescriptor( CtoPstr(user) );
					fPassword->SetDescriptor( CtoPstr(pass) );
					
					if (pass && *pass)
						fSavePassword->SetValue(1);
					else
						fSavePassword->SetValue(0);
					
					XP_FREE(location);
					XP_FREE(user);
					XP_FREE(pass);
				}
			}

		break;
		
		case 'Dflt':	// put in default publish location
			char *BrowseLoc = CPrefs::GetString(CPrefs::PublishLocation);
			if (BrowseLoc && *BrowseLoc) {
				
				char *location = NULL;
				char *user = NULL;
				char *pass = NULL;

				if (NET_ParseUploadURL(BrowseLoc, &location, &user, &pass)) {
					if (pass && *pass) {
						char *newpass = SECNAV_UnMungeString(pass);
						XP_FREE(pass);
						pass = newpass;
					}
					
					fPublishLocation->SetDescriptor( CtoPstr(location) );
					fUserID->SetDescriptor( CtoPstr(user) );
					fPassword->SetDescriptor( CtoPstr(pass) );
					
					XP_FREE(location);
					XP_FREE(user);
					XP_FREE(pass);
				}
			}
		break;
		
		case msg_ControlClicked:
			if ( ioParam )
			{
				LControl *c = (LControl *)ioParam;
				if ( c )
					ListenToMessage( c->GetValueMessage(), NULL );
			}
			break;
		
		default:
			CEditDialog::ListenToMessage( inMessage, ioParam );
		break;
	}
}


/**********************************************************/
#pragma mark CLineProp

CLineProp::CLineProp( LStream* inStream ): CEditDialog( inStream )
{
	ErrorManager::PrepareToInteract();
	UDesktop::Deactivate();
}


CLineProp::~CLineProp()
{
	UDesktop::Activate();
}


#define	kPixelsItem				1
#define kPercentOfWindowItem	2

void CLineProp::FinishCreateSelf()
{
	fLeftAlign = NULL;
	fCenterAlign = NULL;
	fRightAlign = NULL;
	
	fHeightEditText = NULL;
	fWidthEditText = NULL;
	fPixelPercent = NULL;
	fShading = NULL;
		
	CEditDialog::FinishCreateSelf();
}

void CLineProp::InitializeDialogControls()
{
	// Find Controls
	fHeightEditText = (LGAEditField*)this->FindPaneByID( 'ELPh' );
	fWidthEditText = (LGAEditField*)this->FindPaneByID( 'ELPw' );
	
	fLeftAlign = (LControl*)this->FindPaneByID( 'Left' );
	fCenterAlign = (LControl*)this->FindPaneByID( 'Cntr' );
	fRightAlign = (LControl*)this->FindPaneByID( 'Rght' );

	fPixelPercent = (LControl*)this->FindPaneByID( 'eppp' );

	fShading = (LControl*)this->FindPaneByID( 'ELPs' );
	this->SetLatentSub( fHeightEditText );
	fHeightEditText->SelectAll();
	
	// Set control values
	
	EDT_HorizRuleData*	fData;	
	if (EDT_GetCurrentElementType(fContext) == ED_ELEMENT_HRULE)
 		fData = EDT_GetHorizRuleData(fContext);
 	else
 		fData = EDT_NewHorizRuleData();
 	
	ThrowIfNil_(fData);
 	
 	fHeightEditText->SetValue(fData->size);
 	fWidthEditText->SetValue(fData->iWidth);
 	
	if (fData->align == ED_ALIGN_LEFT)
		TurnOn( fLeftAlign );
	else if ( fData->align ==  ED_ALIGN_RIGHT)
		TurnOn( fRightAlign );
	else
		TurnOn( fCenterAlign );

 	fPixelPercent->SetValue( (fData->bWidthPercent) ? kPercentOfWindowItem : kPixelsItem );
 	
 	fShading->SetValue(fData->bNoShade == FALSE);
 	
 	pExtra = fData->pExtra;	// we'll reuse this pointer
 	fData->pExtra = NULL;	// don't want to free it!

	EDT_FreeHorizRuleData(fData);
}


Boolean CLineProp::CommitChanges( Boolean /* allPanes */ )
{
	if ( !IsEditFieldWithinLimits(fHeightEditText, 1, 100))
	{
		SwitchTarget( fHeightEditText );
		fHeightEditText->SelectAll();
		return false;
	}
	
	if ( fPixelPercent->GetValue() != kPercentOfWindowItem 
	&& !IsEditFieldWithinLimits(fWidthEditText, 1, 10000))
	{
		SwitchTarget( fWidthEditText );
		fWidthEditText->SelectAll();
		return false;
	}

	if (fPixelPercent->GetValue() == kPercentOfWindowItem 
	&& !IsEditFieldWithinLimits(fWidthEditText, 1, 100))
	{
		SwitchTarget( fWidthEditText );
		fWidthEditText->SelectAll();
		return false;
	}

	if ( !mUndoInited )
	{
		EDT_BeginBatchChanges( fContext );
		mUndoInited = true;
	}
	
	EDT_HorizRuleData*	fData;	
	if (EDT_GetCurrentElementType(fContext) == ED_ELEMENT_HRULE)
 		fData = EDT_GetHorizRuleData(fContext);
 	else
 		fData = EDT_NewHorizRuleData();
 		
	ThrowIfNil_(fData);
 	
	if (fLeftAlign->GetValue())
		fData->align = ED_ALIGN_LEFT;
	else if (fCenterAlign->GetValue())
		fData->align = ED_ALIGN_DEFAULT;
	else if (fRightAlign->GetValue())
		fData->align = ED_ALIGN_RIGHT;

	fData->size = fHeightEditText->GetValue();
	fData->iWidth = fWidthEditText->GetValue();
	
	fData->bWidthPercent = (fPixelPercent->GetValue() == kPercentOfWindowItem);
	
	fData->bNoShade = !fShading->GetValue();
	
	if ( fData->pExtra )
		XP_FREE( fData->pExtra );
	fData->pExtra = ( pExtra ) ? XP_STRDUP( pExtra ) : NULL;
		
	if (EDT_GetCurrentElementType(fContext) == ED_ELEMENT_HRULE)
 		EDT_SetHorizRuleData( fContext, fData );
 	else
 		EDT_InsertHorizRule( fContext, fData );
	
	EDT_FreeHorizRuleData(fData);

	return true;
}


void CLineProp::Help()
{
	ShowHelp( HELP_PROPS_HRULE );
}


/**********************************************************/
#pragma mark -
#pragma mark CEditTabSwitcher

CEditTabSwitcher::CEditTabSwitcher(LStream* inStream) : CTabSwitcher(inStream)
{
	fLinkName = NULL;
}


CEditTabSwitcher::~CEditTabSwitcher()
{
	XP_FREEIF(fLinkName);
}


void CEditTabSwitcher::SetData(MWContext* context, Boolean insert)
{
	fContext = context;
	fInsert = insert;
}


void CEditTabSwitcher::Help()
{
	CEditContain* page = (CEditContain*)mCurrentPage;
	if ( page )
		page->Help();
}


void CEditTabSwitcher::DoPostLoad(LView* inLoadedPage, Boolean inFromCache)
{
	CEditContain * freshPane = (CEditContain*) inLoadedPage;
	XP_ASSERT(freshPane);
	freshPane->SetContext(fContext);
	freshPane->SetInsertFlag(fInsert);
	freshPane->SetLinkToLinkName(&fLinkName);
	freshPane->SetExtraHTMLString( NULL );
	if (!inFromCache)			// don't blast the contents of the text edit fields if there is something already there.
		freshPane->ControlsFromPref();
}


/**********************************************************/
#pragma mark -
#pragma mark CTabbedDialog

CTabbedDialog::CTabbedDialog( LStream* inStream ): CEditDialog( inStream )
{
	ErrorManager::PrepareToInteract();
	UDesktop::Deactivate();
}


CTabbedDialog::~CTabbedDialog()
{
	UDesktop::Activate();
}


void CTabbedDialog::FinishCreateSelf()
{
	mTabControl = NULL;
	mTabSwitcher = NULL;

	LDialogBox::FinishCreateSelf();
}


void CTabbedDialog::InitializeDialogControls()
{
	LControl *ApplyButton = (LControl*)FindPaneByID('APPY');
	if (fInsert && ApplyButton)
		ApplyButton->Disable();
	SetDefaultButton('OKOK');
	
	mTabControl = (CTabControl*)FindPaneByID(CTabControl::class_ID);
	ThrowIfNULL_(mTabControl);

	mTabSwitcher = (CEditTabSwitcher*)FindPaneByID(CTabSwitcher::class_ID);
	ThrowIfNULL_(mTabSwitcher);
	
	mTabSwitcher->SetData(fContext, fInsert);
	
	mTabControl->DoLoadTabs(fInWindowID);
	mTabControl->SetValue(fInitTabValue);
}


void CTabbedDialog::RegisterViewTypes()
{
#ifdef COOL_IMAGE_RADIO_BUTTONS
	RegisterClass_( CImageAlignButton);
#endif
	RegisterClass_( CLargeEditField);
	RegisterClass_( CLargeEditFieldBroadcast);
	RegisterClass_( CCaption);

	RegisterClass_( CTabControl);
	RegisterClass_( CTabbedDialog);
	RegisterClass_( CEDCharacterContain);
	RegisterClass_( CEDParagraphContain);
	RegisterClass_( CEDLinkContain);
	RegisterClass_( CEDImageContain);
	RegisterClass_( CEDDocPropGeneralContain);
	RegisterClass_( CEDDocPropAppearanceContain);
	RegisterClass_( CEDDocPropAdvancedContain);
	RegisterClass_( LToggleButton);
	RegisterClass_( CUnknownTag);
	RegisterClass_( CPublish);
	RegisterClass_( CTableInsertDialog);
	RegisterClass_( CEditTabSwitcher);
	RegisterClass_( CEDTableContain);
	RegisterClass_( CEDTableCellContain);

	RegisterClass_( CEDDocAppearanceNoTab);
	RegisterClass_( CFormatMsgColorAndImageDlog);
	
}


Boolean CTabbedDialog::CommitChanges(Boolean allPanes)
{
	if ( !allPanes )
	{
		MessageT thePageMessage = mTabControl->GetMessageForValue(mTabControl->GetValue());
		CEditContain* thePage = (CEditContain*)mTabSwitcher->FindPageByID(thePageMessage);
		if (thePage && !thePage->AllFieldsOK())
			return FALSE;
		
		if (thePage != NULL)
		{
			if ( !mUndoInited )
			{
				EDT_BeginBatchChanges( fContext );
				mUndoInited = true;
			}
			
			thePage->PrefsFromControls();
		}
		return true;
	}
		
	Int32	start = 1;
	Int32	end = mTabControl->GetMaxValue();

	// First, lets make sure that all the panes are happy.
	for (int i = start; i <= end; i ++ ) {
	
		MessageT thePageMessage = mTabControl->GetMessageForValue(i);
		CEditContain* thePage = (CEditContain*)mTabSwitcher->FindPageByID(thePageMessage);

		if (thePage && !thePage->AllFieldsOK())
		{
			mTabControl->SetValue(i);			// go to the offending pane.
			return FALSE;
		}
	}
	
	// If the CImagesLocal pane is up, commit that one first (in case we are doing an insert)
	if ( !mUndoInited )
	{
		EDT_BeginBatchChanges( fContext );
		mUndoInited = true;
	}
	
	for (int i = start; i <= end; i ++ )
	{
		MessageT thePageMessage = mTabControl->GetMessageForValue(i);
		CEditContain* thePage = (CEditContain*)mTabSwitcher->FindPageByID(thePageMessage);

		if (thePage && (thePage->GetUserCon() == CEDImageContain::class_ID ))
			thePage->PrefsFromControls();
	}
	
	// If the CEDLinkContain pane is up, commit that one next (in case we are doing an insert)
	for (int i = start; i <= end; i ++ )
	{
		MessageT thePageMessage = mTabControl->GetMessageForValue(i);
		CEditContain* thePage = (CEditContain*)mTabSwitcher->FindPageByID(thePageMessage);

		if (thePage && (thePage->GetUserCon() == CEDLinkContain::class_ID ))
			thePage->PrefsFromControls();
	}
	
	// If the CEDDocPropAppearanceContain pane is up, commit that one first (in case we are doing a doc properties)
	for (int i = start; i <= end; i ++ )
	{
		MessageT thePageMessage = mTabControl->GetMessageForValue(i);
		CEditContain* thePage = (CEditContain*)mTabSwitcher->FindPageByID(thePageMessage);

		if (thePage && (thePage->GetUserCon() == CEDDocPropAppearanceContain::class_ID ))
			thePage->PrefsFromControls();
	}
	
	// If the CEDDocPropGeneralContain pane is up, commit that one next (in case we are doing a doc properties)
	for (int i = start; i <= end; i ++ )
	{
		MessageT thePageMessage = mTabControl->GetMessageForValue(i);
		CEditContain* thePage = (CEditContain*)mTabSwitcher->FindPageByID(thePageMessage);

		if (thePage && (thePage->GetUserCon() == CEDDocPropGeneralContain::class_ID ))
			thePage->PrefsFromControls();
	}
	
	// commit the rest
	for (int i = start; i <= end; i ++ )
	{
		MessageT thePageMessage = mTabControl->GetMessageForValue(i);
		CEditContain* thePage = (CEditContain*)mTabSwitcher->FindPageByID(thePageMessage);
		if (thePage && (thePage->GetUserCon() != CEDImageContain::class_ID ) && (thePage->GetUserCon() != CEDLinkContain::class_ID ) &&
			(thePage->GetUserCon() != CEDDocPropAppearanceContain::class_ID ) && (thePage->GetUserCon() != CEDDocPropGeneralContain::class_ID ))
			thePage->PrefsFromControls();
	}
	

// I admit this is not a very clean design... May cause button flicker when you hit "Insert"
//
// When you are inserting an image or a link and you hit "Apply", the "Insert" button should change to "OK"
// For simplicity, we do it anytime the user hits "Apply", "Insert" or "OK" since the other 2 times the dlog will soon disappear anyway.

	LControl *OKButton = (LControl*)FindPaneByID('OKOK');
	if (OKButton)
		OKButton->Show();

	return TRUE;
}


void CTabbedDialog::Help()
{
	mTabSwitcher->Help();
}

 
/**********************************************************/
#pragma mark -
#pragma mark Table Dialogs

static void GetTableDataFromControls( EDT_TableData *fData, LControl *borderCheckBox, LGAEditField *borderWidth,
							LGAEditField *cellSpacing, LGAEditField *cellPadding, 
							LControl *customWidth, LControl *isWPercent, LGAEditField *widthSize,
							LControl *customHeight, LControl *isHPercent, LGAEditField *heightSize,
							LControl *hasColor, CColorButton *customColor, LO_Color *backgroundColor,
							LControl *fastLayout, LGAPopup *alignmentMenu,
							LControl *useImage, CLargeEditField *imageName, LControl *leaveImage, char *pExtra )
{
	fData->bBorderWidthDefined = borderCheckBox->GetValue();
	if ( fData->bBorderWidthDefined )
		fData->iBorderWidth = borderWidth->GetValue();
	else
		fData->iBorderWidth = 0;
	
	fData->iCellSpacing = cellSpacing->GetValue();
	fData->iCellPadding = cellPadding->GetValue();
	
	if ( customWidth->GetValue() )
	{
		fData->bWidthDefined = TRUE;
		fData->bWidthPercent = isWPercent->GetValue() == kPercentOfWindowItem;
		fData->iWidth = widthSize->GetValue();
	}
	else
		fData->bWidthDefined = FALSE;
	
	if (customHeight->GetValue())
	{
		fData->bHeightDefined = TRUE;
		fData->bHeightPercent = isHPercent->GetValue() == kPercentOfWindowItem;
		fData->iHeight = heightSize->GetValue();
	}
	else
		fData->bHeightDefined = FALSE;
	
	if (fData->pColorBackground)
		XP_FREE(fData->pColorBackground);			// we'll replace it with our own if we use it at all.
	fData->pColorBackground = NULL;
	if (hasColor->GetValue())
	{
		*backgroundColor = UGraphics::MakeLOColor(customColor->GetColor());
		fData->pColorBackground = backgroundColor;		// I hope that EDT_SetTableData() doesn't hang onto this pointer for long...
	}

	switch ( alignmentMenu->GetValue() )
	{
		default:
		case 1:	fData->align = ED_ALIGN_LEFT;		break;
		case 2:	fData->align = ED_ALIGN_ABSCENTER;	break;
		case 3:	fData->align = ED_ALIGN_RIGHT;		break;
	}

	fData->bUseCols = fastLayout->GetValue();
	
	if ( fData->pBackgroundImage )
	{
		XP_FREE( fData->pBackgroundImage );
		fData->pBackgroundImage = NULL;
	}
	if ( useImage->GetValue() )
		fData->pBackgroundImage = imageName->GetLongDescriptor();
	fData->bBackgroundNoSave = leaveImage->GetValue();

	if ( fData->pExtra )
		XP_FREE( fData->pExtra );
 	fData->pExtra = pExtra;	// temporarily assign; we don't want to free it though!
}


CTableInsertDialog::CTableInsertDialog( LStream* inStream ) : CEditDialog( inStream )
{
	ErrorManager::PrepareToInteract();
	UDesktop::Deactivate();
}


CTableInsertDialog::~CTableInsertDialog()
{
	UDesktop::Activate();
}


void CTableInsertDialog::FinishCreateSelf()
{
	// Create Controls
	CEditDialog::FinishCreateSelf();
	
	UReanimator::LinkListenerToControls( this, this, 5115 );		// the table guts are in RIDL 5115
	
	// Find Controls
	fNumRowsEditText = (LGAEditField*)FindPaneByID( 'rows' );
	fNumColsEditText = (LGAEditField*)FindPaneByID( 'cols' );
	
	fBorderCheckBox = (LControl*)FindPaneByID( 'BrdW' );
	fBorderCheckBox->SetValueMessage( 'BrdW' );
	
	fBorderWidthEditText = (LGAEditField*)FindPaneByID( 'brdw' );
	fCellSpacingEditText = (LGAEditField*)FindPaneByID( 'clsp' );
	fCellPaddingEditText = (LGAEditField*)FindPaneByID( 'clpd' );
	
	fCustomWidth = (LControl*)FindPaneByID( 'twth' );
	fWidthEditText = (LGAEditField*)FindPaneByID( 'wdth' );
	fWidthPopup = (LControl*)FindPaneByID( 'WdPU' );
	
	fCustomHeight = (LControl*)FindPaneByID( 'thgt' );
	fHeightEditText = (LGAEditField*)FindPaneByID( 'hght' );
	fHeightPopup = (LControl*)FindPaneByID( 'HtPU' );
	
	fCustomColor = (LControl*)FindPaneByID( 'tclr' );
	fColorCustomColor = (CColorButton*)FindPaneByID( 'Colo' );
	
	fIncludeCaption = (LControl*)FindPaneByID( 'cptn' );
	fCaptionAboveBelow = (LControl*)FindPaneByID( 'cPop' );

	mTableAlignment = (LGAPopup *)FindPaneByID( 'HzPU' );
	
	mFastLayout = (LControl *)FindPaneByID( 'FLay' );
	mUseImage = (LControl*)FindPaneByID( 'UseI' );
	mImageFileName = (CLargeEditField *)FindPaneByID( 'TImg' );
	mImageFileName->AddListener(this);
	mLeaveImage = (LControl *)FindPaneByID( 'LvIm' );

	SetLatentSub( fNumRowsEditText );
	fNumRowsEditText->SelectAll();
	
	// Set control values
	EDT_TableData *fData = EDT_NewTableData();
	if (fData != NULL)
	{
	 	fNumRowsEditText->SetValue( fData->iRows );
	 	fNumColsEditText->SetValue( fData->iColumns );
	 	
	 	fBorderCheckBox->SetValue( fData->bBorderWidthDefined );
	 	fBorderWidthEditText->SetValue( fData->iBorderWidth );
	 	fCellSpacingEditText->SetValue( fData->iCellSpacing );
	 	fCellPaddingEditText->SetValue( fData->iCellPadding );

		fCustomWidth->SetValue( fData->bWidthDefined );
		fWidthEditText->SetValue( fData->iWidth );
		fWidthPopup->SetValue( fData->bWidthPercent ? kPercentOfWindowItem : kPixelsItem );
		
		fCustomHeight->SetValue( fData->bHeightDefined );
		fHeightEditText->SetValue( fData->iHeight );
		fHeightPopup->SetValue( fData->bHeightPercent ? kPercentOfWindowItem : kPixelsItem );
		
		mUseImage->SetValue( fData->pBackgroundImage != NULL );
		if ( fData->pBackgroundImage )
			mImageFileName->SetLongDescriptor( fData->pBackgroundImage );
		mLeaveImage->SetValue( fData->bBackgroundNoSave );

		int value;
		switch ( fData->align )
		{
			default:
			case ED_ALIGN_LEFT:			value = 1;	break;
			case ED_ALIGN_ABSCENTER:	value = 2;	break;
			case ED_ALIGN_RIGHT:		value = 3;	break;
		}
		mTableAlignment->SetValue( value );
		
		mFastLayout->SetValue( fData->bUseCols );
		pExtra = fData->pExtra;		// don't bother to make our own copy
		fData->pExtra = NULL;		// set to NULL so backend doesn't delete
		
		EDT_FreeTableData( fData );
	}

	RGBColor macColor;
	fCustomColor->SetValue(FALSE);				// no custom color
	macColor.red = 0xFFFF;						// but start with white if they pick one.
	macColor.green = 0xFFFF;
	macColor.blue = 0xFFFF;
	fColorCustomColor->SetColor(macColor);

	fIncludeCaption->SetValue(FALSE);			//assume no caption
	fCaptionAboveBelow->SetValue( 1 );

	AdjustEnable();
}


void CTableInsertDialog::InitializeDialogControls()
{
	short 			resID;
	CStr255			title;
	StringHandle	titleH;

	if (EDT_IsInsertPointInTable(fContext))
		resID = EDITOR_PERCENT_PARENT_CELL;
	else
		resID = EDITOR_PERCENT_WINDOW;

	titleH = GetString( resID );
	if (titleH)
	{
		SInt8 hstate = HGetState( (Handle)titleH );
		HLock( (Handle)titleH );
		title = *titleH;
		HSetState( (Handle)titleH, hstate );

		MenuHandle menuh = ((LGAPopup *)fWidthPopup)->GetMacMenuH();
		if (menuh)
		{
			SetMenuItemText( menuh, kPercentOfWindowItem, title );
			((LGAPopup *)fWidthPopup)->SetMacMenuH( menuh );	// resets menu width
		}
		
		menuh = ((LGAPopup *)fHeightPopup)->GetMacMenuH();
		if (menuh)
		{
			SetMenuItemText( menuh, kPercentOfWindowItem, title );
			((LGAPopup *)fHeightPopup)->SetMacMenuH( menuh );	// resets menu width
		}
	}
}


void CTableInsertDialog::AdjustEnable()
{
	LView *bordercaption = (LView *)FindPaneByID(1);
	if ( fBorderCheckBox->GetValue() )
	{
		bordercaption->Enable();
		fBorderWidthEditText->Enable();
	}
	else
	{
		bordercaption->Disable();
		fBorderWidthEditText->Disable();
	}
	
	if (fCustomWidth->GetValue()) {
		fWidthEditText->Enable();
		fWidthPopup->Enable();
	} else {
		fWidthEditText->Disable();
		fWidthPopup->Disable();
	}

	if (fCustomHeight->GetValue()) {
		fHeightEditText->Enable();
		fHeightPopup->Enable();
	} else {
		fHeightEditText->Disable();
		fHeightPopup->Disable();
	}

	if (fIncludeCaption->GetValue())
		fCaptionAboveBelow->Enable();
	else
		fCaptionAboveBelow->Disable();
	
	if ( mUseImage->GetValue() )
		mLeaveImage->Enable();
	else
		mLeaveImage->Disable();
}


Boolean CTableInsertDialog::CommitChanges(Boolean /* allPanes */ )
{
	if (!IsEditFieldWithinLimits(fNumRowsEditText, 1, 100)) {
		SwitchTarget( fNumRowsEditText );
		fNumRowsEditText->SelectAll();
		return FALSE;
	}

	if (!IsEditFieldWithinLimits(fNumColsEditText, 1, 100)) {
		SwitchTarget( fNumColsEditText );
		fNumColsEditText->SelectAll();
		return FALSE;
	}

	if ( fBorderCheckBox->GetValue() )
	{
		if (!IsEditFieldWithinLimits(fBorderWidthEditText, 0, 10000)) {
			SwitchTarget( fBorderWidthEditText );
			fBorderWidthEditText->SelectAll();
			return FALSE;
		}
	}

	if (!IsEditFieldWithinLimits(fCellSpacingEditText, 0, 10000)) {
		SwitchTarget( fCellSpacingEditText );
		fCellSpacingEditText->SelectAll();
		return FALSE;
	}

	if (!IsEditFieldWithinLimits(fCellPaddingEditText, 0, 10000)) {
		SwitchTarget( fCellPaddingEditText );
		fCellPaddingEditText->SelectAll();
		return FALSE;
	}

	if (fCustomWidth->GetValue()) {
		if (fWidthPopup->GetValue() == kPixelsItem && !IsEditFieldWithinLimits(fWidthEditText, 1, 10000)) {
			SwitchTarget( fWidthEditText );
			fWidthEditText->SelectAll();
			return FALSE;
		}

		if (fWidthPopup->GetValue() == kPercentOfWindowItem && !IsEditFieldWithinLimits(fWidthEditText, 1, 100)) {
			SwitchTarget( fWidthEditText );
			fWidthEditText->SelectAll();
			return FALSE;
		}
	}

	if (fCustomHeight->GetValue()) {
		if (fHeightPopup->GetValue() == kPixelsItem && !IsEditFieldWithinLimits(fHeightEditText, 1, 10000)) {
			SwitchTarget( fHeightEditText );
			fHeightEditText->SelectAll();
			return FALSE;
		}

		if (fHeightPopup->GetValue() == kPercentOfWindowItem && !IsEditFieldWithinLimits(fHeightEditText, 1, 100)) {
			SwitchTarget( fHeightEditText );
			fHeightEditText->SelectAll();
			return FALSE;
		}
	}

	LO_Color backgroundColor;
	EDT_TableData *fData = EDT_NewTableData();
	if (fData == NULL)
		return FALSE;
	
	EDT_BeginBatchChanges( fContext );

	fData->iRows = fNumRowsEditText->GetValue();
	fData->iColumns = fNumColsEditText->GetValue();
	
	GetTableDataFromControls( fData, fBorderCheckBox, fBorderWidthEditText,
							fCellSpacingEditText, fCellPaddingEditText, 
							fCustomWidth, fWidthPopup, fWidthEditText,
							fCustomHeight, fHeightPopup, fHeightEditText,
							fCustomColor, fColorCustomColor, &backgroundColor,
							mFastLayout, mTableAlignment,
							mUseImage, mImageFileName, mLeaveImage, pExtra );
	
 	EDT_InsertTable( fContext, fData );
 	
 	fData->pExtra = NULL;				// don't free this! we still use it!
 	fData->pColorBackground = NULL;		// no need to free this, it is our local variable!
	EDT_FreeTableData(fData);
	
	
	// clean this up later after we're sure the XP interfaces aren't improving.
	if (fIncludeCaption->GetValue())
	{
		EDT_TableCaptionData* fCaptionData;
		fCaptionData = EDT_NewTableCaptionData();
		if (fCaptionData)
		{
			if ( fCaptionAboveBelow->GetValue() == 1 )
				fCaptionData->align = ED_ALIGN_ABSTOP;
			else
				fCaptionData->align = ED_ALIGN_ABSBOTTOM;	// got this constant from CEditTableElement::SetCaption();
				
			EDT_InsertTableCaption( fContext, fCaptionData );
			EDT_FreeTableCaptionData( fCaptionData );
		}
	}

	EDT_EndBatchChanges( fContext );

	return TRUE;
}


void CTableInsertDialog::Help()
{
	ShowHelp( HELP_NEW_TABLE_PROPS );
}


void CTableInsertDialog::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch ( inMessage )
	{
		case 'BrdW':
		case 'twth':
		case 'thgt':
		case 'tclr':
		case 'cptn':
			AdjustEnable();
			break;
		
		case msg_LinkColorChange:
			fCustomColor->SetValue(TRUE);
			break;
		
		case 'UseI':
			if (mUseImage->GetValue() == 1)
			{				// we are trying to set the image
				CStr255 url;
				mImageFileName->GetDescriptor(url);
				if (url == CStr255::sEmptyString)
				{		// but it doesn't exist
					ChooseImageFile( mImageFileName );	// so try to get it
					mImageFileName->GetDescriptor( url );
					if (url == CStr255::sEmptyString) 	// but, if we were unsuccessful
						mUseImage->SetValue( 0 );		// revert back.
				}
			}
			AdjustEnable();
			break;
		
		case 'wst1':
			{
			ChooseImageFile( mImageFileName );	// try to get it
			CStr255 url2;
			mImageFileName->GetDescriptor( url2 );
			if (url2 == CStr255::sEmptyString) 	// if we were unsuccessful
				mUseImage->SetValue( 0 );		// don't try to use image
			else
				mUseImage->SetValue( 1 );
			}
			AdjustEnable();
			break;
		
		case msg_EditField2:
			{
			CStr255 url3;
			mImageFileName->GetDescriptor( url3 );
			mUseImage->SetValue( url3[ 0 ] ? Button_On : Button_Off );
			}
			break;
				
		case 'Xtra':
			char * newExtraHTML = NULL;
			Boolean canceled = !GetExtraHTML( pExtra, &newExtraHTML  ,GetWinCSID() );
			if (!canceled)
			{
				if ( pExtra )
					XP_FREE( pExtra );
				pExtra = newExtraHTML;
			}
			break;
		
		default:
			CEditDialog::ListenToMessage( inMessage, ioParam );
			break;
	}
}


void CEDTableContain::FinishCreateSelf()
{
	// Find Controls
	fBorderCheckBox = (LControl*)FindPaneByID( 'BrdW' );
	fBorderCheckBox->SetValueMessage( 'BrdW' );
	
	fBorderWidthEditText = (LGAEditField*)this->FindPaneByID( 'brdw' );
	fCellSpacingEditText = (LGAEditField*)this->FindPaneByID( 'clsp' );
	fCellPaddingEditText = (LGAEditField*)this->FindPaneByID( 'clpd' );
	
	fCustomWidth = (LControl*)this->FindPaneByID( 'twth' );
	fWidthEditText = (LGAEditField*)this->FindPaneByID( 'wdth' );
	fWidthPopup = (LControl*)this->FindPaneByID( 'WdPU' );
	
	fCustomHeight = (LControl*)this->FindPaneByID( 'thgt' );
	fHeightEditText = (LGAEditField*)this->FindPaneByID( 'hght' );
	fHeightPopup = (LControl*)this->FindPaneByID( 'HtPU' );
	
	fCustomColor = (LControl*)this->FindPaneByID( 'tclr' );
	fColorCustomColor = (CColorButton*)FindPaneByID( 'Colo' );
	
	fIncludeCaption = (LControl*)this->FindPaneByID( 'cptn' );
	fCaptionAboveBelow = (LControl*)this->FindPaneByID( 'cPop' );

	mTableAlignment = (LGAPopup *)FindPaneByID( 'HzPU' );

	mFastLayout = (LControl *)FindPaneByID( 'FLay' );
	mUseImage = (LControl*)FindPaneByID( 'UseI' );
	mImageFileName = (CLargeEditField *)FindPaneByID( 'TImg' );
	mImageFileName->AddListener(this);
	mLeaveImage = (LControl *)FindPaneByID( 'LvIm' );
	pExtra = NULL;

	UReanimator::LinkListenerToControls( this, this, 5115 );
	SetLatentSub( fBorderWidthEditText );
	fBorderWidthEditText->SelectAll();
}


void CEDTableContain::Help()
{
	ShowHelp( HELP_TABLE_PROPS_TABLE );
}


void CEDTableContain::AdjustEnable()
{
	LView *bordercaption = (LView *)FindPaneByID(1);
	if ( fBorderCheckBox->GetValue() )
	{
		bordercaption->Enable();
		fBorderWidthEditText->Enable();
	}
	else
	{
		bordercaption->Disable();
		fBorderWidthEditText->Disable();
	}
	
	if (fCustomWidth->GetValue()) {
		fWidthEditText->Enable();
		fWidthPopup->Enable();
	} else {
		fWidthEditText->Disable();
		fWidthPopup->Disable();
	}

	if (fCustomHeight->GetValue()) {
		fHeightEditText->Enable();
		fHeightPopup->Enable();
	} else {
		fHeightEditText->Disable();
		fHeightPopup->Disable();
	}

	if (fIncludeCaption->GetValue())
		fCaptionAboveBelow->Enable();
	else
		fCaptionAboveBelow->Disable();
	
	if ( mUseImage->GetValue() )
		mLeaveImage->Enable();
	else
		mLeaveImage->Disable();
}


void CEDTableContain::PrefsFromControls()
{
	EDT_TableData *fData = EDT_GetTableData(fContext);
	if (fData == NULL)
		return;
	
	LO_Color backgroundColor;
	GetTableDataFromControls( fData, fBorderCheckBox, fBorderWidthEditText,
							fCellSpacingEditText, fCellPaddingEditText, 
							fCustomWidth, fWidthPopup, fWidthEditText,
							fCustomHeight, fHeightPopup, fHeightEditText,
							fCustomColor, fColorCustomColor, &backgroundColor,
							mFastLayout, mTableAlignment,
							mUseImage, mImageFileName, mLeaveImage, pExtra );

 	EDT_SetTableData( fContext, fData );
 	
 	fData->pExtra = NULL;					// don't free this! we still use it!
 	fData->pColorBackground = NULL;			// no need to free this, it is our local variable!
	EDT_FreeTableData(fData);
	
// deal with caption
	EDT_TableCaptionData* fCaptionData = EDT_GetTableCaptionData(fContext);				// find out if we have a caption....
	if (fIncludeCaption->GetValue() == FALSE)
	{
		if (fCaptionData)
		{
			EDT_DeleteTableCaption(fContext);				// there is one, but we don't want it.
			EDT_FreeTableCaptionData(fCaptionData);
		}
	}
	else
	{
		if (fCaptionData)
		{									// we want one, and there is one!!!
			if ( fCaptionAboveBelow->GetValue() == 1 )
				fCaptionData->align = ED_ALIGN_ABSTOP;
			else
				fCaptionData->align = ED_ALIGN_ABSBOTTOM;
				
			EDT_SetTableCaptionData( fContext, fCaptionData );
		}
		else
		{
			fCaptionData = EDT_NewTableCaptionData();		// we want one and there isn't one!!
			if (fCaptionData)
			{
				if ( fCaptionAboveBelow->GetValue() == 1 )
					fCaptionData->align = ED_ALIGN_ABSTOP;
				else
					fCaptionData->align = ED_ALIGN_ABSBOTTOM;
					
				EDT_InsertTableCaption( fContext, fCaptionData );
			}
		}
		
		if (fCaptionData)
			EDT_FreeTableCaptionData(fCaptionData);		// we have to check this, because EDT_NewTableCaptionData() may have returned NULL
	}
}


// Initialize from preferences
void CEDTableContain::ControlsFromPref()
{
	short 			resID;
	CStr255			title;
	StringHandle	titleH;

	if (EDT_IsInsertPointInNestedTable(fContext))
		resID = EDITOR_PERCENT_PARENT_CELL;
	else
		resID = EDITOR_PERCENT_WINDOW;

	titleH = GetString( resID );
	if (titleH)
	{
		SInt8 hstate = HGetState( (Handle)titleH );
		HLock( (Handle)titleH );
		title = *titleH;
		HSetState( (Handle)titleH, hstate );

		MenuHandle menuh = ((LGAPopup *)fWidthPopup)->GetMacMenuH();
		if (menuh)
		{
			SetMenuItemText( menuh, kPercentOfWindowItem, title );
			((LGAPopup *)fWidthPopup)->SetMacMenuH( menuh );	// resets menu width
		}
		
		menuh = ((LGAPopup *)fHeightPopup)->GetMacMenuH();
		if (menuh)
		{
			SetMenuItemText( menuh, kPercentOfWindowItem, title );
			((LGAPopup *)fHeightPopup)->SetMacMenuH( menuh );	// resets menu width
		}
	}

 	EDT_TableData* fData = EDT_GetTableData(fContext);
 	if (fData == NULL)
 		return;

	switch (fData->align)
	{
		default:
		case ED_ALIGN_LEFT: 	mTableAlignment->SetValue( 1 ); break;
		case ED_ALIGN_ABSCENTER: mTableAlignment->SetValue( 2 ); break;
		case ED_ALIGN_RIGHT: 	mTableAlignment->SetValue( 3 );	break;
	}
	
	mFastLayout->SetValue( fData->bUseCols );
	
	fBorderCheckBox->SetValue( fData->bBorderWidthDefined );
	fBorderWidthEditText->SetValue( fData->iBorderWidth );
 	fCellSpacingEditText->SetValue( fData->iCellSpacing );
 	fCellPaddingEditText->SetValue( fData->iCellPadding );

	fCustomWidth->SetValue( fData->bWidthDefined );
	fWidthEditText->SetValue( fData->iWidth );
	fWidthPopup->SetValue( fData->bWidthPercent ? kPercentOfWindowItem : kPixelsItem );
	
	fCustomHeight->SetValue( fData->bHeightDefined );
	fHeightEditText->SetValue( fData->iHeight );
	fHeightPopup->SetValue( fData->bHeightPercent ? kPercentOfWindowItem : kPixelsItem );
	
	RGBColor macColor;
	fCustomColor->SetValue( fData->pColorBackground != NULL );
 	if (fData->pColorBackground)
		macColor = UGraphics::MakeRGBColor(*fData->pColorBackground);
	else
		macColor = UGraphics::MakeRGBColor(255, 255, 255);
	fColorCustomColor->SetColor( macColor );

	if ( fData->pBackgroundImage )
	{
		mImageFileName->SetLongDescriptor( fData->pBackgroundImage );
		// turn on after we set the descriptor so we don't handle as click when msg is broadcast
		TurnOn( mUseImage );
	}
	mLeaveImage->SetValue( fData->bBackgroundNoSave );
	
	pExtra = fData->pExtra;
	fData->pExtra = NULL;		// we'll take over this copy; don't let backend dispose of it
	EDT_FreeTableData(fData);

// check the caption....
	EDT_TableCaptionData* fCaptionData = EDT_GetTableCaptionData( fContext );
	if (fCaptionData)
	{
		fIncludeCaption->SetValue(TRUE);
		if (fCaptionData->align != ED_ALIGN_ABSBOTTOM)
			fCaptionAboveBelow->SetValue( 1 );
		else
			fCaptionAboveBelow->SetValue( 2 );
		EDT_FreeTableCaptionData(fCaptionData);
	}
	else
	{
		fIncludeCaption->SetValue(FALSE);
		fCaptionAboveBelow->SetValue( 1 );
	}
	
	AdjustEnable();
}


Boolean CEDTableContain::AllFieldsOK()
{
	if ( fBorderCheckBox->GetValue() )
	{
		if (!IsEditFieldWithinLimits(fBorderWidthEditText, 0, 10000)) {
			SwitchTarget( fBorderWidthEditText );
			fBorderWidthEditText->SelectAll();
			return FALSE;
		}
	}
	
	if (!IsEditFieldWithinLimits(fCellSpacingEditText, 0, 10000)) {
		SwitchTarget( fCellSpacingEditText );
		fCellSpacingEditText->SelectAll();
		return FALSE;
	}

	if (!IsEditFieldWithinLimits(fCellPaddingEditText, 0, 10000)) {
		SwitchTarget( fCellPaddingEditText );
		fCellPaddingEditText->SelectAll();
		return FALSE;
	}

	if (fCustomWidth->GetValue()) {
		if (fWidthPopup->GetValue() == kPixelsItem && !IsEditFieldWithinLimits(fWidthEditText, 1, 10000)) {
			SwitchTarget( fWidthEditText );
			fWidthEditText->SelectAll();
			return FALSE;
		}

		if (fWidthPopup->GetValue() == kPercentOfWindowItem && !IsEditFieldWithinLimits(fWidthEditText, 1, 100)) {
			SwitchTarget( fWidthEditText );
			fWidthEditText->SelectAll();
			return FALSE;
		}
	}

	if (fCustomHeight->GetValue()) {
		if (fHeightPopup->GetValue() == kPixelsItem && !IsEditFieldWithinLimits(fHeightEditText, 1, 10000)) {
			SwitchTarget( fHeightEditText );
			fHeightEditText->SelectAll();
			return FALSE;
		}

		if (fHeightPopup->GetValue() == kPercentOfWindowItem && !IsEditFieldWithinLimits(fHeightEditText, 1, 100)) {
			SwitchTarget( fHeightEditText );
			fHeightEditText->SelectAll();
			return FALSE;
		}
	}

	return TRUE;
}


void CEDTableContain::ListenToMessage( MessageT inMessage, void* /* ioParam */ )
{
	//Intercept messages we should act on....
	switch (inMessage)
	{
		case 'BrdW':
		case 'twth':
		case 'thgt':
		case 'tclr':
		case 'cptn':
			AdjustEnable();
			break;
		
		case msg_LinkColorChange:
			fCustomColor->SetValue(TRUE);
			break;
		
		case 'UseI':
			if (mUseImage->GetValue() == 1)
			{				// we are trying to set the image
				CStr255 url;
				mImageFileName->GetDescriptor(url);
				if (url == CStr255::sEmptyString)
				{		// but it doesn't exist
					CEditDialog::ChooseImageFile( mImageFileName );	// so try to get it
					mImageFileName->GetDescriptor( url );
					if (url == CStr255::sEmptyString) 	// but, if we were unsuccessful
						mUseImage->SetValue( 0 );		// revert back.
				}
			}
			AdjustEnable();
			break;
		
		case 'wst1':
			{
			CEditDialog::ChooseImageFile( mImageFileName );	// try to get it
			CStr255 url2;
			mImageFileName->GetDescriptor( url2 );
			if (url2 == CStr255::sEmptyString) 	// if we were unsuccessful
				mUseImage->SetValue( 0 );		// don't try to use image
			else
				mUseImage->SetValue( 1 );
			}
			AdjustEnable();
			break;
				
		case msg_EditField2:
			{
			CStr255 url3;
			mImageFileName->GetDescriptor( url3 );
			mUseImage->SetValue( url3[ 0 ] ? Button_On : Button_Off );
			}
			break;
				
 		case 'Xtra':
			char * newExtraHTML = NULL;
			Boolean canceled = !GetExtraHTML( pExtra, &newExtraHTML  ,GetWinCSID() );
			if (!canceled)
			{
				if ( pExtra )
					XP_FREE( pExtra );
				pExtra = newExtraHTML;
			}
			break;
	}
	// Pass all messages on...
}


void CEDTableCellContain::FinishCreateSelf()
{
	fRowSpanEditText = (LGAEditField*)this->FindPaneByID( 'rows' );
	fColSpanEditText = (LGAEditField*)this->FindPaneByID( 'cols' );

	fHorizontalAlignment = (LGAPopup *)this->FindPaneByID( 'HzPU' );
	fVerticalAlignment = (LGAPopup *)this->FindPaneByID( 'VtPU' );

	fHeaderStyle = (LControl*)this->FindPaneByID( 'head' );
	fWrapText = (LControl*)this->FindPaneByID( 'wrap' );

	fCustomWidth = (LControl*)this->FindPaneByID( 'cwth' );
	fWidthEditText = (LGAEditField*)this->FindPaneByID( 'wdth' );
	fWidthPopup = (LControl*)this->FindPaneByID( 'WdPU' );	// popup menu
	
	fCustomHeight = (LControl*)this->FindPaneByID( 'chgt' );
	fHeightEditText = (LGAEditField*)this->FindPaneByID( 'hght' );
	fHeightPopup = (LControl*)this->FindPaneByID( 'HtPU' );	// popup menu
	
	fCustomColor = (LControl*)this->FindPaneByID( 'cclr' );
	fColorCustomColor = (CColorButton*)FindPaneByID( 'Colo' );
	
	mUseImage = (LControl*)FindPaneByID( 'UseI' );
	mImageFileName = (CLargeEditField *)FindPaneByID( 'TImg' );
	mImageFileName->AddListener(this);
	mLeaveImage = (LControl *)FindPaneByID( 'LvIm' );
	pExtra = NULL;
	
	UReanimator::LinkListenerToControls( this, this, mPaneID );
}


void CEDTableCellContain::Help()
{
	ShowHelp( HELP_TABLE_PROPS_CELL );
}


void CEDTableCellContain::AdjustEnable()
{
	if (fCustomWidth->GetValue())
	{
		fWidthEditText->Enable();
		fWidthPopup->Enable();
	}
	else
	{
		fWidthEditText->Disable();
		fWidthPopup->Disable();
	}

	if (fCustomHeight->GetValue())
	{
		fHeightEditText->Enable();
		fHeightPopup->Enable();
	}
	else
	{
		fHeightEditText->Disable();
		fHeightPopup->Disable();
	}

	if ( mUseImage->GetValue() )
		mLeaveImage->Enable();
	else
		mLeaveImage->Disable();
}


void CEDTableCellContain::PrefsFromControls()
{
	EDT_TableCellData* cellData = EDT_GetTableCellData( fContext );
	if (cellData == NULL)
		return;
	
	if ( fRowSpanEditText->IsEnabled() )
		cellData->iRowSpan = fRowSpanEditText->GetValue();
	if ( fColSpanEditText->IsEnabled() )
		cellData->iColSpan = fColSpanEditText->GetValue();
	
	switch ( fHorizontalAlignment->GetValue() )
	{
		default:
		case 1: cellData->align = ED_ALIGN_LEFT;		break;
		case 2: cellData->align = ED_ALIGN_ABSCENTER;	break;
		case 3: cellData->align = ED_ALIGN_RIGHT;		break;
		case 4:	break; // mixed state; don't reset
	}
	
	switch ( fVerticalAlignment->GetValue() )
	{
		default:
		case 1: cellData->valign = ED_ALIGN_ABSTOP;
		case 2: cellData->valign = ED_ALIGN_ABSCENTER;
		case 3: cellData->valign = ED_ALIGN_BASELINE;
		case 4: cellData->valign = ED_ALIGN_ABSBOTTOM;
		case 5:	break;	// mixed state; don't reset
	}
	
	cellData->bHeader = fHeaderStyle->GetValue();
	cellData->bNoWrap = fWrapText->GetValue();
		
	if ( fCustomWidth->GetValue() )
	{
		cellData->bWidthDefined = TRUE;
		cellData->bWidthPercent = fWidthPopup->GetValue() == kPercentOfWindowItem;
		cellData->iWidth = fWidthEditText->GetValue();
	}
	else
		cellData->bWidthDefined = FALSE;
	
	if (fCustomHeight->GetValue())
	{
		cellData->bHeightDefined = TRUE;
		cellData->bHeightPercent = fHeightPopup->GetValue() == kPercentOfWindowItem;
		cellData->iHeight = fHeightEditText->GetValue();
	}
	else
		cellData->bHeightDefined = FALSE;
	
	XP_FREEIF( cellData->pColorBackground );	// we'll replace it with our own if we use it at all.
	cellData->pColorBackground = NULL;
	
	LO_Color pColor;
	if ( fCustomColor->GetValue() )
	{
		pColor = UGraphics::MakeLOColor(fColorCustomColor->GetColor());
		cellData->pColorBackground = &pColor;
	}
	else
		cellData->pColorBackground = NULL;
	
	XP_FREEIF( cellData->pBackgroundImage );
	cellData->pBackgroundImage = NULL;
	
	if ( mUseImage->GetValue() )
		cellData->pBackgroundImage = mImageFileName->GetLongDescriptor();
	cellData->bBackgroundNoSave = mLeaveImage->GetValue();

	LView* extrahtmlbutton = (LView *)FindPaneByID( 'Xtra' );
	XP_ASSERT( extrahtmlbutton != NULL );
	if ( extrahtmlbutton->IsEnabled() )
	{
		XP_FREEIF( cellData->pExtra );
		cellData->pExtra = pExtra;
	}
	else
		cellData->pExtra = NULL;	/* this is probably not the right thing to do but... */
	
	EDT_SetTableCellData( fContext, cellData );
	
	cellData->pColorBackground = NULL;		// don't try to free our local variable.
	cellData->pExtra = NULL;				// this is our copy; don't let backend delete!

	EDT_FreeTableCellData( cellData );
}

#if FIRST_PASS_AT_MASK
typedef enum {
    ED_ALIGN_LEFT_MASK        = 0x0001,
    ED_ALIGN_CENTER_MASK      = 0x0002,
    ED_ALIGN_RIGHT_MASK       = 0x0004,
    ED_ALIGN_ABSTOP_MASK      = 0x0010,
    ED_ALIGN_ABSCENTER_MASK   = 0x0020,
    ED_ALIGN_BASELINE_MASK    = 0x0040,
    ED_ALIGN_ABSBOTTOM_MASK   = 0x0080
} ED_AlignmentMask;

/* ED_AlignmentMask: add the values which are set */
/* Boolean:  true means all agree; false means selection has different values */
struct _EDT_TableCellMask {
	ED_AlignmentMask bHalign;
	ED_AlignmentMask bValign;
	Boolean bColAndRowSpan;
	Boolean bHeader;
	Boolean bNoWrap;
	Boolean bWidth;
	Boolean bHeight;
	Boolean bColor;
	Boolean bBackgroundImage;
	Boolean bExtraHTML;
};
#endif

// Initialize from preferences
void CEDTableCellContain::ControlsFromPref()
{
	EDT_TableCellData* cellData = EDT_GetTableCellData( fContext );
	if (cellData == NULL)
		return;

#if FIRST_PASS_AT_MASK
	_EDT_TableCellMask cellDataMask;
	cellDataMask.bHalign = ED_ALIGN_LEFT_MASK;
	cellDataMask.bValign = ED_ALIGN_BASELINE_MASK;
	cellDataMask.bColAndRowSpan = true;
	cellDataMask.bHeader = cellDataMask.bNoWrap = true;
	cellDataMask.bWidth = cellDataMask.bHeight = true;
	cellDataMask.bColor = cellDataMask.bBackgroundImage = true;
	cellDataMask.bExtraHTML = true;
#endif

// set popup menus depending if nested in another table or just in window
	short 			resID;
	CStr255			title;
	StringHandle	titleH;
	if (EDT_IsInsertPointInNestedTable(fContext))
		resID = EDITOR_PERCENT_PARENT_CELL;
	else
		resID = EDITOR_PERCENT_WINDOW;

	titleH = GetString( resID );
	if (titleH)
	{
		SInt8 hstate = HGetState( (Handle)titleH );
		HLock( (Handle)titleH );
		title = *titleH;
		HSetState( (Handle)titleH, hstate );

		MenuHandle menuh = ((LGAPopup *)fWidthPopup)->GetMacMenuH();
		if (menuh)
		{
			SetMenuItemText( menuh, kPercentOfWindowItem, title );
			((LGAPopup *)fWidthPopup)->SetMacMenuH( menuh );	// resets menu width
		}
		
		menuh = ((LGAPopup *)fHeightPopup)->GetMacMenuH();
		if (menuh)
		{
			SetMenuItemText( menuh, kPercentOfWindowItem, title );
			((LGAPopup *)fHeightPopup)->SetMacMenuH( menuh );	// resets menu width
		}
	}

/* col and row span */
#if FIRST_PASS_AT_MASK
	if ( cellDataMask.bColAndRowSpan )
#endif
	{
		fRowSpanEditText->SetValue( cellData->iRowSpan );
		fColSpanEditText->SetValue( cellData->iColSpan );
	}
#if FIRST_PASS_AT_MASK
	else
	{
		fRowSpanEditText->Disable();
		fColSpanEditText->Disable();
		// should disable the rest of this too...
	}
#endif
	
/* horizontal alignment */
	switch ( cellData->align )
	{
		case ED_ALIGN_LEFT:			fHorizontalAlignment->SetValue( 1 );	break;
		case ED_ALIGN_ABSCENTER:	fHorizontalAlignment->SetValue( 2 );	break;
		case ED_ALIGN_RIGHT:		fHorizontalAlignment->SetValue( 3 );	break;
		default:	/* mixed */		fHorizontalAlignment->SetValue( 4 );	break;
	}

/* vertical alignment */
	switch ( cellData->valign )
	{
		case ED_ALIGN_ABSTOP:		fVerticalAlignment->SetValue( 1 );	break;
		case ED_ALIGN_ABSCENTER:	fVerticalAlignment->SetValue( 2 );	break;
		case ED_ALIGN_BASELINE:		fVerticalAlignment->SetValue( 3 );	break;
		case ED_ALIGN_ABSBOTTOM:	fVerticalAlignment->SetValue( 4 );	break;
		default:	/* mixed */		fVerticalAlignment->SetValue( 5 );	break;
	}

/* text */
#if FIRST_PASS_AT_MASK
	fHeaderStyle->SetValue( cellDataMask.bHeader ? cellData->bHeader : 2 );
	fWrapText->SetValue( cellDataMask.bNoWrap ? cellData->bNoWrap : 2 );
#else
	fHeaderStyle->SetValue( cellData->bHeader );
	fWrapText->SetValue( cellData->bNoWrap );
#endif

#if FIRST_PASS_AT_MASK
	fCustomWidth->SetValue( cellDataMask.bWidth ? cellData->bWidthDefined : 2 );
#else
	fCustomWidth->SetValue( cellData->bWidthDefined );
#endif
	if ( cellData->bWidthDefined )
	{
		fWidthEditText->SetValue( cellData->iWidth );
		fWidthPopup->SetValue( (cellData->bWidthPercent) ? kPercentOfWindowItem : kPixelsItem );
	} 
	else
	{
		fWidthEditText->SetValue(20);		// where do we get the default value?
		fWidthPopup->SetValue( kPercentOfWindowItem );
	}
	
#if FIRST_PASS_AT_MASK
	fCustomHeight->SetValue( cellDataMask.bHeight ? cellData->bHeightDefined : 2 );
#else
	fCustomHeight->SetValue( cellData->bHeightDefined );
#endif
	if ( cellData->bHeightDefined )
	{
		fHeightEditText->SetValue(cellData->iHeight);
		fHeightPopup->SetValue( (cellData->bHeightPercent) ? kPercentOfWindowItem : kPixelsItem );
	}
	else
	{
		fHeightEditText->SetValue(20);		// where do we get the default value?
		fHeightPopup->SetValue( kPercentOfWindowItem );
	}
		
	fCustomColor->SetValue( cellData->pColorBackground != NULL );
	RGBColor rgb;
	if ( cellData->pColorBackground )
		rgb = UGraphics::MakeRGBColor( *cellData->pColorBackground );
	else
		rgb = UGraphics::MakeRGBColor( 0xFF, 0xFF, 0xFF );	// something pretty... (or, better yet, get the default color - yeah, right!)
	fColorCustomColor->SetColor( rgb );
	
	if ( cellData->pBackgroundImage )
	{
		mImageFileName->SetLongDescriptor( cellData->pBackgroundImage );
		// turn on after we set the descriptor so we don't handle as click when msg is broadcast
		TurnOn( mUseImage );
	}
	mLeaveImage->SetValue( cellData->bBackgroundNoSave );

	LView* extrahtmlbutton = (LView *)FindPaneByID( 'Xtra' );
	XP_ASSERT( extrahtmlbutton != NULL );
#if FIRST_PASS_AT_MASK
	if ( cellDataMask.bExtraHTML )
#endif
	{
		extrahtmlbutton->Enable();
		pExtra = cellData->pExtra;
		cellData->pExtra = NULL;	// don't let backend free!
	}
#if FIRST_PASS_AT_MASK
	else
	{
		/* don't agree; disable for now */
		extrahtmlbutton->Disable();
		pExtra = NULL;
	}
#endif

	EDT_FreeTableCellData(cellData);
	AdjustEnable();
}


Boolean CEDTableCellContain::AllFieldsOK()
{
	if ( fRowSpanEditText->IsEnabled() 
	&& !IsEditFieldWithinLimits( fRowSpanEditText, 1, 100 ) )
	{
		SwitchTarget( fRowSpanEditText );
		fRowSpanEditText->SelectAll();
		return FALSE;
	}

	if ( fColSpanEditText->IsEnabled()
	&& !IsEditFieldWithinLimits(fColSpanEditText, 1, 100 ) )
	{
		SwitchTarget( fColSpanEditText );
		fColSpanEditText->SelectAll();
		return FALSE;
	}

	if ( fCustomWidth->GetValue() == 1 )
	{
		int	popupValue = fWidthPopup->GetValue();
		if (popupValue == kPercentOfWindowItem && !IsEditFieldWithinLimits(fWidthEditText, 1, 100)) {
			SwitchTarget( fWidthEditText );
			fWidthEditText->SelectAll();
			return FALSE;
		}

		if (popupValue == kPixelsItem && !IsEditFieldWithinLimits(fWidthEditText, 1, 10000)) {
			SwitchTarget( fWidthEditText );
			fWidthEditText->SelectAll();
			return FALSE;
		}
	}

	if ( fCustomHeight->GetValue() == 1 )
	{
		int	popupValue = fHeightPopup->GetValue();
		if (popupValue == kPercentOfWindowItem && !IsEditFieldWithinLimits(fHeightEditText, 1, 100)) {
			SwitchTarget( fHeightEditText );
			fHeightEditText->SelectAll();
			return FALSE;
		}

		if (popupValue == kPixelsItem && !IsEditFieldWithinLimits(fHeightEditText, 1, 10000)) {
			SwitchTarget( fHeightEditText );
			fHeightEditText->SelectAll();
			return FALSE;
		}
	}

	return TRUE;
}


void CEDTableCellContain::ListenToMessage( MessageT inMessage, void* /* ioParam */ )
{
	switch ( inMessage )
	{
		case 'cwth':
		case 'chgt':
		case 'cclr':
			AdjustEnable();
			break;
		
		case msg_LinkColorChange:
			fCustomColor->SetValue( TRUE );
			break;

		case 'UseI':
			if (mUseImage->GetValue() == 1)
			{				// we are trying to set the image
				CStr255 url;
				mImageFileName->GetDescriptor(url);
				if (url == CStr255::sEmptyString)
				{		// but it doesn't exist
					CEditDialog::ChooseImageFile( mImageFileName );	// so try to get it
					mImageFileName->GetDescriptor( url );
					if (url == CStr255::sEmptyString) 	// but, if we were unsuccessful
						mUseImage->SetValue( 0 );		// revert back.
				}
			}
			AdjustEnable();
			break;
		
		case 'wst1':
			{
			CEditDialog::ChooseImageFile( mImageFileName );	// try to get it
			CStr255 url2;
			mImageFileName->GetDescriptor( url2 );
			if (url2 == CStr255::sEmptyString) 	// if we were unsuccessful
				mUseImage->SetValue( 0 );		// don't try to use image
			else
				mUseImage->SetValue( 1 );
			}
			AdjustEnable();
			break;
				
		case msg_EditField2:
			{
			CStr255 url3;
			mImageFileName->GetDescriptor( url3 );
			mUseImage->SetValue( url3[ 0 ] ? Button_On : Button_Off );
			}
			break;
				
 		case 'Xtra':
			char * newExtraHTML = NULL;
			Boolean canceled = !GetExtraHTML( pExtra, &newExtraHTML  ,GetWinCSID() );
			if (!canceled)
			{
				if ( pExtra )
					XP_FREE( pExtra );
				pExtra = newExtraHTML;
			}
			break;
	}
}


/**********************************************************/
#pragma mark -
#pragma mark Standard Editor Dialogs

void CEDCharacterContain::FinishCreateSelf()
{
	fTextSizePopup = (LControl*)FindPaneByID( 'txsz' );
	mFontMenu = (LControl*)FindPaneByID( 'Font' );
	mFontChanged = false;
	
	fColorDefaultRadio = (LControl*)FindPaneByID( 'DocD' );
	fColorCustomRadio = (LControl*)FindPaneByID( 'CstC' );
	fColorCustomColor = (CColorButton*)FindPaneByID( 'ChCr' );
	
	fTextBoldCheck = (LControl*)FindPaneByID( 'Bold' );
	fTextItalicCheck = (LControl*)FindPaneByID( 'Ital' );
	fTextSuperscriptCheck = (LControl*)FindPaneByID( 'Supe' );
	fTextSubscriptCheck = (LControl*)FindPaneByID( 'Subs' );
	fTextNoBreaksCheck = (LControl*)FindPaneByID( 'NoBR' );
	fTextUnderlineCheck = (LControl*)FindPaneByID( 'Unln' );
	fTextStrikethroughCheck = (LControl*)FindPaneByID( 'Stri' );
	fTextBlinkingCheck = (LControl*)FindPaneByID( 'Blin' );
		
	fClearTextStylesButton = (LControl*)FindPaneByID( 'CTSB' );
	fClearAllStylesButton = (LControl*)FindPaneByID( 'CASB' );

	LView::FinishCreateSelf();
	UReanimator::LinkListenerToControls( this, this, mPaneID );
	
	fTextSizePopup->Show();
}

void CEDCharacterContain::PrefsFromControls()
{
	LO_Color pColor;		// we pass EDT_SetCharacterData() a pointer to this, so it has to be around for awhile.
	
	if (EDT_GetCurrentElementType(fContext) != ED_ELEMENT_TEXT
			&& EDT_GetCurrentElementType(fContext) != ED_ELEMENT_SELECTION)
		return;
	
	EDT_CharacterData* better = EDT_NewCharacterData();
	if (better == NULL)
		return;
	
	better->mask = TF_NONE;
	better->values = TF_NONE;
	
	if (fColorChanged) {
		better->mask |= TF_FONT_COLOR;
		if (fColorDefaultRadio->GetValue()) {
			better->pColor = NULL;
		} else {
			better->values |= TF_FONT_COLOR;
			pColor = UGraphics::MakeLOColor(fColorCustomColor->GetColor());
			better->pColor = &pColor;				// I hope that EDT_SetCharacterData() doesn't hang onto this pointer for long...
		}
	}
	
	if (fSizeChanged) {
		better->mask |= TF_FONT_SIZE;
		better->values |= TF_FONT_SIZE;		// I'm not supposed to set this if we are using the default font size. But how do I know? An extra menu item??
		better->iSize = fTextSizePopup->GetValue();
	}
	
	if ( mFontChanged )
	{
		better->mask |= TF_FONT_FACE;
		if ( better->pFontFace )
		{
			XP_FREE( better->pFontFace );
			better->pFontFace = NULL;
		}
		
		int menuitem = mFontMenu->GetValue();
		if ( menuitem == 1 || menuitem == 2 )	// default menu items
		{
			better->values &= ~TF_FONT_FACE;	// I'm supposed to clear TF_FONT_FACE if we are using the default font.
			if ( menuitem == 2 )				//  if fixed width, do it differently
				better->values |= TF_FIXED;
			else
				better->values &= ~TF_FIXED;	// not fixed if we have a font!
		}
		else
		{
			better->values |= TF_FONT_FACE;
			better->values &= ~TF_FIXED;		// not fixed if we have a font!
			
			Str255 s;
			s[ 0 ] = 0;
			((LGAPopup *)mFontMenu)->GetCurrentItemTitle( s );
			p2cstr( s );
			better->pFontFace = XP_STRDUP( (char *)s );
		}
	}
	
	if (fTextBoldCheck->GetValue() == 1)			better->values |= TF_BOLD;		// what values to set to
	if (fTextItalicCheck->GetValue()  == 1)			better->values |= TF_ITALIC;
	if (fTextSuperscriptCheck->GetValue()  == 1)	better->values |= TF_SUPER;
	if (fTextSubscriptCheck->GetValue()  == 1)		better->values |= TF_SUB;
	if (fTextNoBreaksCheck->GetValue()  == 1)		better->values |= TF_NOBREAK;
	if (fTextUnderlineCheck->GetValue() == 1)		better->values |= TF_UNDERLINE;
	if (fTextStrikethroughCheck->GetValue()  == 1)	better->values |= TF_STRIKEOUT;
	if (fTextBlinkingCheck->GetValue()  == 1)		better->values |= TF_BLINK;
	
	if (fTextBoldCheck->GetValue() != 2)			better->mask |= TF_BOLD;		// if the checkbox is still mixed, don't set anything
	if (fTextItalicCheck->GetValue() != 2)			better->mask |= TF_ITALIC;
	if (fTextSuperscriptCheck->GetValue() != 2)		better->mask |= TF_SUPER;
	if (fTextSubscriptCheck->GetValue() != 2)		better->mask |= TF_SUB;
	if (fTextNoBreaksCheck->GetValue() != 2)		better->mask |= TF_NOBREAK;
	if (fTextUnderlineCheck->GetValue() != 2)		better->mask |= TF_UNDERLINE;
	if (fTextStrikethroughCheck->GetValue() != 2)	better->mask |= TF_STRIKEOUT;
	if (fTextBlinkingCheck->GetValue() != 2)		better->mask |= TF_BLINK;

	EDT_SetFontFace( fContext, better, -1, better->pFontFace );
//	EDT_SetCharacterData(fContext, better);
	better->pColor = NULL;					// this is ours. Don't free it.
	EDT_FreeCharacterData(better);	
}


static void SetFontMenuItemByString( LGAPopup *menu, char *font )
{
	if ( font == NULL )
	{
		menu->SetValue( 1 );
		return;
	}
	
	Str255 str;
	int i, maxitem;
	maxitem = menu->GetMaxValue();
	MenuHandle	popupMenu = menu->GetMacMenuH ();
	for ( i = 1; i <= maxitem; i++ )
	{
		::GetMenuItemText ( popupMenu, i, str );
		p2cstr(str);
		if ( XP_STRSTR( font, (char *)str ) != NULL )
		{
			menu->SetValue( i );
			::SetItemMark( popupMenu, i, '-' );
			break;
		}
	}
}


// Initialize from preferences
void CEDCharacterContain::ControlsFromPref()
{
	if (EDT_GetCurrentElementType(fContext) != ED_ELEMENT_TEXT
		&& EDT_GetCurrentElementType(fContext) != ED_ELEMENT_SELECTION) return;
	
	EDT_CharacterData* better = EDT_GetCharacterData( fContext );
	if (better == NULL) return;
	
	fTextBoldCheck->SetValue(better->values & TF_BOLD ? 1 : 0);
	fTextItalicCheck->SetValue(better->values & TF_ITALIC ? 1 : 0);
	fTextSuperscriptCheck->SetValue(better->values & TF_SUPER ? 1 : 0);
	fTextSubscriptCheck->SetValue(better->values & TF_SUB ? 1 : 0);
	fTextNoBreaksCheck->SetValue(better->values & TF_NOBREAK ? 1 : 0);
	fTextUnderlineCheck->SetValue(better->values & TF_UNDERLINE ? 1 : 0);
	fTextStrikethroughCheck->SetValue(better->values & TF_STRIKEOUT ? 1 : 0);
	fTextBlinkingCheck->SetValue(better->values & TF_BLINK ? 1 : 0);
	
	if (!(better->mask & TF_BOLD))		fTextBoldCheck->SetValue(2);
	if (!(better->mask & TF_ITALIC))	fTextItalicCheck->SetValue(2);
	if (!(better->mask & TF_SUPER))		fTextSuperscriptCheck->SetValue(2);
	if (!(better->mask & TF_SUB))		fTextSubscriptCheck->SetValue(2);
	if (!(better->mask & TF_NOBREAK))	fTextNoBreaksCheck->SetValue(2);
	if (!(better->mask & TF_UNDERLINE))	fTextUnderlineCheck->SetValue(2);
	if (!(better->mask & TF_STRIKEOUT))	fTextStrikethroughCheck->SetValue(2);
	if (!(better->mask & TF_BLINK))		fTextBlinkingCheck->SetValue(2);
	
	if (better->mask & TF_FONT_SIZE)
		fTextSizePopup->SetValue(better->iSize);
	else
		fTextSizePopup->SetValue(EDT_GetFontSize( fContext ));
	fSizeChanged = FALSE;
	
	if ( better->values & TF_FONT_FACE )
		SetFontMenuItemByString( (LGAPopup *)mFontMenu, better->pFontFace );
	else if ( better->values & TF_FIXED )
		mFontMenu->SetValue( 2 );
	else
		mFontMenu->SetValue( 1 );
	
	mFontChanged = false;
	
	LO_Color pColor = {0, 0, 0};
	
	if (better->mask & TF_FONT_COLOR) {		// consistant
		if (better->pColor) {
			pColor = *better->pColor;
			TurnOn( fColorCustomRadio );
		} else {
			TurnOn( fColorDefaultRadio );
			EDT_PageData *pagedata = EDT_GetPageData( fContext );
			if (pagedata && pagedata->pColorText)
				pColor = *pagedata->pColorText;
			EDT_FreePageData(  pagedata );
		}
	} else {								// inconsistant
		if (EDT_GetFontColor(fContext, &pColor))
			TurnOn( fColorCustomRadio );
		else {
			TurnOn( fColorDefaultRadio );
				// get default color
			EDT_PageData *pagedata = EDT_GetPageData( fContext );
			pColor = *pagedata->pColorText;
			EDT_FreePageData(  pagedata );
		}
	}
	
	fColorCustomColor->SetColor(UGraphics::MakeRGBColor(pColor));
	fColorChanged = FALSE;
	
	EDT_FreeCharacterData(better);
}


void CEDCharacterContain::Help()
{
	ShowHelp( HELP_PROPS_CHARACTER );
}


void CEDCharacterContain::ListenToMessage( MessageT inMessage, void* /* ioParam */ )
{
	//Intercept messages we should act on....
	switch (inMessage)
	{
		case msg_Font_Face_Changed:
			mFontChanged = true;
			break;
		
		case 'Supe':
			if (fTextSuperscriptCheck->GetValue())
				fTextSubscriptCheck->SetValue(0);
			break;
				
		case 'Subs':
			if (fTextSubscriptCheck->GetValue())
				fTextSuperscriptCheck->SetValue(0);
			break;
		
		case msg_Clear_Text_Styles:
			fTextBoldCheck->SetValue(0);
			fTextItalicCheck->SetValue(0);
			fTextSuperscriptCheck->SetValue(0);
			fTextSubscriptCheck->SetValue(0);
			fTextNoBreaksCheck->SetValue(0);
			fTextUnderlineCheck->SetValue(0);
			fTextStrikethroughCheck->SetValue(0);
			fTextBlinkingCheck->SetValue(0);
			break;

		case msg_Clear_All_Styles:
			fTextBoldCheck->SetValue(0);
			fTextItalicCheck->SetValue(0);
			fTextSuperscriptCheck->SetValue(0);
			fTextSubscriptCheck->SetValue(0);
			fTextNoBreaksCheck->SetValue(0);
			fTextUnderlineCheck->SetValue(0);
			fTextStrikethroughCheck->SetValue(0);
			fTextBlinkingCheck->SetValue(0);
			
			if ( fTextSizePopup->GetValue() != 3 )
			{
				fSizeChanged = true;
				fTextSizePopup->SetValue(3);
			}
			
			TurnOn( fColorDefaultRadio );
			fColorChanged = TRUE;
			
			if ( mFontMenu->GetValue() != 1 )
				mFontChanged = true;
			mFontMenu->SetValue( 1 );
			break;
		
		case msg_LinkColorChange:
			fColorChanged = TRUE;
			TurnOn( fColorCustomRadio );
			break;
		
		case msg_Default_Color_Radio:
			fColorChanged = TRUE;
			break;
		
		case msg_Custom_Color_Radio:
			fColorChanged = TRUE;
			break;
		
		case msg_Font_Size_Changed:
			fSizeChanged = TRUE;
			break;
	}
	
	// Pass all messages on...
}

/**********************************************************/
// There is no other good place for these. These correspond to menu item numbers which are
// in a resource (not a rez file)

#define ED_NO_CONTAINER	1
#define ED_LIST_ITEM	2
#define ED_BLOCKQUOTE	3

void CEDParagraphContain::FinishCreateSelf()
{
	fParagraphStylePopup = (LControl*)FindPaneByID( 'pspu' );
	fContainerStylePopup = (LControl*)FindPaneByID( 'cspu' );
	
	fListStylePopup = (LControl*)FindPaneByID( 'lspu' );
	fNumberPopup = (LControl*)FindPaneByID( 'nobp' );
	fBulletPopup = (LControl*)FindPaneByID( 'bulp' );
	fStartNumberCaption = (LControl*)FindPaneByID( 'junk' );
	fStartNumberEditText = (LGAEditField*)FindPaneByID( 'snet' );

	fLeftAlignRadio = (LControl*)FindPaneByID( 'Left' );
	fCenterAlignRadio = (LControl*)FindPaneByID( 'Cent' );
	fRightAlignRadio = (LControl*)FindPaneByID( 'Righ' );
	
	LView::FinishCreateSelf();
	UReanimator::LinkListenerToControls( this, this, mPaneID );
	
	fParagraphStylePopup->Show();
	fContainerStylePopup->Show();
	fListStylePopup->Show();
	fLeftAlignRadio->Show();
	fCenterAlignRadio->Show();
	fRightAlignRadio->Show();
}


void CEDParagraphContain::PrefsFromControls()
{
// This only differs from the Windows FE code in two ways:
// 1) EDT_MorphContainer is called BEFORE the EDT_Outdent "while" loop instead of AFTER
// 2) if fContainerStylePopup->GetValue() == ED_NO_CONTAINER we NEVER call EDT_SetListData(); the windows code DOES if list != NULL.

	if (fLeftAlignRadio->GetValue())
		EDT_SetParagraphAlign(fContext, ED_ALIGN_LEFT);
	else if (fRightAlignRadio->GetValue())
		EDT_SetParagraphAlign(fContext, ED_ALIGN_RIGHT);
	else
		EDT_SetParagraphAlign(fContext, ED_ALIGN_CENTER);
		
	switch (fParagraphStylePopup->GetValue()) {
		case 1:  EDT_MorphContainer( fContext, P_PARAGRAPH); break;
		case 2:  EDT_MorphContainer( fContext, P_HEADER_1); break;
		case 3:  EDT_MorphContainer( fContext, P_HEADER_2); break;
		case 4:  EDT_MorphContainer( fContext, P_HEADER_3); break;
		case 5:  EDT_MorphContainer( fContext, P_HEADER_4); break;
		case 6:  EDT_MorphContainer( fContext, P_HEADER_5); break;
		case 7:  EDT_MorphContainer( fContext, P_HEADER_6); break;
		case 8:  EDT_MorphContainer( fContext, P_ADDRESS); break;
		case 9:  EDT_MorphContainer( fContext, P_PREFORMAT); break;
		case 10: EDT_MorphContainer( fContext, P_LIST_ITEM); break;
		case 11: EDT_MorphContainer( fContext, P_DESC_TITLE); break;
		case 12: EDT_MorphContainer( fContext, P_DESC_TEXT); break;
	}
	
	if (fContainerStylePopup->GetValue() == ED_NO_CONTAINER)
	{
	    EDT_ListData * pListData = EDT_GetListData(fContext);				// remove 
	    while (pListData) {
	        EDT_FreeListData(pListData);
	        EDT_Outdent(fContext);
	        pListData = EDT_GetListData(fContext);
	    }
	}
	else
	{
		EDT_ListData *list = EDT_GetListData( fContext );
		if (list == NULL)
		{						// do we need to create a container?
			EDT_Indent( fContext );
			list = EDT_GetListData( fContext );
			if (list == NULL)
				return; 			// assert?
		}
		
		list->eType = ED_LIST_TYPE_DEFAULT;		// defaults
		list->iStart = 1;
		
		if (fContainerStylePopup->GetValue() == ED_BLOCKQUOTE)
		
			list->iTagType = P_BLOCKQUOTE;
			
		else
		{
			switch (fListStylePopup->GetValue()) {
				case 1: list->iTagType = P_UNUM_LIST; break;
				case 2: list->iTagType = P_NUM_LIST; break;
				case 3: list->iTagType = P_DIRECTORY; break;
				case 4: list->iTagType = P_MENU; break;
				case 5: list->iTagType = P_DESC_LIST; break;
			}
			
			if (list->iTagType == P_UNUM_LIST) {
				
				switch (fBulletPopup->GetValue()) {
					case 1: list->eType = ED_LIST_TYPE_DEFAULT; break;
					case 2: list->eType = ED_LIST_TYPE_DISC; break;
					case 3: list->eType = ED_LIST_TYPE_CIRCLE; break;
					case 4: list->eType = ED_LIST_TYPE_SQUARE; break;
				}
				
			} else if (list->iTagType == P_NUM_LIST) {
				
				switch (fNumberPopup->GetValue()) {
					case 1: list->eType = ED_LIST_TYPE_DEFAULT; break;
					case 2: list->eType = ED_LIST_TYPE_DIGIT; break;
					case 3: list->eType = ED_LIST_TYPE_BIG_ROMAN; break;
					case 4: list->eType = ED_LIST_TYPE_SMALL_ROMAN; break;
					case 5: list->eType = ED_LIST_TYPE_BIG_LETTERS; break;
					case 6: list->eType = ED_LIST_TYPE_SMALL_LETTERS; break;
				}
				
				list->iStart = fStartNumberEditText->GetValue();
			}
		}
						
		EDT_SetListData(fContext, list);
		EDT_FreeListData(list);
	}
}


// Initialize from preferences
void CEDParagraphContain::ControlsFromPref()
{
	switch (EDT_GetParagraphAlign(fContext)) {
		case ED_ALIGN_LEFT:		TurnOn(fLeftAlignRadio); break;
		case ED_ALIGN_RIGHT:	TurnOn(fRightAlignRadio); break;
		default: 				TurnOn(fCenterAlignRadio); break;
	}

	fParagraphStylePopup->SetValue(1);
	switch (EDT_GetParagraphFormatting(fContext)) {
		case P_HEADER_1:	fParagraphStylePopup->SetValue(2); break;
		case P_HEADER_2:	fParagraphStylePopup->SetValue(3); break;
		case P_HEADER_3:	fParagraphStylePopup->SetValue(4); break;
		case P_HEADER_4:	fParagraphStylePopup->SetValue(5); break;
		case P_HEADER_5:	fParagraphStylePopup->SetValue(6); break;
		case P_HEADER_6:	fParagraphStylePopup->SetValue(7); break;
		case P_ADDRESS:		fParagraphStylePopup->SetValue(8); break;
		case P_PREFORMAT:	fParagraphStylePopup->SetValue(9); break;
		case P_LIST_ITEM:	fParagraphStylePopup->SetValue(10); break;
		case P_DESC_TITLE:	fParagraphStylePopup->SetValue(11); break;
		case P_DESC_TEXT:	fParagraphStylePopup->SetValue(12); break;
	}
	
	EDT_ListData *list = EDT_GetListData( fContext );
	
	fContainerStylePopup->SetValue(ED_NO_CONTAINER);
	if (list) {
		if  (list->iTagType == P_BLOCKQUOTE)
			fContainerStylePopup->SetValue(ED_BLOCKQUOTE);
		else
			fContainerStylePopup->SetValue(ED_LIST_ITEM);		// list item
	}
	
	fListStylePopup->SetValue(1);		// set up default values
	fBulletPopup->SetValue(1);
	fNumberPopup->SetValue(1);
	fStartNumberEditText->SetValue(1);
	
	if (fContainerStylePopup->GetValue() == ED_LIST_ITEM) {		// list item
	
		switch (list->iTagType) {
			case P_UNUM_LIST:	fListStylePopup->SetValue(1); break;
			case P_NUM_LIST:	fListStylePopup->SetValue(2); break;
			case P_DIRECTORY:	fListStylePopup->SetValue(3); break;
			case P_MENU:		fListStylePopup->SetValue(4); break;
			case P_DESC_LIST:	fListStylePopup->SetValue(5); break;
			default: 			fListStylePopup->SetValue(1); break;	// assert?
		}
			
		if (list->iTagType == P_UNUM_LIST) {
			
			switch (list->eType) {
				case ED_LIST_TYPE_DEFAULT:	fBulletPopup->SetValue(1); break;
				case ED_LIST_TYPE_DISC:		fBulletPopup->SetValue(2); break;
				case ED_LIST_TYPE_CIRCLE:	fBulletPopup->SetValue(3); break;
				case ED_LIST_TYPE_SQUARE:	fBulletPopup->SetValue(4); break;
				default:					fBulletPopup->SetValue(1); break;	// assert?
			}
		}

		if (list->iTagType == P_NUM_LIST) {
			
			switch (list->eType) {
				case ED_LIST_TYPE_DEFAULT:			fNumberPopup->SetValue(1); break;
				case ED_LIST_TYPE_DIGIT:			fNumberPopup->SetValue(2); break;
				case ED_LIST_TYPE_BIG_ROMAN:		fNumberPopup->SetValue(3); break;
				case ED_LIST_TYPE_SMALL_ROMAN:		fNumberPopup->SetValue(4); break;
				case ED_LIST_TYPE_BIG_LETTERS:		fNumberPopup->SetValue(5); break;
				case ED_LIST_TYPE_SMALL_LETTERS:	fNumberPopup->SetValue(6); break;
				default:							fNumberPopup->SetValue(1); break;	// assert?
			}
			
			fStartNumberEditText->SetValue(list->iStart);
		}
	}
	
	if (list) EDT_FreeListData(list);
	
	AdjustPopupsVisibility();
}


void CEDParagraphContain::AdjustPopupsVisibility()
{
	if (fContainerStylePopup->GetValue() != ED_LIST_ITEM) {		// list item
	
		fListStylePopup->Disable();
		fStartNumberCaption->Hide();
		fStartNumberEditText->Hide();
		
		switch (fListStylePopup->GetValue()) {
			case 1:		// Unumbered List
				fNumberPopup->Hide();
				fBulletPopup->Disable();
				fBulletPopup->Show();
			break;
			
			case 2:		// Numbered List
				fNumberPopup->Disable();
				fNumberPopup->Show();
				fBulletPopup->Hide();
			break;
			
			default:
				fNumberPopup->Hide();
				fBulletPopup->Hide();
			break;
		}
		
	} else {
	
		fListStylePopup->Enable();
		
		switch (fListStylePopup->GetValue()) {
			case 1:		// Unnumbered List
				fNumberPopup->Hide();
				fBulletPopup->Enable();
				fBulletPopup->Show();
				fStartNumberCaption->Hide();
				fStartNumberEditText->Hide();
			break;
			
			case 2:		// Numbered List
				fNumberPopup->Enable();
				fNumberPopup->Show();
				fBulletPopup->Hide();
				fStartNumberCaption->Show();
				fStartNumberEditText->Show();
			break;
			
			default:
				fNumberPopup->Hide();
				fBulletPopup->Hide();
				fStartNumberCaption->Hide();
				fStartNumberEditText->Hide();
			break;
			
		}
	}
}


void CEDParagraphContain::Help()
{
	ShowHelp( HELP_PROPS_PARAGRAPH );
}


Boolean CEDParagraphContain::AllFieldsOK()
{
	if ( fStartNumberEditText->IsVisible() && fStartNumberEditText->IsEnabled()
	&& !IsEditFieldWithinLimits( fStartNumberEditText, 1, 2000000000 ) )
	{
		SwitchTarget( fStartNumberEditText );
		fStartNumberEditText->SelectAll();
		return FALSE;
	}
	
	return true;
}


void CEDParagraphContain::ListenToMessage( MessageT inMessage, void* /* ioParam */ )
{
	//Intercept messages we should act on....
	
	switch (inMessage) {
		case msg_Paragraph_Style_Popup:
			if (fParagraphStylePopup->GetValue() == 10)		// "List item" menu item
				fContainerStylePopup->SetValue(ED_LIST_ITEM);			// "List" menu item (doesn't seem to be necessary)
			else if (fContainerStylePopup->GetValue() == ED_LIST_ITEM)
				fContainerStylePopup->SetValue(ED_NO_CONTAINER);			// (IS necessary)
			AdjustPopupsVisibility();
		break;
		
		case msg_Paragraph_Addtnl_Style_Popup:
			if (fContainerStylePopup->GetValue() == ED_LIST_ITEM)		// "List" menu item
				fParagraphStylePopup->SetValue(10);			// "List item" menu item
//			else if (fParagraphStylePopup->GetValue() == 10)
//				fParagraphStylePopup->SetValue(1);			// windows FE doesn't do this. Do we know what we're doing?
			AdjustPopupsVisibility();
		break;
		
		case msg_List_Style_Popup:
			AdjustPopupsVisibility();
		break;
	}
	
	// Pass all messages on...
}


/**********************************************************/
void CEDLinkContain::FinishCreateSelf()
{
	fLinkedTextEdit = (CLargeEditField *)FindPaneByID( 'LTte' );
	
	fChooseFileLinkButton = (LControl*)FindPaneByID( 'CFLb' );
	fRemoveLinkButton = (LControl*)FindPaneByID( 'RLbt' );
	fLinkPageTextEdit = (CLargeEditField *)FindPaneByID( 'LPte' );
	
	fCurrentDocumentRadio = (LControl*)FindPaneByID( 'CDro' );
	fSelectedFileRadio = (LControl*)FindPaneByID( 'SFro' );
	fTargetList = (OneRowLListBox*)FindPaneByID( 'TAli' );
	
	LView::FinishCreateSelf();
	UReanimator::LinkListenerToControls( this, this, mPaneID );
	
	fTargs = NULL;
}


void CEDLinkContain::PrefsFromControls()
{
	if (fInsert && LO_GetSelectionText(fContext) == NULL)
	{
		char *link = fLinkPageTextEdit->GetLongDescriptor();
	
		if (link && XP_STRLEN(link))
		{
			char *anchor = fLinkedTextEdit->GetLongDescriptor();
			if ( anchor && XP_STRLEN(anchor) )
			{
				if ( EDT_PasteHREF( fContext, &link, &anchor, 1 ) != EDT_COP_OK )
					ErrorManager::PlainAlert( CUT_ACROSS_CELLS );
			}
			else if ( EDT_PasteHREF( fContext, &link, &link, 1 ) != EDT_COP_OK )
			{
				// use the link as the anchor if there is no anchor
				ErrorManager::PlainAlert( CUT_ACROSS_CELLS );
			}
			
			XP_FREEIF( anchor);
		}
		
		XP_FREEIF( link );
	}
	else // if ( EDT_CanSetHREF( fContext ) )	// should always be true
	{
		EDT_HREFData *linkdata = EDT_GetHREFData( fContext );
		if ( linkdata )
		{
			if ( linkdata->pURL )
			{
				XP_FREE( linkdata->pURL );
				linkdata->pURL = NULL;
			}
			linkdata->pURL = fLinkPageTextEdit->GetLongDescriptor();

			if ( linkdata->pExtra )
			{
				XP_FREE( linkdata->pExtra );
				linkdata->pExtra = NULL;
			}
			
			if ( pExtra )
				linkdata->pExtra = XP_STRDUP( pExtra );
			
			EDT_SetHREFData( fContext, linkdata );
			EDT_FreeHREFData( linkdata );
		}
	}
}


void CEDLinkContain::Show()
{
	if ( *fLinkName )
		fLinkedTextEdit->SetDescriptor( CStr255(*fLinkName) );
	
	CEditContain::Show();
}


void CEDLinkContain::Hide()
{
	if ( *fLinkName )
		XP_FREE( *fLinkName );
	*fLinkName = fLinkedTextEdit->GetLongDescriptor();
	CEditContain::Hide();
}

#define msg_ClickOnTarget 23000
#define msg_DblClickOnTarget 23001
#define msg_ClickOnTarget2 23002				/* really should put these in resgui.h or similar */
#define msg_DblClickOnTarget2 23003

// Initialize from preferences
void CEDLinkContain::ControlsFromPref()
{
	SetTextTraitsIDByCsid(fLinkedTextEdit, GetWinCSID());
	if ( *fLinkName )
	{			// image pane has already been here
		fLinkedTextEdit->SetDescriptor( CStr255(*fLinkName) );
		fLinkedTextEdit->Disable();
		SwitchTarget( fLinkPageTextEdit );
		fLinkPageTextEdit->SelectAll();
	}
	else
	{
		char *selection = (char*)LO_GetSelectionText( fContext );
		if ( selection ) 
			fLinkedTextEdit->SetDescriptor( CtoPstr(selection) );
		
		if ( fInsert && selection == NULL )
		{
			fLinkedTextEdit->Enable();
			SwitchTarget( fLinkedTextEdit );
			fLinkedTextEdit->SelectAll();
		}
		else
		{
			fLinkedTextEdit->Disable();
			SwitchTarget( fLinkPageTextEdit );
			fLinkPageTextEdit->SelectAll();
		}
		
		XP_FREEIF( selection );
	}

	EDT_HREFData *linkdata = EDT_GetHREFData( fContext );
	if ( linkdata )
	{
		if ( linkdata->pURL )
			fLinkPageTextEdit->SetDescriptor( CtoPstr(linkdata->pURL) );
		pExtra = linkdata->pExtra;	// let's not realloc
		linkdata->pExtra = NULL;	// don't dispose of this!
		
		EDT_FreeHREFData( linkdata );
	}
	
	TurnOn( fCurrentDocumentRadio );
	CurrentFileTargs();
	
	fTargetList->FocusDraw();
	fTargetList->SetDoubleClickMessage( msg_DblClickOnTarget );
	fTargetList->SetSingleClickMessage( msg_ClickOnTarget );
	fTargetList->AddListener( this );	
	fTargetList->SetValue(-1);
}


CEDLinkContain::~CEDLinkContain()
{
	if ( fTargs )
		XP_FREE( fTargs );
	if ( pExtra )
		XP_FREE( pExtra );
	pExtra = NULL;
}


void CEDLinkContain::Help()
{
	ShowHelp( HELP_PROPS_LINK );
}


void CEDLinkContain::SelectedFileUpdate()
{
	// clear list
	while ( fTargetList->GetRows() )
		fTargetList->RemoveRow(0);
	
	if ( fTargs )
		XP_FREE(fTargs);
	fTargs = NULL;
	
	// get file to retrieve targets from
	char *link = fLinkPageTextEdit->GetLongDescriptor();
	if ( link == NULL )
		return;
	
	// I assume a pound would only confuse things; remove it if present
	char *pound = XP_STRCHR(link, '#');
	if ( pound )
		*pound = '\0';
	
	// need to pass an xpURL format "link" below ***FIX***THIS***
	fTargs = EDT_GetAllDocumentTargetsInFile( fContext, link );
	XP_FREE( link );
	char *parse = fTargs;
	
	int16 rowNum = fTargetList->GetRows();			// add to the bottom?
	
	while ( parse && *parse )
	{
		fTargetList->AddRow( rowNum++, parse, XP_STRLEN(parse) );
		parse += XP_STRLEN(parse) + 1;
	}
}


void CEDLinkContain::CurrentFileTargs()
{
	while (fTargetList->GetRows())
		fTargetList->RemoveRow(0);
	
	if ( fTargs )
		XP_FREE( fTargs );

	fTargs = EDT_GetAllDocumentTargets(fContext);
	char *parse = fTargs;
	
	int16 rowNum = fTargetList->GetRows();			// add to the bottom?
	
	while (parse && *parse)
	{
		fTargetList->AddRow( rowNum++, parse, XP_STRLEN(parse));
		parse += XP_STRLEN(parse) + 1;
	}
}


void CEDLinkContain::ListenToMessage( MessageT inMessage, void* /* ioParam */ )
{
	//Intercept messages we should act on....
	
	switch (inMessage)
	{
		case 'CDro':		// targets from current document radio button
			if ( fCurrentDocumentRadio->GetValue() )
				CurrentFileTargs();
			break;
		
		case 'SFro':		// targets from selected file radio button
			if ( fSelectedFileRadio->GetValue() )
				SelectedFileUpdate();
			break;
		
		case msg_Link_Clear_Link:
			fLinkPageTextEdit->SetDescriptor("\p");
			break;
		
		case msg_Link_Browse_File:
		{	
			StPrepareForDialog	preparer;	
			StandardFileReply	reply;
			Point				loc = { -1, -1 };
			OSType				types[ 4 ];
			
			types[ 0 ] = 'GIFf';
			types[ 1 ] = 'TEXT';
			types[ 2 ] = 'JPEG';
		
			::StandardGetFile( NULL, 3, types, &reply );

			if ( reply.sfGood )
			{
				char *fileLink = CFileMgr::GetURLFromFileSpec( reply.sfFile );
				if ( fileLink )
				{
					if (CPrefs::GetBoolean( CPrefs::PublishMaintainLinks ))
					{
						char *abs = NULL;		// let's try making it relative
						if (NET_MakeRelativeURL( LO_GetBaseURL( fContext ), fileLink, &abs) != NET_URL_FAIL && abs)
						{
							XP_FREE( fileLink );
							fileLink = abs;
							abs = NULL;
						}
						
						XP_FREEIF( abs );
					}
					
					fLinkPageTextEdit->SetLongDescriptor( fileLink );
					XP_FREE( fileLink );
					
					if ( fSelectedFileRadio->GetValue() )
						SelectedFileUpdate();
				}
			}
		}
		break;
		
		case msg_DblClickOnTarget:
		case msg_ClickOnTarget:
			int16	index =  fTargetList->GetValue();
			
			// first take care of the case where the user tries to "deselect" the item in the list..
			if (index == -1)
			{
				char *file = fLinkPageTextEdit->GetLongDescriptor();	// we are going to save the url if it exists
				if ( file )
				{
					char *pound = XP_STRCHR( file, '#' );
					if ( pound )
					{
						*pound = '\0';
						fLinkPageTextEdit->SetLongDescriptor( file );
					}
					
					XP_FREE( file );
				}
				break;
			}
			
			char *parse = fTargs;
			while ( parse && *parse && index-- )
				parse += XP_STRLEN(parse) + 1;

			if ( parse && *parse )
			{	// why is it that I have no other way to do this?
				CStr255	temp = "";
				
				if (fSelectedFileRadio->GetValue())
				{
					char *file2 = fLinkPageTextEdit->GetLongDescriptor();	// we are going to save the url if it exists
					if ( file2 )
					{
						char *pound = XP_STRCHR(file2, '#');
						if ( pound )
							*pound = '\0';
						
						temp = file2;
						XP_FREE( file2 );
					}
				}

				temp += '#';
				temp += parse;
				fLinkPageTextEdit->SetDescriptor(temp);

			}
			
			if ( inMessage == msg_DblClickOnTarget )
				ListenToMessage( msg_OK, NULL );
		break;
		
		case 'Xtra':
			char * newExtraHTML = NULL;
			Boolean canceled = !GetExtraHTML( pExtra, &newExtraHTML, GetWinCSID() );
			if (!canceled)
			{
				XP_FREEIF( pExtra );
				pExtra = newExtraHTML;
			}
			break;
	}	
	
	// Pass all messages on...
}


#ifdef COOL_IMAGE_RADIO_BUTTONS
#pragma mark -

// ---------------------------------------------------------------------------
//		¥ SetValue
// ---------------------------------------------------------------------------
//	Turn a ToggleButton on or off

void
CImageAlignButton::SetValue( Int32 inValue )
{
	if (inValue != mValue)
	{
		LControl::SetValue(inValue);
		Refresh();
	}

	// If turning RadioButton on, broadcast message so that the RadioGroup
	// (if present) will turn off the other RadioButtons in the group.
	if (inValue == Button_On)
		BroadcastMessage( msg_ControlClicked, (void*) this );
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	HotSpotAction
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CImageAlignButton::HotSpotAction(short /* inHotSpot */, Boolean inCurrInside, Boolean inPrevInside)
{
	if ( GetValue() == 0 )
	{
		if ( inCurrInside != inPrevInside )
		{
			SetTrackInside( inCurrInside );
			Draw( NULL );
			SetTrackInside( false );
		}
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	HotSpotResult
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CImageAlignButton::HotSpotResult(Int16 inHotSpot)
{
	SetValue(1);
	
	// Although value doesn't change, send message to inform Listeners
	// that button was clicked
//	BroadcastValueMessage();		
}
#endif


/**********************************************************/
CEDImageContain::CEDImageContain( LStream* inStream ) : CEditContain( inStream )
{
	fSrcStr = NULL;
	fLowSrcStr = NULL;
	fLooseImageMap = FALSE;
	mBorderUnspecified = false;
}

CEDImageContain::~CEDImageContain()
{
	XP_FREEIF( fSrcStr );
	XP_FREEIF( fLowSrcStr );
	XP_FREEIF( pExtra );
	pExtra = NULL;
}

void CEDImageContain::FinishCreateSelf()
{
	fImageFileName = (CLargeEditField *)FindPaneByID( 'WST1' );
	fImageFileName->AddListener(this);

	fImageAltFileName = (CLargeEditField *)FindPaneByID( 'WST3' );
	fImageAltFileName->AddListener(this);
	fImageAltTextEdit = (CLargeEditField *)FindPaneByID( 'WST5' );
	fImageAltTextEdit->AddListener(this);
	
	fHeightTextEdit = (LGAEditField*)FindPaneByID( 'WSTd' );
	fHeightTextEdit->AddListener( this );
	fWidthTextEdit = (LGAEditField*)FindPaneByID( 'WSTf' );
	fWidthTextEdit->AddListener( this );
	fImageLockedCheckBox = (LControl *)FindPaneByID( 'Lock' );	// constrain
	
	fLeftRightBorderTextEdit = (LGAEditField*)FindPaneByID( 'WSTh' );
	fTopBottomBorderTextEdit = (LGAEditField*)FindPaneByID( 'WSTi' );
	fSolidBorderTextEdit = (LGAEditField*)FindPaneByID( 'WSTj' );
	fSolidBorderTextEdit->AddListener( this );

	fCopyImageCheck = (LControl*)FindPaneByID( 'WSTl' );
	fBackgroundImageCheck = (LControl*)FindPaneByID( 'Bkgd' );
	fRemoveImageMapButton = (LControl*)FindPaneByID( 'WSTm' );
	fEditImageButton = (LControl*)FindPaneByID( 'WSTn' );
	
	mImageAlignmentPopup = (LControl*)FindPaneByID( 'ImgA' );

	LView::FinishCreateSelf();
	UReanimator::LinkListenerToControls( this, this, mPaneID );
}


void CEDImageContain::AdjustEnable()
{
	Boolean allEmpty = false;	// assume at least one has text unless proven otherwise
	Str255 str;
	fImageFileName->GetDescriptor( str );
	if ( str[0] == 0 )
	{
		fCopyImageCheck->Disable();
		fEditImageButton->Disable();
		fBackgroundImageCheck->Disable();

		fImageAltFileName->GetDescriptor( str );
		if ( str[0] == 0 )
		{
			fImageAltTextEdit->GetDescriptor( str );
			if ( str[0] == 0 )
				allEmpty = true;
		}
	}
	else
	{
		fCopyImageCheck->Enable();
		fEditImageButton->Enable();
		fBackgroundImageCheck->Enable();
	}
	
	LView* dimensions = (LView *)FindPaneByID( 'C003' );
	LView* spacearound = (LView *)FindPaneByID( 'C004' );
	LView* aligncaption = (LView *)FindPaneByID( 'Cptn' );	// alignment caption
	LView* extrahtmlbutton = (LView *)FindPaneByID( 'Xtra' );
	if ( allEmpty || fBackgroundImageCheck->GetValue() )
	{
		dimensions->Disable();
		spacearound->Disable();
		extrahtmlbutton->Disable();

		mImageAlignmentPopup->Disable();
		if ( aligncaption )
			aligncaption->Disable();
		
		LView* altreps = (LView *)FindPaneByID( 'C002' );
		if ( fBackgroundImageCheck->GetValue() )
			altreps->Disable();
		else
			altreps->Enable();
	}
	else
	{
		LView* altreps = (LView *)FindPaneByID( 'C002' );
		altreps->Enable();
		
		dimensions->Enable();
		spacearound->Enable();
		extrahtmlbutton->Enable();

		mImageAlignmentPopup->Enable();
		if ( aligncaption )
			aligncaption->Enable();

		// can't constrain if it's original size or if either/both are "% of window"
		Boolean doEnable = false;
		LControl *control = (LControl *)FindPaneByID( 'Orig' );
		if ( control && ( control->GetValue() == 0 ) )
		{
			LGAPopup *widthPopup = (LGAPopup *)FindPaneByID( 'WdPU' );
			LGAPopup *heightPopup = (LGAPopup *)FindPaneByID( 'HtPU' );
			doEnable = ( widthPopup->GetValue() != kPercentOfWindowItem ) 
						&& ( heightPopup->GetValue() != kPercentOfWindowItem );
		}
		
		if ( doEnable )
			fImageLockedCheckBox->Enable();
		else
			fImageLockedCheckBox->Disable();
	}
}


EDT_ImageData *CEDImageContain::ImageDataFromControls()
{
	EDT_ImageData *image;
	
	if (fInsert)
		image = EDT_NewImageData();
	else
	{
		if ( EDT_GetCurrentElementType(fContext) != ED_ELEMENT_IMAGE )
			return NULL;
		image = EDT_GetImageData( fContext );
	}
		
	if (image == NULL)
		return NULL;
	
					// what about ED_ALIGN_BOTTOM & ED_ALIGN_ABSTOP
	image->align = ED_ALIGN_DEFAULT;	
	switch (mImageAlignmentPopup->GetValue())
	{
		case 1:	image->align = ED_ALIGN_TOP;		break;
		case 2:	image->align = ED_ALIGN_ABSCENTER;	break;
		case 3:	image->align = ED_ALIGN_CENTER;		break;
		case 4:	image->align = ED_ALIGN_BASELINE;	break;
		case 5:	image->align = ED_ALIGN_ABSBOTTOM;	break;
		case 6:	image->align = ED_ALIGN_LEFT;		break;
		case 7:	image->align = ED_ALIGN_RIGHT;		break;
	}
	
	image->bNoSave = fCopyImageCheck->GetValue();
		
	XP_FREEIF( image->pSrc );
	image->pSrc = fImageFileName->GetLongDescriptor();
	
	XP_FREEIF( image->pLowSrc );
	image->pLowSrc = fImageAltFileName->GetLongDescriptor();

	XP_FREEIF( image->pAlt );
	image->pAlt = fImageAltTextEdit->GetLongDescriptor();

	XP_FREEIF( image->pExtra );
	image->pExtra = ( pExtra ) ? XP_STRDUP( pExtra ) : NULL;

	// we need a valid URL for this image (for instance, something that starts with 'file://'
	
/*	char *absURL;
	
	if (image->pSrc && (absURL = NET_MakeAbsoluteURL( LO_GetBaseURL( fContext ), image->pSrc ))) {
		XP_FREE(image->pSrc);
		image->pSrc = absURL;
	}

	if (image->pLowSrc && (absURL = NET_MakeAbsoluteURL( LO_GetBaseURL( fContext ), image->pLowSrc ))) {
		XP_FREE(image->pLowSrc);
		image->pLowSrc = absURL;
	}

	FSSpec	file;
	char *path;
	
	file = fImageFileName->GetFSSpec();
	if (file.vRefNum != 0 || file.parID != 0 || file.name[0] != 0) {
		char *path = CFileMgr::GetURLFromFileSpec(file);
		if (path) {
			if (fSrcStr == NULL || strcmp(path, fSrcStr)) {
				if (image->pSrc) XP_FREE(image->pSrc);
				image->pSrc = path;
			} else XP_FREE(path);
		}
	}
	
	file = fImageAltFileName->GetFSSpec();
	if (file.vRefNum != 0 || file.parID != 0 || file.name[0] != 0) {
		char *path = CFileMgr::GetURLFromFileSpec(file);
		if (path) {
			if (fLowSrcStr == NULL || strcmp(path, fLowSrcStr)) {
				if (image->pLowSrc) XP_FREE(image->pLowSrc);
				image->pLowSrc = path;
			} else XP_FREE(path);
		}
	}
*/
	LControl *useCustomSize = (LControl*)FindPaneByID( 'Cust' );
	if ( useCustomSize->GetValue() )
	{
		LGAPopup *popup = (LGAPopup *)FindPaneByID( 'HtPU' );
		image->bHeightPercent = (popup ? popup->GetValue() == kPercentOfWindowItem : false);
		image->iHeight = fHeightTextEdit->GetValue();
		
		popup = (LGAPopup *)FindPaneByID( 'WdPU' );
		image->bWidthPercent = (popup ? popup->GetValue() == kPercentOfWindowItem : false);
		image->iWidth = fWidthTextEdit->GetValue();
	}
	else
	{
		image->iWidth = 0;
		image->iHeight = 0;
		image->bWidthPercent = false;
		image->bHeightPercent = false;
	}
	
	image->iHSpace = fLeftRightBorderTextEdit->GetValue();
	image->iVSpace = fTopBottomBorderTextEdit->GetValue();
	if ( mBorderUnspecified )
		image->iBorder = -1;
	else
		image->iBorder = fSolidBorderTextEdit->GetValue();

	// Mac "remove image map" button now removes ISMAP instead of USEMAP
	// Stupid behavior, but that's what Win and UNIX do.
	if ( fLooseImageMap && image->bIsMap )
	{
		image->bIsMap = false;
	}
	
	return image;
}

void CEDImageContain::PrefsFromControls()
{
	// if background image--do it and return
	if ( fBackgroundImageCheck->GetValue() )
	{
		EDT_PageData *pageData = EDT_GetPageData( fContext );
		if (pageData == NULL)
			return;
		
		if (pageData->pBackgroundImage)
		{
			XP_FREE( pageData->pBackgroundImage );
			pageData->pBackgroundImage = NULL;
		}
		
		char *url = fImageFileName->GetLongDescriptor();
		if ( url && XP_STRLEN( url ) )
			pageData->pBackgroundImage = url;
		else if ( url )
			XP_FREE( url );
		
		EDT_SetPageData(fContext, pageData);
		EDT_FreePageData(pageData);
		
		return;
	}

	EDT_ImageData *image = ImageDataFromControls();
	if (image)
	{
		if (fInsert)
			EDT_InsertImage( fContext, image, fCopyImageCheck->GetValue() );
		else
			EDT_SetImageData( fContext, image, fCopyImageCheck->GetValue() );
		
		EDT_FreeImageData(image);
	}
}


// Initialize from preferences
void CEDImageContain::ControlsFromPref()
{
	SetTextTraitsIDByCsid(fImageAltTextEdit, GetWinCSID());
	fCopyImageCheck->Enable();
	fCopyImageCheck->SetValue( false );
	
	fOriginalWidth = 0;
    fOriginalHeight = 0;
	
	fEditImageButton->Disable();

	Boolean amInsertingNewImage = ( EDT_GetCurrentElementType(fContext) != ED_ELEMENT_IMAGE );

	EDT_ImageData *image = ( amInsertingNewImage ) ? NULL : EDT_GetImageData( fContext );
	
	// set alignment
	int value = 4;		// default == ED_ALIGN_BASELINE
	if ( image )
	{
		switch (image->align)
		{			// what about ED_ALIGN_BOTTOM & ED_ALIGN_ABSTOP
			case ED_ALIGN_TOP:			value = 1;	break;
			case ED_ALIGN_ABSCENTER:	value = 2;	break;
			case ED_ALIGN_CENTER:		value = 3;	break;
			case ED_ALIGN_BASELINE:		value = 4;	break;
			case ED_ALIGN_ABSBOTTOM: 	value = 5;	break;
			case ED_ALIGN_LEFT:			value = 6;	break;
			case ED_ALIGN_RIGHT:		value = 7;	break;
			default:	break;
		}
	}
	mImageAlignmentPopup->SetValue( value );
	
	if ( image )
	{
		fCopyImageCheck->SetValue( image->bNoSave );
		
		if (image->pSrc)
			fImageFileName->SetLongDescriptor( image->pSrc );
		
		if ( image->pLowSrc )
			fImageAltFileName->SetLongDescriptor( image->pLowSrc );
		
		if ( image->pAlt )
			fImageAltTextEdit->SetLongDescriptor( image->pAlt );

		LControl *customSize = (LControl *)FindPaneByID( 'Cust' );
		
		if ( image->iOriginalWidth == 0 )
		{
			// we know for sure we have custom if either h or w is %
			customSize->SetValue( image->bHeightPercent || image->bWidthPercent );
			fOriginalWidth = image->iWidth;
			fOriginalHeight = image->iHeight;
		}
		else
		{
			if ( image->iWidth == 0 && image->iHeight == 0 )
			{
				image->iHeight = image->iOriginalHeight;
				image->iWidth = image->iOriginalWidth;
			}
			
			customSize->SetValue( image->iOriginalWidth != image->iWidth 
								|| image->iOriginalHeight != image->iHeight);
			fOriginalWidth = image->iOriginalWidth;        /* Width and Height we got on initial loading */
			fOriginalHeight = image->iOriginalHeight;
		}

		fHeightTextEdit->SetValue(image->iHeight);
		fWidthTextEdit->SetValue(image->iWidth);

		fLeftRightBorderTextEdit->SetValue(image->iHSpace);
		fTopBottomBorderTextEdit->SetValue(image->iVSpace);

		// This is weird. If the cross platform code gives us a -1, then we are "default"
		// "default" is zero if there is no link or two if there is.
		// why doesn't the XP set this up for us? I don't know...
		if (image->iBorder == -1)
		{
			mBorderUnspecified = true;
			image->iBorder = EDT_GetDefaultBorderWidth( fContext );
		}
		else
			mBorderUnspecified = false;
		fSolidBorderTextEdit->SetValue( image->iBorder );
		
		pExtra = image->pExtra;	// let's not realloc
		image->pExtra = NULL;	// don't dispose of this!
	}
	
	// assume "Original Size" radio button is default
	LGAPopup *popup = (LGAPopup *)FindPaneByID( 'HtPU' );
	if ( popup )
		popup->SetValue( image && image->bHeightPercent ? kPercentOfWindowItem : kPixelsItem );
	popup = (LGAPopup *)FindPaneByID( 'WdPU' );
	if ( popup )
		popup->SetValue( image && image->bWidthPercent ? kPercentOfWindowItem : kPixelsItem );

	// edit image logic
	if (image == NULL || (image->pSrc == NULL || XP_STRLEN(image->pSrc) < 1)) 
		fEditImageButton->Disable();
	else
		fEditImageButton->Enable();

	// image map button
	if ( image == NULL )
		fRemoveImageMapButton->Hide();
	else
	{
		// we already have image in document; don't copy to background
		fBackgroundImageCheck->Hide();
		
		if ( image->bIsMap )
			fRemoveImageMapButton->Enable();
		else
			fRemoveImageMapButton->Disable();
	}
	
	if ( image )
		EDT_FreeImageData(image);

	// select the editField for the main image file name
	SwitchTarget( fImageFileName );
	fImageFileName->SelectAll();
	
	AdjustEnable();
}

void CEDImageContain::Show()
{
	if (*fLinkName)
		fImageFileName->SetLongDescriptor( *fLinkName );
	CEditContain::Show();
}

void CEDImageContain::Hide()
{
	if ( *fLinkName )
		XP_FREE( *fLinkName );
	*fLinkName = fImageFileName->GetLongDescriptor();
	
/*	FSSpec	file;
	file = fImageFileName->GetFSSpec();
	if (file.vRefNum != 0 || file.parID != 0 || file.name[0] != 0) {
	
		*fLinkName = CFileMgr::GetURLFromFileSpec(file);
		if (*fLinkName) {
			char *temp = FE_URLToLocalName(NET_UnEscape(*fLinkName));
			if (temp) {
				XP_FREE(*fLinkName);
				*fLinkName = temp;
			}
		}
	} else {
		*fLinkName = (char *)XP_ALLOC(sizeof(char)*2);			// just fill with some filler so the link pane won't mess with us!
		if (*fLinkName) XP_STRCPY(*fLinkName, "-");
	}
*/
	CEditContain::Hide();
}

void CEDImageContain::Help()
{
	ShowHelp( HELP_PROPS_IMAGE );
}

Boolean CEDImageContain::AllFieldsOK()
{
	Str255	str;
	fImageFileName->GetDescriptor( str );
	if ( str[0] == 0 )
	{
		SwitchTarget( fImageFileName );
		return false;
	}
	
	// if it's a background image we're done!
	if ( fBackgroundImageCheck->GetValue() )
		return true;
	
	LControl *customSize = (LControl *)FindPaneByID( 'Cust' );
	if ( customSize->GetValue() )
	{
		LGAPopup *popup = (LGAPopup *)FindPaneByID( 'HtPU' );
		if ( popup && popup->GetValue() == kPercentOfWindowItem )
		{
			if ( !IsEditFieldWithinLimits( fHeightTextEdit, 1, 100 ) )
			{
				SwitchTarget( fHeightTextEdit );
				fHeightTextEdit->SelectAll();
				return FALSE;
			}
		}
		else
		{
			if ( !IsEditFieldWithinLimits( fHeightTextEdit, 1, 10000 ) )
			{
				SwitchTarget( fHeightTextEdit );
				fHeightTextEdit->SelectAll();
				return FALSE;
			}
		}
		
		popup = (LGAPopup *)FindPaneByID( 'WdPU' );
		if ( popup && popup->GetValue() == kPercentOfWindowItem )
		{
			if ( !IsEditFieldWithinLimits( fWidthTextEdit, 1, 100 ) )
			{
				SwitchTarget( fWidthTextEdit );
				fWidthTextEdit->SelectAll();
				return FALSE;
			}
		}
		else
		{
			if ( !IsEditFieldWithinLimits( fWidthTextEdit, 1, 10000 ) )
			{
				SwitchTarget( fWidthTextEdit );
				fWidthTextEdit->SelectAll();
				return FALSE;
			}
		}
	}
		
	if ( !IsEditFieldWithinLimits( fLeftRightBorderTextEdit, 0, 10000 ) )
	{
		SwitchTarget( fLeftRightBorderTextEdit );
		fLeftRightBorderTextEdit->SelectAll();
		return FALSE;
	}

	if ( !IsEditFieldWithinLimits( fTopBottomBorderTextEdit, 0, 10000 ) )
	{
		SwitchTarget( fTopBottomBorderTextEdit );
		fTopBottomBorderTextEdit->SelectAll();
		return FALSE;
	}

	if ( !IsEditFieldWithinLimits( fSolidBorderTextEdit, 0, 10000 ) )
	{
		SwitchTarget( fSolidBorderTextEdit );
		fSolidBorderTextEdit->SelectAll();
		return FALSE;
	}

	return TRUE;
}


void CEDImageContain::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch (inMessage)
	{
		case 'HtPU':
		case 'WdPU':
			// set radio button to custom size (not original anymore)
			LControl *customSize = (LControl *)FindPaneByID( 'Cust' );
			if ( customSize && customSize->GetValue() == 0 )
				customSize->SetValue( true );
			// continue below
			
		case 'Calc':
			AdjustEnable();
			break;
		
		case 'WSTk':		// Original Size Button
			LControl *origSizeButton = (LControl *)FindPaneByID( 'Orig' );
			if ( origSizeButton->GetValue() )
			{
				fHeightTextEdit->SetValue(fOriginalHeight);		// these are not set by the XP yet, apparently.
				fWidthTextEdit->SetValue(fOriginalWidth);
			}
			AdjustEnable();
			break;
		
		case 'WSTm':	// Remove Image Map Button
			fLooseImageMap = TRUE;
			fRemoveImageMapButton->Disable();
			break;

		case msg_EditField2:
			if ( ioParam == NULL )
				return;
			
			if ( ioParam == fHeightTextEdit || ioParam == fWidthTextEdit )
			{
				LControl *customSize = (LControl *)FindPaneByID( 'Cust' );
				if ( customSize && customSize->GetValue() == 0 )
					customSize->SetValue( true );
				
				// don't divide by 0!
				if ( fOriginalWidth == 0 || fOriginalHeight == 0 )
					return;
				
				if ( fImageLockedCheckBox->IsEnabled() && fImageLockedCheckBox->GetValue() )
				{
					int h, w;
					
					if ( ioParam == fHeightTextEdit )
					{
						h = fHeightTextEdit->GetValue();
						w = h * fOriginalWidth / fOriginalHeight;
						fWidthTextEdit->SetValue( w );
					}
					else
					{
						w = fWidthTextEdit->GetValue();
						h = w * fOriginalHeight / fOriginalWidth;
						fHeightTextEdit->SetValue( h );
					}
				}
			}
			else
				AdjustEnable();	// typing in a different edit field; adjust controls appropriately
			break;
		
		case 'wst1':	// "Choose File..." image
			CEditDialog::ChooseImageFile(fImageFileName);
			AdjustEnable();
			break;
		
		case 'wst3':	// "Choose File..." Alt Image
			CEditDialog::ChooseImageFile(fImageAltFileName);
			AdjustEnable();
			break;
		
		case 'WSTn':	// Edit Image Button
			char *imageURL = fImageFileName->GetLongDescriptor();
			if ( imageURL && XP_STRLEN(imageURL) < 1 )
			{
				XP_FREE( imageURL );
				imageURL = NULL;
			}
			
			if ( imageURL == NULL )
				break;
			
			char *absURL = NET_MakeAbsoluteURL( LO_GetBaseURL( fContext ), imageURL );
			XP_FREE(imageURL);
			
			Boolean isImageLocal = false;
    		if (absURL && XP_STRSTR(absURL, "file://") == absURL)
    		{
				FSSpec	theImage;
				
    			if (CFileMgr::FSSpecFromLocalUnixPath(absURL + XP_STRLEN("file://"), &theImage, FALSE) == noErr)
    			{				// Skip file://
					isImageLocal = true;

				    // Get the FSSpec for the editor
				    FSSpec	theApplication;
				    XP_Bool hasEditor = false;
					PREF_GetBoolPref( "editor.use_image_editor", &hasEditor );
					if ( hasEditor )
					    theApplication = CPrefs::GetFolderSpec(CPrefs::ImageEditor);
				    
				    // Oops, the user has not picked an app in the preferences yet.
					if ( !hasEditor || (theApplication.vRefNum == -1 && theApplication.parID == -1 ) )
					{
						ErrorManager::PlainAlert( NO_IMG_EDITOR_PREF_SET );
						CPrefsDialog::EditPrefs(	CPrefsDialog::eExpandEditor, 
								PrefPaneID::eEditor_Main, CPrefsDialog::eIgnore);
					}
					else if ( StartDocInApp(theImage, theApplication) == noErr )
						;
			    }
		    }
		    
		    if ( !isImageLocal )
				ErrorManager::PlainAlert( EDITOR_ERROR_EDIT_REMOTE_IMAGE );
		    
			XP_FREEIF(absURL);
			break;
		
		case 'WSTj':
			mBorderUnspecified = false;
			break;
		
		case 'Xtra':
			char * newExtraHTML = NULL;
			Boolean canceled = !GetExtraHTML( pExtra, &newExtraHTML  ,GetWinCSID() );
			if (!canceled)
			{
				if ( pExtra )
					XP_FREE( pExtra );
				pExtra = newExtraHTML;
			}
			break;
	}
}

/**********************************************************/
#pragma mark -
#pragma mark Document Properties

void CEDDocPropGeneralContain::FinishCreateSelf()
{
	fLocation = (CLargeEditField *)FindPaneByID( 'Loca' );
	fTitle = (CLargeEditField *)FindPaneByID( 'Titl' );
	fAuthor = (CLargeEditField *)FindPaneByID( 'Auth' );
	fDescription = (CLargeEditField *)FindPaneByID( 'Desc' );
	fKeywords = (CLargeEditField *)FindPaneByID( 'Keyw' );
	fClassification = (CLargeEditField *)FindPaneByID( 'Clas' );
	
	UReanimator::LinkListenerToControls( this, this, mPaneID );
}


void CEDDocPropGeneralContain::AddMeta(char *Name, CLargeEditField* value)
{
    EDT_MetaData *metaData = EDT_NewMetaData();			// it seems like we could 
    if (metaData == NULL)
    	return;
    
	metaData->bHttpEquiv = FALSE;
	metaData->pName = Name;
    metaData->pContent = value->GetLongDescriptor();
    
	/* if we try to SetMetaData with an empty string for pContent, then "(null)" is used instead; which is ugly. */
	
	if ( metaData->pContent == NULL || XP_STRLEN(metaData->pContent) == 0 )
    	EDT_DeleteMetaData( fContext, metaData ); 			// hopefully it won't be a problem if this doesn't exist already.
    else
     	EDT_SetMetaData( fContext, metaData ); 
   
	metaData->pName = NULL;			// don't free this, please
	
    EDT_FreeMetaData( metaData );
}


void CEDDocPropGeneralContain::PrefsFromControls()
{
	EDT_PageData *pageData = EDT_GetPageData(fContext);
	if ( pageData )
	{
		if ( pageData->pTitle )
			XP_FREE( pageData->pTitle );
		pageData->pTitle = fTitle->GetLongDescriptor();

		EDT_SetPageData(fContext, pageData);
		EDT_FreePageData(pageData);
	}
	
	AddMeta("Author", fAuthor);
	AddMeta("Description", fDescription);
	AddMeta("KeyWords", fKeywords);
	AddMeta("Classification", fClassification);
}


// Initialize from preferences
void CEDDocPropGeneralContain::ControlsFromPref()
{
	int16 win_csid = GetWinCSID();
	SetTextTraitsIDByCsid(fTitle, win_csid);
	SetTextTraitsIDByCsid(fAuthor, win_csid);
	SetTextTraitsIDByCsid(fDescription, win_csid);
	SetTextTraitsIDByCsid(fKeywords, win_csid);
	SetTextTraitsIDByCsid(fClassification, win_csid);
	History_entry * hist_ent = SHIST_GetCurrent( &fContext->hist );
	if ( hist_ent && hist_ent->address )
		fLocation->SetLongDescriptor( hist_ent->address );
		
	EDT_PageData *pageData = EDT_GetPageData( fContext );
	if ( pageData )
	{
		if ( pageData->pTitle )
			fTitle->SetLongDescriptor( pageData->pTitle );
		EDT_FreePageData( pageData );
	}
   
   
    // Get data from meta tags:

    int count = EDT_MetaDataCount(fContext);
    for ( int i = 0; i < count; i++ )
    {
	    EDT_MetaData* pData = EDT_GetMetaData(fContext, i);

        if ( !pData->bHttpEquiv )
        {
			if (strcasecomp(pData->pName, "Author") == 0)
				fAuthor->SetLongDescriptor( pData->pContent );
			else if (strcasecomp(pData->pName, "Description") == 0)
				fDescription->SetLongDescriptor( pData->pContent );
			else if (strcasecomp(pData->pName, "KeyWords") == 0)
				fKeywords->SetLongDescriptor( pData->pContent );
			else if (strcasecomp(pData->pName, "Classification") == 0)
				fClassification->SetLongDescriptor( pData->pContent );
        }

		EDT_FreeMetaData( pData );
    }
}


void CEDDocPropGeneralContain::Help()
{
	ShowHelp( HELP_DOC_PROPS_GENERAL );
}


static void AddNewColorData( XP_List *pSchemeData, int r1, int g1, int b1, int r2, int g2, int b2, int r3, int g3, int b3, int r4, int g4, int b4, int r5, int g5, int b5 );
static void AddNewColorData( XP_List *pSchemeData, int r1, int g1, int b1, int r2, int g2, int b2, int r3, int g3, int b3, int r4, int g4, int b4, int r5, int g5, int b5 )
{
	EDT_ColorSchemeData * pColorData = XP_NEW( EDT_ColorSchemeData );
	if ( !pColorData )
        return;
    
	memset(pColorData, 0, sizeof(EDT_ColorSchemeData));
    XP_ListAddObjectToEnd( pSchemeData, pColorData );
    
    pColorData->ColorText.red = r1;
    pColorData->ColorText.green = g1;
    pColorData->ColorText.blue = b1;
    
    pColorData->ColorLink.red = r3;
    pColorData->ColorLink.green = g3;
    pColorData->ColorLink.blue = b3;
    
    pColorData->ColorActiveLink.red = r5;
    pColorData->ColorActiveLink.green = g5;
    pColorData->ColorActiveLink.blue = b5;
    
    pColorData->ColorFollowedLink.red = r4;
    pColorData->ColorFollowedLink.green = g4;
    pColorData->ColorFollowedLink.blue = b4;
    
    pColorData->ColorBackground.red = r2;
    pColorData->ColorBackground.green = g2;
    pColorData->ColorBackground.blue = b2;
}


void AppearanceContain::FinishCreateSelf()
{
	fCustomColor = (LControl*)FindPaneByID( 'Cust' );
	fBrowserColor = (LControl*)FindPaneByID( 'Brow' );

	fColorScheme = (LControl*)FindPaneByID( 'Sche' );

	fExampleView = (CChameleonView*)FindPaneByID( 'Exam' );
	fNormalText = (CColorButton*)FindPaneByID( 'Norm' );
	fLinkedText = (CColorButton*)FindPaneByID( 'Link' );
	fActiveLinkedText = (CColorButton*)FindPaneByID( 'Acti' );
	fFollowedLinkedText = (CColorButton*)FindPaneByID( 'Foll' );

	fExampleNormalText = (CChameleonCaption*)FindPaneByID( 'norm' );
	fExampleLinkedTex = (CChameleonCaption*)FindPaneByID( 'link' );
	fExampleActiveLinkedText = (CChameleonCaption*)FindPaneByID( 'actv' );
	fExampleFollowedLinkedText = (CChameleonCaption*)FindPaneByID( 'fllw' );
	
	fSolidColor = (CColorButton*)FindPaneByID( 'Choo' );
	
	fImageFile = (LControl*)FindPaneByID( 'Imag' );
	fImageFileName = (CLargeEditField *)FindPaneByID( 'bthr' );
	
	UReanimator::LinkListenerToControls( this, this, mPaneID );

	LO_Color colorBackground = UGraphics::MakeLOColor( CPrefs::GetColor( CPrefs::WindowBkgnd ) );
	LO_Color colorText = UGraphics::MakeLOColor( CPrefs::GetColor( CPrefs::Black ) );
	LO_Color colorLink = UGraphics::MakeLOColor( CPrefs::GetColor( CPrefs::Blue ) );
	LO_Color colorActiveLink = UGraphics::MakeLOColor( CPrefs::GetColor( CPrefs::Blue ) );				// this must be the same as colorLink ?!
	LO_Color colorFollowedLink = UGraphics::MakeLOColor( CPrefs::GetColor( CPrefs::Magenta ) );

    fSchemeData = XP_ListNew();
	AddNewColorData( fSchemeData,	
				colorText.red, colorText.green, colorText.blue,
				colorBackground.red, colorBackground.green, colorBackground.blue,
				colorLink.red, colorLink.green, colorLink.blue,
				colorFollowedLink.red, colorFollowedLink.green, colorFollowedLink.blue,
				colorActiveLink.red, colorActiveLink.green, colorActiveLink.blue);
	AddNewColorData( fSchemeData,	0,0,0,			255,240,240,	255,0,0,		128,0,128,		0,0,255 );
	AddNewColorData( fSchemeData,	0,0,0,			255,255,192,	0,0,255,		128,0,128,		255,0,255 );
	AddNewColorData( fSchemeData,	64,0,64,		255,255,128,	0,0,255,		0,128,0,		255,0,128 );
	AddNewColorData( fSchemeData,	0,0,0,			192,192,255,	0,0,255,		128,0,128,		255,0,128 );
	AddNewColorData( fSchemeData,	0,0,0,			128,128,192,	255,255,255,	128,0,128,		255,255,0 );
	AddNewColorData( fSchemeData,	0,0,128,		255,192,64,		0,0,255,		0,128,0,		0,255,255 );
	AddNewColorData( fSchemeData,	255,255,255,	0,0,0,			255,255,0,		192,192,192,	192,255,192 );
	AddNewColorData( fSchemeData,	255,255,255,	0,64,0,			255,255,0,		128,255,128,	0,255,64 );
	AddNewColorData( fSchemeData,	255,255,255,	0,0,128,		255,255,0,		128,128,255,	255,0,255 );
	AddNewColorData( fSchemeData,	255,255,255,	128,0,128,		0,255,255,		128,255,255,	0,255,0 );
	
	fColorScheme->SetValue(2);			// Netscape default
	ListenToMessage('Sche', NULL);			
}


void AppearanceContain::UpdateTheWholeDamnDialogBox()
{
	RGBColor color;
	RGBColor backColor;
	
	backColor = fSolidColor->GetColor();			// what about the default??
	fExampleView->SetColor( backColor );

	color = fNormalText->GetColor();
	fExampleNormalText->SetColor( color, backColor );
	
	color = fLinkedText->GetColor();
	fExampleLinkedTex->SetColor( color, backColor );
	
	color = fActiveLinkedText->GetColor();
	fExampleActiveLinkedText->SetColor( color, backColor );
	
	color = fFollowedLinkedText->GetColor();
	fExampleFollowedLinkedText->SetColor( color, backColor );
		
	fExampleView->Refresh();
	fNormalText->Refresh();
	fLinkedText->Refresh();
	fActiveLinkedText->Refresh();
	fFollowedLinkedText->Refresh();
	fSolidColor->Refresh();	
	
	if (fBrowserColor->GetValue())
	{
		fColorScheme->Disable();
		fExampleView->Disable();
		fNormalText->Disable();
		fLinkedText->Disable();
		fActiveLinkedText->Disable();
		fFollowedLinkedText->Disable();
		fSolidColor->Disable();
	}
	else
	{
		fColorScheme->Enable();
		fExampleView->Enable();
		fNormalText->Enable();
		fLinkedText->Enable();
		fActiveLinkedText->Enable();
		fFollowedLinkedText->Enable();
		fSolidColor->Enable();
	}
}


void AppearanceContain::ListenToMessage( MessageT inMessage, void* /* ioParam */ )
{
	switch (inMessage)
	{
		case 'Norm':
		case 'Link':
		case 'Acti':
		case 'Foll':
		case 'Choo':
			fColorScheme->SetValue(1);			// custom
			ListenToMessage('Sche', NULL);			
		case 'Cust':
		case 'Brow':
			UpdateTheWholeDamnDialogBox();
			break;
		
		
		case 'Imag':
			if (fImageFile->GetValue() == 1)
			{				// we are trying to set the image
				CStr255 url;
				fImageFileName->GetDescriptor(url);
				if (url == CStr255::sEmptyString)
				{		// but it doesn't exist
					CEditDialog::ChooseImageFile( fImageFileName );		// so try to get it
					fImageFileName->GetDescriptor(url);
					if (url == CStr255::sEmptyString) 	// but, if we were unsuccessful
						fImageFile->SetValue(0);				// revert back.
				}
			}
			break;
		
		case 'wst1':
			CEditDialog::ChooseImageFile( fImageFileName );	// try to get it
			CStr255 url;
			fImageFileName->GetDescriptor(url);
			if (url == CStr255::sEmptyString) 	// if we were unsuccessful
				fImageFile->SetValue(0);				// don't try to use
			else
				fImageFile->SetValue(1);				// ok
			break;
				
		case 'Sche':
			int scheme = fColorScheme->GetValue();
			scheme--;

			if (scheme)
			{
				EDT_ColorSchemeData * pColorData = (EDT_ColorSchemeData *)XP_ListGetObjectNum(fSchemeData, scheme);
				if (pColorData)
				{
					fSolidColor->			SetColor(UGraphics::MakeRGBColor(pColorData->ColorBackground));
					fNormalText->			SetColor(UGraphics::MakeRGBColor(pColorData->ColorText));
					fLinkedText->			SetColor(UGraphics::MakeRGBColor(pColorData->ColorLink));
					fActiveLinkedText->		SetColor(UGraphics::MakeRGBColor(pColorData->ColorActiveLink));
					fFollowedLinkedText->	SetColor(UGraphics::MakeRGBColor(pColorData->ColorFollowedLink));
				}
				
				fImageFile->SetValue(0);				// no background
				UpdateTheWholeDamnDialogBox();
			}
		break;
		
	}
}


CEDDocPropAppearanceContain::~CEDDocPropAppearanceContain()
{
    if ( fSchemeData )
    {
        EDT_ColorSchemeData *pColorData;
	    XP_List * list_ptr = fSchemeData;
	    pColorData = (EDT_ColorSchemeData *)XP_ListNextObject(list_ptr);
        while ( pColorData )
        {
        	XP_FREE(pColorData);
	   		pColorData = (EDT_ColorSchemeData *)XP_ListNextObject(list_ptr);
        }
        XP_ListDestroy(fSchemeData);
//        XP_FREE(fSchemeData);		already done by XP_ListDestroy()!!
    }
}

void CEDDocPropAppearanceContain::PrefsFromControls()
{
	LControl *prefCheckBox = (LControl *)FindPaneByID( 'Dflt' );
	if ( prefCheckBox && prefCheckBox->GetValue() && prefCheckBox->IsVisible() )
	{
		PREF_SetBoolPref( "editor.use_custom_colors", fCustomColor->GetValue() );
		if ( fCustomColor->GetValue() )
		{
			LO_Color colorBackground =		UGraphics::MakeLOColor(fSolidColor->GetColor());
			LO_Color colorText =			UGraphics::MakeLOColor(fNormalText->GetColor());
			LO_Color colorLink =			UGraphics::MakeLOColor(fLinkedText->GetColor());
			LO_Color colorActiveLink =		UGraphics::MakeLOColor(fActiveLinkedText->GetColor());
			LO_Color colorFollowedLink =	UGraphics::MakeLOColor(fFollowedLinkedText->GetColor());

			PREF_SetColorPref( "editor.background_color",
								colorBackground.red,
								colorBackground.green,
								colorBackground.blue );
			PREF_SetColorPref( "editor.text_color",
								colorText.red,
								colorText.green,
								colorText.blue );
			PREF_SetColorPref( "editor.link_color",
								colorLink.red,
								colorLink.green,
								colorLink.blue );
			PREF_SetColorPref( "editor.active_link_color",
								colorActiveLink.red,
								colorActiveLink.green,
								colorActiveLink.blue );
			PREF_SetColorPref( "editor.followed_link_color",
								colorFollowedLink.red,
								colorFollowedLink.green,
								colorFollowedLink.blue );
		}

		PREF_SetBoolPref( "editor.use_background_image", fImageFile->GetValue());
		if (fImageFile->GetValue())
		{
			char *url = fImageFileName->GetLongDescriptor();
			if ( url && XP_STRLEN( url ) )
				PREF_SetCharPref("editor.background_image", url);
			XP_FREEIF( url );
		}
	}
	
	EDT_PageData *pageData = EDT_GetPageData( fContext );
	if (pageData == NULL)
		return;
	
	if (pageData->pColorBackground) XP_FREE(pageData->pColorBackground);	// don't care about the old values...
	if (pageData->pColorText) XP_FREE(pageData->pColorText);
	if (pageData->pColorLink) XP_FREE(pageData->pColorLink);
	if (pageData->pColorActiveLink) XP_FREE(pageData->pColorActiveLink);
	if (pageData->pColorFollowedLink) XP_FREE(pageData->pColorFollowedLink);
	if (pageData->pBackgroundImage)
	{
		XP_FREE( pageData->pBackgroundImage );
		pageData->pBackgroundImage = NULL;
	}
	
	if ( fImageFile->GetValue() )
	{
		char *url = fImageFileName->GetLongDescriptor();
		if ( url && XP_STRLEN( url ) )
			pageData->pBackgroundImage = url;
		else if ( url )
			XP_FREE( url );
	}
	 		
	if (fBrowserColor->GetValue())
	{
		pageData->pColorBackground = NULL;
		pageData->pColorText = NULL;
		pageData->pColorLink = NULL;
		pageData->pColorActiveLink = NULL;
		pageData->pColorFollowedLink = NULL;
	}
	else
	{
		LO_Color colorBackground =		UGraphics::MakeLOColor(fSolidColor->GetColor());
		LO_Color colorText =			UGraphics::MakeLOColor(fNormalText->GetColor());
		LO_Color colorLink =			UGraphics::MakeLOColor(fLinkedText->GetColor());
		LO_Color colorActiveLink =		UGraphics::MakeLOColor(fActiveLinkedText->GetColor());
		LO_Color colorFollowedLink =	UGraphics::MakeLOColor(fFollowedLinkedText->GetColor());
		
		pageData->pColorBackground =	&colorBackground;
		pageData->pColorText =			&colorText;
		pageData->pColorLink =			&colorLink;
		pageData->pColorActiveLink =	&colorActiveLink;
		pageData->pColorFollowedLink =	&colorFollowedLink;
	}
	
	EDT_SetPageData( fContext, pageData );
	
	pageData->pColorBackground = NULL;		// of course, we don't want to FREE these...
	pageData->pColorText = NULL;
	pageData->pColorLink = NULL;
	pageData->pColorActiveLink = NULL;
	pageData->pColorFollowedLink = NULL;
	
	EDT_FreePageData( pageData );
}


static
inline
bool operator==( const LO_Color& lhs, const LO_Color& rhs )
{
	return lhs.red   == rhs.red
		&& lhs.green == rhs.green
		&& lhs.blue  == rhs.blue;
}


void CEDDocPropAppearanceContain::ControlsFromPref()
{
	EDT_PageData *pageData = EDT_GetPageData( fContext );
	if (pageData == NULL)
		return;
	
	if (pageData->pColorBackground
		|| pageData->pColorLink
		|| pageData->pColorText
		|| pageData->pColorFollowedLink
		|| pageData->pColorActiveLink)
	{
		TurnOn(fCustomColor);
		fColorScheme->SetValue(1);			// switch to custom by default
		
		int scheme;
		for ( scheme = 0; scheme < fColorScheme->GetMaxValue(); scheme++ )
		{
			EDT_ColorSchemeData * pColorData = (EDT_ColorSchemeData *)XP_ListGetObjectNum( fSchemeData, scheme );
			if ( pColorData && (pColorData->ColorBackground == *(pageData->pColorBackground))
							&& (pColorData->ColorText == *(pageData->pColorText))
							&& (pColorData->ColorLink == *(pageData->pColorLink))
							&& (pColorData->ColorActiveLink == *(pageData->pColorActiveLink))
							&& (pColorData->ColorFollowedLink == *(pageData->pColorFollowedLink)) )
			{
				fColorScheme->SetValue( scheme + 1 );
				break;
			}
		}
			
	}
	else
	{
		TurnOn(fBrowserColor);
	}

	fImageFile->SetValue(0);
	if (pageData->pBackgroundImage && XP_STRLEN(pageData->pBackgroundImage) > 0)
	{
		fImageFileName->SetLongDescriptor( pageData->pBackgroundImage );
		fImageFile->SetValue(1);
	}
	
	if (pageData->pColorBackground)
		fSolidColor->SetColor(UGraphics::MakeRGBColor(*pageData->pColorBackground));
	if (pageData->pColorText)
		fNormalText->SetColor(UGraphics::MakeRGBColor(*pageData->pColorText));
	if (pageData->pColorLink)
		fLinkedText->SetColor(UGraphics::MakeRGBColor(*pageData->pColorLink));
	if (pageData->pColorActiveLink)
		fActiveLinkedText->SetColor(UGraphics::MakeRGBColor(*pageData->pColorActiveLink));
	if (pageData->pColorFollowedLink)
		fFollowedLinkedText->SetColor(UGraphics::MakeRGBColor(*pageData->pColorFollowedLink));
	
	EDT_FreePageData( pageData );
	UpdateTheWholeDamnDialogBox();
}


void CEDDocPropAppearanceContain::Help()
{
	ShowHelp( HELP_DOC_PROPS_APPEARANCE );
}


/* TO DO:
Well, this is somewhat of a mess because:
1) we store the name and value together separated only by a '='. Easily screwed up if one or the other contains a '='
2) We don't remove spaces or any other white space.
3) we don't handle quotes at all. Shouldn't we do something about escaping '"' chanacters?
4) The UI needs work.
5) could use stack space for the majority of small META tags and only resort to malloc in exceptional cases
6) when the page is reloaded, we re-blast in the meta values starting from cell number 1 without first clearing the table
7) no UI for user who tries to insert an "Author" meta tag, etc.
8) not too sure what we are supposed to do about case. layout uses case insensitive tags. EDT_GetMetaData() uses case sensitive tags.
*/

CEDDocPropAdvancedContain::~CEDDocPropAdvancedContain()
{
	XP_FREEIF( fbuffer );
}

void CEDDocPropAdvancedContain::FinishCreateSelf()
{
	fbufferlen = 1000;
	fbuffer = (char *)malloc( fbufferlen );					// we'll keep this fbuffer for temp use for various things as long as dlog is around.
	if ( fbuffer == NULL )
		fbufferlen = 0;
	
	fSystemVariables = (OneRowLListBox*)FindPaneByID( 'Syst' );
	fUserVariables = (OneRowLListBox*)FindPaneByID( 'User' );
	
	fName = (CLargeEditField *)FindPaneByID( 'Name' );
	fValue = (CLargeEditField *)FindPaneByID( 'Valu' );
	
	UReanimator::LinkListenerToControls( this, this, mPaneID );
}

void CEDDocPropAdvancedContain::PrefsFromControls()
{
	if (fbuffer == NULL)
		return;	// the fbuffer is now as big or bigger (it grows) than all MetaData and all cell entries in the table. Don't worry about the size anymore.
	
	EDT_MetaData* pData;
	int16 i, count;
	
	// First, clear METAs all out because we are going to rebuild the lists from scratch.
    count = EDT_MetaDataCount(fContext);
    for (i = count - 1; i >= 0; i-- )
    {
       	pData = EDT_GetMetaData(fContext, i);

		if (strcasecomp(pData->pName, "Author") && 					// Skip the fields used in General Info page
			strcasecomp(pData->pName, "Description") &&
			strcasecomp(pData->pName, "Generator") &&
			strcasecomp(pData->pName, "Last-Modified") &&
			strcasecomp(pData->pName, "Created") &&
			strcasecomp(pData->pName, "Classification") &&
			strcasecomp(pData->pName, "Keywords"))
			EDT_DeleteMetaData( fContext, pData );					// remove all the other ones.
			
		EDT_FreeMetaData( pData );
	}
	
	pData = EDT_NewMetaData();
	
	
	// Do the system METAs first, bHttpEquiv = FALSE;
	pData->bHttpEquiv = FALSE;
	count = fSystemVariables->GetRows();
	for (i = 0; i < count; i++)
	{
		int16 len = fbufferlen;
		fSystemVariables->GetCell(i, fbuffer, &len);
		fbuffer[len] = '\0';
		
		pData->pName = fbuffer;
		
		char *equal = strchr(fbuffer, '=');		// look for the '='. Of course this screws up if the stupid user put an '=' in the TEditField
		
		if (equal)
		{
			*equal++ = '\0';
			pData->pContent = equal;
			EDT_SetMetaData( fContext, pData );
		}
		else
		{
			pData->pContent = NULL;								// I should check if this is valid....
			EDT_DeleteMetaData( fContext, pData );				// you might wonder why I'm calling EDT_DeleteMetaData() if all Tags are supposed to be gone.... Well, what if a user wanted to delete the "Author" tag here by defining it to be NULL?!
		}
	}
	
	// Do the user METAs next, bHttpEquiv = TRUE;
	pData->bHttpEquiv = TRUE;
	count = fUserVariables->GetRows();
	for (i = 0; i < count; i++)
	{
		int16 len = fbufferlen;
		fUserVariables->GetCell(i, fbuffer, &len);
		fbuffer[len] = '\0';
		
		pData->pName = fbuffer;
		
		char *equal = strchr(fbuffer, '=');
		
		if (equal)
		{
			*equal++ = '\0';
			pData->pContent = equal;
		}
		else
			pData->pContent = NULL;								// I should check if this is valid....
			
		EDT_SetMetaData( fContext, pData );
	}
	
	pData->pName = NULL;			// we don't want EDT_FreeMetaData() to free our fbuffer for us.
	pData->pContent = NULL;			// we don't want EDT_FreeMetaData() to free our fbuffer for us.

	EDT_FreeMetaData(pData);	
}


void CEDDocPropAdvancedContain::ControlsFromPref()
{
	int16 sysRowNum = fSystemVariables->GetRows();			// add to the bottom? shouldn't we clear the tables first and skip this?
	int16 usrRowNum = fUserVariables->GetRows();			// add to the bottom? shouldn't we clear the tables first and skip this?
	
	sysRowNum = 1;			// forget what is in the table now. We'll start from the beginning.
	usrRowNum = 1;

	int newlength;
    int count = EDT_MetaDataCount(fContext);
    for ( int i = 0; i < count; i++ )
    {
        EDT_MetaData* pData = EDT_GetMetaData(fContext, i);
        
        newlength = XP_STRLEN(pData->pName) + 1 + XP_STRLEN(pData->pContent) + 1;
        if ( fbuffer && newlength > fbufferlen )
        {		// grow fbuffer if necessary
			fbuffer = (char *) XP_REALLOC(&fbuffer, fbufferlen);
			if ( fbuffer )
				fbufferlen = newlength;
			else
				fbufferlen = 0;
		}
        
        if ( fbuffer )
        {
        	sprintf(fbuffer, "%s=%s", pData->pName, pData->pContent);

	        if ( pData->bHttpEquiv )
	        {			// which table do we put it in?
				fSystemVariables->AddRow( sysRowNum++, fbuffer, XP_STRLEN(fbuffer) );
	        }
	        else if (strcasecomp(pData->pName, "Author") && 					// Skip the fields used in General Info page
				strcasecomp(pData->pName, "Description") &&
				strcasecomp(pData->pName, "Generator") &&
				strcasecomp(pData->pName, "Last-Modified") &&
				strcasecomp(pData->pName, "Created") &&
				strcasecomp(pData->pName, "Classification") &&
				strcasecomp(pData->pName, "Keywords"))
			{
	            // TODO: PUT META STRINGS IN RESOURCES?
				fUserVariables->AddRow( usrRowNum++, fbuffer, XP_STRLEN(fbuffer) );
	        }
		}
		
		EDT_FreeMetaData( pData );
    }

	fSystemVariables->FocusDraw();
	fSystemVariables->SetDoubleClickMessage( msg_DblClickOnTarget );
	fSystemVariables->SetSingleClickMessage(msg_ClickOnTarget);
	fSystemVariables->AddListener( this );	
	fSystemVariables->SetValue(-1);

	fUserVariables->FocusDraw();
	fUserVariables->SetDoubleClickMessage( msg_DblClickOnTarget2 );
	fUserVariables->SetSingleClickMessage( msg_ClickOnTarget2 );
	fUserVariables->AddListener( this );	
	fUserVariables->SetValue(-1);
}

void CEDDocPropAdvancedContain::PutStringsInBuffer()
{
	if (fbuffer == NULL)
		return;
	
	char *name = fName->GetLongDescriptor();
	if ( name == NULL )
		return;
	
	char *value = fValue->GetLongDescriptor();
	if ( value == NULL )
	{
		XP_FREE( name );
		return;
	}
	
    if ( XP_STRLEN(name) + 1 + XP_STRLEN(value) + 1 > fbufferlen )
    {		// grow the fbuffer if necessary
    	fbuffer = (char *) XP_REALLOC(&fbuffer, fbufferlen);
    	if ( fbuffer )
    		fbufferlen = XP_STRLEN( name ) + 1 + XP_STRLEN( value ) + 1;
    	else
    		fbufferlen = 0;
    }
	
	if ( fbuffer )
		sprintf(fbuffer, "%s=%s", name, value);
	
	XP_FREE( name );
	XP_FREE( value );
}

Boolean CEDDocPropAdvancedContain::BufferUnique()
{
	if (fbuffer == NULL) return false;
	
	if (!fSystemVariables->IsTarget() && !fUserVariables->IsTarget())
		return false;			// we compare against the target: at least one must be target
	
	OneRowLListBox* lBox;
	if (fSystemVariables->IsTarget())
		lBox = fSystemVariables;
	else
		lBox = fUserVariables;
	
	int16 count = lBox->GetRows();
	char *tempbuff = (char *)malloc(fbufferlen);			// we need to copy the strings out of the table one at a time to compare with them. don't copy more than bufflen since they wouldn't be equal then anyway.
	if (tempbuff == NULL)
		return false;
	
	int16 i;
	for (i = 0; i < count; i++) {
		int16 len = fbufferlen;
		lBox->GetCell(i, tempbuff, &len);
		tempbuff[len] = '\0';
		if (XP_STRCMP(tempbuff, fbuffer) == NULL)
		{				// we should probably do a case insensitive compare on the Name and a case sensitive compare on the Value. What a pain.
			XP_FREE(tempbuff);
			return false;
		}
	}
	
	XP_FREE(tempbuff);
	return true;		// no matches; must be unique
}


void CEDDocPropAdvancedContain::Help()
{
	ShowHelp( HELP_DOC_PROPS_ADVANCED );
}


void CEDDocPropAdvancedContain::ListenToMessage( MessageT inMessage, void* /* ioParam */ )
{
	switch (inMessage) {
		case msg_Doc_Advanced_Prop_New_Tag:
			PutStringsInBuffer();
			if (BufferUnique()) {
				if (fSystemVariables->IsTarget())
					fSystemVariables->AddRow(fSystemVariables->GetRows(),fbuffer,strlen(fbuffer));
				
				if (fUserVariables->IsTarget())
					fUserVariables->AddRow(fUserVariables->GetRows(),fbuffer,strlen(fbuffer));
			}			
			break;

		case msg_Doc_Advanced_Prop_Set_Tag:
			PutStringsInBuffer();
			if (BufferUnique()) {
			
				if (fSystemVariables->IsTarget() && fSystemVariables->GetValue() != -1)
						fSystemVariables->SetCell(fSystemVariables->GetValue(),fbuffer,strlen(fbuffer));
				
				if (fUserVariables->IsTarget() && fUserVariables->GetValue() != -1)
						fUserVariables->SetCell(fUserVariables->GetValue(),fbuffer,strlen(fbuffer));
				
			}			
			break;

		case msg_Doc_Advanced_Prop_Delete_Tag:
			if (fSystemVariables->IsTarget() && fSystemVariables->GetValue() != -1)
				fSystemVariables->RemoveRow(fSystemVariables->GetValue());
			
			if (fUserVariables->IsTarget() && fUserVariables->GetValue() != -1)
				fUserVariables->RemoveRow(fUserVariables->GetValue());
			break;

		case msg_ClickOnTarget:
		case msg_DblClickOnTarget:
			if (fbuffer && fSystemVariables->GetValue() != -1)
			{
				int16 len = fbufferlen;
				fSystemVariables->GetCell(fSystemVariables->GetValue(), fbuffer, &len);
				fbuffer[len] = '\0';
				
				char *equal = strchr(fbuffer, '=');
				if ( equal )
				{
					*equal++ = '\0';
					fValue->SetLongDescriptor( equal );
				} 
				else
					fValue->SetLongDescriptor( "" );
				
				fName->SetLongDescriptor( fbuffer );
			}
			break;

		case msg_ClickOnTarget2:
		case msg_DblClickOnTarget2:
			if (fbuffer && fUserVariables->GetValue() != -1)
			{
				int16 len = fbufferlen;
				fUserVariables->GetCell(fUserVariables->GetValue(), fbuffer, &len);
				fbuffer[len] = '\0';
				
				char *equal = strchr(fbuffer, '=');
				if (equal)
				{
					*equal++ = '\0';
					fValue->SetLongDescriptor( equal );
				}
				else
					fValue->SetLongDescriptor( "" );
				
				fName->SetLongDescriptor( fbuffer );
			}
			break;
	}
}


void CEDDocAppearanceNoTab::DrawSelf()
{
// we don't want to draw any frame/bevel border in this case because it is going to
// go into a dialog all by itself (not in a tabbed dialog)
// the following is copied from CPrefContain::DrawSelf() and #if'd out
#if 0
	Rect theFrame;
	if (CalcLocalFrameRect(theFrame))
		{
		StColorPenState theSaver;
		theSaver.Normalize();
		
		SBevelColorDesc theDesc;
		UGraphicGizmos::LoadBevelTraits(5000, theDesc);
	
		::PmForeColor(theDesc.fillColor);
		::PaintRect(&theFrame);
	
		StClipRgnState theClipSaver(theFrame);
		StColorState::Normalize();
		
		theFrame.top -= 5;
		::FrameRect(&theFrame);

		SBooleanRect theBevelSides = { true, false, true, true };	
		UGraphicGizmos::BevelPartialRect(theFrame, 1, eStdGrayBlack, eStdGrayBlack, theBevelSides);
		::InsetRect(&theFrame, 1, 1);
		UGraphicGizmos::BevelPartialRect(theFrame, 2, theDesc.topBevelColor, theDesc.bottomBevelColor, theBevelSides);
		}
#endif
}


void CFormatMsgColorAndImageDlog::InitializeDialogControls()
{
	LControl *prefCheckBox = (LControl *)FindPaneByID( 'Dflt' );
	if ( prefCheckBox )
		prefCheckBox->Hide();
	
	CEDDocPropAppearanceContain *contain = (CEDDocPropAppearanceContain *)FindPaneByID( 5210 );
	if ( contain )
	{
		contain->SetContext( GetContext() );
		contain->ControlsFromPref();
	}
}


Boolean CFormatMsgColorAndImageDlog::CommitChanges( Boolean /* allPanes */ )
{
	CEDDocPropAppearanceContain *contain = (CEDDocPropAppearanceContain *)FindPaneByID( 5210 );
	if ( contain )
	{
		EDT_BeginBatchChanges( fContext );
		contain->PrefsFromControls();
		EDT_EndBatchChanges( fContext );
	}

	return true;
}


void CFormatMsgColorAndImageDlog::Help()
{
	ShowHelp( HELP_DOC_PROPS_APPEARANCE );
}


/**********************************************************/
#pragma mark -
#pragma mark Modal Dialogs

// This is a total hack because I'm in a hurry and this has to be a modal dialog. Please rewrite soon!

void SetItemValue(DialogRef theDialog, short itemNo, short value);
void SetItemValue(DialogRef theDialog, short itemNo, short value)
{
	Rect so;
	ControlHandle	theControl;
	short	type;
	GetDialogItem(theDialog, itemNo, &type, (Handle *)&theControl, &so);
	if (type != kCheckBoxDialogItem) return;
	SetControlValue(theControl, value);
}


static void InformOfLimit(int min, int max)
{
	Str31 minstr, maxstr;

	NumToString( min, minstr );
	NumToString( max, maxstr );
	HandleModalDialog( EDITDLG_LIMITS, minstr, maxstr);
}


static Boolean IsEditFieldWithinLimits(LGAEditField* editField, int minVal, int maxVal )
{
	short value = editField->GetValue();
	if ( value < minVal || value > maxVal )
	{
		InformOfLimit( minVal, maxVal );
		return FALSE;
	}
	
	return TRUE;
}


#define ED_SAVE_OVERWRITE_THIS_BUTTON 1
#define ED_SAVE_OVERWRITE_ALL_BUTTON 2
#define ED_SAVE_DONT_OVERWRITE_THIS_BUTTON 3
#define ED_SAVE_DONT_OVERWRITE_ALL_BUTTON 4
#define ED_SAVE_CANCEL_BUTTON 5

ED_SaveOption FE_SaveFileExistsDialog( MWContext * /* pContext */, char* pFileName )
{
	Str255	pFileNameCopy;
	
	if ( pFileName )
	{	// should use the string classes and manipulation routines and clean up this mess!!!
		char *shorter = WH_FileName(pFileName, xpURL);
		if (!shorter)
			return ED_SAVE_CANCEL;
		BlockMoveData(shorter, pFileNameCopy, strlen(shorter) + 1);
		XP_FREE(shorter);
		c2pstr((char*)pFileNameCopy);
	}

	switch (HandleModalDialog( EDITDLG_SAVE_FILE_EXISTS, pFileNameCopy, NULL ))
	{
		case ED_SAVE_OVERWRITE_THIS_BUTTON:			return ED_SAVE_OVERWRITE_THIS;		break;
		case ED_SAVE_OVERWRITE_ALL_BUTTON:			return ED_SAVE_OVERWRITE_ALL;		break;
		case ED_SAVE_DONT_OVERWRITE_THIS_BUTTON:	return ED_SAVE_DONT_OVERWRITE_THIS;	break;
		case ED_SAVE_DONT_OVERWRITE_ALL_BUTTON:		return ED_SAVE_DONT_OVERWRITE_ALL;	break;
		case ED_SAVE_CANCEL_BUTTON:					return ED_SAVE_CANCEL;				break;
	}
	
	return ED_SAVE_CANCEL;    // shouldn't get here...
}
