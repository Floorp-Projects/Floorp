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



// CSearchWindowBase.h

#pragma once

#include "CMailNewsWindow.h"
#include "LPeriodical.h"
#include "LListener.h"
#include "msg_srch.h"
#include "PascalString.h"
#include "CSearchManager.h"
#include "MailNewsgroupWindow_Defines.h"

class LGAPushButton;
class CPatternProgressCaption;
class CSearchTableView;

const long MAX_SEARCH_MENU_ITEMS = 16;

//======================================
class CSearchWindowBase : public CMailNewsWindow,
					  public LPeriodical,
					  public LListener
//======================================
{
	typedef CMailNewsWindow Inherited;
	
public:
	// IDs for panes in associated view, also messages that are broadcast to this object
	
	enum
	{
		paneID_Search = 'SRCH'				// MSG_Pane *, 	search button
	,	paneID_Stop = 'STOP'				// nil, 		stop button
	,	paneID_ResultsEnclosure = 'RENC'	// Results enclosure
	,	paneID_ScopeEnclosure = 'SENC'		// Scope enclosure
	,	paneID_ResultsTable = 'Tabl'		// Results table
	,	paneID_ProgressBar = kMailNewsStatusPaneID			// Progress bar
	,	paneID_SearchOptions = 'SOPT'		// Search options dialog
	};

	// Stream version number
	// This must be changed every time you change the streamed parameters
	enum { eValidStreamVersion = 0x000b };
	
								CSearchWindowBase(LStream *inStream, DataIDT inWindowType);
	virtual						~CSearchWindowBase();
	virtual void				SetUpBeforeSelecting() = 0;
						
	static MSG_SearchAttribute 	AttributeFromDataType(PaneIDT inCellDataType);
	static PaneIDT 				DataTypeFromSortCommand(CommandT inSortCommand);

protected:

	// Overriden methods
	virtual	MSG_ScopeAttribute	GetWindowScope() const = 0;
	
	virtual void		FinishCreateSelf();
	virtual Boolean		ObeyCommand(CommandT inCommand, void *ioParam = nil);
	virtual void		FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
										  Boolean &outUsesMark, Char16 &outMark,
										  Str255 outName);
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);
	virtual void		SpendTime(const EventRecord &inMacEvent);
	virtual void		DrawSelf();
	virtual void		SetDescriptor(ConstStr255Param inDescriptor);
	virtual void		Activate();
	virtual void		DeactivateSelf();
	virtual void		AboutToClose();
	virtual void 		SearchOptions() ;
	
	// Utility methods

	void				RecalcMinMaxStdSizes();
	void 				ShowHideSearchResults(Boolean inDoShow);
	void				UpdateResultsVertWindExtension();
	Boolean				IsResultsTableVisible()
						{
							return mResultsTableVisible;
						}
	Boolean				GetDefaultSearchTable( CMailNewsWindow*& outWindow, CMailFlexTable*& outMailFlexTable);
	
	CStr255&			GetFolderDisplayName(const char *inFolderName, CStr255& outFolderName);

	void				MessageSearchParamsResized(Int16 inResizeAmount);
	virtual void		MessageWindSearch();
	virtual void		MessageWindStop(Boolean inUserAborted);
	
	void				AddOneScopeMenuItem(
							Int16 inStringIndex,
							Int16 inAttrib);
	void				AddOneScopeMenuItem(
							const CStr255& inString,
							Int16 inAttrib);

	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
	virtual void		UpdateTableStatusDisplay();


	virtual void		SetWinCSID(Int16 wincsid);
	virtual UInt16 		GetValidStatusVersion() const;

	// Instance variables
	
	Int16 				mMinVResultsSize;
	Int16				mResultsVertWindExtension;
	Boolean 			mResultsTableVisible;
	LPane				*mResultsEnclosure;
	CSearchTableView	*mResultsTable;
	LGAPushButton		*mSearchButton;
	
	Boolean 			mCanRotateTarget;
	
	CSearchManager		mSearchManager;
	LArray				mSearchFolders;
	
	CPatternProgressCaption		*mProgressBar;
	Int16				mNumBasicScopeMenuItems;
	Int16				mNumMenuItems;
	MSG_SearchMenuItem	mSearchMenuItems[MAX_SEARCH_MENU_ITEMS];
	
	private:
	
	LGAPushButton		*mSearchOptionsButton;
	
}; // class CSearchWindowBase
