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



// CSubscribeView.h

#pragma once

// PowerPlant
#include <LTableView.h>
#include <LPeriodical.h>

// Mail/News FE
#include "CMailFlexTable.h"
#include "CProgressListener.h"

#define kRootLevel 1 		// news host, local mail etc.

//----------------------------------------------------------------------------
//	CNewsgroup
//
//----------------------------------------------------------------------------

class CNewsgroup 
{
public:
	CNewsgroup(TableIndexT inRow, MSG_Pane* inNewsgroupList);	

	// Cache management
	void				InvalidateCache() const;
	void				UpdateCache() const;

	// misc. info accessors
	MSG_GroupNameLine*	GetNewsgroupLine() const;
	char* 				GetName() const;
	char*				GetPrettyName() const;
	Int32 				CountPostings() const; 	// neg if unknown
	UInt32 				GetLevel() const;
	Boolean 			CanContainThreads() const;
	UInt32				CountChildren() const;
	ResIDT				GetIconID() const;

	// Flag property accessors:
	UInt32 				GetNewsgroupFlags() const;
	Boolean 			IsOpen() const;
	Boolean 			HasChildren() const;
	Boolean 			IsSubscribed() const;
	Boolean 			IsNew() const;


	// Data
public:
	MSG_ViewIndex		mIndex;
	
protected:
	static MSG_GroupNameLine	sNewsgroupData;
	static MSG_ViewIndex		sCachedIndex;
	MSG_Pane*					mNewsgroupList;
};



//----------------------------------------------------------------------------
//	CSubscribeView
//
//----------------------------------------------------------------------------

class CSubscribeView :	public CMailFlexTable,
						public LPeriodical
{
private:
	typedef CMailFlexTable Inherited;

public:
						CSubscribeView(LStream *inStream);
	enum  {	class_ID = 'subV' };
	virtual				~CSubscribeView();

	//-----------------------------------
	// Public folder fun
	//-----------------------------------
	void				SetContext(CNSContext* inContext) 
								{mContext = inContext;};

	void				RefreshList(
								MSG_Host*			newsHost,
								MSG_SubscribeMode	subscribeMode);

	void				SearchForString(const StringPtr	searchStr);

	virtual void		SubscribeCancel();
	virtual void		SubscribeCommit();


	//-----------------------------------
	// Command implementation
	//-----------------------------------
	virtual	void		OpenRow(TableIndexT inRow);

	virtual Boolean		ProcessKeyPress(
								const EventRecord&	inKeyEvent);
	
	virtual void		DrawCell(
								const STableCell	&inCell,
								const Rect			&inLocalRect);
	
	//-----------------------------------
	// Hierarchy	
	//-----------------------------------
	virtual Boolean		CellHasDropFlag(
								const STableCell& 	inCell,
								Boolean& 			outIsExpanded) const;

	virtual void		SetCellExpansion(
								const STableCell& 	inCell,
								Boolean				inExpand);

	virtual TableIndexT	GetHiliteColumn() const;
	virtual UInt16		GetNestedLevel(TableIndexT inRow) const;
	virtual Boolean		GetHiliteTextRect(
									const TableIndexT	inRow,
									Rect&				outRect) const;
	virtual void		GetMainRowText(
							TableIndexT			inRow,
							char*				outText,
							UInt16				inMaxBufferLength) const;
	virtual ResIDT		GetIconID(TableIndexT inRow) const;


	virtual void		DeleteSelection(void) 		{ /* do nothing */ };


	//-----------------------------------
	// Commands
	//-----------------------------------
	virtual void		FindCommandStatus(
								CommandT			inCommand,
								Boolean				&outEnabled,
								Boolean				&outUsesMark,
								Char16				&outMark,
								Str255				outName);


	virtual Boolean		ObeyCommand(
								CommandT			inCommand,
								void				*ioParam);

	virtual Boolean		ClickSelect(
								const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown);
	virtual void		ClickCell(
								const STableCell	&inCell,
								const SMouseDownEvent &inMouseDown);

	//-----------------------------------
	// Messaging
	//-----------------------------------
protected:
	virtual void		ListenToMessage(
								MessageT			inMessage,
								void				*ioParam);

	virtual void		FindSubscribeCommandStatus(
								CommandT			inCommand,
								Boolean				&outEnabled);

	virtual	void		SpendTime(const EventRecord &);

	//-----------------------------------
	// Other
	//-----------------------------------
	virtual void		CloseParentWindow();	
	void				SetProgressBarLaziness(
								CProgressListener::ProgressBarLaziness inLaziness);	
	Boolean				CanSubscribeToNewsgroup(const CNewsgroup& newsgroup) const
								{ return ((newsgroup.CountChildren() == 0) || MSG_IsIMAPHost(mNewsHost));}


	//-----------------------------------
	// Data change notification
	// Callbacks from MSGlib come here.
	//-----------------------------------
	virtual void		ChangeStarting(
								MSG_Pane*			inPane,
								MSG_NOTIFY_CODE		inChangeCode,
								TableIndexT			inStartRow,
								SInt32				inRowCount);

	virtual void		ChangeFinished(
								MSG_Pane*			inPane,
								MSG_NOTIFY_CODE		inChangeCode,
								TableIndexT			inStartRow,
								SInt32				inRowCount);

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
	Boolean				mStillLoadingFullList;
	
private:
	MSG_Host*			mNewsHost;
	MSG_SubscribeMode	mSubscribeMode;
	enum { kIdle, kInterrupting, kCommitting }	mCommitState;
};
