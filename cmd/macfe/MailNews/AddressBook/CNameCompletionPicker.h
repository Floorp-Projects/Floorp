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


// CNameCompletionPicker.h

#pragma once

#include "abcom.H"
#ifdef MOZ_NEWADDR
#include "CMailNewsWindow.h"
//¥#include "CAddressBookViews.h"
#include "CMailFlexTable.h"
#include "msgcom.h"
#include "PascalString.h"

//----------------------------------------------------------------------------
//		¥ CNameCompletionTable class
//
//----------------------------------------------------------------------------
//
class CNameCompletionTable : public CMailFlexTable
{
public:
	typedef CMailFlexTable Inherited;
	enum { class_ID = 'NcVw', pane_ID = 'Tabl' };

	enum EColType {
		eTableHeaderBase = 'Col0'	// Columns are called 'Col0', 'Col1', 'Col2' etc...
	};								// but 'Col0' is not used (it used to contain the person/list icon)

public:
					CNameCompletionTable(LStream *inStream);
	virtual			~CNameCompletionTable();
	void			SetColumnHeaders();

	virtual void		DeleteSelection() {};
	virtual void		SelectionChanged();	
	virtual	void		OpenRow(TableIndexT inRow);
	virtual void		ReceiveMessagePane(MSG_Pane* inMsgPane) {SetMessagePane(inMsgPane);};
	virtual	void		DestroyMessagePane( MSG_Pane* inPane );

protected:
	virtual void 		GetCellDisplayData(const STableCell &inCell, ResIDT &ioIcon, CStr255 &ioDisplayString );					
	virtual void		DrawCellContents(	
								const STableCell		&inCell,
								const Rect				&inLocalRect);																
};


//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker class
//
//----------------------------------------------------------------------------
//
static const UInt16 cNameCompletionSaveWindowStatusVersion = 	0x0200;

class CNameCompletionPicker :	public CMailNewsWindow
							,	public LListener
{
public:
	typedef	CMailNewsWindow Inherited;

	enum { class_ID = 'NcWn', pane_ID = class_ID, res_ID = 8970 };

	enum {
		paneID_OkButton				= 'BtOk',
		paneID_CancelButton			= 'Canc',
		paneID_NameCompletionTable = 'Tabl'
		};

	// Dialog handler
	static int		DisplayDialog(LEditField* inEditField, MSG_Pane* inPane, CMailNewsContext* inContext, int inNumResults);

	// Stream creator method
						CNameCompletionPicker(LStream *inStream);
	virtual				~CNameCompletionPicker();
	virtual ResIDT		GetStatusResID() const { return res_ID; }
	virtual CNSContext* CreateContext() const { return (CNSContext*)mLastReceivedContext; }
	virtual void		CalcStandardBoundsForScreen(
							const Rect 	&inScreenBounds,
							Rect 		&outStdBounds) const;
	virtual void		DoClose();
	virtual void		ListenToMessage(
							MessageT		inMessage,
							void			*ioParam);


protected:

	// Overriden methods
	virtual void		FinishCreateSelf();
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

	// Utility methods
	virtual CMailFlexTable* GetActiveTable() { return mNameCompletionTable; }
	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
	virtual UInt16		GetValidStatusVersion() const { return cNameCompletionSaveWindowStatusVersion;}
	virtual void		ReceiveComposeWindowParameters() {
							mEditField	= mLastReceivedEditField;
							mPickerPane = mLastReceivedPickerPane;
							mContext	= mLastReceivedContext;
							mNumResults	= mLastReceivedNumResults;
						}

	// Instance variables
	CNameCompletionTable	*mNameCompletionTable;
	LEditField*				mEditField;
	MSG_Pane*				mPickerPane;
	CMailNewsContext*		mContext;
	int						mNumResults;

private:
	static LEditField*			mLastReceivedEditField;
	static MSG_Pane*			mLastReceivedPickerPane;
	static CMailNewsContext*	mLastReceivedContext;
	static int					mLastReceivedNumResults;
};
#endif //MOZ_NEWADDR
