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



// CComposeAddressTableView.cp

#include "abcom.h"
#include "addrbook.h"
#include "CComposeAddressTableView.h"

#include <LDropFlag.h>
#include <LTableMultiGeometry.h>
#include "NetscapeDragFlavors.h"
#include "msgcom.h"
#include "uprefd.h"
#include "UGraphicGizmos.h"
#include "CMailComposeWindow.h"
#include "CMailFlexTable.h"
#include "dirprefs.h"
#include "CComposeSession.h"
#include "CDrawingState.h"
#include "CPasteSnooper.h"
#include "resgui.h"
#include "CStandardFlexTable.h" 
#include "LTableRowSelector.h"
#include "CTSMEditField.h"
#include "libi18n.h"
#include "prefapi.h"

#include <Folders.h>

const Int16 kIconWidth = 16;
const Int16 kIconMargin = 1;

const Int16 gsPopup_ArrowHeight				= 	5;		//	Actual height of the arrow
const Int16 gsPopup_ArrowWidth				= 	9;		//	Actual width of the arrow at widest

const Int16 kLeftPaddingOfAddressTypeLabel	=	14;
const Int16 kLeftPaddingOfPopupArrow		=	7;
const Int16 kRightPaddingOfPopupArrow		=	3;

const Int16	kTextDistFromTop				=	2;

#define kAddressTypeTextTraitsID			10009
#define kAddressTypePopupTextTraitsID		130

#ifdef MOZ_NEWADDR
#include "CNameCompletionPicker.h"



//----------------------------------------------------------------------------
//		е CMailAddressEditField class
//
//----------------------------------------------------------------------------
//
CMailAddressEditField::CMailAddressEditField(LStream* inStream)
	:	CTSMEditField(inStream)
	,	mTimeLastCall(0)
	,	mIsCompletedName(false)
	,	mNeedToAutoCompleteAtIdleTime(false)
	,	mMailNewsContext(nil)
	,	mPickerPane(nil)
	,	mDisplayString(nil)
	,	mHeaderString(nil)
	,	mExpandHeaderString(nil)
{	
}


CMailAddressEditField::~CMailAddressEditField()
{
	if (mMailNewsContext)
		mMailNewsContext->RemoveUser(this);

	if (mPickerPane)
		AB_ClosePane(mPickerPane);

	XP_FREE(mDisplayString);
	XP_FREE(mHeaderString);
	XP_FREE(mExpandHeaderString);
}


void CMailAddressEditField::Init()
{
	mNeedToAutoCompleteAtIdleTime = false;
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::FinishCreateSelf
//
//----------------------------------------------------------------------------
//
void CMailAddressEditField::FinishCreateSelf()
{
	LEditField::FinishCreateSelf();

	AddAttachment(new CPasteSnooper("\r\t ", ",,\a"));

//е	CMailComposeWindow* window = dynamic_cast<CMailComposeWindow*>(LWindow::FetchWindowObject(GetMacPort()));
//е	ThrowIfNil_(window);

			// Can't reuse the Composition context: we need an Address Book context
	mMailNewsContext = new CMailNewsContext(MWContextAddressBook);
	mMailNewsContext->SetWinCSID(INTL_DocToWinCharSetID(mMailNewsContext->GetDefaultCSID()));
	mMailNewsContext->AddListener(this);
	mMailNewsContext->AddUser(this);
	StartListening();

	(void)AB_CreateABPickerPane(&mPickerPane, *mMailNewsContext, CMailNewsContext::GetMailMaster(), 10);
	if (mPickerPane)
		CMailCallbackListener::SetPane(mPickerPane);
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::HandleKeyPress
//
//----------------------------------------------------------------------------
//
Boolean CMailAddressEditField::HandleKeyPress(const EventRecord& inKeyEvent)
{
	Boolean		keyHandled		= true;
	EKeyStatus	theKeyStatus	= keyStatus_Input;
	Int16		theKey			= inKeyEvent.message & charCodeMask;

	if (inKeyEvent.modifiers & cmdKey)
		theKeyStatus = keyStatus_PassUp;	// Always pass up when Cmd-Key down
	else
		if (mKeyFilter != nil)
		{
			Int16		theChar = inKeyEvent.message & charCodeMask;
			
			theKeyStatus = (*mKeyFilter)(mTextEditH, inKeyEvent.message,
									theChar, inKeyEvent.modifiers);
		}

	if (mIsCompletedName)
	{
		if (mNumResults >= 2)
		{
			switch (theKey)
			{
				case char_Enter:
				case char_Tab:
				case char_Return:
					XP_Bool showPicker;
					PREF_GetBoolPref("ldap_1.autoComplete.showDialogForMultipleMatches", &showPicker);
					if (showPicker)
					{
						// save location of the edited cell
						LWindow* window = LWindow::FetchWindowObject(GetMacPort());
						CComposeAddressTableView* addressTable = dynamic_cast<CComposeAddressTableView*>(window->FindPaneByID('Addr'));
						ThrowIfNil_(addressTable);
						STableCell editCell = addressTable->GetEditCell();

						// save caret pos & selection
						TEHandle teH = GetMacTEH();
						short selStart = (*teH)->selStart;
						short selEnd = (*teH)->selEnd;

						// display the name completion picker
						int selectedRow = CNameCompletionPicker::DisplayDialog(this, mPickerPane, mMailNewsContext, mNumResults);

						// restore the edited cell
						window->Activate();
						window->Select();
						addressTable->StartEditCell(editCell);

						if (selectedRow > 0)
						{
							// if we have a result, put it in the cell
							AB_NameCompletionCookie* cookie = AB_GetNameCompletionCookieForIndex(mPickerPane, selectedRow-1);
							NameCompletionBECallbackFunction(cookie, 1, this);
						}
						else
						{
							// otherwise restore the previous selection
							StFocusAndClipIfHidden	focus(this);
							::TESetSelect(selStart, selEnd, teH);
							return true; // key was handled
						}
					}
					break;
			}
		}

		StFocusAndClipIfHidden	focus(this);
		switch (theKeyStatus) 
		{
			// If the user deletes a char, we need to do 2 deletes.
			// The first deletes the nickname completion.
			// The second actually erases a character.
			case keyStatus_TEDelete:
				if ((**mTextEditH).selEnd > 0)
				{
					if (mTypingAction == nil)
					{
						mTypingAction = new LTETypingAction(mTextEditH, this, this);
						PostAction(mTypingAction);
					}

					if (mTypingAction != nil)
						mTypingAction->BackwardErase();
					else
						::TEKey(char_Backspace, mTextEditH);
					// UserChangedText();
				}
				break;

			// If the user moves the cursor, we need to
			// get rid of the "multiple names found".
			case keyStatus_TECursor:
				if (mNumResults >= 2)
				{
					::TEKey(char_Backspace, mTextEditH);
					mIsCompletedName = false;
					switch (theKey)
					{
						case char_UpArrow:
						case char_DownArrow:
							return LCommander::HandleKeyPress(inKeyEvent); 
					
					}
				}
				break;
		}
	}
	else
	{
		if (theKeyStatus == keyStatus_TEDelete)
		{
			CStr255 currentText;
			GetDescriptor(currentText);
			if (currentText.Length() == 0)
				return LCommander::HandleKeyPress(inKeyEvent); 
		}
		switch (theKey)
		{
			case char_UpArrow:
			case char_DownArrow:
				return LCommander::HandleKeyPress(inKeyEvent); 
		
		}
	}
	keyHandled = Inherited::HandleKeyPress(inKeyEvent);
	return keyHandled;
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::SpendTime
//
//----------------------------------------------------------------------------
//
void CMailAddressEditField::SpendTime(const EventRecord& inMacEvent)
{
	if (mNeedToAutoCompleteAtIdleTime)
	{
		if ((::TickCount() - mTimeLastCall) > ::LMGetKeyThresh() / 2)
			StartNameCompletion();
	}
	Inherited::SpendTime(inMacEvent);
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::UserChangedText
//
//----------------------------------------------------------------------------
//
void CMailAddressEditField::UserChangedText()
{
	mIsCompletedName = false;
	mNeedToAutoCompleteAtIdleTime = true;

	if ((::TickCount() - mTimeLastCall) > ::LMGetKeyThresh() / 2)
		StartNameCompletion();

	mTimeLastCall = ::TickCount();
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::NameCompletionBECallbackFunction
//
//----------------------------------------------------------------------------
//
int CMailAddressEditField::NameCompletionBECallbackFunction(AB_NameCompletionCookie* cookie, int numResults, void* FECookie)
{
	CMailAddressEditField* editField = (CMailAddressEditField*)FECookie;
	if (cookie != NULL)
	{
		char* displayString = AB_GetNameCompletionDisplayString(cookie);
		char* headerString = AB_GetHeaderString(cookie);
		char* expandHeaderString = AB_GetExpandedHeaderString(cookie);

		editField->SetNameCompletionResults(numResults, displayString, headerString, expandHeaderString);

		AB_FreeNameCompletionCookie(cookie);
	}
	return(0);
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::SetNameCompletionResults
//
//----------------------------------------------------------------------------
//
void CMailAddressEditField::SetNameCompletionResults(int numResults, char* displayString, char* headerString, char* expandHeaderString)
{
	XP_FREE(mDisplayString);
	XP_FREE(mHeaderString);
	XP_FREE(mExpandHeaderString);

	TEHandle teH = GetMacTEH();
	short caretPos = (*teH)->selStart;

	mNumResults = numResults;
	if (numResults == 1)
	{
		mDisplayString = displayString;
		mHeaderString = headerString;
		mExpandHeaderString = expandHeaderString;

		SetDescriptor(CStr255(mDisplayString));
	}
	else // we got 2 or more results
	{
		CStr255 userEntry;
		GetDescriptor(userEntry);
		userEntry[0] = caretPos;

		mDisplayString = XP_STRDUP(userEntry);
		mHeaderString = XP_STRDUP(userEntry);
		mExpandHeaderString = XP_STRDUP(userEntry);

		CStr255 multipleHitsStr;
		CStr255 resultStr;
		::GetIndString(multipleHitsStr, 10610, 2);
		resultStr = mDisplayString + multipleHitsStr;
		SetDescriptor(resultStr);
	}
	StFocusAndClipIfHidden	focus(this);
	::TESetSelect(caretPos, 1000, teH);

	mIsCompletedName = true;
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::StartNameCompletion
//
//----------------------------------------------------------------------------
//
void CMailAddressEditField::StartNameCompletion()
{
	mNeedToAutoCompleteAtIdleTime = false;

	if (!mPickerPane)
		return;

	CStr255 currentText;
	GetDescriptor(currentText);
	(void)AB_NameCompletionSearch(mPickerPane, (char*)currentText, NameCompletionBECallbackFunction, (void*)this);
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::PaneChanged
//
//----------------------------------------------------------------------------
//
 void CMailAddressEditField::PaneChanged(
 								MSG_Pane*	/*inPane*/,
								MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
								int32		/*value*/)
{
	switch (inNotifyCode)
	{
	}
}


//----------------------------------------------------------------------------
//		е CMailAddressEditField::FinalizeEdit
//
//----------------------------------------------------------------------------
//
const char* CMailAddressEditField::FinalizeEdit()
{
	if (mIsCompletedName)
	{
		mIsCompletedName = false;

		//еее should also return mExpandHeaderString
		return XP_STRDUP(mHeaderString);
	}
	else
	{
		CStr255 currentText;
		GetDescriptor(currentText);
    	return XP_STRDUP(currentText);
	}
}




#else //MOZ_NEWADDR
#pragma mark -
CMailAddressEditField::CMailAddressEditField( LStream* inStream )
	:	CTSMEditField( inStream )
	,	mTimeLastCall(0)
	,	mDirServerList(nil)
	,	mAddressBook(nil)
	, mIsCompletedName( false )
	, mCheckAddress( false )
	, mEntryID( MSG_MESSAGEIDNONE )
{	
}

void CMailAddressEditField::Init()
{
	mCheckAddress = false;
	mEntryID = MSG_MESSAGEIDNONE;
}

//-----------------------------------
void CMailAddressEditField::FinishCreateSelf()
// override to do address-book name completion.
//-----------------------------------
{
	LEditField::FinishCreateSelf();
	Try_
	{
		AddAttachment(new CPasteSnooper("\r\t ", ",,\a"));
		XP_List *list = FE_GetDirServers();
		ThrowIf_( DIR_GetComposeNameCompletionAddressBook(list, &mDirServerList) != 0);
	#if 0 // Currently FE_GetAddressbook ignores the message pane.
		 // Since we would like to use this control else where( AddressPicker, and Mailing list window 
		 // its not a good idea for this class to be dependant on being created from a CMailComposeWindow
		CMailComposeWindow* window
			= dynamic_cast<CMailComposeWindow*>(
				LWindow::FetchWindowObject(GetMacPort())
				);
		ThrowIfNil_(window);
		
		mAddressBook = FE_GetAddressBook(
			window->GetComposeSession()->GetMSG_Pane());
	#else if
			mAddressBook = FE_GetAddressBook( NULL);
	#endif
		ThrowIfNil_(mAddressBook);
	}
	Catch_(inErr)
	{
		mDirServerList = nil;
		mAddressBook = nil;
	}
	EndCatch_
}

void
CMailAddressEditField::SpendTime(
	const EventRecord&	 inMacEvent )
{
	
	if( mCheckAddress )
	{
		UInt32 currentTime = ::TickCount();
		if (currentTime - mTimeLastCall > ::LMGetKeyThresh() / 2)
			UserChangedText();
	}
	LEditField::SpendTime( inMacEvent );
}
//-----------------------------------
void CMailAddressEditField::UserChangedText()
// override to do address-book name completion.
//-----------------------------------
{
	UInt32 currentTime = ::TickCount();
	mIsCompletedName = false;
	mEntryID =  MSG_MESSAGEIDNONE;
	mCheckAddress = true;
	if (currentTime - mTimeLastCall > ::LMGetKeyThresh() / 2)
	{
		ABID field;
		if (mAddressBook)
		{
			mCheckAddress = false;
			// Got a personal address book
			if (mDirServerList)
			{
				// Got a DIR_Server entry for the address book
				CStr255 currentText;
				GetDescriptor(currentText);
				if (::AB_GetIDForNameCompletion(
					mAddressBook,
					mDirServerList,
					&mEntryID, 
					&field,
					currentText) == 0 && mEntryID != MSG_MESSAGEIDNONE)
				{
					char newText[256];
					newText[0] = '\0';
				    if (field == ABNickname)
						AB_GetNickname(mDirServerList, mAddressBook, mEntryID, newText);
				    else
						AB_GetFullName(mDirServerList, mAddressBook, mEntryID, newText);
					if (newText[0])
					{
						// whew!
						TEHandle teH = GetMacTEH();
						short selStart = (*teH)->selStart;
						SetDescriptor(CStr255(newText));
						FocusDraw();
						::TESetSelect(selStart, 1000, teH);
						mIsCompletedName = true;
					}
				}
			}
		}
	}
	mTimeLastCall = ::TickCount();
} // CMailAddressEditField::UserChangedText

Boolean CMailAddressEditField::HandleKeyPress( const EventRecord&	inKeyEvent)
{
	Boolean		keyHandled = true;
	EKeyStatus	theKeyStatus = keyStatus_Input;
	Int16		theKey = inKeyEvent.message & charCodeMask;
	
	if (inKeyEvent.modifiers & cmdKey)
	{	// Always pass up when the command
			theKeyStatus = keyStatus_PassUp;	//   key is down
	}
	else if (mKeyFilter != nil)
	{
		Int16		theChar = inKeyEvent.message & charCodeMask;
		
		theKeyStatus = (*mKeyFilter)(mTextEditH, inKeyEvent.message,
								theChar, inKeyEvent.modifiers);
	}
	
	if( mIsCompletedName )
	{
		
		StFocusAndClipIfHidden	focus(this);
		// If the user deletes a char we need to do 2 deletes
		// The first deletes the nickname completion
		// the second actually erases a character
		
		switch (theKeyStatus) 
		
			case keyStatus_TEDelete:{
				if ((**mTextEditH).selEnd > 0) {
					if (mTypingAction == nil) {
						mTypingAction = new LTETypingAction(mTextEditH, this, this);
						PostAction(mTypingAction);
					}
					if (mTypingAction != nil) {
						mTypingAction->BackwardErase();
					} else {
						::TEKey(char_Backspace, mTextEditH);
					}
					//UserChangedText();
				}
				break;
			}
	}
	else
	{
		if( theKeyStatus == keyStatus_TEDelete )
		{
				CStr255 currentText;
				GetDescriptor(currentText);
				if( currentText.Length() == 0 )
					return LCommander::HandleKeyPress(inKeyEvent); 
		}
		switch (theKey)
		{
			case char_UpArrow:
			case char_DownArrow:
				return LCommander::HandleKeyPress(inKeyEvent); 
		
		}
	}
	keyHandled = LEditField::HandleKeyPress(inKeyEvent);
	return keyHandled;
}
//-----------------------------------
const char* CMailAddressEditField::FinalizeEdit()
//-----------------------------------
{
	char* fullName = nil;
	CStr255 currentText;
	GetDescriptor(currentText);
    if (mDirServerList)
    {
    	 if ( mEntryID == MSG_MESSAGEIDNONE ) 
		 {
			ABID field;
		    ::AB_GetIDForNameCompletion(
			    mAddressBook,
			    mDirServerList,
			    &mEntryID,
			    &field,
			    currentText);
	    }
	    if ( mEntryID != MSG_MESSAGEIDNONE)
	    {
		    ::AB_GetExpandedName(mDirServerList, mAddressBook, mEntryID, &fullName);
		}
    }
    if (!fullName)
    	return XP_STRDUP(currentText);
    return fullName;
} // CMailAddressEditField::FinalizeEdit()

#endif //MOZ_NEWADDR

#pragma mark -

CComposeAddressTableView::CComposeAddressTableView(LStream* inStream) :
	LTableView(inStream), mInputField(NULL)
,	LDragAndDrop(GetMacPort(), this)
,	mCurrentlyAddedToDropList(true) // because LDragAndDrop constructor adds us.
,	mDirty(false)
,	mTextTraits( 10610 )
,	mEditCell( 0, 0)
,	mAddressTypeHasFocus(false)
,	mTypedownTable(nil)
{
}

CComposeAddressTableView::~CComposeAddressTableView() 
{
	XP_FREEIF(mTypedownTable);
}

void CComposeAddressTableView::SetUpTableHelpers()
{
	SetTableGeometry(new LTableMultiGeometry(this, CalculateAddressTypeColumnWidth(), 16));
	SetTableSelector(new LTableRowSelector(this));
	SetTableStorage(new CComposeAddressTableStorage(this));
}

void CComposeAddressTableView::FinishCreateSelf()
{
	SetUpTableHelpers();
	
	Assert_(mTableStorage != NULL);

	InsertCols(2, 1, NULL, 0, false);
	AdjustColumnWidths();
	// Get the Editfield
	mInputField = dynamic_cast< CMailAddressEditField*>( FindPaneByID ('Aedt') );
	// highlight colors
	RGBColor ignore;
	UGraphicGizmos::CalcWindowTingeColors(GetMacPort(), ignore, mDropColor );
	
	// Set up table for type-ahead
	MenuHandle popupMenu = (MenuHandle)::GetResource('MENU', kAddressTypeMenuID );
	ThrowIfNil_(popupMenu);
	
	Int16	numItems = CountMItems(popupMenu);

	// this will hold lower case of first letters, with trailing null	
	mTypedownTable = (char *)XP_ALLOC(numItems + 1);
	
	for (Int16 i = 1; i <= numItems; i ++)		// menu items are 1-based
	{
		CStr255		itemName;
		GetMenuItemText(popupMenu, i, itemName);
		mTypedownTable[i - 1] = tolower(itemName[1]);
	}
	mTypedownTable[numItems] = '\0';
	
	ReleaseResource((Handle)popupMenu);
}

// adjust address column to be width of pane minus width of
// first column
void CComposeAddressTableView::AdjustColumnWidths()
{
	SDimension16 frameSize;
	UInt16 firstColWidth = GetColWidth(1);
	GetFrameSize(frameSize);
	SetColWidth(frameSize.width - firstColWidth, 2, 2);
}

void		CComposeAddressTableView::ResizeFrameBy(
								Int16				inWidthDelta,
								Int16				inHeightDelta,
								Boolean				inRefresh)
{
	LTableView::ResizeFrameBy( inWidthDelta, inHeightDelta, inRefresh);
	AdjustColumnWidths();
	// May have to adjust edit field
	if(	mEditCell.row!=0 )
	{
		Rect cellRect;
		if( GetLocalCellRect( mEditCell, cellRect))
		{
			// put edit field over cell
			cellRect.left += kIconWidth + 3;
			cellRect.top +=1;
			mInputField->ResizeFrameTo(cellRect.right - cellRect.left,
									   cellRect.bottom - cellRect.top,
									   false);
			mInputField->PlaceInSuperImageAt(cellRect.left, cellRect.top, false);
		}
	}
}


void CComposeAddressTableView::ClickCell(const STableCell &inCell, const SMouseDownEvent &inMouseDown)
{
	StopInputToAddressColumn();		// on any click
	
	if (inCell.col == 1)
	{
#if 0
		LCommander::SwitchTarget(this);
		LTableView::ClickCell(inCell, inMouseDown); //Does nothing
#endif //0
		
		EAddressType oldAddressType = GetRowAddressType( inCell.row );
			
		// load and install the menu
		MenuHandle popupMenu = ::GetMenu( kAddressTypeMenuID );
		if (popupMenu)
		{
			Int16 	result;
			
			FocusDraw();
			
			{
				StSysFontState theSysFontState;
				
				// е We also detach the resource so we don't run into problems
				// for cases where there are multiple popups referencing the same
				// menu resource.
				::DetachResource( (Handle) popupMenu);
				::InsertMenu( popupMenu, hierMenu);	
				
				Point where = inMouseDown.wherePort;	
				PortToGlobalPoint( where );
				Rect cellRect;
				if (GetLocalCellRect( inCell, cellRect ))
				{
					where = topLeft( cellRect );
					::LocalToGlobal( &where );
				}

				// Adjust text traits to be the appropriate text traits for the popup
				
				theSysFontState.SetTextTraits(/* kAddressTypePopupTextTraitsID */ );

				Int16 defaultChoice = oldAddressType;
				result = ::PopUpMenuSelect( popupMenu, where.v, where.h, defaultChoice );
			}
			
			// е Clean up the menu
			::DeleteMenu( kAddressTypeMenuID );
			::DisposeHandle( (Handle) popupMenu); // OK because we detached it.
			
			STableCell	addressCell( inCell.row, 2);
			
			if (result)
			{
				if( CellIsSelected( addressCell) )
					SetSelectionAddressType( EAddressType( result ) );
				else
				{
					SetRowAddressType( inCell.row, EAddressType( result ) );
					RefreshCell( addressCell );	
					BroadcastMessage( msg_AddressChanged, NULL );
				}
			}
			RefreshCell( inCell );
		}	
	}
	else
	{ // in address cell
	  // was the click on the icon?
	 	Rect iconRect;
	 	GetLocalCellRect( inCell, iconRect );
	 	iconRect.right = iconRect.left + kIconWidth+kIconMargin;
	 	if (!(::PtInRect(inMouseDown.whereLocal, &iconRect)) )
			StartEditCell( inCell );
	}
}

void CComposeAddressTableView::DrawSelf()
{
	Rect		r;
	if ( CalcLocalFrameRect( r ) )
		EraseRect( &r );
	Int16 bottom = r.bottom;
	Int16 right	= r.right;
	// What Color should these lines be?
	RGBColor lightTinge,darkTinge;
	UGraphicGizmos::CalcWindowTingeColors(GetMacPort(), lightTinge, darkTinge);
	::RGBForeColor(&lightTinge);
	// vertical seperator
	short width=mTableGeometry->GetColWidth(1);
	::MoveTo(width,r.top);
	::LineTo(width,r.bottom);
	// Horizontal seperators	
	STableCell	topLeftCell, botRightCell;
	FetchIntersectingCells(r, topLeftCell, botRightCell);
	GetLocalCellRect(topLeftCell,r);

	short height = mTableGeometry->GetRowHeight(1); // All Rows are the same size
	short currentHeight = r.top - height;
	if (height > 0)
	{

		while (currentHeight<bottom)
		{
			currentHeight+=height;
			::MoveTo(r.left,currentHeight);
			::LineTo(right,currentHeight);
		}
	}
	LTableView::DrawSelf();
}

Uint16 CComposeAddressTableView::CalculateAddressTypeColumnWidth()
{
	Uint16				width;
	MenuHandle			menu = nil;
	
	// Save port and port origin
	
	StPortOriginState	thePortOriginState(GetMacPort());

	// Adjust port and port origin
	
	FocusDraw();

	// Get the menu
	
	menu = ::GetMenu(kAddressTypeMenuID);
	ThrowIfNil_(menu);
	
	// Adjust system font state so menuWidth is calculated
	// with the same metrics as when PopupMenuSelect is called.
	
	StSysFontState theSysFontState;
	theSysFontState.SetTextTraits(kAddressTypeTextTraitsID);	// We *do* want kAddressTypeTextTraitsID and
																// *not* kAddressTypePopupTextTraitsID.
	// Calculate the width
	
	::CalcMenuSize(menu);
	width = (*menu)->menuWidth + kLeftPaddingOfPopupArrow + gsPopup_ArrowWidth + kRightPaddingOfPopupArrow;
	
	// Release the menu
	
	::ReleaseResource((Handle) menu);

	return width;
}

void CComposeAddressTableView::DrawCell(const STableCell &inCell, const Rect &inLocalRect)
{
	EAddressType type = GetRowAddressType( inCell.row );

	if ( inCell.col == 1 )
	{
		// Draw the hilite region if we are taking keyboard input
		if (mAddressTypeHasFocus)
		{
			STableCell	addressCell(mEditCell.row, 1);
			Rect		cellRect;
			
			if (GetLocalCellRect(addressCell, cellRect))
			{
				StColorPenState	stateSaver;
				StRegion		hiliteRgn;
				
				cellRect.top ++;
				::RectRgn(hiliteRgn, &cellRect);
				
				{
					StRegion		tempRgn(hiliteRgn);
					
					::InsetRgn(tempRgn, 2, 2);
					::DiffRgn(hiliteRgn, tempRgn, hiliteRgn);
				}

				UDrawingUtils::SetHiliteModeOn();
				::InvertRgn(hiliteRgn);
			}
		}
		
		// Draw the address type string
		
		UTextTraits::SetPortTextTraits(kAddressTypeTextTraitsID);
			
		MoveTo(inLocalRect.left + kLeftPaddingOfAddressTypeLabel, inLocalRect.bottom - 4);
		
		MenuHandle menu = ::GetMenu( kAddressTypeMenuID );
		if ( menu )
		{
			// е We also detach the resource so we don't run into problems
			// for cases where there are multiple popups referencing the same
			// menu resource.
			::DetachResource( (Handle) menu);
			Str255 typeText;
			:: GetMenuItemText( menu, type, typeText);
			::DrawString( typeText );
			::DisposeHandle( (Handle) menu); // OK because we detach
		}
		
		// Draw the drop flag

		Rect iconRect;
		
		UInt32	vDiff = (inLocalRect.bottom - inLocalRect.top - (gsPopup_ArrowHeight - 1)) >> 1;
		iconRect.top 	= inLocalRect.top + vDiff;
		iconRect.bottom = iconRect.top + gsPopup_ArrowHeight - 1;
		iconRect.left 	= inLocalRect.right - gsPopup_ArrowWidth - 1 - kRightPaddingOfPopupArrow;
		iconRect.right 	= iconRect.left + gsPopup_ArrowWidth - 1;	
		
		UGraphicGizmos::DrawPopupArrow(iconRect, true, true, false);
	}
	else 
	{
		// Draw the icon
		Rect iconRect = inLocalRect;
		iconRect.left +=  kIconMargin;
		iconRect.right 	=  iconRect.left + kIconWidth;
		
		ResIDT iconResIDT = 15260; // Default is a person id
		if( type == eNewsgroupType )
			iconResIDT = 15231 ;
		CStandardFlexTable::DrawIconFamily(iconResIDT , 16, 16, 0, iconRect);
		
		if (inCell.row != mEditCell.row)
		{	
			// we're trying to draw an address string cell that
			// isn't the one with the LEditField over it.
			char* addr = NULL;
			Uint32 size = sizeof(addr);
			GetCellData(inCell, &addr, size);
			
			if (addr)
			{
				Uint8	stringLen = LString::CStringLength(addr);
				UTextTraits::SetPortTextTraits( mTextTraits );
				
				Rect		textRect = inLocalRect;
				
				textRect.left = iconRect.right + 3;
				textRect.right = inLocalRect.right;
				textRect.top += kTextDistFromTop;
				
				UGraphicGizmos::PlaceTextInRect( addr, stringLen, textRect, teFlushLeft, teFlushTop );		//flushtop to match the TEUpdate used by LEditField
			}
		}
	}
}


//-----------------------------------
void CComposeAddressTableView::HiliteRow(	TableIndexT	inRow, Boolean inUnderline )
//-----------------------------------
{
	STableCell	cell;
	Rect		r;		
	
	cell.row = inRow;
	cell.col = 1;
	
	GetLocalCellRect(cell, r);
	
	r.left 	= 0;
	r.right	= mFrameSize.width;

	if (inUnderline)
	{
		r.top = r.bottom-1;
		r.bottom += 1;
	}
	
	//RGBColor savedHiliteColor;	
	UDrawingUtils::SetHiliteModeOn();
	//LMGetHiliteRGB(&savedHiliteColor);
	//::HiliteColor(&mDropColor);
	::InvertRect(&r);
	//::HiliteColor(&savedHiliteColor);
}

void CComposeAddressTableView::DirectInputToAddressColumn()
{
	if (mAddressTypeHasFocus) return;
	
	mAddressTypeHasFocus = true;
	SwitchTarget(this);
	
	STableCell		theCell(mEditCell.row, 1);
	RefreshCell(theCell);
	theCell.col = 2;
	RefreshCell(theCell);
}


void CComposeAddressTableView::StopInputToAddressColumn()
{
	if (!mAddressTypeHasFocus) return;
	
	mAddressTypeHasFocus = false;
	SwitchTarget(mInputField);

	STableCell		theCell(mEditCell.row, 1);
	RefreshCell(theCell);
	theCell.col = 2;
	RefreshCell(theCell);
}


Boolean CComposeAddressTableView::HandleKeyPress(const EventRecord &inKeyEvent)
{
	Boolean 	keyHandled = true;
	Boolean		cmdKeyDown = (inKeyEvent.modifiers & cmdKey) != 0;
	Char16		c = inKeyEvent.message & charCodeMask;
	
	if (mAddressTypeHasFocus)
	{
		EAddressType		newAddressType = eNoAddressType;
		char				*thisOne = mTypedownTable;
		
		for (Int16 i = 0; *thisOne; i++, thisOne ++)
		{
			if (tolower(c) == *thisOne)
			{
				newAddressType = (EAddressType)(i + 1);	// enum is 1-based
				break;
			}
		}
		
		if (newAddressType != eNoAddressType)
		{
			STableCell		addressCell(mEditCell.row, 1);
			SetRowAddressType(mEditCell.row, newAddressType);
			RefreshCell(addressCell);
			BroadcastMessage( msg_AddressChanged, NULL );
			return keyHandled;
		}
	}

	switch (c)
	{
		case char_Backspace:
		case char_FwdDelete:
			DeleteSelection();
			return true;
			break;
			
		case char_UpArrow:
			EndEditCell();
			STableCell previousCell( GetFirstSelectedCell().row, 2 );
			if( previousCell.row != 1 )
			{			
				previousCell.row--;
				StartEditCell( previousCell );
			}
			keyHandled = true;
			break;
			
		
			/*EndEditCell();
			STableCell nextCell( GetFirstSelectedCell().row, 2 );
			nextCell.row++;
			StartEditCell( nextCell );
			keyHandled = true;
			break;*/
		case char_Enter:
				if( !mEditCell.IsNullCell() )
				{
					EndEditCell();
					STableCell cellToSelect( GetFirstSelectedCell().row, 2);
					SelectCell( cellToSelect );
				}
				else
				{
					STableCell cellToSelect( GetFirstSelectedCell().row, 2);
					StartEditCell( cellToSelect );
				}
			break;
		
		case char_LeftArrow:
			if (!mAddressTypeHasFocus && cmdKeyDown)
				DirectInputToAddressColumn();
			else
				keyHandled = LCommander::HandleKeyPress(inKeyEvent);
			break;
		case char_RightArrow:
			if (mAddressTypeHasFocus && cmdKeyDown)
				StopInputToAddressColumn();
			else
				keyHandled = LCommander::HandleKeyPress(inKeyEvent);
			break;
		case char_DownArrow:
		case char_Return:
		case char_Tab:
			EventRecord tabKeyEvent(inKeyEvent);
			tabKeyEvent.message &= ~charCodeMask; // clear char code bits
			tabKeyEvent.message |= char_Tab;
			Boolean backward = ((inKeyEvent.modifiers & shiftKey) != 0);
			TableIndexT nRows, nCols;
			GetTableSize(nRows, nCols);
			EndEditCell();
			StopInputToAddressColumn();
			STableCell cell( GetFirstSelectedCell().row, 2);
			if (backward && cell.row <= 1)
			{
				keyHandled = LCommander::HandleKeyPress(tabKeyEvent); // tab out of here
				break;
			}
			else if (!backward && cell.row >= nRows)
			{
				// If we were editing, and we tabbed out...
				if (cell.row != 0)
				{
					// Tabbing out of a non-empty addressee field adds another new addressee
					char* addr = NULL;
					Uint32 size = sizeof(addr);
					GetCellData( cell, &addr, size);
					if (addr && *addr && (c != char_Tab) )
					{
						EAddressType addressType = GetRowAddressType( nRows );
						char emptyString[1]="\0";
						InsertNewRow( addressType, emptyString, true );
						keyHandled = true;
					}
					else
					{
						keyHandled = LCommander::HandleKeyPress(tabKeyEvent); // tab out of here
					}
				}
				else if (nRows==0)
				{
					InsertNewRow(true, true);
					keyHandled = true;
				}
				break;
			}
			else if (backward)
				cell.row--;
			else
				cell.row++;
			StartEditCell( cell );
			break;

		default:
			keyHandled = LCommander::HandleKeyPress(inKeyEvent);
	} // switch
	return keyHandled;
}

Boolean CComposeAddressTableView::ClickSelect(const STableCell &inCell,
									   const SMouseDownEvent &inMouseDown)
{
	
	if( inCell.col == 1 )
	{
		return true;
	}
	else
	{
		SwitchTarget( this );
		EndEditCell();
		if( !mEditCell.IsNullCell() )
			UnselectCell( mEditCell );
		return LTableView::ClickSelect( inCell, inMouseDown );
	}
}

void
CComposeAddressTableView::ClickSelf(
	const SMouseDownEvent &inMouseDown)
{
	STableCell	hitCell;
	SPoint32	imagePt;
	
	LocalToImagePoint(inMouseDown.whereLocal, imagePt);
	
	if (GetCellHitBy(imagePt, hitCell)) {
		if (ClickSelect(hitCell, inMouseDown)) {
			ClickCell(hitCell, inMouseDown);
		}
		
	} else {							// Click is outside of any Cell
		InsertNewRow( true, true);      // so create a new cell
	}
}

void CComposeAddressTableView::FillInRow( Int32 row, EAddressType inAddressType, const char* inAddress)
{
	STableCell cell;
	cell.col = 1;
	cell.row = row;
	SetCellData( cell, &inAddressType, sizeof(inAddressType));
	RefreshCell( cell );
	cell.col = 2;
	SetCellData( cell, inAddress, sizeof(inAddress));
	RefreshCell( cell );
}

// Takes in a string  explodes it, and inserts it into the table
Int32 CComposeAddressTableView::CommitRow(  const char* inString, STableCell cell)
{
	EAddressType 	originAddressType = GetRowAddressType( cell.row );
	Boolean 		dirty = false;
	// Update and do name replacement
	// Explode string to 1 entry per line
	MSG_HeaderEntry *returnList = nil;	
	Int32 numberItems = MSG_ExplodeHeaderField( originAddressType, inString, &returnList );
	Int32 numberRowsAdded = 0;
	
	if ( numberItems > 1)
	{
		InsertRows( (numberItems - 1), cell.row, NULL, NULL, false );
		dirty = true;
	}
	 // We will get 0 if we passed in a Null string, this occurs when the user
	 // deletes an exsiting email address. Should the row be removed?
	if (numberItems == 0)
	{ 
		if( !dirty )
		{
			char *addr = NULL;
			Int32 size = sizeof(addr);
			GetCellData( cell, &addr, size);
			if( addr !=NULL )
			dirty = true;
		}
		SetCellData( cell, inString, sizeof(inString) );
	}
	else if ( numberItems != -1 )
	{
		for (Int32 currentItem = 0; currentItem < numberItems; currentItem ++ )
		{
			if ( !dirty )
			{
				char *addr = NULL;
				Int32 size = sizeof(addr);
				GetCellData( cell, &addr, size);
				if( addr == NULL )
					dirty = true;
				else
					dirty = XP_STRCMP( addr, returnList[currentItem].header_value);
			}
			// Use input field to do name expansion on each entry
			char*			unexpandedName = returnList[currentItem].header_value;
			char*			actualName = unexpandedName;
			EAddressType	addressType = (EAddressType)returnList[currentItem].header_type;		// this is just what we passed in above
			
			if ( XP_STRNCASECMP(unexpandedName, "mailto:", 7) == 0 )
			{
				actualName = unexpandedName + 7;
				//addressType = eToType;
			}
			else if (	(XP_STRNCASECMP(unexpandedName, "news://", 7) == 0) ||
					 	(XP_STRNCASECMP(unexpandedName, "snews://", 8) == 0) )
			{
				// test whether this URL points to the current server being used for this message.
				// if not, and there is no other news recipient so far, set the news post url
				// in the backend, and just enter the group name
				//MSG_SetCompHeader( , MSG_NEWSPOSTURL_HEADER_MASK, ); etc
				
				// no backend support yet, so just put in the whole URL
				actualName = unexpandedName;
				addressType = eNewsgroupType;
			}
			else if ( 	(XP_STRNCASECMP(unexpandedName, "news:", 5) == 0) ||
						(XP_STRNCASECMP(unexpandedName, "snews:", 6) == 0) )
			{
				actualName = unexpandedName + 5;
				addressType = eNewsgroupType;
			}
			
			mInputField->Init();
			mInputField->SetDescriptor( CStr255( actualName ) );
			const char* expandedName = mInputField->FinalizeEdit();
		
			FillInRow( cell.row, addressType, expandedName );
			
			XP_FREE( unexpandedName );
			XP_FREE( (char*)expandedName );
			cell.row++;
		}
		XP_FREE ( returnList );
		numberRowsAdded = numberItems-1;
	}
	
	if ( dirty )
		BroadcastMessage( msg_AddressChanged, NULL );
	
	Refresh();
	
	return numberRowsAdded;
}

Int32 CComposeAddressTableView::FinalizeAddrCellEdit()
{
	int32 numRowsAdded;
	const char* fullName = nil;
	Try_
	{
		fullName = mInputField->FinalizeEdit();
		if (fullName)
		{
			numRowsAdded = CommitRow( fullName, mEditCell );
			XP_FREE((char*)fullName);	
		}
	}
	Catch_(inErr)
	{
		XP_FREEIF((char*)fullName);
	}
	EndCatch_
	return numRowsAdded;
} 

Boolean CComposeAddressTableView::ObeyCommand(CommandT inCommand, void *ioParam)
{
	switch (inCommand)
	{
		case msg_TabSelect:
			STableCell cell(1, 2);
			StartEditCell( cell );
			return true;
	}
	return LCommander::ObeyCommand(inCommand, ioParam);
}

void CComposeAddressTableView::HideEditField()
{
	mInputField->Hide();
	LView::OutOfFocus(NULL);
	RefreshCell(mEditCell);
}

#if 0
void CComposeAddressTableView::ListenToMessage(MessageT inMessage, void* ioParam)
{
#if 0 // NO longer have buttons to listen to
	switch(inMessage)
	{
		case msg_AddAddressee:
			InsertNewRow(true, true);
			break;
		case msg_RemoveAddressee:
			DeleteSelection();
			break;
	}
#endif //0
}
#endif

void CComposeAddressTableView::InsertNewRow(Boolean inRefresh, Boolean inEdit)
{
	char* dummy = NULL;
	EndEditCell();
	TableIndexT numRows;
	GetNumRows(numRows);
	InsertRows(1, numRows++, &dummy, sizeof(dummy), inRefresh);
	STableCell cell(numRows, 2);
	if (inEdit)
		StartEditCell(cell);
}

void CComposeAddressTableView::InsertNewRow(EAddressType inAddressType, const char* inAddress, Boolean inEdit)
{
	InsertNewRow(false, false);
	STableCell cell;
	cell.col = 1;
	GetNumRows(cell.row);
	SetCellData(cell, &inAddressType, sizeof(inAddressType));
	RefreshCell(cell);
	cell.col = 2;
	SetCellData(cell, inAddress, sizeof(inAddress));	// this makes a copy of the data, getting the size from strlen
	RefreshCell(cell);	
	if ( inEdit )
		StartEditCell( cell );
}


void CComposeAddressTableView::StartEditCell(const STableCell &inCell)
{
	UnselectAllCells();
	Assert_( inCell.col == 2 ); 
	Rect		cellRect;
	
	// Make sure the edit filed is fully visible
	ScrollCellIntoFrame(inCell);
	// now move edit field over cell
	
	if (GetLocalCellRect(inCell, cellRect))
	{
		// put edit field over cell
		mInputField->PutInside(this);
		
		Rect		textRect = cellRect;
		
		textRect.left += kIconWidth + 3;
		textRect.top += kTextDistFromTop;		// text is drawn flush top
		
		mInputField->ResizeFrameTo(textRect.right - textRect.left,
								   textRect.bottom - textRect.top,
								   false);
		mInputField->PlaceInSuperImageAt(textRect.left, cellRect.bottom - (textRect.bottom - textRect.top), false);
		mInputField->Init();
		// set text of edit field
		char *addr = NULL;
		Uint32 size = sizeof(addr);
		GetCellData(inCell, &addr, size);
		if (addr)
		{
			LStr255 pstr(addr);
			mInputField->SetDescriptor(pstr);
			mInputField->SelectAll();
		}
		else
			mInputField->SetDescriptor("\p");
		mInputField->Show();
		SwitchTarget(mInputField);
		mEditCell.row = inCell.row;
		mEditCell.col = inCell.col;
	}
}


void CComposeAddressTableView::EndEditCell()
{
	if (!mEditCell.IsNullCell())
	{	// Copy text out of LEditField into corresponding cell if
		// user was editing an address
		Int32 numRowsAdded = FinalizeAddrCellEdit();
		HideEditField();
		RefreshCell( mEditCell );
		mEditCell.row += numRowsAdded;
		SelectCell( mEditCell );
		mEditCell.row = 0;
		mEditCell.col = 0;
	}
}

void CComposeAddressTableView::TakeOffDuty()
{
	// This causes unwanted behaviour -- CommitRow when window is deactivated,
	// and causes deactivated windows to lose keyboard focus when reactivated
	
	
	// Unfortunately, we have to do it so that rows are committed when the user
	// clicks in the subject or the body without tabbing out of the field first
	EndEditCell();
	UnselectAllCells();
}

// Commands			
void CComposeAddressTableView::DeleteSelection()
{
	EndEditCell();
	TableIndexT currentRow;
	TableIndexT nextRow;
	nextRow = mTableSelector->GetFirstSelectedRow();
	if( nextRow == 0 )
		return;
	while ( nextRow != 0)
	{	
		currentRow = nextRow;
		RemoveRows(1, currentRow, true);
		nextRow = mTableSelector->GetFirstSelectedRow();
	}
	// Set the focus
	STableCell currentCell( currentRow, 2 );
	currentCell.col = 2 ;
	if( currentCell.row != 1 )
	{
		currentCell.row -= 1;
	}
	else if (mRows == 0 )
	{
		InsertNewRow(true, true);
		currentCell.row = 1;
	}
	StartEditCell( currentCell );
}

void CComposeAddressTableView::SetSelectionAddressType( EAddressType inAddressType )
{
	STableCell currentCell;
	STableCell currentAddressTypeCell(0,1);

	while ( GetNextSelectedCell( currentCell ))
	{	
		currentAddressTypeCell.row = currentCell.row;
		SetRowAddressType( currentAddressTypeCell.row, inAddressType );
		RefreshCell( currentCell );
		RefreshCell( currentAddressTypeCell );
	}
	BroadcastMessage( msg_AddressChanged, NULL );
} // CComposeAddressTableView::SetSelectionAddressType

// utility functions
void CComposeAddressTableView::GetNumRows(TableIndexT &inRowCount)
{
	TableIndexT colCount;
	GetTableSize(inRowCount, colCount);
}

EAddressType CComposeAddressTableView::GetRowAddressType( TableIndexT inRow )
{
	EAddressType addressType;
	Uint32 size = sizeof( addressType );
	STableCell cell( inRow,1 );
	GetCellData( cell, &addressType, size );
	return addressType;
} // CComposeAddressTableView::GetRowAddressType


void CComposeAddressTableView::SetRowAddressType( TableIndexT inRow, EAddressType inAddressType )
{
	STableCell cell( inRow,1 );
	SetCellData(cell, &inAddressType, sizeof(EAddressType));
	
} // CComposeAddressTableView::GetRowAddressType


// utility function to put a comma seperated list of
// addressees of type inAddressType in the table view
// into an LHandleStream
void CComposeAddressTableView::CreateCompHeader(EAddressType inAddressType, LHandleStream& inStream)
{
	TableIndexT rowCount, i = 1;
	EAddressType cellType;
	STableCell cell;
	Uint32 size;
	char* addr = NULL;
	Boolean first = true;

	GetNumRows(rowCount);
	try {
		do {
			cell.col = 1;
			cell.row = i;
			size = sizeof(cellType);
			GetCellData(cell, &cellType, size);
			if (cellType == inAddressType)
			{
				cell.col = 2;
				size = sizeof(addr);
				GetCellData(cell, &addr, size);
				if (addr)
				{
					if( strlen( addr ) > 0 ) // ignore cells with no data
					{
						if (!first)
							inStream << (unsigned char) ',';
						else
							first = false; 
						inStream.WriteBlock(addr, strlen(addr));
					}
				}
			}
			++i;
		} while (i <= rowCount);
		if (inStream.GetMarker() > 0)
			// write null terminator if we have data
			inStream << (unsigned char) '\0';
	} catch (...) {
		// not enough memory to make header!
	}
}

// I hate this. I need to override the default method, because we want
// to ignore the item in drags from CMailFlexTable and descendents that
// contains the selection
Boolean CComposeAddressTableView::DragIsAcceptable(DragReference inDragRef)
{
	Boolean	isAcceptable = true;
	Boolean	gotOneOrMore = false;
	
	Uint16	itemCount;
	::CountDragItems(inDragRef, &itemCount);
	
	for (Uint16 item = 1; item <= itemCount; item++)
	{
		ItemReference	itemRef;
		::GetDragItemReferenceNumber(inDragRef, item, &itemRef);
		
		// ignore flex table selection data
		if ( itemRef == CMailFlexTable::eMailNewsSelectionDragItemRefNum )
		{
			FlavorFlags flavorFlags;
		
			if (::GetFlavorFlags( inDragRef, itemRef, kMailNewsSelectionDragFlavor, &flavorFlags) == noErr )
				continue;
		}
		
		isAcceptable = ItemIsAcceptable(inDragRef, itemRef);
		if (!isAcceptable) {
			break;				// Stop looping upon finding an
		}						//   unaccepatable item
		
		gotOneOrMore = true;
	}
	
	return isAcceptable && gotOneOrMore;
}

/*
	A drag flavor being promoted by Apple for text drags containing mailto
	information. Will contain a comma-separated list of email addresses.
	One characteristic of this flavor that we don't yet support is that
	quotes within the real name part will be escaped, e.g.
	
	"Joe \"MacHead\" Bloggs"
	
	Note that the TEXT flavor for this drag contains a tab-separated list.
*/
#define kAppleMailAddressDragFlavor		'a822'

// Drag and Drop Support
Boolean CComposeAddressTableView::ItemIsAcceptable(	DragReference	inDragRef,
											ItemReference	inItemRef	)
{
	FlavorFlags flavorFlags;
	Boolean 	acceptable = false;
	
	/* This is no longer used
	if (::GetFlavorFlags( inDragRef, inItemRef, kMailAddresseeFlavor, &flavorFlags) == noErr)
		acceptable = true ;
	*/
	
	if (::GetFlavorFlags( inDragRef, inItemRef, kAppleMailAddressDragFlavor, &flavorFlags) == noErr)
		acceptable = true ;
	
	if (::GetFlavorFlags( inDragRef, inItemRef, 'TEXT', &flavorFlags) == noErr)
		acceptable |= true;
				
	return acceptable;
}	

void CComposeAddressTableView::ReceiveDragItem(	DragReference 	inDragRef, 
										DragAttributes	/*flags*/, 
										ItemReference 	inItemRef, 
										Rect& 			itemBounds)
{

/*
	// Check that correct flavor is present, check to see if the entry is not in the list yet
	if ( ::GetFlavorFlags(inDragRef, inItemRef, kMailAddresseeFlavor, &flavorFlags) == noErr )
	{
		// Get the ABID of the item being dragged, either person or list
		dataSize = sizeof( SAddressDragInfo );
		if ( ::GetFlavorData(inDragRef, inItemRef, kMailAddresseeFlavor, &info, &dataSize, 0) == noErr )
		{
			Assert_(dataSize == sizeof( SAddressDragInfo ));
			ABook* pABook;
		//	CMailComposeWindow* window = dynamic_cast<CMailComposeWindow*>(LWindow::FetchWindowObject(GetMacPort()));
			pABook = FE_GetAddressBook( NULL );
			pABook->GetFullAddress( info.dir, info.id, &fullName );
		}
	} 
	else 
	{
*/

	OSType 			dragFlavor = 0;
	FlavorFlags 	flavorFlags = 0;
	OSErr 			err = noErr;
		
	if ( ::GetFlavorFlags (inDragRef, inItemRef, emBookmarkDrag, &flavorFlags) == noErr )
		dragFlavor = emBookmarkDrag;
	else if ( ::GetFlavorFlags (inDragRef, inItemRef, kAppleMailAddressDragFlavor, &flavorFlags) == noErr )
		dragFlavor = kAppleMailAddressDragFlavor;
	else if ( ::GetFlavorFlags (inDragRef, inItemRef, 'TEXT', &flavorFlags) == noErr )
		dragFlavor = 'TEXT';
	
	if ( dragFlavor == 0)
		return;
	
	Size 		dataSize = 0;
	char 		*buffer = NULL;
	
	err = ::GetFlavorDataSize (inDragRef, inItemRef, dragFlavor, &dataSize);
	ThrowIfOSErr_ (err); // caught by PP handler
	
	if (dataSize == 0 || dataSize > 4096)
		return;
	
	DragAttributes		theDragAttributes;
	err = ::GetDragAttributes(inDragRef, &theDragAttributes);
	ThrowIfOSErr_ (err);

	buffer = (char *)XP_ALLOC(dataSize + 1);
	ThrowIfNil_(buffer);
	
	err = ::GetFlavorData (inDragRef, inItemRef, dragFlavor, buffer, &dataSize, 0);
	if (err != noErr) {
		XP_FREE(buffer);
		ThrowIfOSErr_(err);
	}
	
	buffer[dataSize] = '\0';	// Terminate the string	
	
	// we need to munge the text to take out white space etc.
	// URLs originating in Communicator are like: <URL part>/r<text part>, and we need to distinguish
	// that from plain text containing /r from outside.
	
	char	*title = NULL;
	
	if ( (theDragAttributes & kDragInsideSenderApplication) == 0)		// if an external drag
	{
		// it would be nice to user the CPasteActionSnooper here to clean stuff up, but we don't
		// have a TEHandle to work with.
		
		char	*lastChar = &buffer[dataSize - 1];
		while (lastChar > buffer && (*lastChar == '\r' || *lastChar == '\n' || *lastChar == '\t' || *lastChar == ' ') )
		{
			lastChar --;
			dataSize --;
		}
		buffer[dataSize] = '\0';	// Re-terminate the string
		
		//convert \r or \t to comma so subsequent items get separated
		lastChar = buffer;
		while (*lastChar) {
			if (*lastChar == '\r' || *lastChar == '\t')
				*lastChar = ',';
			lastChar ++;
		}
	}
	else {
		// internal drag
		title = strchr(buffer, '\r');		// may return NULL
	}
	
	char 	*addressText = NULL;
	
	if ( ( XP_STRNCASECMP( buffer, "news:", 5) == 0 ) ||
		( XP_STRNCASECMP( buffer, "snews:", 6) == 0 ) ||
		( XP_STRNCASECMP( buffer, "mailto:", 7) == 0 ) )
	{
		if (title)
			*title = '\0';	// use the URL part of the text
			
		addressText = XP_STRDUP(buffer);
	}
	else
	{
		if (title == NULL)
			title = buffer;
		else
			title ++;		// skip the /r and use the text part

		addressText = XP_STRDUP( title );
	}

	XP_FREE(buffer);
	buffer = NULL;
	
	if ( addressText != NULL )
	{
		// Find Cell which intersects itemBounds
		STableCell topLeftCell, bottomRightCell;
		FetchIntersectingCells( itemBounds, topLeftCell, bottomRightCell);
		
		TableIndexT numRows;
		GetNumRows( numRows );
		
		// Dropped in a cell which doesn't exist
		if ( topLeftCell.row > numRows )  
		{  
			InsertNewRow(false, false);
			STableCell cell(0, 1);
			
			GetNumRows(cell.row);
			
			CommitRow( addressText, cell );
		}
		else
		{
			if( topLeftCell.row == mEditCell.row)
				EndEditCell();
				
			topLeftCell.col = 2;
			char *addr = NULL;
			Int32 size = sizeof(addr);
			GetCellData(topLeftCell, &addr, size);
			Int32 stringSize = strlen( addr) + strlen(addressText) + 2;
			char *newCellStr = new char[ stringSize ];
			
			if( XP_STRCMP(addr,"\0") )
			{
				XP_STRCPY( newCellStr, addr );
				XP_STRCAT( newCellStr,"," );
				XP_STRCAT( newCellStr, addressText );
			}
			else
			{
				XP_STRCPY( newCellStr, addressText );
			}
			//SetRowAddressType(topLeftCell.row, addressType);
			CommitRow( newCellStr, topLeftCell );
			delete newCellStr;
		}	
		
		XP_FREE( addressText );
	}
}
	


void CComposeAddressTableView::AddDropAreaToWindow(LWindow* inWindow)
{
	if (!mCurrentlyAddedToDropList)
		LDropArea::AddDropArea(this, inWindow->GetMacPort());
	mCurrentlyAddedToDropList = true;
}

void CComposeAddressTableView::RemoveDropAreaFromWindow(LWindow* inWindow)
{
	if (mCurrentlyAddedToDropList)
		LDropArea::RemoveDropArea(this, inWindow->GetMacPort());
	mCurrentlyAddedToDropList = false;
}

void CComposeAddressTableView::InsideDropArea(DragReference inDragRef)
{

	Point			mouseLoc;
	SPoint32		imagePt;
	TableIndexT		newDropRow;
	Boolean			newIsDropBetweenFolders;
		
	FocusDraw();
	
	::GetDragMouse(inDragRef, &mouseLoc, NULL);
	::GlobalToLocal(&mouseLoc);
	LocalToImagePoint(mouseLoc, imagePt);

	newDropRow = mTableGeometry->GetRowHitBy(imagePt);
	
	imagePt.v += 3;
	newIsDropBetweenFolders = false;
       // (newDropRow != mTableGeometry->GetRowHitBy(imagePt));

	
	if (newDropRow != mDropRow || newIsDropBetweenFolders != mIsDropBetweenFolders)
	{
		TableIndexT	nRows, nCols;
		
		HiliteRow(mDropRow, mIsDropBetweenFolders);
		mIsDropBetweenFolders = newIsDropBetweenFolders;
		
		GetTableSize(nRows, nCols);
		if (newDropRow > 0 && newDropRow <= nRows)
		{
			HiliteRow(newDropRow, mIsDropBetweenFolders);
			mDropRow = newDropRow;
		}
		else {
			mDropRow =0;
		}
	}
}
							
void CComposeAddressTableView::LeaveDropArea(DragReference	inDragRef)
{

	FocusDraw();
	if (mDropRow != 0) {
		HiliteRow( mDropRow, false );
	}	
	mDropRow = 0;
	LDragAndDrop::LeaveDropArea( inDragRef);
}

void CComposeAddressTableView::SetTextTraits( ResIDT textTraits )
{
	mTextTraits = textTraits;
	mInputField->SetTextTraitsID(textTraits);
	Refresh();	
}


#pragma mark -

CComposeAddressTableStorage::CComposeAddressTableStorage(LTableView* inTableView) :
LTableStorage(inTableView)
{
	try {
		mAddrTypeArray = new LArray(sizeof(EAddressType));
	} catch (...) {
		mAddrTypeArray = NULL;
	}
	try {
		mAddrStrArray = new LArray(sizeof(char*));
	} catch (...) {
		mAddrStrArray = NULL;
	}
}

CComposeAddressTableStorage::~CComposeAddressTableStorage()
{
	delete mAddrTypeArray;
	// iterate over mAddrStrArray, deleting address strings
	LArrayIterator iter(*mAddrStrArray, LArrayIterator::from_End);
	char* str;
	while (iter.Previous(&str))
		delete [] str;
	// now we can delete address string array
	delete mAddrStrArray;
}

// ---------------------------------------------------------------------------
//		е SetCellData
// ---------------------------------------------------------------------------
//	Store data for a particular Cell

void
CComposeAddressTableStorage::SetCellData(
	const STableCell	&inCell,
	const void			*inDataPtr,
	Uint32				inDataSize)
{
	if ( inCell.col == 1 )
		mAddrTypeArray->AssignItemsAt(1, inCell.row, inDataPtr, inDataSize);
	else
	{	// copy address string and store string in mAddrStrArray
		const char* inStr = (const char*)inDataPtr;
		size_t strLength = strlen(inStr);
		char* str = new char[strLength + 1];
		::BlockMoveData(inStr, str, strLength);
		str[strLength] = '\0';
		char* current;
		// if there's already a string in mAddrStrArray, delete it
		if (mAddrStrArray->FetchItemAt(inCell.row, &current))
			delete [] current;
		mAddrStrArray->AssignItemsAt(1, inCell.row, &str, sizeof(str));
	}
}

// ---------------------------------------------------------------------------
//		е GetCellData
// ---------------------------------------------------------------------------
//	Retrieve data for a particular Cell
//
//	If outDataPtr is nil, pass back the size of the Cell data
//
//	If outDataPtr is not nil, it must point to a buffer of at least
//	ioDataSize bytes. On output, ioDataSize is set to the minimum
//	of the Cell data size and the input value of ioDataSize and that
//	many bytes are copied to outDataPtr.

void
CComposeAddressTableStorage::GetCellData(
	const STableCell	&inCell,
	void				*outDataPtr,
	Uint32				&ioDataSize) const
{
	LArray* array = (inCell.col == 1) ? mAddrTypeArray : mAddrStrArray;
	if (outDataPtr == nil)
	{
		ioDataSize = array->GetItemSize(inCell.row);
	}
	else
	{
		array->FetchItemAt(inCell.row, outDataPtr, ioDataSize);
	}
}


// ---------------------------------------------------------------------------
//		е FindCellData
// ---------------------------------------------------------------------------
//	Pass back the Cell containing the specified data. Returns whether
//	or not such a Cell was found.
//	For a CComposeAddressTableView, I assume the address string column is the only
//	interesting data to find

Boolean
CComposeAddressTableStorage::FindCellData(
	STableCell	&outCell,
	const void	*inDataPtr,
	Uint32		inDataSize) const
{
	Boolean	found = false;
	
	Int32	dataIndex = mAddrStrArray->FetchIndexOf(inDataPtr, inDataSize);
	
	if (dataIndex != LArray::index_Bad) {
		outCell.col = 2;
		outCell.row = dataIndex;
		found = true;
	}
	
	return found;
}


// ---------------------------------------------------------------------------
//		е InsertRows
// ---------------------------------------------------------------------------
//	Insert rows into an ArrayStorage.
//
//	inDataPtr points to the data for the new cells. Each new cell will
//		have the same data.

void
CComposeAddressTableStorage::InsertRows(
	Uint32		inHowMany,
	TableIndexT	inAfterRow,
	const void*	/*inDataPtr*/,
	Uint32		/*inDataSize*/)
{
	EAddressType type = eToType;
	mAddrTypeArray->InsertItemsAt(inHowMany, inAfterRow + 1, &type, sizeof(type));
	char *nothing = NULL;
	mAddrStrArray->InsertItemsAt(inHowMany, inAfterRow + 1, &nothing, sizeof(nothing));
}


// ---------------------------------------------------------------------------
//		е RemoveRows
// ---------------------------------------------------------------------------
//	Removes rows from an ArrayStorage

void
CComposeAddressTableStorage::RemoveRows(
	Uint32		inHowMany,
	TableIndexT	inFromRow)
{
	mAddrTypeArray->RemoveItemsAt(inHowMany, inFromRow);
	// deallocate strings in mAddrStrArray
	char *string = NULL;
	Uint32	itemSize = sizeof(string);
	for (TableIndexT i = 0; i < inHowMany; i++)
	{
		mAddrStrArray->FetchItemAt(inFromRow + i, &string, itemSize);
		if (string)
			delete [] string;
		mAddrStrArray->RemoveItemsAt(inHowMany, inFromRow);
	}
}


// ---------------------------------------------------------------------------
//		е GetStorageSize
// ---------------------------------------------------------------------------
//	Pass back the number of rows and columns represented by the data
//	in an ArrayStorage

void
CComposeAddressTableStorage::GetStorageSize(
	TableIndexT		&outRows,
	TableIndexT		&outCols)
{
		// An Array is one-dimensional. By default, we assume a
		// single column with each element being a sepate row.

	outRows = mAddrStrArray->GetCount();
	outCols = 2;
	if (outRows == 0) {
		outCols = 0;
	}
}
