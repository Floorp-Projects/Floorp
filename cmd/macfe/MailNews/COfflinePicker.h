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



// COfflinePicker.h

#pragma once

#include "CMailNewsWindow.h"
#include "CSimpleFolderView.h"


//------------------------------------------------------------------------------
//		¥ COfflinePickerView
//------------------------------------------------------------------------------
//
class COfflinePickerView :	public CSimpleFolderView
{
	friend class COfflineItem;

private:
	typedef CSimpleFolderView Inherited;

public:
	enum { class_ID = 'ofVW' };

						COfflinePickerView(LStream *inStream);
	virtual				~COfflinePickerView();

	//-----------------------------------
	// LDAP folders
	//-----------------------------------
protected:
	typedef enum { kRowMailNews = 1, kRowLDAPHdr, kRowLDAP} RowType;

	typedef struct SSaveItemRec
	{
		RowType	itemType;
		void *	itemInfo;
		UInt32	originalPrefsFlags;
	} SSaveItemRec;

	virtual RowType		GetRowType(TableIndexT inRow) const;
	virtual UInt16		CountLDAPItems() const {return mLDAPCount;};
	virtual Boolean		IsLDAPExpanded() const {return mLDAPExpanded;};
	virtual	void		AppendLDAPList();
	virtual void		SaveItemPrefFlags(const COfflineItem * inOfflineItem, UInt32 inPrefsFlags);

public:
	virtual void		CancelSelection();
	virtual void		CommitSelection();

	//-----------------------------------
	// Command implementation
	//-----------------------------------
public:
	virtual void		DrawCell(
								const STableCell		&inCell,
								const Rect				&inLocalRect);
	virtual Boolean		ClickSelect(
								const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown);
	virtual void		ClickCell(
								const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown);

	//-----------------------------------
	// Commands
	//-----------------------------------

	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

	//-----------------------------------
	// Drawing (Overrides of CSimpleFolderView)
	//-----------------------------------
	virtual Boolean		TableDesiresSelectionTracking ( ) { return false; }
	virtual ResIDT		GetIconID(TableIndexT inRow) const;
	virtual UInt16		GetNestedLevel(TableIndexT inRow) const;

	virtual void		ApplyTextStyle(TableIndexT inRow) const;

	virtual Boolean		CellHasDropFlag(const STableCell& inCell, Boolean& outIsExpanded) const;
	virtual void		SetCellExpansion(const STableCell& inCell, Boolean inExpand);
	virtual void		GetMainRowText(
									TableIndexT			inRow,
									char*				outText,
									UInt16				inMaxBufferLength) const;

	virtual void		ChangeFinished(
									MSG_Pane*		inPane,
									MSG_NOTIFY_CODE	inChangeCode,
									TableIndexT		inStartRow,
									SInt32			inRowCount);

	// ------------------------------------------------------------
	// QA Partner support
	// ------------------------------------------------------------
#if defined(QAP_BUILD)		
public:
	virtual void		GetQapRowText(TableIndexT inRow, char* outText, UInt16 inMaxBufferLength) const;
#endif

	//-----------------------------------
	// Data
	//-----------------------------------
protected:
	Boolean		mWantLDAP;
	XP_List *	mLDAPList;
	UInt16		mLDAPCount;
	Boolean		mLDAPExpanded;
	Str63		mLDAPHdrStr;

	LArray		mSaveItemArray;
};


//------------------------------------------------------------------------------
//		¥ COfflinePickerWindow
//------------------------------------------------------------------------------
//
class COfflinePickerWindow :	public CMailNewsWindow,
								public LListener
{
private:
	typedef CMailNewsWindow Inherited;

public:
	enum { class_ID = 'ofWN', res_ID = 20003};

	enum {
		paneID_OkButton			= 'BtOk',
		paneID_CancelButton		= 'Canc'
		};

protected:
	virtual ResIDT			GetStatusResID(void) const			{ return res_ID; }
	virtual UInt16			GetValidStatusVersion(void) const	{ return 0x0112; }

public:
							COfflinePickerWindow(LStream *inStream);
	virtual					~COfflinePickerWindow();

	virtual void			FinishCreateSelf();
	virtual void			CalcStandardBoundsForScreen(
									const Rect 		&inScreenBounds,
									Rect 			&outStdBounds) const;
	virtual void			ListenToMessage(MessageT inMessage, void* ioParam);
	virtual Boolean			HandleKeyPress(const EventRecord &inKeyEvent);

	virtual CMailFlexTable *		GetActiveTable();
	static COfflinePickerWindow *	FindAndShow(Boolean inMakeNew);
	static Boolean					DisplayDialog();

protected:
	COfflinePickerView *	mList;
};
