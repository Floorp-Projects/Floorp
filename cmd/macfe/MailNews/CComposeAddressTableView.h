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



// CComposeAddressTableView.h

#pragma once

#include "abcom.h"
#include <LListener.h>
#include <LCommander.h>
#include <LTableView.h>
#include <UTableHelpers.h>
#include "MailNewsAddressBook.h"
#include "CTSMEditField.h"
#include "MailNewsCallbacks.h"
#include "CMailNewsContext.h"

#define kAddressTypeMenuID 10611

typedef enum {
	eNoAddressType = 0,
	eToType,
	eCcType,
	eBccType,
	eReplyType,
	eNewsgroupType,
	eFollowupType
} EAddressType;

class ABook;
typedef struct DIR_Server DIR_Server;

#ifdef MOZ_NEWADDR
//======================================
class CMailAddressEditField	:	public CTSMEditField
							,	public CMailCallbackListener
//======================================
{
private:
	typedef 	CTSMEditField	Inherited;

public:
	enum { class_ID = 'Aedt' };

						CMailAddressEditField( LStream* inStream );
	virtual				~CMailAddressEditField();

	void 				Init();
	virtual void		FinishCreateSelf();
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

	virtual void		SpendTime(const EventRecord& inMacEvent);
	virtual void		UserChangedText(); 	// override to do address-book name completion.
	virtual void		StartNameCompletion();
	virtual void		SetNameCompletionResults(
								int			numResults,
								char*		displayString,
								char*		headerString,
								char*		expandHeaderString);
	virtual void 		PaneChanged(
								MSG_Pane*						inPane,
								MSG_PANE_CHANGED_NOTIFY_CODE	inNotifyCode,
								int32 							value);

	const char*			FinalizeEdit(); 	// user has tabbed out, etc.  Returns Mallocked string.

protected:
	static int 			NameCompletionBECallbackFunction(
								AB_NameCompletionCookie*	cookie,
								int							numResults,
								void*						FECookie);


//-----
// Data
//-----
protected:
	UInt32				mTimeLastCall;
	Boolean				mIsCompletedName;
	Boolean				mNeedToAutoCompleteAtIdleTime;
	CMailNewsContext*	mMailNewsContext;
	MSG_Pane*			mPickerPane;

	int					mNumResults;
	char*				mDisplayString;
	char*				mHeaderString;
	char*				mExpandHeaderString;
};

#else //MOZ_NEWADDR

//======================================
class CMailAddressEditField	:	public CTSMEditField
							//	,public LBroadcaster 
//======================================
{
public:
	enum { class_ID = 'Aedt' };
						CMailAddressEditField( LStream* inStream );
	virtual void		FinishCreateSelf();
	virtual void		UserChangedText(); // override to do address-book name completion.
	const char*			FinalizeEdit(); // user has tabbed out, etc.  Returns Mallocked string.
	virtual Boolean		HandleKeyPress( const EventRecord	&inKeyEvent );
	virtual void		SpendTime( const EventRecord&	 inMacEvent );
	void 				Init();
//protected:
//	virtual void		TakeOffDuty();

//-----
// Data
//-----
protected:
	UInt32				mTimeLastCall;
	ABook*				mAddressBook;
	DIR_Server* 		mDirServerList;
	Boolean				mIsCompletedName;
	Boolean				mCheckAddress;
	ABID				mEntryID;
	
};
#endif //MOZ_NEWADDR


const MessageT msg_AddressChanged ='AdCh';

//======================================
class CComposeAddressTableView :	public LTableView,
									public LCommander,
									//public LListener,
									public LDragAndDrop,
									public LBroadcaster
//======================================
{
private:
	typedef 	LTableView	Inherited;
	
public:
	enum { class_ID = 'AdTV' };
						CComposeAddressTableView(LStream* inStream);
	
	virtual				~CComposeAddressTableView() ;
	
	virtual void		FinishCreateSelf();
	virtual Boolean		ObeyCommand(CommandT inCommand, void *ioParam = nil);
	
	void				SetUpTableHelpers();
	
	void				AdjustColumnWidths();
	virtual void		ResizeFrameBy( Int16 inWidthDelta, Int16 inHeightDelta,
										 Boolean inRefresh);

	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

	virtual Boolean		ClickSelect( const STableCell &inCell, 
									 const SMouseDownEvent &inMouseDown );
	virtual void		ClickSelf( const SMouseDownEvent	&inMouseDown );
	// virtual void		ListenToMessage(MessageT inMessage, void* ioParam );

	void				InsertNewRow(Boolean inRefresh, Boolean inEdit );
	void				InsertNewRow(EAddressType inAddressType,
									 const char* inAddress, Boolean inEdit = false);

	void 				FillInRow( Int32 row, EAddressType inAddressType, const char* inAddress);
	void				StartEditCell(const STableCell &inCell);
	void				EndEditCell();
	STableCell			GetEditCell() {return mEditCell;}

	void				CreateCompHeader(EAddressType inAddressType, LHandleStream& inStream);
	// Drag and Drop
	virtual	void 		InsideDropArea( DragReference inDragRef);
	virtual void		LeaveDropArea(DragReference	inDragRef);
	virtual Boolean		DragIsAcceptable(DragReference inDragRef);
	virtual Boolean 	ItemIsAcceptable(DragReference	inDragRef, ItemReference	inItemRef );

	virtual void		ReceiveDragItem(	DragReference 	inDragRef, 
									DragAttributes	flags, 
									ItemReference 	inItemRef, 
									Rect& 			itemBounds);
	void				HiliteRow(	TableIndexT	inRow, Boolean inUnderline );
		
	// utility functions for new compose window because attach view
	// is inside a tab switcher
	void				AddDropAreaToWindow(LWindow* inWindow);
	void				RemoveDropAreaFromWindow(LWindow* inWindow);
	// Commands			
	void 				DeleteSelection();
	void				SetSelectionAddressType( EAddressType inAddressType );
	// utility functions
	void				GetNumRows(TableIndexT &inRowCount);
	EAddressType		GetRowAddressType( TableIndexT inRow );
	void 				SetRowAddressType( TableIndexT inRow, EAddressType inAddressType );
	
	void 				SetTextTraits( ResIDT textTraits );
	
protected:
	
	void				DirectInputToAddressColumn();
	void				StopInputToAddressColumn();
	
	virtual void		ClickCell(const STableCell &inCell,
								  const SMouseDownEvent &inMouseDown);

	virtual void		DrawCell(const STableCell &inCell,
								 const Rect &inLocalRect);

	virtual void		DrawSelf();
	


	Int32				FinalizeAddrCellEdit();
	Int32				CommitRow( const char* inString, STableCell cell);
	void				HideEditField();

	virtual void		TakeOffDuty();

	virtual Uint16		CalculateAddressTypeColumnWidth();

//------
// data
//------
protected:

	CMailAddressEditField*	mInputField;
	STableCell			mEditCell;
	Boolean				mCurrentlyAddedToDropList;
	Boolean				mAddressTypeHasFocus;
	char				*mTypedownTable;
	RGBColor			mDropColor;
	TableIndexT			mDropRow;	
	Boolean				mIsDropBetweenFolders;	// changing order
	Boolean				mDirty;
	ResIDT 				mTextTraits;
};


//======================================
class CComposeAddressTableStorage	:	public LTableStorage
//======================================
{
public:
						CComposeAddressTableStorage(LTableView* inTableView);
	virtual				~CComposeAddressTableStorage();

	virtual void		SetCellData(
								const STableCell	&inCell,
								const void			*inDataPtr,
								Uint32				inDataSize);
	virtual void		GetCellData(
								const STableCell	&inCell,
								void				*outDataPtr,
								Uint32				&ioDataSize) const;
	virtual Boolean		FindCellData(
								STableCell			&outCell, 
								const void			*inDataPtr,
								Uint32				inDataSize) const;
	virtual void		InsertRows(
								Uint32				inHowMany,
								TableIndexT			inAfterRow,
								const void			*inDataPtr,
								Uint32				inDataSize);
	virtual void		InsertCols(

								Uint32				/* inHowMany */,
								TableIndexT			/* inAfterCol */,
								const void*			/* inDataPtr */,
								Uint32				/* inDataSize */) { };
	virtual void		RemoveRows(
								Uint32				inHowMany,
								TableIndexT			inFromRow);
	virtual void		RemoveCols(
								Uint32				/* inHowMany */,
								TableIndexT			/* inFromCol */) { };
	virtual void		GetStorageSize(
								TableIndexT			&outRows,
								TableIndexT			&outCols);

protected:
	LArray*				mAddrTypeArray;
	LArray*				mAddrStrArray;
};

