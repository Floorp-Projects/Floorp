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


// CAddressBookViews.h

#include "CMailFlexTable.h"

#include "PascalString.h"
#include "abcom.h"
#include "dirprefs.h"

class CSearchEditField;
class LDividedView;
class LGAPushButton;
class CPatternProgressCaption;

// These should be replaced as there is a JS pref
enum { eDontCheckTypedown = 0xFFFFFFFF, eCheckTypedownInterval = 30 /* 1/2 sec */ , eNewTarget = 'NwTg'};

struct SAddressDelayedDragInfo;

#pragma mark CAddressBookPane
//------------------------------------------------------------------------------
//	¥ CAddressBookPane
//------------------------------------------------------------------------------
//
class CAddressBookPane : public CMailFlexTable, public LPeriodical
{
private:
	typedef CMailFlexTable Inherited;

public:
						enum EColType 
						{	// Sort column header ids
							eTableHeaderBase = 'col0'
						,	eCol0 = eTableHeaderBase
						,	eCol1 
						,	eCol2
						,	eCol3
						,	eCol4
						,	eCol5
						,	eCol6
						};

						enum 
						{	// Command sort
							cmd_SortAscending = 'Ascd'
						,	cmd_SortDescending = 'Dscd'
						};

						enum 
						{ 
							eInvalidCachedRowIDType = 0x7FFFFFFF
							, eNewEntryID = 0x7FFFFFFF
							, eInvalidEntryID = 0 	
						};
							
						enum
						{	// Icon resource IDs
							ics8_Person = 15260	
						,	ics8_List = 15263
						};
						
							CAddressBookPane(LStream *inStream);
	
	
			UInt32 			SortTypeFromColumnType(EColType inColType);
			AB_CommandType 	SortCommandFromColumnType(EColType inColType);
		
	virtual Boolean 		ObeyCommand(CommandT inCommand, void *ioParam);
	virtual void 			FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
									  Boolean &outUsesMark, Char16 &outMark,
									  Str255 outName);
									  
	virtual	void			DeleteSelection();
	virtual AB_ContainerInfo* 	GetContainer( TableIndexT /* inRow */  ) { return mContainer; }
			Boolean			CurrentBookIsPersonalBook();
	virtual void 			PaneChanged(  MSG_Pane *inPane,  MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode, int32 value);
	virtual Boolean			CellInitiatesDrag(const STableCell &/*inCell*/) const { return true; }
	virtual Boolean 		ItemIsAcceptable( DragReference	inDragRef, ItemReference 	inItemRef	);					
protected:	
	virtual void 		GetCellDisplayData(const STableCell &inCell, ResIDT &ioIcon, CStr255 &ioDisplayString );					
	virtual void	 	DestroyMessagePane(MSG_Pane* inPane);
	virtual	void 		SpendTime(const EventRecord &inMacEvent);
	virtual Boolean		CellHasDropFlag(const STableCell &/*inCell*/, Boolean &/*outIsExpanded*/) const { return false; }
	virtual void		DrawCellContents(const STableCell &inCell, const Rect &inLocalRect);
			
	
	
	AB_AttribID 		GetAttribForColumn( TableIndexT col );
	virtual ResIDT 		GetIconID(TableIndexT inRow) const;
	void 				DrawCellText( const STableCell& inCell, const Rect& inLocalRect, AB_AttribID inAttrib );
	
	
	
	virtual	void		OpenRow(TableIndexT inRow);
	void 				SetCellExpansion( const STableCell& inCell,  Boolean inExpanded);

	virtual Boolean		RowCanAcceptDrop(
							DragReference	inDragRef,
							TableIndexT		inDropRow);
	virtual Boolean		RowCanAcceptDropBetweenAbove(
							DragReference	inDragRef,
							TableIndexT		inDropRow);
	virtual void		ReceiveDragItem(
							DragReference	inDragRef,
							DragAttributes	inDragAttrs,
							ItemReference	inItemRef,
							Rect&			inItemBounds);	
protected:
	AB_ContainerInfo* 	mContainer;
	AB_CommandType		mColumnSortCommand[7];
	AB_AttribID			mColumnAttribID[7];
	// Delayed drag info
	SAddressDelayedDragInfo *mDelayedDragInfo;
};

#pragma mark CAddressBookTableView
//------------------------------------------------------------------------------
//	¥ CAddressBookTableView
//------------------------------------------------------------------------------
//
class CAddressBookTableView : public CAddressBookPane
{
private:
	typedef CAddressBookPane Inherited;

public:
						enum { class_ID = 'AbTb' };
						enum { eTableEditField = 'TbEd' };
						
						CAddressBookTableView(LStream *inStream) :
										 	  CAddressBookPane(inStream),mIsLDAPSearching ( false ) {
							
													};
						
	virtual				~CAddressBookTableView();

	Boolean				LoadAddressBook( AB_ContainerInfo* inContainer, MWContext* inContext );
	void				SetColumnHeaders();
	virtual void		ChangeSort(const LTableHeader::SortChange *inSortChange);
	void				UpdateToTypedownText(CStr255 inTypedownText);
	void				ConferenceCall( );
	void				SetLDAPSearching( Boolean inIsSearching ) { mIsLDAPSearching = inIsSearching; }
	char*				GetFullAddress( TableIndexT  inRow    );
	Boolean				IsLDAPSearching() const { return mIsLDAPSearching; }
protected:
	
	virtual void		ChangeFinished(MSG_Pane *inPane, MSG_NOTIFY_CODE inChangeCode,
							 		   TableIndexT inStartRow, SInt32 inRowCount);
	virtual void		AddRowDataToDrag(TableIndexT inRow, DragReference inDragRef);
	virtual void 		EnableStopButton(Boolean inBusy);
private:
	Boolean 			mIsLDAPSearching;
};


#pragma mark CMailingListTableView
//------------------------------------------------------------------------------
//	¥ CMailingListTableView
//------------------------------------------------------------------------------
//
class CMailingListTableView : public CAddressBookPane
{

private:
	typedef CAddressBookPane Inherited;

public:
						enum { class_ID = 'AlTb' };

						enum { // Broadcast messages
							msg_EntriesAddedToList = 'EnAd' 	// this
						,	msg_EntriesRemovedFromList ='EnRe'
						};
						
						CMailingListTableView(LStream *inStream) :
										 	  	  CAddressBookPane(inStream) {
							
						};
						
	virtual				~CMailingListTableView();
	void				LoadMailingList( MSG_Pane* inPane );
	virtual AB_ContainerInfo* 	GetContainer( TableIndexT inRow );

protected:
	virtual void 		GetCellDisplayData(const STableCell &inCell, ResIDT& ioIcon, CStr255 &ioDisplayString );
	virtual void	 	DestroyMessagePane(MSG_Pane* inPane);
};

#pragma mark CAddressBookContainerView
//------------------------------------------------------------------------------
//	¥ CAddressBookContainerView
//------------------------------------------------------------------------------
//
class CAddressBookContainerView : public CAddressBookPane
{
private:
	typedef CAddressBookPane Inherited;
	
public:
		enum { class_ID = 'AcTb' };	
		
		// Place holders while I wait for the final art work
		enum { 	
				ics8_LDAP = CAddressBookPane::ics8_Person ,
				ics8_List = CAddressBookPane::ics8_List , 
				ics8_PAB = CAddressBookPane::ics8_Person 
			};
			
								CAddressBookContainerView(LStream *inStream);
	virtual 					~CAddressBookContainerView();
	
	void 						Setup( MWContext* inContext);
	virtual AB_ContainerInfo* 	GetContainer( TableIndexT inRow );
	void						SetIndexToSelectOnLoad( Int32 index){ mDirectoryRowToLoad = index; }
protected:
	virtual Boolean 			CellHasDropFlag( const STableCell& inCell, Boolean& outIsExpanded) const;
	virtual UInt16 				GetNestedLevel(TableIndexT inRow) const;
	virtual ResIDT 				GetIconID(TableIndexT inRow) const;

			void 				DrawCellText( const STableCell& inCell, const Rect& inLocalRect  );
	virtual void 				DrawCellContents( const STableCell& inCell, const Rect& inLocalRect );
	virtual void 				DrawIconsSelf( const STableCell& inCell, IconTransformType inTransformType, const Rect& inIconRect) const;
private:
	MWContext*	 					mContext;
	Int32							mDirectoryRowToLoad;	

};


#pragma mark CAddressBookController
//------------------------------------------------------------------------------
//	¥ CAddressBookController
//------------------------------------------------------------------------------
//
class	CAddressBookController:public LView,
		 public LListener, public LPeriodical, public LCommander
{	
private:
	typedef LView Inherited;
		
public:
			enum { class_ID = 'AbCn' };
			enum {
				paneID_DirServers = 'DRSR'			// CDirServersPopupMenu *, 	this
			,	paneID_Search = 'SRCH'				// MSG_Pane *, 	search button
			,	paneID_Stop = 'STOP'				// nil, 		stop button
			,	paneID_AddressBookTable = 'Tabl'	// Address book table
			,	paneID_TypedownName = 'TYPE'		// Typedown name search edit field in window
			,	paneID_SearchEnclosure = 'SCHE'		// Enclosure for search items
			, 	paneID_DividedView = 'A2Vw'				// the divided view
			, 	paneID_ContainerView = 'AcTb',
				paneID_DirectoryName = 'DiCp'			// Directory caption
			};
			
							
						CAddressBookController(LStream* inStream );								  	    
						~CAddressBookController();

	virtual void		ReadStatus(LStream *inStatusData);
	virtual void		WriteStatus(LStream *outStatusData);
	CAddressBookTableView*	GetAddressBookTable() const { return mAddressBookTable; }
	CAddressBookContainerView* GetAddressBookContainerView() const { return mABContainerView; }		
	virtual Boolean			HandleKeyPress(const EventRecord &inKeyEvent);
	virtual Boolean 		ObeyCommand(CommandT inCommand, void *ioParam);
	virtual void 		FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
									  Boolean &outUsesMark, Char16 &outMark,
									  Str255 outName);		
protected:
	
	virtual	void		FinishCreateSelf();
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam);
	
	virtual	void		SpendTime(const EventRecord &inMacEvent);
	virtual void		ActivateSelf() {
							StartRepeating();
							Inherited::ActivateSelf();
						}
	
	virtual void		DeactivateSelf() {
							StopRepeating();
							Inherited::DeactivateSelf();
						}
public:						
	
	void				SelectDirectoryServer( AB_ContainerInfo* inContainer );
	void 				SetContext( MWContext* inContext) { mContext = inContext; }
	
protected:
	void				Search();
	void				StopSearch( );
	
protected:
		MWContext*						mContext;
		UInt32							mNextTypedownCheckTime;
		
		CSearchEditField*				mTypedownName;
		LDividedView*					mDividedView;
		CAddressBookTableView*			mAddressBookTable;
		CMailNewsContext*				mAddressBookContext;					
		
		CAddressBookContainerView*		mABContainerView;
		
		LGAPushButton*					mSearchButton;
		LGAPushButton*					mStopButton;
		
		
};


