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



// SearchHelpers.h

#pragma once

/*======================================================================================
	DESCRIPTION:	Defines classes related to the Mail & News search dialog.
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CDateView.h"
#include "CGAIconPopup.h"
#include "CSaveWindowStatus.h"
#include "LTableRowSelector.h"
#include "CMailFlexTable.h"

#include <LGAEditField.h>
#include <LGACheckbox.h>

// FE Stuff

#include "macutil.h"

class CFolderScopeGAPopup;
class CSearchPopupMenu;
class LBroadcasterEditField;
class CSearchDateField;
class CSearchTabGroup;
class LTableView;
class CString;

#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/

typedef struct MSG_SearchMenuItem MSG_SearchMenuItem;


#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark EXTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/

inline Boolean IsBetween(Int32 inVal, Int32 inMin, Int32 inMax) { 
	return ((inVal >= inMin) && (inVal <= inMax)); 
}

#ifdef DEBUGGER_ASSERTIONS
	#undef AssertFail_
	#undef Assert_
	#ifdef Debug_Signal
		#define AssertFail_(test)		ThrowIfNot_(test)
		#define Assert_(test)			do { 						\
											if ( !(test) ) {		\
												Try_ {				\
													Throw_(noErr);	\
												}					\
												Catch_(inErr) {		\
												}					\
												EndCatch_			\
											}						\
										} while ( 0 )
	#else // Debug_Signal
		#define AssertFail_(test)		((void) 0)
		#define Assert_(test)			((void) 0)
	#endif // Debug_Signal
#endif // DEBUGGER_ASSERTIONS

#undef FailNILRes_
#define FailNILRes_(res)			ThrowIfResFail_(res)
#undef FailResErr_
#define FailResErr_()				ThrowIfResError_()


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

// USearchHelper

class USearchHelper {

public:
													
	// Indexes into STR#8600
	
	enum { 
		uStr_AllMailFolders = 1
	,  	uStr_AllNewsgroups
	,  	uStr_AllMailFoldersNewsgroups
	,  	uStr_OneMessageFoundSelected
	,  	uStr_NoMessagesFound
	,  	uStr_MultipleMessagesFound
	,  	uStr_OneMessageFound
	,  	uStr_MultipleMessagesSelected
	,  	uStr_SelectedMailFolders
	,  	uStr_SelectedNewsgroups
	,  	uStr_SelectedMailFoldersNewsgroups
	,  	uStr_InFrontFolder
	,  	uStr_SearchForMessages
	,  	uStr_SearchForAddresses
	,  	uStr_FilterDefaultName
	,  	uStr_FilterDefaultDescription
	,	uStr_SearchFunctionNotImplemented
	,  	uStr_SearchFunctionParamError
	,	uStr_ExceptionErrorOccurred
	,	uStr_FilterFunctionNotImplemented
	,  	uStr_FilterFunctionParamError
	,	uStr_SearchFunctionNothingToSearch
	};

	static void				RegisterClasses();

	static LControl			*FindViewControl(LView *inView, PaneIDT inID);
	static LPane			*FindViewSubpane(LView *inView, PaneIDT inID);
	static LPane			*FindViewVisibleSubpane(LView *inView, PaneIDT inID);
	static LView			*FindViewSubview(LView *inView, PaneIDT inID);
	static LBroadcasterEditField	*FindViewEditField(LView *inView, PaneIDT inID);
	static CSearchPopupMenu	*FindSearchPopup(LView *inView, PaneIDT inID);
	static CFolderScopeGAPopup	*FindFolderScopePopup(LView *inView, PaneIDT inID);
	static CSearchDateField	*FindViewDateField(LView *inView, PaneIDT inID);
	static CSearchTabGroup	*FindWindowTabGroup(LArray *inSubcommanders);

	static void				RemoveSizeBoxFromVisRgn(LWindow *inWindow);
	static void				RefreshPortFrameRect(LPane *inPane, Rect *inPortRect);
	static void				RefreshLocalFrameRect(LPane *inPane, Rect *inLocalRect);

	static LWindow			*GetPaneWindow(LPane *inPane);
	static void				LinkListenerToBroadcasters(LView *inContainer, LListener *inListener);
	static Int16			GetEditFieldLength(LEditField *inEditField);
	static void				AssignUString(Int16 inStringIndex, CString& outString);
	static void				EnableDisablePane(LPane *inPane, Boolean inEnable, Boolean inRefresh = true);
	static void				ShowHidePane(LPane *inPane, Boolean inDoShow);
	static void				SetPaneDescriptor(LPane *inPane, const CStr255& inDescriptor);
	static void				SetDefaultButton(LView *inView, PaneIDT inID, Boolean inIsDefault = true);
	static void				SelectEditField(LEditField *inEditField);
	static void				SelectDateView(CDateView *inDateView);
	static Boolean			LastPaneWasActiveClickPane();
	static void				CalcDiscreteWindowResizing(LWindow *inWindow, Rect *ioNewBounds, 
											 	   	   const Int16 inHIncrement, const Int16 inVIncrement, 
											 	   	   Boolean inCheckDesktop = true, Int16 inSizeBoxLeft = 15,
											   	   	   Int16 inSizeBoxTop = 15);
	static void				ScrollViewBits(LView *inView, Int32 inLeftDelta, Int32 inTopDelta);
	
							enum {
								char_OptionKey = 0x003A
							,	char_ControlKey = 0x003B
							,	char_CommandKey = 0x0037
							,	char_ShiftKey = 0x0038
							};
	static Boolean			KeyIsDown(Int16 inKeyCode);
	static void				GenericExceptionAlert(OSErr inErr);


							enum {
								paneID_Okay = 'OKAY'				// OK button
							,	paneID_Cancel = 'CNCL'				// Cancel button
							};
	static Boolean			HandleDialogKey(LView *inView, const EventRecord &inKeyEvent);

	static Boolean			MenuItemIsEnabled(MenuHandle inMenuH, Int16 inItem) {
								return ((**inMenuH).enableFlags & (1L<<(inItem - 0)));
							}

#ifdef NOT_YET
	static void			GetFolderDisplayName(const char *inFolderName, 
											  	  CStr255& outFolderName);
#endif // NOT_YET
	
private:

	// Instance variables ==========================================================
	
	static CQDProcs		sStdProcs;
};
					  
////////////////////////////////////////////////////////////////////////////////////////////
// CSearchPopupMenu

class CSearchPopupMenu : public CGAIconPopup {


	typedef CGAIconPopup Inherited;

					  
public:

						enum { class_ID = 'SPUP' };

						CSearchPopupMenu(LStream *inStream);
						
	Boolean 			IsLatentVisible() {
							return ((mVisible == triState_Latent) || (mVisible == triState_On));
						}
						
						enum { cmd_InvalidMenuItem = -1, cmd_DividerMenuItem = -2 };

	virtual	CommandT	GetCurrentItemCommandNum(Int16 inMenuItem = 0);														
	Int16				GetCommandItemIndex(CommandT inMenuCommand);														
	void				SetCurrentItemByCommand(CommandT inMenuCommand);
	Int16				GetNumCommands() {
							return mCommands.GetCount();
						}													
	CommandT 			GetOldItemCommand() const;

	virtual void		SetValue(Int32 inValue);
	void				SetToDefault() {
							SetValue(mDefaultItem);
						}

	void				PopulateMenu(MSG_SearchMenuItem *inMenuItems, const Int16 inNumItems,
									 CommandT inNewCommandIfInvalid, UInt32 inPopulateID = 0);
	void				ClearMenu(Int16 inStartItem = 1);
	void				AppendMenuItemCommand(CommandT inCommand, ConstStr255Param inName, Boolean inIsEnabled);
	void				SetMenuItemCommand(CommandT inOldCommand, CommandT inNewCommand,
										   ConstStr255Param inName, Boolean inIsEnabled);
	void				SetIndexCommand(Int16 inIndex, CommandT inCommand) {
							mCommands.AssignItemsAt(1, inIndex, &inCommand);
						}
	void				RemoveMenuItemCommand(CommandT inRemoveCommand, 
											  CommandT inNewCommandIfInvalid);
// unused code - 11/21/97
//
//	Boolean				UpdateMenuItemCommand(Int16 inMenuItem, CommandT inNewCommand,
//											  ConstStr255Param inName, Boolean inIsEnabled);
// 	void				SetCurrentItemByName(ConstStr255Param inName);

protected:

	virtual void		FinishCreateSelf();
	virtual void		BroadcastValueMessage();
	virtual Boolean		TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers);

	// Instance variables ==========================================================
	
	LArray				mCommands;
	UInt32 				mLastPopulateID;
	Int16 				mDefaultItem;
	Int16 				mOldValue;

}; // CSearchPopupMenu

// CSearchEditField

class CSearchEditField : public LGAEditField {


	typedef LGAEditField Inherited;

					  
public:

						enum { class_ID = 'SEFD' };
						
						// Broadcast messages
						
						enum { 
							msg_UserChangedText = 'cTxt' 	// PaneIDT *
						};

						CSearchEditField(LStream *inStream) : 
								   	 	 LGAEditField(inStream) {
							SetRefreshAllWhenResized(false);
						}
						
	Int16				GetTextLength() {
							return (**mTextEditH).teLength;
						}
	Boolean 			IsLatentVisible() {
							return ((mVisible == triState_Latent) || (mVisible == triState_On));
						}
	virtual void		UserChangedText() {
							PaneIDT paneID = mPaneID;
							BroadcastMessage(msg_UserChangedText, &paneID);
						}
	virtual void		SetDescriptor(ConstStr255Param inDescriptor);
	
	void				SetText(const char *inText);
	char				*GetText(char *outText);
	virtual void		ResizeFrameBy(Int16 inWidthDelta, Int16 inHeightDelta, 
									  Boolean inRefresh);
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);
						
protected:

};


// CSearchDateField

class CSearchDateField : public CDateView {
					  
public:

						enum { class_ID = 'SDFD' };

						CSearchDateField(LStream *inStream) : 
								   	 	 CDateView(inStream) {
						}
						
	Boolean 			IsLatentVisible() {
							return ((mVisible == triState_Latent) || (mVisible == triState_On));
						}
						
protected:

};


// CSearchTabGroup

class CSearchTabGroup : public LTabGroup {

public:

	// Stream creator method

						enum { class_ID = 'SrTg' };
						
						CSearchTabGroup(LStream* /*inStream*/) :
										LTabGroup(),
										mCanRotate(nil) {
						}
						CSearchTabGroup() :
										LTabGroup(),
										mCanRotate(nil) {
						}
						
	void				SetRotateWatchValue(Boolean *inWatchValue) {
							mCanRotate = inWatchValue;
						}
	virtual void		RotateTarget(Boolean inBackward);

protected:
	
	// Instance variables ==========================================================

	Boolean 			*mCanRotate;
};


// CSearchCaption

class CSearchCaption : public LCaption {

public:

	// Stream creator method

						enum { class_ID = 'SrCp' };
						
						CSearchCaption(LStream *inStream) :
									   LCaption(inStream) {
						}
						
protected:
	
	virtual void		DrawSelf();
	virtual void		EnableSelf() {
							Draw(nil);
						}
	virtual void		DisableSelf() {
							Draw(nil);
						}
};

// StTableClipRgnState

class StSectClipRgnState {

public:

						StSectClipRgnState(const Rect *inLocalRect, const Rect *inDisplayRect = nil);
						~StSectClipRgnState();
						
	static Boolean		RectEnclosesRect(const Rect *inEnclosingRect, const Rect *inInsideRect);
						
protected:
	
	// Instance variables ==========================================================

	static RgnHandle 	sClipRgn;
	static Boolean		sSetClip;
#ifdef Debug_Signal
	static Boolean 		sTableClipRgnAround;
#endif // Debug_Signal
};

#if 0
// CRowTableSelector

class CRowTableSelector : public LTableRowSelector {

public:

						CRowTableSelector(LTableView *inTableView, Boolean inAllowMultiple = true) :
										  LTableRowSelector(inTableView, inAllowMultiple),
										  mRowCount(0) {
						
							mItemSize = sizeof(UInt32);
							mComparator = LLongComparator::GetComparator();
							mKeepSorted = true;
						}
	
	virtual Boolean		CellIsSelected(const STableCell	&inCell) const;
	virtual TableIndexT	GetSelectedIndices(TableIndexT **outIndices, Int16 inBase) const;
	virtual TableIndexT	GetFirstSelectedRow() const;
	UInt32				GetRowCount() const { return mRowCount; }

protected:

						enum { eInvalidID = 0xFFFFFFFF };
	virtual UInt32		GetRowUniqueID(const TableIndexT inRow) const = 0L;
	virtual TableIndexT	GetUniqueIDRow(const UInt32 inID) const = 0L;

	virtual void		DoSelect(const TableIndexT inRow, Boolean inSelect, 
								 Boolean inHilite, Boolean inNotify);
	virtual void		InsertRows(UInt32 inHowMany, TableIndexT /*inAfterRow*/) {
							mRowCount += inHowMany;
						}
	virtual void		RemoveRows(Uint32 inHowMany, TableIndexT inFromRow);
	
	// Instance variables ==========================================================

	UInt32				mRowCount;	// Count of the number of rows in the table. Need this
									// since RemoveRows() is called after rows have been removed!
};
#endif //0
#if 0 //0
// CExTable

class CExTable : public CMailFlexTable {

private:
	typedef CMailFlexTable Inherited;

public:

						CExTable(LStream *inStream) :
								CMailFlexTable(inStream),
								mFlags(0),
								mBlackAndWhiteHiliting(false) {
						
						}
	
	Boolean				CellsAreSelected() {
							return IsValidCell(GetFirstSelectedCell());
						}
	TableIndexT			GetNumRows() {
							return mRows;
						}
	void				SelectScrollCell(const STableCell &inCell) {
							SelectCell(inCell);
							ScrollCellIntoFrame(inCell);						
						}

	void				RefreshRowRange(TableIndexT inStartRow, TableIndexT inEndRow) {
							TableIndexT topRow, botRow;
							if ( inStartRow < inEndRow ) {
								topRow = inStartRow; botRow = inEndRow;
							} else {
								topRow = inEndRow; botRow = inStartRow;
							}
							RefreshCellRange(STableCell(topRow, 1), STableCell(botRow, mCols));
						}
	virtual void		ReadSavedTableStatus(LStream *inStatusData);
	virtual void		WriteSavedTableStatus(LStream *outStatusData);
	virtual Boolean		CellHasDropFlag(const STableCell &/*inCell*/, Boolean &/*outIsExpanded*/) const { return false; }											

protected:

						enum {	// Flags
							flag_DontDrawHiliting = 0x01
						,	flag_IsDrawingSelf = 0x02
						,	flag_IsLoosingTarget = 0x04
						};

	virtual MSG_Pane	*GetBEPane() const = 0L;
	virtual Boolean		IsMyPane(void* info) const {
							return (GetBEPane() == reinterpret_cast<SMailCallbackInfo *>(info)->pane);
						}

	virtual void		FindCommandStatus(CommandT inCommand, Boolean &outEnabled, Boolean &outUsesMark,
										  Char16 &outMark, Str255 outName);
	virtual Boolean		ObeyCommand(CommandT inCommand, void *ioParam = nil);

	virtual void		ScrollBits(Int32 inLeftDelta, Int32 inTopDelta);
	virtual void		DrawSelf();
	virtual void		ActivateSelf();
	virtual void		DeactivateSelf();
	virtual void		BeTarget();
	virtual void		DontBeTarget();
	virtual void		HiliteSelection(Boolean inActively, Boolean inHilite);
	virtual void		HiliteCellInactively(const STableCell &inCell, Boolean inHilite);
	virtual void		HiliteCellActively(const STableCell &inCell, Boolean inHilite);
	
	Boolean				GetBlackAndWhiteHiliting() {
							return mBlackAndWhiteHiliting;
						}
	Boolean				DrawHiliting() {
							return !(mFlags & flag_DontDrawHiliting);
						}
	Boolean				DrawFullHiliting() {
							return !(mFlags & flag_IsLoosingTarget);
						}
						
	void				FocusDrawSelectedCells(Boolean inHilite, const STableCell *inCell = nil);
	void				DrawCellRow(STableCell &inCell, Boolean inErase, Boolean inActive);
	void				PlaceHilitedTextInRect(const char *inText, Uint32 inTextLength, const Rect &inRect,
									  		   Int16 inHorizJustType, Int16 inVertJustType, 
									 		   const FontInfo *inFontInfo, Boolean inDoTruncate,
									  		   TruncCode inTruncWhere);
	Boolean				GetVisibleRowRange(TableIndexT *inStartRow, TableIndexT *inEndRow);

	// Backend methods
#if 0
	virtual void		MsgPaneChanged() = 0L;
	virtual Int32		GetBENumRows() = 0L;
	virtual void		ChangeStarting(MSG_Pane *inPane, MSG_NOTIFY_CODE inChangeCode,
							 		   TableIndexT inStartRow, SInt32 inRowCount);
	virtual void		ChangeFinished(MSG_Pane *inPane, MSG_NOTIFY_CODE inChangeCode,
							 		   TableIndexT inStartRow, SInt32 inRowCount);
#endif
	// Instance variables ==========================================================

	UInt8 				mFlags;
	Boolean				mBlackAndWhiteHiliting;
};

#endif
