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



// MailNewsAddressBook.cp

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#define DEBUGGER_ASSERTIONS


#include "ABCom.h"
#ifndef MOZ_NEWADDR
#include "MailNewsAddressBook.h"
#include "SearchHelpers.h"
#include "CSizeBox.h"
#include "MailNewsgroupWindow_Defines.h"
#include "Netscape_Constants.h"
#include "resgui.h"
#include "CMailNewsWindow.h"
#include "CMailNewsContext.h"
#include "UGraphicGizmos.h"
#include "LFlexTableGeometry.h"
#include "CProgressBroadcaster.h"
#include "CPatternProgressBar.h"
#include "CThreadWindow.h"
#include "CGAStatusBar.h"
#include "uprefd.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "PascalString.h"
#include "CGATabBox.h"
#include "CGAStatusBar.h"
#include "URobustCreateWindow.h"
#include "UMailSelection.h" // Couldn't detect any order here, just stuck it in - jrm
#include "CMouseDragger.h"
#include "UStdDialogs.h"
#include "macutil.h"
#include "UStClasses.h"
#include "CComposeAddressTableView.h"
#include "msgcom.h"

#include <LGAPushButton.h>
#include <LGAIconButton.h>
#include <LDragAndDrop.h>
#include "divview.h"
#include "CTargetFramer.h"
// get string constants
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
#include "MailNewsSearch.h"
#include "addrbook.h"
#include "msgcom.h"
#include "msg_srch.h"
#include "abdefn.h"
#include "dirprefs.h"
#include "prefapi.h"
#include "UProcessUtils.h"
#include "CAppleEventHandler.h"

#include "secnav.h"
#include "CTableKeyAttachment.h"
#include "intl_csi.h"
#include "xp_help.h"
#include "CLDAPQueryDialog.h"

class CNamePropertiesWindow;
class CListPropertiesWindow;
class CAddressBookListTableView;
class CAddressBookController;

const ResIDT kConferenceExampleStr 		= 8901;
const ResIDT kAddressbookErrorStrings	= 8902;
#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

// Save window status version

static const UInt16 cAddressSaveWindowStatusVersion = 	0x0218;
static const UInt16 cNamePropertiesSaveWindowStatusVersion = 	0x0200;
static const UInt16 cListPropertiesSaveWindowStatusVersion = 	0x0200;


#pragma mark -
/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/

#define ASSERT_ADDRESS_ERROR(err)		Assert_(!(err) || ((err) == 1))


#pragma mark -
/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/


// CAddressBookPane


class CAddressBookPane : public CMailFlexTable {

private:
	typedef CMailFlexTable Inherited;

public:

						CAddressBookPane(LStream *inStream) :
										 Inherited(inStream),
										
									  	 mProgressBar(nil) {
							SetRefreshAllWhenResized(false);
						}
	
						enum EColType {	// Sort column header ids
							eColType = 'Type'
						,	eColName = 'Name'
						,	eColEmail = 'Emal'
						,	eColCompany = 'Comp'
						,	eColLocality = 'Loca'
						,	eColNickname = 'Nick'
						,	eColWorkPhone ='Phon'
						};

						enum {	// Command sort
							cmd_SortAscending = 'Ascd'
						,	cmd_SortDescending = 'Dscd'
						};

						enum { eInvalidCachedRowIDType = 0x7FFFFFFF, eNewEntryID = 0x7FFFFFFF, eInvalidEntryID = 0 };

						enum {
							paneID_ProgressBar = 'Prog'
						};
						
						// Unimplemented string IDs
						
						enum { 
						  	uStr_OneEntryFoundSelected = 8906
						,  	uStr_NoEntriesFound = 8907
						,  	uStr_MultipleEntriesFound = 8908
						,  	uStr_OneEntryFound = 8909
						,  	uStr_MultipleEntriesSelected = 8910
						};

	void 				GetEntryIDAndType(TableIndexT inRow, ABID *outID, ABID *outType);
	ABID 				GetEntryID(TableIndexT inRow);
	TableIndexT 		GetRowOfEntryID(ABID inEntryID) {
							Assert_(GetMessagePane() != nil);
							return (AB_GetIndexOfEntryID( (AddressPane*) GetMessagePane(), inEntryID) + 1);
						}
	
	void			GetFullAddress( TableIndexT inRow, char** inName  )
		{
			ABID id;
			ABID type;
			ABook* pABook = CAddressBookManager::GetAddressBook();
			GetEntryIDAndType( inRow, &id, &type );
			//pABook->GetFullAddress( sCurrentBook, id, inName );
			AB_GetExpandedName(sCurrentBook, pABook, id, inName);
		}
	
	
	static UInt32 			SortTypeFromColumnType(EColType inColType);
	static AB_CommandType 	SortCommandFromColumnType(EColType inColType);

	static Int32		GetBENumEntries(ABID inType, ABID inListID = eInvalidEntryID);
	virtual
	
	
	void				ComposeMessage();
	void				ShowEditWindowByEntryID(ABID inEntryID, Boolean inOptionKeyDown);
	void				ShowEditWindow(ABID inID, ABID inType);
	static DIR_Server*	GetCurrentBook(){ return sCurrentBook; }
protected:
	
						enum {	// Icon resource IDs
							ics8_Person = 15260
						,	ics8_List = 15263
						};

	void 	DestroyMessagePane(MSG_Pane* inPane);
	virtual void		FinishCreateSelf();

	virtual void		SetUpTableHelpers();	
	
	virtual void		DrawCellContents(const STableCell &inCell, const Rect &inLocalRect);
	
	virtual void		AddSelectionToDrag(DragReference inDragRef, RgnHandle inDragRgn);
	virtual void		AddRowDataToDrag(TableIndexT inRow, DragReference inDragRef);

	void				GetSortParams(UInt32 *outSortKey, Boolean *outSortBackwards);

	// Backend methods
	static void			GetDisplayData(EColType inColType, ABID inEntryID, ABID inType, 
									   CStr255 *outDisplayText, ResIDT *outIconID);
						
	// Instance variables ==========================================================
	
	CGAStatusBar		*mProgressBar;
	
	static DIR_Server	*sCurrentBook;	// Current address book server
};


// CAddressBookChildWindow

class CAddressBookChildWindow : public CMediatedWindow,
					  	   	    public LListener,
					  	   	  	public LBroadcaster,
					  	   	  	public CSaveWindowStatus {
					  
private:
	typedef CMediatedWindow Inherited;

public:

						CAddressBookChildWindow(LStream *inStream, DataIDT inWindowType) : 
							 		  	   	    CMediatedWindow(inStream, inWindowType),
							 		  	   	    LListener(),
							 		  	   	    LBroadcaster(),
							 		  	   	    CSaveWindowStatus(this),
							 		  	   	    mEntryID(CAddressBookPane::eNewEntryID)
							 		  	   	   {
							SetRefreshAllWhenResized(false);
						}
	virtual				~CAddressBookChildWindow() {
						}
	void				FinishCreateSelf();
	ABID				GetEntryID() const {
							return mEntryID;
						}
	virtual void		UpdateBackendToUI(DIR_Server *inDir, ABook *inABook) = 0L;
	virtual void		UpdateUIToBackend(DIR_Server *inDir, ABook *inABook, 
												 ABID inEntryID) = 0L;
												 

	virtual void		DoClose() {
							Inherited::AttemptClose();
						}
	virtual Boolean  	HandleKeyPress( const EventRecord	&inKeyEvent);
	
protected:

	// Overriden methods

	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	virtual void		UpdateTitle()=0;
	// Instance variables
						
	ABID				mEntryID;			// BE ID of the property that this window is editing
											// CAddressBookPane::eInvalidEntryID if none

};


// CAddressBookTableView

class CAddressBookTableView : public CAddressBookPane {

private:
	typedef CAddressBookPane Inherited;

public:

						enum { class_ID = 'AbTb' };
						enum {	// Command IDs
							cmd_NewAddressCard = 'NwCd'				// New Card
						,	cmd_NewAddressList = 'NwLs'				// New List
						,	cmd_HTMLDomains = 'HtDm'				// domains dialog.
						,	cmd_EditProperties = 'EdPr'				// Edit properties for a list or card
						,	cmd_DeleteEntry = 'DeLe'				// Delete list or card
						,	cmd_ComposeMailMessage = 'Cmps'			// Compose a new mail message
						, 	cmd_ConferenceCall	= 'CaLL'
						,	cmd_ViewMyCard = 'vmyC'
						};
						CAddressBookTableView(LStream *inStream) :
										 	  CAddressBookPane(inStream) {
							SetRefreshAllWhenResized(false);
						}
	virtual				~CAddressBookTableView();
	
	void 				ButtonMessageToCommand( MessageT inMessage, void* ioParam = NULL );
	
	Boolean				LoadAddressBook( DIR_Server *inDir = nil);

	void				NewCard(PersonEntry* pPerson = nil);
	void				NewList();
	void				SaveAs();
	virtual void		ChangeSort(const LTableHeader::SortChange *inSortChange);
	virtual void		DeleteSelection();

	static Boolean		CurrentBookIsPersonalBook() {
							return (sCurrentBook == CAddressBookManager::GetPersonalBook());
						}
	void				ImportAddressBook();

	void				UpdateToTypedownText(Str255 inTypedownText);
	void				ConferenceCall( );
	virtual	void		PaneChanged(MSG_Pane *inPane,
									MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
									int32 value);
	void				SetColumnHeaders(DIR_Server* server);
	void				ShowMyAddressCard();
	void				SetModalWindow( Boolean value) { mModalWindow = value; } 


	virtual Boolean		ObeyCommand(CommandT inCommand, void *ioParam = nil);
	virtual void 		FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
									  Boolean &outUsesMark, Char16 &outMark,
									  Str255 outName);
protected:
	virtual	void		OpenRow(TableIndexT inRow);
	virtual Boolean		CellInitiatesDrag(const STableCell &/*inCell*/) const { 
							return true; 
						}
	
	void				CloseEditWindow(ABID inID, ABID inType);
	
	void				ForceCurrentSort();
	
	//void				DisposeAddressBookPane(Boolean inRefresh = false);

	Boolean				IsLDAPSearching();
	
	// Backend methods

	virtual void		ChangeFinished(MSG_Pane *inPane, MSG_NOTIFY_CODE inChangeCode,
							 		   TableIndexT inStartRow, SInt32 inRowCount);

	virtual Int32		GetBENumRows() {
							return Inherited::GetBENumEntries(ABTypeAll);
						}
private:
	Boolean mModalWindow;
};


class CListPropertiesWindow : public CAddressBookChildWindow {
					  
private:
	typedef CAddressBookChildWindow Inherited;

public:

						enum { class_ID = 'LpWn', pane_ID = class_ID, res_ID = 8940 };
	
						// IDs for panes in associated view, also messages that are broadcast to this object
						
						enum {
							paneID_Name = 'NAME'
						,	paneID_Nickname = 'NICK'
						,	paneID_Description = 'DESC'
						,	paneID_AddressBookListTable = 'Tabl'	// Address book list table
						};
						
						enum {	// Broadcast messages
							msg_ListPropertiesHaveChanged = 'lpCH'	// this
						};
						
						// Unimplemented string IDs
						
						enum { 
						  	uStr_ListWindowTitle = 8912
						,  	uStr_ListDefaultName = 8914
						,  	uStr_ListEmptyWindowTitle = 8916
						};
						
	// Stream creator method

						CListPropertiesWindow(LStream *inStream);
	virtual				~CListPropertiesWindow();
						
	virtual ResIDT		GetStatusResID() const { return res_ID; }

	virtual	void		UpdateBackendToUI(DIR_Server *inDir, ABook *inABook);
	virtual void		UpdateUIToBackend(DIR_Server *inDir, ABook *inABook, 
												 ABID inEntryID);
protected:

	// Overriden methods

	virtual void		FinishCreateSelf();
	virtual void		DrawSelf();
	virtual UInt16		GetValidStatusVersion() const {
							return cListPropertiesSaveWindowStatusVersion;
						}
	virtual void		UpdateTitle();
	virtual Boolean		ObeyCommand(CommandT inCommand, void *ioParam = nil);

	// Instance variables
	
	CAddressBookListTableView	*mAddressBookListTable;
	LBroadcasterEditField		*mTitleField;
};


// CAddressBookListTableView

//======================================
class CAddressBookListTableView : public CAddressBookPane
//======================================
{

private:
	typedef CAddressBookPane Inherited;

public:

						enum { class_ID = 'AlTb' };

						enum { // Broadcast messages
							msg_EntriesAddedToList = 'EnAd' 	// this
						,	msg_EntriesRemovedFromList ='EnRe'
						};
						CAddressBookListTableView(LStream *inStream) :
										 	  	  CAddressBookPane(inStream),
										 	  	  mNumItemsDragAdded(0) {
							SetRefreshAllWhenResized(false);
						}
	virtual				~CAddressBookListTableView();
	
	void				LoadAddressBookList(ABID &ioEntryListID);
	//Drag Support
protected:
	virtual Boolean 	DragIsAcceptable(DragReference inDragRef);
	virtual Boolean 	ItemIsAcceptable(DragReference inDragRef, ItemReference inItemRef);
	virtual void 		ReceiveDragItem(DragReference inDragRef, DragAttributes inDragAttrs,
									 	ItemReference inItemRef, Rect &inItemBounds);
	
	virtual Boolean		RowCanAcceptDropBetweenAbove( DragReference	/*inDragRef*/, TableIndexT /*inDropRow*/)	
		{ return true; }
	Boolean			CanInsertDragItem( DragReference inDragRef, ItemReference inItemRef,
										  SAddressDragInfo *outInfo);
	Boolean 		RowCanAcceptDrop( DragReference	/*inDragRef*/, TableIndexT /*inDropRow*/)
		{ return true;	} 
protected:

	virtual void		FinishCreateSelf();
	
	virtual void		DeleteSelection();
	virtual	void		OpenRow(TableIndexT inRow);
	
	virtual Boolean		CellInitiatesDrag(const STableCell &/*inCell*/) const {
							return CAddressBookTableView::CurrentBookIsPersonalBook(); 
						}
											
	// Backend methods

	virtual Int32		GetBENumRows() {
							return Inherited::GetBENumEntries(ABTypeAll, ((CListPropertiesWindow *) 
											USearchHelper::GetPaneWindow(this))->GetEntryID());
						}
						
	void				DisposeAddressListPane(Boolean inRefresh = false);

	// Instance variables ==========================================================

	TableIndexT 		mNumItemsDragAdded;
};

class CAddressBookWindow : public CMailNewsWindow
					  	    {
private:
	typedef CMailNewsWindow Inherited;					  
public:

						enum { class_ID = 'AbWn', pane_ID = class_ID, res_ID = 8900 };
	
						// IDs for panes in associated view, also messages that are broadcast to this object
						
						enum {
							paneID_DirServers = 'DRSR'			// CDirServersPopupMenu *, 	this
						,	paneID_Search = 'SRCH'				// MSG_Pane *, 	search button
						,	paneID_Stop = 'STOP'				// nil, 		stop button
						,	paneID_AddressBookTable = 'Tabl'	// Address book table
						,	paneID_TypedownName = 'TYPE'		// Typedown name search edit field in window
						,	paneID_SearchEnclosure = 'SCHE'		// Enclosure for search items
						, 	paneID_AddressBookController = 'AbCn'
						};
						#if 0
						enum {	// Command IDs
							cmd_NewAddressCard = 'NwCd'				// New Card
						,	cmd_NewAddressList = 'NwLs'				// New List
						,	cmd_HTMLDomains = 'HtDm'				// domains dialog.
						,	cmd_EditProperties = 'EdPr'				// Edit properties for a list or card
						,	cmd_DeleteEntry = 'DeLe'				// Delete list or card
						,	cmd_ComposeMailMessage = 'Cmps'			// Compose a new mail message
						, 	cmd_ConferenceCall	= 'CaLL'
						,	cmd_ViewMyCard = 'vmyC'
						};
						#endif
	// Stream creator method

						CAddressBookWindow(LStream *inStream) : 
							 		  	   CMailNewsWindow(inStream, WindowType_Address),
									  	   mAddressBookController(nil)
										   {
							SetPaneID(pane_ID);
							SetRefreshAllWhenResized(false);
						}
	virtual				~CAddressBookWindow();
						
	virtual ResIDT		GetStatusResID() const { return res_ID; }
	

	static Boolean					CloseAddressBookChildren();
	static MWContext				*GetMailContext();
	

	void				ShowEditWindowByEntryID(ABID inEntryID, Boolean inOptionKeyDown);
	
	//CAddressBookTableView	* GetAddressBookTable() { return mAddressBookTable; }
	CAddressBookController 	* GetAddressBookController() { return mAddressBookController;}
protected:

	// Overriden methods

	virtual void		FinishCreateSelf();	
	
	// Utility methods
	
	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
	virtual UInt16		GetValidStatusVersion() const { return cAddressSaveWindowStatusVersion; }
	
	// Instance variables
	CAddressBookController 	*mAddressBookController;
};


class CNamePropertiesWindow : public CAddressBookChildWindow {
					  
private:
	typedef CAddressBookChildWindow Inherited;

public:

						enum { class_ID = 'NpWn', pane_ID = class_ID, res_ID = 8930 };
	
						// IDs for panes in associated view, also messages that are broadcast to this object
						
						enum {
							paneID_GeneralView = 'GNVW'			// General preferences view
						,		paneID_FirstName = 'FNAM'
						,		paneID_LastName = 'LNAM'
						,		paneID_EMail = 'EMAL'
						,		paneID_Nickname = 'NICK'
						,		paneID_Notes = 'NOTE'
						,		paneID_PrefersHTML = 'HTML'
						,	paneID_ContactView = 'CNVW'			// Contact preferences view
						,		paneID_Company = 'COMP'
						,		paneID_Title = 'TITL'
						,		paneID_Address = 'ADDR'
						,		paneID_City = 'CITY'
						,		paneID_State = 'STAT'
						,		paneID_ZIP = 'ZIP '
						, 		paneID_Country = 'Coun'
						,		paneID_WorkPhone = 'WPHO'
						,		paneID_FaxPhone = 'FPHO'
						,		paneID_HomePhone = 'HPHO'
						,	paneID_SecurityView = 'SCVW'		// Security preferences view
						,	paneID_CooltalkView = 'CLVW'		// Cooltalk preferences view
						,		paneID_ConferenceAddress = 'CAED',
								paneID_ConferenceServer	 = 'CnPu'
						};
						
						enum {	// Broadcast messages
								paneID_ConferencePopup ='CoPU'	// conference pop up button
						};


	// Stream creator method

						CNamePropertiesWindow(LStream *inStream) : 
							 		  	   	  CAddressBookChildWindow(inStream, WindowType_AddressPerson),
							 		  	   	  mFirstNameField(nil),
							 		  	   	  mLastNameField(nil) {

							SetPaneID(pane_ID);
							
						}
						
	virtual ResIDT		GetStatusResID() const { return res_ID; }
	virtual void		UpdateBackendToUI(DIR_Server *inDir, ABook *inABook );
	void				UpdateUIToPerson( PersonEntry* person , ABID inEntryID );
	void				UpdateUIToBackend(DIR_Server *inDir, ABook *inABook,  ABID inEntryID);
	void				SetConferenceText( );
protected:

	// Overriden methods

	virtual void		FinishCreateSelf();
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	virtual UInt16		GetValidStatusVersion() const { return cNamePropertiesSaveWindowStatusVersion; }
						
	virtual void		UpdateTitle();
	
	// Instance variables
	
	LBroadcasterEditField		*mFirstNameField;
	LBroadcasterEditField		*mLastNameField;
};


// StPersonEntry

class StPersonEntry {

public:

						StPersonEntry();
						~StPersonEntry() {
								XP_FREEIF(mPerson);
							}
						
	PersonEntry			*GetEntry() {
							return mPerson;
						}
private:
	PersonEntry			*mPerson;
};


// StMailingListEntry

class StMailingListEntry {

public:

						StMailingListEntry();
						~StMailingListEntry() {
							XP_FREEIF(mMailingList);
						}
						
	MailingListEntry	*GetEntry() {
							return mMailingList;
						}
private:
	MailingListEntry	*mMailingList;
};


// The CAddressBookController handles the interaction between the container pane, CAddressBookTableView,
// and the typedown edit field

class	CAddressBookController:public LView, LCommander, public LListener, public LPeriodical
{	
private:
	typedef LView Inherited;	
public:
			enum { class_ID = 'AbCn' };
			enum { eDontCheckTypedown = 0xFFFFFFFF, eCheckTypedownInterval = 20 /* 1/3 sec */ };
			
			enum {
				paneID_DirServers = 'DRSR'			// CDirServersPopupMenu *, 	this
			,	paneID_Search = 'SRCH'				// MSG_Pane *, 	search button
			,	paneID_Stop = 'STOP'				// nil, 		stop button
			,	paneID_AddressBookTable = 'Tabl'	// Address book table
			,	paneID_TypedownName = 'TYPE'		// Typedown name search edit field in window
			,	paneID_SearchEnclosure = 'SCHE'		// Enclosure for search items
			, 	paneID_DividedView = 'A2Vw'				// the divided view
			};
			
			
						
				CAddressBookController(LStream* inStream ):LView( inStream),
										mNextTypedownCheckTime(eDontCheckTypedown),
										   mTypedownName(nil),  mSearchButton(nil),mStopButton(nil)
									  	   {};
				~CAddressBookController(){ };
protected:
		virtual void					ListenToMessage(MessageT inMessage, void *ioParam);

// LCommander overrides:
protected:

	virtual Boolean		ObeyCommand(CommandT inCommand, void *ioParam);
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

	virtual void		ActivateSelf() {
							StartRepeating();
							Inherited::ActivateSelf();
						}
	
	virtual void		DeactivateSelf() {
							StopRepeating();
							Inherited::DeactivateSelf();
						}
	
// LPeriodical	
	virtual	void		SpendTime(const EventRecord &inMacEvent);

// CSaveWindowStatus helpers:
public:
	void				ReadStatus(LStream *inStatusData);
	void				WriteStatus(LStream *outStatusData);
		
// Specials
public:
	void				FinishCreateSelf();
	void				UpdateToDirectoryServers();
	void				PopulateDirectoryServers();
	void				SelectDirectoryServer(DIR_Server *inServer, Int32 inServerIndex);
	Boolean				IsLDAPSearching() {
							return ((mStopButton != nil) ? mStopButton->IsVisible() : false);
						}
	Int32				GetServerIndexFromPopup();
	void				SetPopupFromServerIndex(Int32 inServerIndex);
	CAddressBookTableView*	GetAddressBookTable(){ 
									    Assert_( mAddressBookTable );
										return mAddressBookTable;
									 }

protected:
	void				MessageWindSearch( char* text = NULL);
	void				MessageWindStop(Boolean inUserAborted);
	
// Data
protected:
		CSearchEditField*				mTypedownName;
		LDividedView*					mDividedView;
		CAddressBookTableView*			mAddressBookTable;
		//CAddressBookContainerView*	mABContainerView;
		UInt32							mNextTypedownCheckTime;
		
		LGAPushButton			*mSearchButton;
		LGAPushButton			*mStopButton;
		CPatternProgressCaption	*mProgressCaption;
		CMailNewsContext		*mAddressBookContext;					

};

class UAddressBookUtilites
{
public:
	enum { invalid_command = 0xFFFFFFFF };
	static	AB_CommandType GetABCommand( CommandT inCommand );
	static void	ABCommandStatus( 
		CMailFlexTable*	inTable,  CommandT			inCommand,
		Boolean				&outEnabled,
		Boolean				&outUsesMark,
		Char16				&outMark,
		Str255				outName);
};

AB_CommandType 	UAddressBookUtilites::GetABCommand( CommandT inCommand )
{
	switch ( inCommand )
	{
	
		case	cmd_NewMailMessage:
		case	CAddressBookTableView::cmd_ComposeMailMessage:			return AB_NewMessageCmd;
		case 	cmd_ImportAddressBook:			return AB_ImportCmd;	
		case 	cmd_SaveAs:						return  AB_SaveCmd;					
//		case 	cmd_Close: 						return  AB_CloseCmd;					

//		case return AB_NewAddressBook
//		case return  AB_NewLDAPDirectory

		//case return  AB_UndoCmd

		//case return  AB_RedoCmd
		
		case 	CAddressBookTableView::cmd_DeleteEntry:				return  AB_DeleteCmd;	

//		case return  AB_LDAPSearchCmd,	

		case CAddressBookPane::eColType:		return  AB_SortByTypeCmd;		
		case CAddressBookPane::eColName:			return  AB_SortByFullNameCmd;		
		case CAddressBookPane::eColLocality:	return  AB_SortByLocality;		
		case CAddressBookPane::eColNickname:	return  AB_SortByNickname;			
		case CAddressBookPane::eColEmail:		return  AB_SortByEmailAddress;		
		case CAddressBookPane::eColCompany:		return  AB_SortByCompanyName;			
		case CAddressBookPane::cmd_SortAscending: return AB_SortAscending;				
		case CAddressBookPane::cmd_SortDescending: return AB_SortDescending;			

		case	CAddressBookTableView::cmd_NewAddressCard:				return  AB_AddUserCmd;				
		case	CAddressBookTableView::cmd_NewAddressList:				return AB_AddMailingListCmd;		
		case	CAddressBookTableView::cmd_EditProperties:	 			return AB_PropertiesCmd;			
		case 	CAddressBookTableView::cmd_ConferenceCall:				return  AB_CallCmd;					
	//	case return  AB_ImportLdapEntriesCmd		
		default:								return AB_CommandType( invalid_command );
	}
}

void UAddressBookUtilites::ABCommandStatus( 
	CMailFlexTable*	inTable, 
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
{
	AB_CommandType abCommand = AB_CommandType(inCommand);
	if( abCommand != UAddressBookUtilites::invalid_command )
	{
		CMailSelection selection;
		inTable->GetSelection(selection);
		
		XP_Bool selectable;
		MSG_COMMAND_CHECK_STATE checkedState;
		const char* display_string = nil;
		XP_Bool plural;
	
		
		if ( AB_CommandStatus(
			(ABPane*)inTable->GetMessagePane(),
			abCommand,
			(MSG_ViewIndex*)selection.GetSelectionList(),
			selection.selectionSize,
			&selectable,
			&checkedState,
			&display_string,
			&plural)
		>= 0)
		{

			outEnabled = (Boolean)selectable;
			outUsesMark = true;
			if (checkedState == MSG_Checked)
				outMark = checkMark;
			else
				outMark = 0;
			
			if (display_string && *display_string)
						*(CStr255*)outName = display_string;
		}
	}	
}


#pragma mark -
/*====================================================================================*/
#pragma mark Address Picker class
/*====================================================================================*/
static const UInt16 cAddressPickerWindowStatusVersion = 	0x004;
class CAddressPickerWindow : public CAddressBookWindow, public LListener
{
public:
	enum { class_ID = 'ApWn', pane_ID = class_ID, res_ID = 8990 };
	enum { eOkayButton = 'OkBt', eCancelButton ='CnBt', eHelp ='help'};
	// Buttons to enable and disable
	enum { eToButton = 'ToBt', eCcButton = 'CcBt', eBccButton = 'BcBt',ePropertiesButton = 'PrBt' };
	
			CAddressPickerWindow(LStream *inStream): CAddressBookWindow( inStream ), LListener(),
			 mInitialTable( nil ), mPickerAddresses(nil), mAddressBookTable(nil) {};
	void	Setup( CComposeAddressTableView* initialTable );
	virtual void		FinishCreateSelf();
	
	
protected:
	virtual	void 		ListenToMessage(MessageT inMessage, void *ioParam);
	
	virtual ResIDT		GetStatusResID() const { return res_ID; }
	virtual UInt16		GetValidStatusVersion() const { return cAddressPickerWindowStatusVersion; }
private:
	void AddSelection( EAddressType inAddressType );
	CAddressBookTableView	* mAddressBookTable;
	CComposeAddressTableView* mInitialTable;
	CComposeAddressTableView* mPickerAddresses;
};


#pragma mark -
/*====================================================================================*/
	#pragma mark BE CALLBACKS
/*====================================================================================*/

/*======================================================================================
	Get the global address book database.
======================================================================================*/

XP_List *FE_GetDirServers() {
	return CAddressBookManager::GetDirServerList();
}


/*======================================================================================
	Get the global address book database.
======================================================================================*/

ABook *FE_GetAddressBook(MSG_Pane *pane) {
#pragma unused (pane)
	return CAddressBookManager::GetAddressBook();
}


/*======================================================================================
	Returns the address book context, creating it if necessary.  A mail pane is passed in, 
	on the unlikely chance that it might be useful for the FE.  If "viewnow" is TRUE, 
	then present the address book window to the user; otherwise, don't (unless it is 
	already visible).
======================================================================================*/

MWContext *FE_GetAddressBookContext(MSG_Pane *pane, XP_Bool viewnow) {
#pragma unused (pane)

	if ( viewnow ) {
		CAddressBookManager::ShowAddressBookWindow();
	}
	
	return CAddressBookWindow::GetMailContext();
}

/****************************************************************************
This is a callback into the FE to bring up a modal property sheet for modifying
an existing entry or creating a new one from a person structure. If 
entryId != MSG_MESSAGEIDNONE then it is the entryID of the entry to modify.  
Each FE should Return TRUE if the user hit ok return FALSE if they hit cancel 
and return -1 if there was a problem creating the window or something 
****************************************************************************/
int FE_ShowPropertySheetFor (MWContext* context, ABID entryID, PersonEntry* pPerson)
{
#pragma unused (context)
	CAddressBookWindow * addressWindow = CAddressBookManager::ShowAddressBookWindow();
	
	addressWindow->GetAddressBookController()->SelectDirectoryServer(CAddressBookManager::GetPersonalBook(), 0);
	
	if (entryID == MSG_MESSAGEIDNONE)
	{
		addressWindow->GetAddressBookController()->GetAddressBookTable()->NewCard(pPerson);
	}
	else
	{
		// Not implemented yet	const Boolean optionKeyDown = USearchHelper::KeyIsDown(USearchHelper::char_OptionKey);
		const Boolean optionKeyDown = false;	
		AB_ModifyUser(CAddressBookManager::GetPersonalBook(), CAddressBookManager::GetAddressBook(), entryID, pPerson);
		addressWindow->GetAddressBookController()->GetAddressBookTable()->ShowEditWindowByEntryID(entryID, optionKeyDown);
	}
	return true;
}


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

XP_List *CAddressBookManager::sDirServerList = nil;
ABook *CAddressBookManager::sAddressBook = nil;
Boolean CAddressBookManager::sDirServerListChanged = false;

/*======================================================================================
	Open the address book at application startup. This method sets the initial address
	book to the local personal address book for the user (creates one if it doesn't
	exist already).
======================================================================================*/

void CAddressBookManager::OpenAddressBookManager() {

	AssertFail_(sDirServerList == nil);

	RegisterAddressBookClasses();
	
	char fileName[64];
	fileName[63] = '\0';
	char *oldName = nil;
	Try_ {
		// Get the default address book path
		FSSpec spec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
		// The real, new way!
		CStr255 pfilename;
		::GetIndString(pfilename, 300, addressBookFile);	// Get address book name from prefs		
		strncpy( fileName,(char*)pfilename, 63) ;
		// Create directory servers
		Int32 error = DIR_GetServerPreferences(&sDirServerList, fileName);
			// No listed return error codes, who knows what they are!
		FailOSErr_(error);
		AssertFail_(sDirServerList != nil);
		
		// Get the akbar address book.  It's in HTML format (addressbook.html)
		spec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
		::GetIndString(spec.name, 300, htmlAddressBook);	// Get address book name from prefs
		oldName = CFileMgr::GetURLFromFileSpec(spec);
		FailNIL_(oldName);
		
		CAddressBookManager::FailAddressError(
			AB_InitializeAddressBook(GetPersonalBook(), &sAddressBook,
				oldName + XP_STRLEN("file://"))
			);
		XP_FREE(oldName);
	}
	Catch_(inErr) {
		CAddressBookManager::CloseAddressBookManager();
		XP_FREEIF(oldName);
	}
	EndCatch_
	
	PREF_RegisterCallback("ldap_1", DirServerListChanged, NULL);
}


/*======================================================================================
	Closes any open resources used by the address book manager.
======================================================================================*/

void CAddressBookManager::CloseAddressBookManager() {

	if ( sAddressBook != nil ) {
		Int32 error = AB_CloseAddressBook(&sAddressBook);
		sAddressBook = nil;
		ASSERT_ADDRESS_ERROR(error);
	}
	
	if ( sDirServerList != nil ) {
		Int32 error = DIR_DeleteServerList(sDirServerList);
		sDirServerList = nil;
		Assert_(!error);
	}
}


/*======================================================================================
	Stupid comment that explains something that doesn't occur till after the comment.
======================================================================================*/

void CAddressBookManager::ImportLDIF(const FSSpec& inFileSpec) {
	
	char* path = CFileMgr::EncodedPathNameFromFSSpec(inFileSpec, true /*wantLeafName*/);
	if (path)
	{
		int result = AB_ImportData(
			nil, // no container, create new
			path,
			strlen(path),
			AB_Filename	// FIX ME: need the bit that tells the backend to delete the file.
			);
		XP_FREE(path);
	}
}


/*======================================================================================
	Show the search dialog by bringing it to the front if it is not already. Create it
	if needed.
======================================================================================*/

CAddressBookWindow * CAddressBookManager::ShowAddressBookWindow() {

	// Find out if the window is already around
	CAddressBookWindow *addressWindow = dynamic_cast<CAddressBookWindow *>(
			CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Address));

	if ( addressWindow == nil ) {
		// Search dialog has not yet been created, so create it here and display it.
		addressWindow = dynamic_cast<CAddressBookWindow *>(
				URobustCreateWindow::CreateWindow(CAddressBookWindow::res_ID, 
												  LCommander::GetTopCommander()));
		AssertFail_(addressWindow != nil);
	}
	
	// Select the window
	
	addressWindow->Show();
	addressWindow->Select();
	return addressWindow;
}


/*======================================================================================
	Return the list of directory servers for the application. Why are this
	method and variable here? Can also call the BE method FE_GetDirServers() to return
	the same result.
======================================================================================*/

XP_List *CAddressBookManager::GetDirServerList() {

	AssertFail_(sDirServerList != nil);
	
	if (sDirServerListChanged) {
		// directory list may have been updated via a remote admin file
		sDirServerListChanged = false;
		XP_List* newList = nil;
		CStr255 pfilename;
		::GetIndString(pfilename, 300, addressBookFile);	
		
		if ( DIR_GetServerPreferences(&newList, pfilename) == 0 && newList ) {
			SetDirServerList(newList, false);
		}
	}
	
	return sDirServerList;
}


/*======================================================================================
	This method should be called to set the current directory servers list. The old list
	is destroyed and is replaced with inList; the caller does NOT dispose of inList, since
	it is managed by this class after calling this method. This method also calls the
	BE method DIR_SaveServerPreferences() if inSavePrefs is true.
======================================================================================*/

void CAddressBookManager::SetDirServerList(XP_List *inList, Boolean inSavePrefs) {

	AssertFail_(inList != nil);

	XP_List *tempDirServerList = sDirServerList;
	sDirServerList = inList;	// This needs to be set correctly for the callback to work.
	if ( inSavePrefs ) {
		Int32 error = DIR_SaveServerPreferences(inList);	// No listed return error codes, 
															// who know what they are!
		if (error)
		{
			sDirServerList = tempDirServerList;	// Put this back if there are problems.
		}
		FailOSErr_(error);
	}

	if ( tempDirServerList != nil ) {
		Int32 error = DIR_DeleteServerList(tempDirServerList);
		Assert_(!error);
	}
}


/*======================================================================================
	Return the local personal address book.
======================================================================================*/

DIR_Server *CAddressBookManager::GetPersonalBook() {

	AssertFail_((sDirServerList != nil));
	DIR_Server *personalBook = nil;
	DIR_GetPersonalAddressBook(sDirServerList, &personalBook);
	AssertFail_(personalBook != nil);
	return personalBook;
}


/*======================================================================================
	Return the address book database. Can also call the BE method FE_GetAddressBook() to 
	return the same result.
======================================================================================*/

ABook *CAddressBookManager::GetAddressBook() {

	AssertFail_(sAddressBook != nil);
	return sAddressBook;
}

void CAddressBookManager::FailAddressError(Int32 inError)
{
	if ( inError == 0 ) return;
	switch (	inError )
	{
		case MK_MSG_NEED_FULLNAME:
		case MK_MSG_NEED_GIVENNAME:
			Throw_( MK_MSG_NEED_FULLNAME );
			break;
		case MK_OUT_OF_MEMORY:
			Throw_(memFullErr); 
			break;
		case MK_ADDR_LIST_ALREADY_EXISTS:
		case MK_ADDR_ENTRY_ALREADY_EXISTS:
			Throw_( inError );
			break;
		case MK_UNABLE_TO_OPEN_FILE:
		case XP_BKMKS_CANT_WRITE_ADDRESSBOOK:
			Throw_(ioErr);
			break;
		case XP_BKMKS_NICKNAME_ALREADY_EXISTS:
			Throw_(XP_BKMKS_INVALID_NICKNAME);
			break;
		default:
			Assert_(false);
			Throw_(32000);	// Who knows?
			break;		
	}	
}


/*======================================================================================
	Register all classes associated with the address book window.
======================================================================================*/

void CAddressBookManager::RegisterAddressBookClasses() {

	RegisterClass_(CAddressBookWindow);
	RegisterClass_(CAddressBookTableView);
	RegisterClass_(CAddressBookListTableView);
	RegisterClass_(CAddressPickerWindow);
	RegisterClass_(CSearchPopupMenu);

	RegisterClass_(CNamePropertiesWindow);
	RegisterClass_(CListPropertiesWindow);

	RegisterClass_(CAddressBookController);
	RegisterClass_( CLDAPQueryDialog );
	CGATabBox::RegisterTabBoxClasses();
}

/*======================================================================================
	Find an open instance of the name properties window with the specified ID.
======================================================================================*/

CNamePropertiesWindow *CAddressBookManager::FindNamePropertiesWindow(ABID inEntryID) {

	CNamePropertiesWindow *propertiesWindow = nil;
	
	// Try to locate the specific window
	CMediatedWindow *theWindow;
	CWindowIterator theIterator(WindowType_AddressPerson);

	while ( theIterator.Next(theWindow) ) {
		propertiesWindow = dynamic_cast<CNamePropertiesWindow *>(theWindow);
		AssertFail_(propertiesWindow != nil);
		if ( propertiesWindow->GetEntryID() == inEntryID ) {
			break;	// We found our window!
		} else {
			propertiesWindow = nil;
		}
	}

	return propertiesWindow;
}


/*======================================================================================
	Return an instance of the name properties window. If inEntryID is not equal to 
	CAddressBookPane::eNewEntryID, try to locate the window with the specified entry
	ID; otherwise, try to locate the first top window. If inDoCreate is true, create the 
	window if it is not found, otherwise return the found window or nil. The caller should 
	always call Show() and Select() on the window.
======================================================================================*/

CNamePropertiesWindow *CAddressBookManager::GetNamePropertiesWindow(ABID inEntryID, Boolean inOptionKeyDown) {

	CNamePropertiesWindow *propertiesWindow = nil;
	
	// Try to locate a window with the specified ID
	
	if ( inEntryID != CAddressBookPane::eNewEntryID ) {
		propertiesWindow = FindNamePropertiesWindow(inEntryID);
	}
	
	if ( propertiesWindow == nil ) {
		// Couldn't find a window with the specified ID
		if ( inOptionKeyDown ) {
			// Try to locate the top window
			propertiesWindow = dynamic_cast<CNamePropertiesWindow *>(
				CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_AddressPerson));
		}
		if ( propertiesWindow == nil ) {
			// Create a new window
			CAddressBookWindow *addressWindow = dynamic_cast<CAddressBookWindow *>(
					CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Address));
			AssertFail_(addressWindow != nil);	// Must be around to create name properties window

			// Create the window
			propertiesWindow = dynamic_cast<CNamePropertiesWindow *>(
						URobustCreateWindow::CreateWindow(CNamePropertiesWindow::res_ID, 
														  addressWindow->GetSuperCommander()));
			AssertFail_(propertiesWindow != nil);
		}
	}

	return propertiesWindow;
}


/*======================================================================================
	Find an open instance of the list properties window with the specified ID.
======================================================================================*/

CListPropertiesWindow *CAddressBookManager::FindListPropertiesWindow(ABID inEntryID) {

	CListPropertiesWindow *propertiesWindow = nil;
	
	// Try to locate the specific window
	CMediatedWindow *theWindow;
	CWindowIterator theIterator(WindowType_AddressList);

	while ( theIterator.Next(theWindow) ) {
		propertiesWindow = dynamic_cast<CListPropertiesWindow *>(theWindow);
		AssertFail_(propertiesWindow != nil);
		if ( propertiesWindow->GetEntryID() == inEntryID ) {
			break;	// We found our window!
		} else {
			propertiesWindow = nil;
		}
	}

	return propertiesWindow;
}

//-----------------------------------
CListPropertiesWindow *CAddressBookManager::GetListPropertiesWindow(ABID inEntryID, Boolean inOptionKeyDown)
//	Return an instance of the list properties window. If inEntryID is not equal to 
//	CAddressBookPane::eNewEntryID, try to locate the window with the specified entry
//	ID; otherwise, try to locate the first top window. If inDoCreate is true, create the 
//	window if it is not found, otherwise return the found window or nil. The caller should 
//	always call Show() and Select() on the window.
//-----------------------------------
{

	CListPropertiesWindow *propertiesWindow = nil;
	
	// Try to locate a window with the specified ID
	
	if ( inEntryID != CAddressBookPane::eNewEntryID ) {
		propertiesWindow = FindListPropertiesWindow(inEntryID);
	}
	
	if ( propertiesWindow == nil ) {
		// Couldn't find a window with the specified ID
		if ( inOptionKeyDown ) {
			// Try to locate the top window
			propertiesWindow = dynamic_cast<CListPropertiesWindow *>(
				CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_AddressList));
		}
		if ( propertiesWindow == nil ) {
			// Create a new window
			CAddressBookWindow *addressWindow = dynamic_cast<CAddressBookWindow *>(
					CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Address));
			AssertFail_(addressWindow != nil);	// Must be around to create name properties window

			// Create the window
			propertiesWindow = dynamic_cast<CListPropertiesWindow *>(
						URobustCreateWindow::CreateWindow(CListPropertiesWindow::res_ID, 
														  addressWindow->GetSuperCommander()));
			AssertFail_(propertiesWindow != nil);
		}
	}

	return propertiesWindow;
} // CAddressBookWindow::GetListPropertiesWindow
#pragma mark -
/*======================================================================================
	Stop any current search.
======================================================================================*/

CAddressBookWindow::~CAddressBookWindow() {

	// Loop though current visible windows to close any name properties or list view windows
	
	CloseAddressBookChildren();
#if 0 // Don't know why this code exists
	Boolean canRotate = false;
	if ( IsVisible() ) {
		USearchHelper::FindWindowTabGroup(&mSubCommanders)->SetRotateWatchValue(&canRotate);
	}
#endif // 0
	
}


/*======================================================================================
	Try to close the address book children windows. Return true if all windows were closed
	successfully, or false if the user cancelled closing an open window.
======================================================================================*/

Boolean CAddressBookWindow::CloseAddressBookChildren() {

	// Loop though current visible windows to close any name properties or list view windows
	
	CWindowMediator *mediator = CWindowMediator::GetWindowMediator();
	Assert_(mediator != nil);
	
	CMediatedWindow *theWindow;
	
	Boolean rtnVal = true;
	
	Try_ {
	
		while ( ((theWindow = mediator->FetchTopWindow(WindowType_AddressPerson)) != nil) ||
				((theWindow = mediator->FetchTopWindow(WindowType_AddressList)) != nil) ) {
		
			theWindow->DoClose();
		}
	}
	Catch_(inErr) {
		rtnVal = false;
	}
	EndCatch_
	
	return rtnVal;
}


/*======================================================================================
	Get the mail context for the address book. We must have an address book window to 
	access the context, so this method creates the window (but doesn't show it) if the
	window is not yet around.
======================================================================================*/

MWContext *CAddressBookWindow::GetMailContext() {

	CAddressBookWindow *addressWindow = dynamic_cast<CAddressBookWindow *>(
				CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Address));
	
	if ( addressWindow == nil ) {
		// Search dialog has not yet been created, so create it here and display it.
		addressWindow = dynamic_cast<CAddressBookWindow *>(
					URobustCreateWindow::CreateWindow(CAddressBookWindow::res_ID, 
													  LCommander::GetTopCommander()));
		AssertFail_(addressWindow != nil);
	}

	return *(addressWindow->GetWindowContext());
}


//-----------------------------------
void CAddressBookWindow::FinishCreateSelf()
//-----------------------------------
{
	
	mAddressBookController = dynamic_cast< CAddressBookController* >
						(FindPaneByID( paneID_AddressBookController) ) ;
	FailNILRes_(mAddressBookController);
	
	CMediatedWindow::FinishCreateSelf();	// Call CMediatedWindow for now since we need to
											// create a different context than CMailNewsWindow

	UReanimator::LinkListenerToControls(mAddressBookController, this,CAddressBookWindow:: res_ID);
	
	// Create context and and progress listener
	
	mMailNewsContext = new CMailNewsContext(MWContextAddressBook);
	FailNIL_(mMailNewsContext);
	mMailNewsContext->AddUser(this);
	mMailNewsContext->AddListener(mAddressBookController);


	mMailNewsContext->SetWinCSID(INTL_DocToWinCharSetID(mMailNewsContext->GetDefaultCSID()));

	
	CMediatedWindow::FinishCreateSelf();	// Call CMediatedWindow for now since we need to
											// create a different context than CMailNewsWindow

	CStr255 windowTitle;
	GetUserWindowTitle(4, windowTitle);
	SetDescriptor(windowTitle);	
	
	// Call inherited method
	FinishCreateWindow();
}



//-----------------------------------
void CAddressBookWindow::ReadWindowStatus(LStream *inStatusData)
//	Adjust the window to stored preferences.
//-----------------------------------
{
	Int32 directoryIndex = 1;
	Inherited::ReadWindowStatus(inStatusData);
	if ( inStatusData != nil )
	{

		*inStatusData >> directoryIndex;
		mAddressBookController->ReadStatus( inStatusData );
	}
	
	// The popup is set up to item 1, and setting the value does nothing if the value
	// is the same as the existing value.  Hence this special case.
	if (directoryIndex == 1)
		mAddressBookController->SelectDirectoryServer(nil, directoryIndex);
	else
		mAddressBookController->SetPopupFromServerIndex(directoryIndex);
	
}

//-----------------------------------
void CAddressBookWindow::WriteWindowStatus(LStream *outStatusData)
//	Write window stored preferences.
//-----------------------------------
{

	Inherited::WriteWindowStatus(outStatusData);
	*outStatusData << mAddressBookController->GetServerIndexFromPopup();
	mAddressBookController->WriteStatus(outStatusData);
	
}

#pragma mark -

DIR_Server *CAddressBookPane::sCurrentBook = nil;

//-----------------------------------
void CAddressBookPane::FinishCreateSelf()
//-----------------------------------
{
	LPane *pane = USearchHelper::GetPaneWindow(this)->FindPaneByID(paneID_ProgressBar);
	mProgressBar = dynamic_cast<CGAStatusBar *>(pane);
	Inherited::FinishCreateSelf();
}

//-----------------------------------
void CAddressBookPane::DrawCellContents(const STableCell &inCell, const Rect &inLocalRect)
//	Draw the table view cell.
//-----------------------------------
{
	CStr255 displayText;
	ResIDT iconID;

	ABID entryID, type;
	GetEntryIDAndType(inCell.row, &entryID, &type);

	GetDisplayData((EColType) GetCellDataType(inCell), entryID, type, &displayText, &iconID);
	
	Rect cellDrawFrame;
	
	if ( iconID != 0 ) {
		::SetRect(&cellDrawFrame, 0, 0, 16, 16);
		UGraphicGizmos::CenterRectOnRect(cellDrawFrame, inLocalRect);
	} else {
		cellDrawFrame = inLocalRect;
		::InsetRect(&cellDrawFrame, 2, 0);	// For a nicer look between cells
	}
	
	Boolean cellIsSelected = CellIsSelected(inCell);

	StSectClipRgnState saveSetClip(&cellDrawFrame);

	//if ( cellIsSelected && DrawHiliting() ) {
		if ( iconID != 0 ) {
			// Display an icon
			::PlotIconID(&cellDrawFrame, ttNone, ttNone, iconID);
		} else {
			DrawTextString( displayText,  &mTextFontInfo, 0, 
								   cellDrawFrame, teFlushLeft,
								   true, truncMiddle);
		}
	#if 0
	} else {
		if ( iconID != 0 ) {
			// Display an icon
			::PlotIconID(&cellDrawFrame, ttNone, ttNone, iconID);
		} else {
			// Display text
			if ( displayText.Length() > 0 ) {
				UGraphicGizmos::PlaceTextInRect((char *) &displayText[1], displayText[0],
												cellDrawFrame, teFlushLeft, teCenter, 
												&mTextFontInfo, true, truncMiddle);
			}
		}
	
	}
	#endif
} //  CAddressBookPane::DrawCell


//-----------------------------------
void CAddressBookPane::SetUpTableHelpers()
//-----------------------------------
{

	SetTableGeometry(new LFlexTableGeometry(this, mTableHeader));
	SetTableSelector(new LTableRowSelector(this));

	AddAttachment( new CTableRowKeyAttachment(this) ); // need for arrow naviagation
	// We don't need table storage. It's safe to have a null one.
	AssertFail_(mTableStorage == nil);
}

//-----------------------------------
void CAddressBookPane::GetEntryIDAndType(TableIndexT inRow, ABID *outID, ABID *outType)
//	Get the entry ID and data type for the specified row.
//-----------------------------------
{

	Assert_(GetMessagePane() != nil);
	Assert_(sCurrentBook != nil);

	*outID = AB_GetEntryIDAt((AddressPane*) GetMessagePane(), inRow - 1);
	Int32 error = AB_GetType(sCurrentBook, CAddressBookManager::GetAddressBook(), 
							 *outID, outType);
	ASSERT_ADDRESS_ERROR(error);
}

//-----------------------------------
ABID CAddressBookPane::GetEntryID(TableIndexT inRow)
//-----------------------------------
{
	Assert_(GetMessagePane() != nil);
	Assert_((inRow <= mRows) && (inRow > 0));
	if (inRow > mRows || inRow == 0)
		return MSG_MESSAGEIDNONE;
	
	ABID id = AB_GetEntryIDAt( (AddressPane*) GetMessagePane(), inRow - 1);
	Assert_(id != MSG_MESSAGEIDNONE);

	return id;
}


//-----------------------------------
void CAddressBookPane::GetDisplayData(EColType inColType, ABID inEntryID, ABID inType, 
									  CStr255 *outDisplayText, ResIDT *outIconID)
//-----------------------------------
{
	*outDisplayText = CStr255::sEmptyString;
	*outIconID = 0;
	
	switch ( inColType ) {
	
		case eColType: {
				if ( inType == ABTypeList ) {
					*outIconID = ics8_List;
				} else {
					*outIconID = ics8_Person;
				}
			}
			break;
	
		case eColName: {
				Int32 error = AB_GetFullName(sCurrentBook, CAddressBookManager::GetAddressBook(), inEntryID, 
											 (char *) ((UInt8 *) &(*outDisplayText)[0]));
				ASSERT_ADDRESS_ERROR(error);
			}
			break;
	
		case eColEmail: {
				Int32 error = AB_GetEmailAddress(sCurrentBook, CAddressBookManager::GetAddressBook(), inEntryID, 
											 	 (char *) ((UInt8 *) &(*outDisplayText)[0]));
				ASSERT_ADDRESS_ERROR(error);
			}
			break;
	
		case eColCompany: {
				Int32 error = AB_GetCompanyName(sCurrentBook, CAddressBookManager::GetAddressBook(), inEntryID, 
											 	(char *) ((UInt8 *) &(*outDisplayText)[0]));
				ASSERT_ADDRESS_ERROR(error);
			}
			break;
	
		case eColLocality: {
				Int32 error = AB_GetLocality(sCurrentBook, CAddressBookManager::GetAddressBook(), inEntryID, 
											 (char *) ((UInt8 *) &(*outDisplayText)[0]));
				ASSERT_ADDRESS_ERROR(error);
			}
			break;
	
		case eColNickname: {
				Int32 error = AB_GetNickname(sCurrentBook, CAddressBookManager::GetAddressBook(), inEntryID, 
											 (char *) ((UInt8 *) &(*outDisplayText)[0]));
				ASSERT_ADDRESS_ERROR(error);
			}
			break;
		case eColWorkPhone:
			Int32 error = AB_GetWorkPhone(sCurrentBook, CAddressBookManager::GetAddressBook(), inEntryID, 
											 (char *) ((UInt8 *) &(*outDisplayText)[0]));
			ASSERT_ADDRESS_ERROR(error);
			break;	
	

		default:
			Assert_(false);	// Should never get here!
			break;
	}
	
	if ( outDisplayText->Length() > 0 ) {
		// We have a c string, convert to pascal
		C2PStr((char *) ((UInt8 *) &(*outDisplayText)[0]));
	}
}


/*======================================================================================
	Get the current sort params for the address book data.
======================================================================================*/

void CAddressBookPane::GetSortParams(UInt32 *outSortKey, Boolean *outSortBackwards) {

	AssertFail_(mTableHeader != nil);
	PaneIDT sortColumnID;
	mTableHeader->GetSortedColumn(sortColumnID);
	*outSortKey = SortTypeFromColumnType((EColType) sortColumnID);
	*outSortBackwards = mTableHeader->IsSortedBackwards();
}


/*======================================================================================
	Call CStandardFlexTable's method and add actual ABIDs instead.
======================================================================================*/

void CAddressBookPane::AddSelectionToDrag(DragReference inDragRef, RgnHandle inDragRgn) {

	CStandardFlexTable::AddSelectionToDrag(inDragRef, inDragRgn);
}


/*======================================================================================
	Add the actual ABID.
======================================================================================*/

void CAddressBookPane::AddRowDataToDrag(TableIndexT inRow, DragReference inDragRef) {


	AssertFail_(mDragFlavor == kMailAddresseeFlavor);
	
	SAddressDragInfo info;
	info.id = GetEntryID(inRow);
	info.dir = sCurrentBook;
	
	OSErr err = ::AddDragItemFlavor(inDragRef, inRow, mDragFlavor, 
									&info, sizeof(info), flavorSenderOnly);
	FailOSErr_(err);


	// temporary while the old address book is still being used.
	char* fullName = NULL;
	
	GetFullAddress( inRow, &fullName);
	
	Size  dataSize  = XP_STRLEN ( fullName );
	err = ::AddDragItemFlavor(inDragRef, inRow, 'TEXT', fullName, dataSize, 0 );
	
	XP_FREE(fullName);
	
	FailOSErr_(err);

}


/*======================================================================================
	Return a valid BE sort type from the specified column type.
======================================================================================*/

UInt32 CAddressBookPane::SortTypeFromColumnType(EColType inColType) {

	UInt32 rtnVal;
	
	switch ( inColType ) {
		case eColType: rtnVal = ABTypeEntry; break;
		case eColName: rtnVal = ABFullName; break;
		case eColEmail: rtnVal = ABEmailAddress; break;
		case eColCompany: rtnVal = ABCompany; break;
		case eColLocality: rtnVal = ABLocality; break;
		case eColNickname: rtnVal = ABNickname; break;
		default: rtnVal = ABFullName; break;
	}
	
	return rtnVal;
}


/*======================================================================================
	Return a valid BE sort type from the specified column type.
======================================================================================*/

AB_CommandType CAddressBookPane::SortCommandFromColumnType(EColType inColType) {

	AB_CommandType rtnVal;
	
	switch ( inColType ) {
		case eColType: rtnVal = AB_SortByTypeCmd; break;
		case eColName: rtnVal = AB_SortByFullNameCmd; break;
		case eColEmail: rtnVal = AB_SortByEmailAddress; break;
		case eColCompany: rtnVal = AB_SortByCompanyName; break;
		case eColLocality: rtnVal = AB_SortByLocality; break;
		case eColNickname: rtnVal = AB_SortByNickname; break;
		default: rtnVal = AB_SortByFullNameCmd; break;
	}
	
	return rtnVal;
}


/*======================================================================================
	Get the number of entries for the specified entry.
======================================================================================*/

Int32 CAddressBookPane::GetBENumEntries(ABID inType, ABID inListID) {

	UInt32 numRows;
	Int32 error = AB_GetEntryCount(sCurrentBook, CAddressBookManager::GetAddressBook(), 
								   &numRows, inType, inListID);
	ASSERT_ADDRESS_ERROR(error);
	return numRows;
}


/*======================================================================================
	Compose a mail message with the selected items.
======================================================================================*/

void CAddressBookPane::ComposeMessage() {

	AssertFail_(GetMessagePane() != nil);

	CMailSelection	selection;
	if ( GetSelection(selection) )
	{
		Try_ {
			
			CAddressBookManager::FailAddressError(AB_Command((ABPane *) GetMessagePane(), 
												  AB_NewMessageCmd,
												   (MSG_ViewIndex*)selection.GetSelectionList(),
												    selection.selectionSize));
		}
		Catch_(inErr) {
			Throw_(inErr);
		}
		EndCatch_
	}
}

/*======================================================================================
	Open and show an edit window for the specified address book entry (list or person). 
======================================================================================*/

void CAddressBookPane::ShowEditWindowByEntryID(ABID inEntryID, Boolean inOptionKeyDown) {
#pragma unused (inOptionKeyDown)
	ABID type;
	Int32 error = AB_GetType(sCurrentBook, CAddressBookManager::GetAddressBook(), inEntryID, &type);
	ASSERT_ADDRESS_ERROR(error);
	
	ShowEditWindow(inEntryID, type);
}


#pragma mark -

/*======================================================================================
	Adjust the window to stored preferences.
======================================================================================*/

CAddressBookTableView::~CAddressBookTableView() {

	SetMessagePane( NULL );
}


/*======================================================================================
	Dispose of the address book pane.
======================================================================================*/

void CAddressBookPane::DestroyMessagePane(MSG_Pane* inPane) {

//	if ( inRefresh && (mRows > 0) ) RemoveRows(mRows, 1, true);

	if ( inPane != nil ) {
		::SetCursor(*::GetCursor(watchCursor));	// Could take forever
		Int32 error = AB_CloseAddressBookPane((ABPane **) &inPane);
		
		ASSERT_ADDRESS_ERROR(error);
	//	sCurrentBook = nil;
	}

	
}


//-----------------------------------
Boolean CAddressBookTableView::LoadAddressBook(DIR_Server *inDir)
//	Load and display the specified address book. A reference is stored to the specified 
//	DIR_Server, so it should not be deleted without deleting the pane. If the method
//	returns true, the address book was able to be loaded; if the method returns false,
//	the address book could not be loaded because the user decided to continue modifying
//	the current one.
//-----------------------------------
{
	AssertFail_(inDir != nil);

//	if ( inDir == sCurrentBook ) return true;	// Already set!

	
	TableIndexT rows, cols;
	GetTableSize(rows, cols);
	if (rows > 0)
		RemoveRows(rows, 1, false);

	sCurrentBook = inDir;
	
	Try_ {
		::SetCursor(*::GetCursor(watchCursor));	// Could take forever
		if ( GetMessagePane() != nil )
		{
			CAddressBookManager::FailAddressError(AB_ChangeDirectory((ABPane *) GetMessagePane(), inDir));
			mTableHeader->SetSortOrder(false);
			// Big hack to avoid having to fix some notification 
			SetRowCount();
			Refresh();
		}
		else
		{
			MWContext *context = CAddressBookWindow::GetMailContext();
			UInt32 sortKey;
			Boolean sortBackwards;
			GetSortParams(&sortKey, &sortBackwards);
			// Sorting on Keys other than ABFullname, ABEmailAddress, ABNickname
			// is slow ( see bug #81275 )
			switch ( sortKey )
			{
				case ABFullName:
				case ABEmailAddress:
				case ABNickname:
					break;
				default:
					sortKey = ABFullName;
					mTableHeader->SetWithoutSort(
						 mTableHeader->ColumnFromID( eColName ), sortBackwards, true );		
			}
			MSG_Pane* pane;
			CAddressBookManager::FailAddressError(
				AB_CreateAddressBookPane(
					(ABPane **) &pane,
					context, 
					CMailNewsContext::GetMailMaster())
				);
			SetMessagePane( pane );
			sCurrentBook = inDir;
			MSG_SetFEData((MSG_Pane *) GetMessagePane(), CMailCallbackManager::Get());
			CAddressBookManager::FailAddressError(
				AB_InitializeAddressBookPane(
					(ABPane *) GetMessagePane(),
					inDir, 
					CAddressBookManager::GetAddressBook(),
					sortKey,
					!sortBackwards)
				);
		}

	}
	Catch_(inErr) {
		SetMessagePane( NULL );
		Throw_(inErr);
	}
	EndCatch_
	
	return true;
} // CAddressBookTableView::LoadAddressBook

//------------------------------------
void CAddressBookTableView::ButtonMessageToCommand( MessageT inMessage, void* ioParam )
//------------------------------------
{
	ObeyCommand( inMessage, ioParam );
} // CAddressBookTableView::ButtonMessageToCommand


//----------------------------------
Boolean CAddressBookTableView::ObeyCommand(CommandT inCommand, void *ioParam)
//-----------------------------------
{
	Boolean rtnVal = true;

	switch ( inCommand ) {
	
		case CAddressBookPane::eColType:
		case CAddressBookPane::eColName:
		case CAddressBookPane::eColEmail:
		case CAddressBookPane::eColCompany:
		case CAddressBookPane::eColLocality:
		case CAddressBookPane::eColNickname:
			GetTableHeader()->SimulateClick(inCommand);
			rtnVal = true;
			break;
	
		case CAddressBookPane::cmd_SortAscending:
		case CAddressBookPane::cmd_SortDescending:
			GetTableHeader()->SetSortOrder(inCommand == CAddressBookPane::cmd_SortDescending);
			break;

		case cmd_NewAddressCard:
			NewCard();
			break;

		case cmd_NewAddressList:
			NewList();
			break;

		case cmd_HTMLDomains:
			MSG_DisplayHTMLDomainsDialog( CAddressBookWindow::GetMailContext() );
			break;

		case cmd_EditProperties:
			OpenSelection();
			break;

		case cmd_DeleteEntry:
			DeleteSelection();
			break;

		case cmd_ImportAddressBook:
			ImportAddressBook();
			break;

		case cmd_NewMailMessage:
		case cmd_ComposeMailMessage:
			ComposeMessage();
			break;
		

		case cmd_SaveAs:
			SaveAs();
			break;
		case cmd_SecurityInfo:
			MWContext * context = CAddressBookWindow::GetMailContext();
			SECNAV_SecurityAdvisor( context, NULL );
			return true;
			
		case cmd_ViewMyCard:
			ShowMyAddressCard();
			break;
			
		default:
			rtnVal = Inherited::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return rtnVal;
}

//-----------------------------------
void CAddressBookTableView:: FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
									  Boolean &outUsesMark, Char16 &outMark,
									  Str255 outName)
//-----------------------------------
{

	switch ( inCommand ) {

		case CAddressBookPane::eColType:
		case CAddressBookPane::eColName:
		case CAddressBookPane::eColEmail:
		case CAddressBookPane::eColCompany:
		case CAddressBookPane::eColLocality:
		case CAddressBookPane::eColNickname:
			outEnabled = IsColumnVisible(inCommand);
			outUsesMark = true;
			if ( outEnabled ) {
				outMark = (GetSortedColumn() == inCommand) ? checkMark : noMark;
			} else {
				outMark = noMark;
			}
			break;

		case CAddressBookPane::cmd_SortAscending:
		case CAddressBookPane::cmd_SortDescending:
			outEnabled = true;
			outUsesMark = true;
			if ( inCommand == CAddressBookPane::cmd_SortAscending ) {
				outMark = (IsSortedBackwards() ? noMark : checkMark);
			} else {
				outMark = (IsSortedBackwards() ? checkMark : noMark);
			}
			break;
	
		case cmd_NewAddressCard:
		case cmd_NewAddressList:
		case cmd_ImportAddressBook:
		case cmd_SaveAs:
		case cmd_ViewMyCard:
		case cmd_ConferenceCall:
			outEnabled = CAddressBookTableView::CurrentBookIsPersonalBook();
			break;

		case cmd_HTMLDomains:
			outEnabled = true;
			break;
		
		case cmd_EditProperties:
		case cmd_DeleteEntry:
			if ( GetSelectedRowCount() > 0 ) {
				outEnabled = CAddressBookTableView::CurrentBookIsPersonalBook();
			}
			break;

		case cmd_ComposeMailMessage:
			outEnabled = true;
			break;
	
		default:
			Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	}
}
//-----------------------------------
void CAddressBookTableView::DeleteSelection()
//	Delete all selected items.
//-----------------------------------
{
	if ( !CAddressBookTableView::CurrentBookIsPersonalBook() ) return;

	AssertFail_(GetMessagePane() != nil);

	TableIndexT *indices = nil;

	CMailSelection selection;
	
	if ( GetSelection( selection ) )
	{
		TableIndexT count = selection.selectionSize;
		indices = (MSG_ViewIndex*)selection.GetSelectionList();
		Try_ {
			
			{	// Close all open windows to delete
				ABID id;
				ABID type;
				
				for( Int32 index = 0; index < count; index ++ )
				{
					GetEntryIDAndType( indices[ index ]+1 , &id, &type);
					CloseEditWindow(id, type);
				} 
			}
			
			CAddressBookManager::FailAddressError(AB_Command((ABPane *) GetMessagePane(), 
												  AB_DeleteCmd, indices, count));
			
		}
		Catch_(inErr) {
			
			Throw_(inErr);
		}
		EndCatch_
	}
}

				
/*======================================================================================
	Import an address book and append it to the local address book.
======================================================================================*/

void CAddressBookTableView::ImportAddressBook() {

	AssertFail_(GetMessagePane() != nil);
	AssertFail_(sCurrentBook == CAddressBookManager::GetPersonalBook());

	CAddressBookManager::FailAddressError(AB_ImportFromFile((ABPane *) GetMessagePane(), 
										  CAddressBookWindow::GetMailContext()));
}

// Save As

void CAddressBookTableView::SaveAs()
{
	CAddressBookManager::FailAddressError(
		AB_ExportToFile( (ABPane *) GetMessagePane(),(MWContext*) CAddressBookWindow::GetMailContext() )  );
}




/*======================================================================================
	Create a new card.
======================================================================================*/

void CAddressBookTableView::NewCard(PersonEntry* pPerson) {

	if ( !CAddressBookTableView::CurrentBookIsPersonalBook() ) return;

	ABID entryID  = CAddressBookPane::eNewEntryID;
	
	CNamePropertiesWindow *namePropertiesWindow = 
			CAddressBookManager::GetNamePropertiesWindow(entryID, false);
	AssertFail_(namePropertiesWindow != nil);	// Must be there!
		
	namePropertiesWindow->AddListener(this);
		
	// Update the window properties
	if( pPerson )
		namePropertiesWindow->UpdateUIToPerson( pPerson, entryID );
	namePropertiesWindow->SetConferenceText( );
	namePropertiesWindow->Show();
	namePropertiesWindow->Select();
}


/*======================================================================================
	Create a new list.
======================================================================================*/

void CAddressBookTableView::NewList() {

	if ( !CAddressBookTableView::CurrentBookIsPersonalBook() ) return;
	ABID entryID  = CAddressBookPane::eNewEntryID;
	#if HOTEDITWINDOWS
	{
		StMailingListEntry listEntry;
		CAddressBookManager::FailAddressError(AB_AddMailingList(sCurrentBook, CAddressBookManager::GetAddressBook(), 
											  listEntry.GetEntry(), &entryID));
	}
	#endif // HOTEDITWINDOWS
	ShowEditWindow(entryID, ABTypeList);
}


/*======================================================================================
	Open the selected message into its own window.
======================================================================================*/

void CAddressBookTableView::OpenRow(TableIndexT inRow) {

	if ( !CAddressBookTableView::CurrentBookIsPersonalBook() ) return;

	ABID id;
	ABID type;
	GetEntryIDAndType(inRow, &id, &type);
	
	ShowEditWindow(id, type);
}


/*======================================================================================
	Open the selected person or list into its own window.
======================================================================================*/

void CAddressBookPane::ShowEditWindow(ABID inID, ABID inType) {

	// Find out if the option key is down
	
// Not implemented yet	const Boolean optionKeyDown = USearchHelper::KeyIsDown(USearchHelper::char_OptionKey);
	const Boolean optionKeyDown = false;	

	LWindow *theWindow;

	if ( inType == ABTypeList ) {
		CListPropertiesWindow *listPropertiesWindow = 
			CAddressBookManager::GetListPropertiesWindow(inID, optionKeyDown);
		AssertFail_(listPropertiesWindow != nil);	// Must be there!
		
		
		// Update the window properties
		listPropertiesWindow->UpdateUIToBackend(sCurrentBook, CAddressBookManager::GetAddressBook(), inID);
		theWindow = listPropertiesWindow;
	} else {
		Assert_(inType == ABTypePerson);
		CNamePropertiesWindow *namePropertiesWindow = 
			CAddressBookManager::GetNamePropertiesWindow(inID, optionKeyDown);
		AssertFail_(namePropertiesWindow != nil);	// Must be there!
		
		
		// Update the window properties
		
		namePropertiesWindow->UpdateUIToBackend(sCurrentBook, CAddressBookManager::GetAddressBook(), inID);
		theWindow = namePropertiesWindow;
	}
	
	theWindow->Show();
	theWindow->Select();
}

// Call the selected person
void	CAddressBookTableView::ConferenceCall( )
{
	TableIndexT selectedRow=0;
	GetNextSelectedRow( selectedRow );
	if ( !CAddressBookTableView::CurrentBookIsPersonalBook() ) 
		return;

	ABID id;
	ABID type;
	GetEntryIDAndType( selectedRow, &id, &type );
	char emailAddress[kMaxEmailAddressLength];
	char ipAddress[kMaxCoolAddress];
	short serverType;
	AB_GetUseServer( sCurrentBook, CAddressBookManager::GetAddressBook(), id, &serverType );
	AB_GetEmailAddress( sCurrentBook, CAddressBookManager::GetAddressBook(), id, emailAddress );
	AB_GetCoolAddress(  sCurrentBook, CAddressBookManager::GetAddressBook(), id, ipAddress );
	
	// Check to see if we have all the data we need
	if ( (serverType==kSpecificDLS || serverType==kHostOrIPAddress ) && !XP_STRLEN( ipAddress ))
	{
		FE_Alert( CAddressBookWindow::GetMailContext(), XP_GetString(MK_MSG_CALL_NEEDS_IPADDRESS));
		return;
	}
	if ( (serverType==kSpecificDLS || serverType==kDefaultDLS) && !XP_STRLEN( emailAddress ))
	{
		FE_Alert( CAddressBookWindow::GetMailContext(), XP_GetString(MK_MSG_CALL_NEEDS_EMAILADDRESS));
		return;
	}
	char *ipAddressDirect = NULL;
	char *serverAddress = NULL;
	char *address = NULL;  
	switch( serverType)
	{
		case kDefaultDLS:
			address = &emailAddress[0];		
			break;
		case kSpecificDLS:
			address = &emailAddress[0];
			serverAddress = &ipAddress[0];
			break;
		case kHostOrIPAddress:
			ipAddressDirect = &ipAddress[0];
			break;
		default:
			break;
	}
	// And now the AE fun begins
	AppleEvent event,reply;
	OSErr error;
	ProcessSerialNumber targetPSN;
	Boolean isAppRunning;
	static const OSType kConferenceAppSig = 'Ncq¹'; // This needs to be in a header file 
													// so that uapp and us share it
	isAppRunning = UProcessUtils::ApplicationRunning( kConferenceAppSig, targetPSN );
	if( !isAppRunning)
	{
		ObeyCommand(cmd_LaunchConference, NULL);
		isAppRunning = UProcessUtils::ApplicationRunning( kConferenceAppSig, targetPSN );
	}
	
	if( !isAppRunning ) // for some reason we were unable to open app
			return;
	Try_
	{
		error = AEUtilities::CreateAppleEvent( 'VFON', 'CALL', event, targetPSN );
		ThrowIfOSErr_(error);		
		if( ipAddressDirect)
 		{
 			error = ::AEPutParamPtr(&event, '----', typeChar, ipAddressDirect, XP_STRLEN(ipAddressDirect) );
 			ThrowIfOSErr_(error);
 		}
 		if( address)
 		{
 			error = ::AEPutParamPtr(&event, 'MAIL', typeChar, address, XP_STRLEN(address) );
 			ThrowIfOSErr_(error);
 		}
 		if( serverAddress )
 		{
 			error = ::AEPutParamPtr(&event, 'SRVR', typeChar, serverAddress, XP_STRLEN(serverAddress) );
 			ThrowIfOSErr_(error);
 		}
 	
		// NULL reply parameter
		reply.descriptorType = typeNull;
		reply.dataHandle = nil;
		
		error = AESend(&event,&reply,kAENoReply + kAENeverInteract 
			+ kAECanSwitchLayer+kAEDontRecord,kAENormalPriority,-1,nil,nil);
		ThrowIfOSErr_(error);
		
		UProcessUtils::PullAppToFront( targetPSN );
		
	}
	Catch_(inError) // in case of errors do nothing
 	{
 	}
	AEDisposeDesc(&reply);
	AEDisposeDesc(&event);
	
}
/*======================================================================================
	Close the selected person or list properties window if there is one.
======================================================================================*/

void CAddressBookTableView::CloseEditWindow(ABID inID, ABID inType) {

	LWindow *theWindow;

	if ( inType == ABTypeList ) {
		theWindow = CAddressBookManager::FindListPropertiesWindow(inID);
	} else {
		Assert_(inType == ABTypePerson);
		theWindow = CAddressBookManager::FindNamePropertiesWindow(inID);
	}
	
	if ( theWindow != nil ) theWindow->DoClose();
}

//-----------------------------------
void CAddressBookTableView::SetColumnHeaders(DIR_Server* server)
//-----------------------------------
{
		MSG_SearchAttribute		attr;
		DIR_AttributeId			id;
		STableCell				aCell;
		const char*				colName;
		LTableViewHeader*		tableHeader = GetTableHeader();
		PaneIDT					headerPaneID;

	for (short col = 1; col <= tableHeader->CountColumns(); col ++)
	{
		headerPaneID = tableHeader->GetColumnPaneID(col);
		switch (headerPaneID)
		{
			case CAddressBookPane::eColName:		attr = attribCommonName;	break;
			case CAddressBookPane::eColEmail:		attr = attrib822Address;	break;
			case CAddressBookPane::eColCompany:		attr = attribOrganization;	break;
			case CAddressBookPane::eColLocality:	attr = attribLocality;		break;
			case CAddressBookPane::eColWorkPhone:	attr = attribPhoneNumber; break;
			case CAddressBookPane::eColNickname:							// no break;
			default:								attr = kNumAttributes;		break;
		}

		if (attr < kNumAttributes)
		{
			MSG_SearchAttribToDirAttrib(attr, &id);
			colName = DIR_GetAttributeName(server, id);
			if (colName && *colName)
			{
				LCaption* headerPane = dynamic_cast<LCaption*>(FindPaneByID(headerPaneID));
				if (headerPane)
				{
					CStr255		cStr(colName);
					headerPane->SetDescriptor((StringPtr)cStr);
				}
			}
		}
	}
}

/*======================================================================================
	Force a sort to occur based upon the current sort column.
======================================================================================*/

void CAddressBookTableView::ForceCurrentSort() {

	UInt32 sortKey;
	Boolean sortBackwards;
	
	GetSortParams(&sortKey, &sortBackwards);

/*
	mSearchManager->SortSearchResults(sortKey, sortBackwards);
*/
}


/*======================================================================================
	Notification to sort the table.
======================================================================================*/

void CAddressBookTableView::ChangeSort(const LTableHeader::SortChange *inSortChange) {

	Assert_(GetMessagePane() != nil);
	Assert_(sCurrentBook != nil);

	AB_CommandType sortCmd = SortCommandFromColumnType((EColType) inSortChange->sortColumnID);
	
	::SetCursor(*::GetCursor(watchCursor));

	// Call BE to sort the table
	
	CAddressBookManager::FailAddressError(AB_Command((ABPane *) GetMessagePane(), sortCmd, nil, 0));
}


/*======================================================================================
	Update the currently displayed and selected row based upon the typed down text.
======================================================================================*/

void CAddressBookTableView::UpdateToTypedownText(Str255 inTypedownText) {

	MSG_ViewIndex index = eInvalidCachedRowIDType;
	Int32 error = AB_GetIndexMatchingTypedown((ABPane *) GetMessagePane(), &index, P2CStr(inTypedownText), 0);
	ASSERT_ADDRESS_ERROR(error);
	
	if ( index != eInvalidCachedRowIDType ) {
		STableCell selectedCell = GetFirstSelectedCell();
		if ( IsValidCell(selectedCell) ) {
			if ( (GetSelectedRowCount() > 1) || (selectedCell.row != (index + 1)) ) {
				UnselectAllCells();
			}
		}
		STableCell cell(index + 1, 1);
		SelectCell( cell );
		ScrollCellIntoFrame( cell );
	}
}


/*======================================================================================
	Return whether or not we are currently performing an LDAP search.
======================================================================================*/

Boolean CAddressBookTableView::IsLDAPSearching() {

	return ((CAddressBookWindow *) USearchHelper::GetPaneWindow(this))->
		 GetAddressBookController()->IsLDAPSearching();
}


/*======================================================================================
	Notification from the BE that something is finished changing in the display table.
======================================================================================*/

void CAddressBookTableView::ChangeFinished(MSG_Pane *inPane, MSG_NOTIFY_CODE inChangeCode,
							  			   TableIndexT inStartRow, SInt32 inRowCount) {
	
	// If it's an initial insert call, and we're doing LDAP search, then simply pass
	//	this info on to the addressbook backend.  We'll get a MSG_NotifyAll when
	//	the backend thinks we should redraw. 
	
	if ( (mMysticPlane < (kMysticUpdateThreshHold + 2)) && 
		 (inChangeCode == MSG_NotifyInsertOrDelete) && 
		 IsLDAPSearching() ) {
		
		AB_LDAPSearchResults((ABPane *) GetMessagePane(), inStartRow - 1, inRowCount);
	}
	Inherited::ChangeFinished(inPane, inChangeCode, inStartRow, inRowCount);
 	//	Because the search is asynchronous, we
	//	don't want to cause a redraw every time a row is inserted, because that causes
	//	drawing of rows in random, unsorted order.
	
	if (inChangeCode == MSG_NotifyInsertOrDelete && IsLDAPSearching())
		DontRefresh();
	else
		Refresh();
	
}
				
void CAddressBookTableView::PaneChanged(MSG_Pane *inPane,
										MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
										int32 value)
{
	switch (inNotifyCode)
	{
		case MSG_PaneDirectoriesChanged:
			BroadcastMessage( MSG_PaneDirectoriesChanged, NULL );
			break;
		default:
			Inherited::PaneChanged(inPane, inNotifyCode, value);
			break;
	}
}

//-----------------------------------
void CAddressBookTableView::ShowMyAddressCard ()
//-----------------------------------
{
	// the StPersonEntry will take care of freeing up it's memory
	PersonEntry		person;
	person.Initialize();
	
	const char		*userEmail = FE_UsersMailAddress();
	const char		*userName = FE_UsersFullName();
	
	if (userEmail)
		person.pGivenName = XP_STRDUP(userName);
	if (userName)
		person.pEmailAddress = XP_STRDUP(userEmail);
	
	DIR_Server	*pab = NULL;
	ABID		entryID;
	ABook		*abook = CAddressBookManager::GetAddressBook();
	
	DIR_GetPersonalAddressBook(CAddressBookManager::GetDirServerList(), &pab);
	XP_ASSERT(pab);
	if (!pab) return;
	
	// this allocates pFamilyName
	AB_BreakApartFirstName (abook, &person);
	AB_GetEntryIDForPerson(pab, abook, &entryID, &person);
	
	if (entryID == MSG_MESSAGEKEYNONE)
		NewCard(&person);
	else 
		ShowEditWindowByEntryID(entryID, false);
	
	// pFamilyName was allocated in AB_BreakApartFirstName
	XP_FREEIF(person.pGivenName);
	XP_FREEIF(person.pFamilyName);
	XP_FREEIF(person.pEmailAddress);
}


#pragma mark -



/*======================================================================================
	Adjust the window to stored preferences.
======================================================================================*/

CAddressBookListTableView::~CAddressBookListTableView() {

	SetMessagePane( NULL );
}


/*======================================================================================
	Dispose of the address list pane.
======================================================================================*/
#if 0
void CAddressBookListTableView::DisposeAddressListPane(Boolean inRefresh) {

	//if ( inRefresh && (mRows > 0) ) RemoveRows(mRows, 1, true);

	if ( GetMessagePane() != nil ) {
		Int32 error = AB_CloseMailingListPane((MLPane **) &GetMessagePane());
		GetMessagePane() = nil;
		ASSERT_ADDRESS_ERROR(error);
	}
}
#endif 

/*======================================================================================
	Load and display the specified address book list.
======================================================================================*/

void CAddressBookListTableView::LoadAddressBookList(ABID &ioEntryListID) {

	Try_ {
		if ( GetMessagePane() == nil )
		  {
			MWContext *context = CAddressBookWindow::GetMailContext();
			MSG_Pane * pane;
			CAddressBookManager::FailAddressError( AB_CreateMailingListPane((MLPane **) &pane,
													context, CMailNewsContext::GetMailMaster() ) );
			SetMessagePane( pane );
			MSG_SetFEData((MSG_Pane *) GetMessagePane(), CMailCallbackManager::Get());	
			CMailCallbackListener::SetPane( (MSG_Pane*) GetMessagePane() );
			
			CAddressBookManager::FailAddressError(
					AB_InitializeMailingListPane((MLPane *) GetMessagePane(), &ioEntryListID, 
								sCurrentBook, CAddressBookManager::GetAddressBook() ) );	
		}
	}
	Catch_(inErr) {
		
		SetMessagePane( NULL );
		
		Throw_(inErr);
	}
	EndCatch_
}

/*======================================================================================
	Return whether a DropArea can accept the specified Drag. Override inherited method
	to return true if ANY item is acceptable, instead of ALL items.
======================================================================================*/

Boolean CAddressBookListTableView::DragIsAcceptable(DragReference inDragRef) {

	Uint16 itemCount;
	::CountDragItems(inDragRef, &itemCount);
	
	for (Uint16 item = 1; item <= itemCount; item++) {
		ItemReference itemRef;
		::GetDragItemReferenceNumber(inDragRef, item, &itemRef);
		
		if ( ItemIsAcceptable(inDragRef, itemRef) ) {
			return true;	// return on first successful item
		}
	}
	
	return false;
}


/*======================================================================================
	Return whether a Drag item is acceptable.
======================================================================================*/

Boolean CAddressBookListTableView::ItemIsAcceptable(DragReference inDragRef,
													ItemReference inItemRef) {
	
	SAddressDragInfo info;
	return CanInsertDragItem(inDragRef, inItemRef, &info);
}


/*======================================================================================
	Process an Item which has been dragged into a DropArea.
======================================================================================*/

void CAddressBookListTableView::ReceiveDragItem(DragReference inDragRef, DragAttributes inDragAttrs,
												ItemReference inItemRef, Rect &inItemBounds) {

#pragma unused (inDragAttrs)
#pragma unused (inItemBounds)
	SAddressDragInfo info;
	if (mDropRow == LArray::index_Last)
	{
		mDropRow = mRows;
	}
	Int32 insertRow = mDropRow;
	if( mIsDropBetweenRows )
		insertRow --;
	if( insertRow < 0 ) 
		insertRow = 0;
	if ( insertRow > GetBENumRows() )
		insertRow = GetBENumRows();
	if ( CanInsertDragItem(inDragRef, inItemRef, &info) ) {
		Try_ {

			CAddressBookManager::FailAddressError(
					AB_AddIDToMailingListAt( (MLPane *) GetMessagePane(), info.id, 
					insertRow ) );

			mDropRow++;
		}
		Catch_(inErr) {
			
		}
		EndCatch_
	}
}


/*======================================================================================
	Initialize the table.
======================================================================================*/

void CAddressBookListTableView::FinishCreateSelf()
{
	
	CAddressBookPane::FinishCreateSelf();
}


/*======================================================================================
	Delete all selected items.
======================================================================================*/

void CAddressBookListTableView::DeleteSelection() {

	AssertFail_(GetMessagePane() != nil);
	CMailSelection selection;
	
	if ( GetSelection( selection ) )
	{
		TableIndexT count = selection.selectionSize;
		TableIndexT *indices = (MSG_ViewIndex*)selection.GetSelectionList();
		int32 numRemoved = count;
		while (  numRemoved-- >=0 ) 
		{
			Try_ {
				CAddressBookManager::FailAddressError(AB_RemoveIDFromMailingListAt((MLPane *) GetMessagePane(), 
															  indices[numRemoved]));
			}
			Catch_(inErr) {
				
				Throw_(inErr);
			}
			EndCatch_
		}
	}
	
}

				
/*======================================================================================
	Open the selected message into its own window.
======================================================================================*/

void CAddressBookListTableView::OpenRow(TableIndexT inRow)
{
	const Boolean optionKeyDown = false;	
	
	ShowEditWindowByEntryID(GetEntryID(inRow), optionKeyDown);
}


/*======================================================================================
	Return true if the specified drag item can be inserted into the list.
======================================================================================*/

Boolean CAddressBookListTableView::CanInsertDragItem(DragReference inDragRef, ItemReference inItemRef,
										  			 SAddressDragInfo *outInfo) {

	FlavorFlags flavorFlags;
	
	// Check that correct flavor is present, check to see if the entry is not in the list yet
	if ( ::GetFlavorFlags(inDragRef, inItemRef, kMailAddresseeFlavor, &flavorFlags) == noErr ) {
		
		// Get the ABID of the item being dragged, either person or list
		Size dataSize = sizeof(SAddressDragInfo);
		
		if ( ::GetFlavorData(inDragRef, inItemRef, kMailAddresseeFlavor, outInfo, &dataSize, 0) == noErr ) {
			Assert_(dataSize == sizeof(SAddressDragInfo));
			
			// Check with BE to see if the entry is already in the list

			if ( (((CAddressBookChildWindow *) USearchHelper::GetPaneWindow(this))->GetEntryID() != outInfo->id) &&
				 (AB_GetIndexOfEntryID((AddressPane*)GetMessagePane(), outInfo->id) == MSG_VIEWINDEXNONE) ) {
				return true;
			 }
		}
	}	
	return false;
}



#pragma mark -
void CAddressBookChildWindow::FinishCreateSelf()
{
	LGAPushButton *control = dynamic_cast<LGAPushButton*> ( FindPaneByID('OkBt') );
	if( control )
	{
		control->AddListener( this );
		control->SetDefaultButton( true );
	}
	control = dynamic_cast<LGAPushButton*> ( FindPaneByID('CnBt') );
	if( control )
		control->AddListener( this );
}

/*======================================================================================
	Respond to broadcaster messages.
======================================================================================*/

void CAddressBookChildWindow::ListenToMessage(MessageT inMessage, void *ioParam) {

	switch ( inMessage ) {
		case	CSearchEditField::msg_UserChangedText:
			if ( ioParam   ) 
			{
				switch( (*( (PaneIDT *) ioParam) ) )
				{
					case CListPropertiesWindow::paneID_Name:
					case CNamePropertiesWindow::paneID_FirstName:
					case CNamePropertiesWindow::paneID_LastName:
						UpdateTitle();
						break;
					default:
						break;
				
				}
			}
			break;
		case msg_OK:
			Try_{
				UpdateBackendToUI( 
					CAddressBookManager::GetPersonalBook(), CAddressBookManager::GetAddressBook() );
				DoClose();	
			} Catch_ ( ioParam )
			{
				ResIDT stringID = 0;
				switch( ioParam )
				{
					case  XP_BKMKS_INVALID_NICKNAME:
						stringID = 4;
						break;	
					case  MK_MSG_NEED_FULLNAME:				
						stringID = 3;
						break;
					case  MK_ADDR_LIST_ALREADY_EXISTS:
						stringID = 2;
						break;
					case  MK_ADDR_ENTRY_ALREADY_EXISTS:
						stringID = 1;
						break;
					default:
					Throw_( ioParam );
				}
				LStr255 errorString( kAddressbookErrorStrings, stringID );
				UStdDialogs::Alert( errorString, eAlertTypeCaution );
			}
			break;
		case msg_Cancel:
			DoClose();
			break;

		default:
			break;
	}
}

Boolean  CAddressBookChildWindow::HandleKeyPress(
	const EventRecord	&inKeyEvent)
{
	Boolean		keyHandled = false;
	LControl	*keyButton = nil;
	
	switch (inKeyEvent.message & charCodeMask) {
	
		case char_Enter:
		case char_Return:
			keyButton = dynamic_cast<LControl*> ( FindPaneByID('OkBt') );
			break;
			
		case char_Escape:
			if ((inKeyEvent.message & keyCodeMask) == vkey_Escape) {
				keyButton =  dynamic_cast<LControl*>( FindPaneByID('CnBt') );
			}
			break;

		default:
			if (UKeyFilters::IsCmdPeriod(inKeyEvent)) {
				keyButton =  dynamic_cast<LControl*>( FindPaneByID('CnBt') );
			} else {
				keyHandled = LWindow::HandleKeyPress(inKeyEvent);
			}
			break;
	}
			
	if (keyButton != nil) {
		keyButton->SimulateHotSpotClick(kControlButtonPart);
		keyHandled = true;
	}
	
	return keyHandled;
}


#pragma mark -

/*======================================================================================
	Initialize the window.
======================================================================================*/

void CNamePropertiesWindow::FinishCreateSelf() {

	mFirstNameField = USearchHelper::FindViewEditField(this, paneID_FirstName);
	mLastNameField = USearchHelper::FindViewEditField(this, paneID_LastName);
	
	Inherited::FinishCreateSelf();	// Call CMediatedWindow for now since we need to
											// create a different context than CMailNewsWindow
	CAddressBookChildWindow::FinishCreateSelf();
	// Call inherited method
	FinishCreateWindow();

	// Lastly, only link to broadcasters in our views
	// need to listen to first/last name fields to update title
	USearchHelper::LinkListenerToBroadcasters(USearchHelper::FindViewSubview(this, paneID_GeneralView), this);
	
	// Need Cooltalk since we listen to the popup
	USearchHelper::LinkListenerToBroadcasters(USearchHelper::FindViewSubview(this, paneID_CooltalkView), this); 
}


/*======================================================================================
	Respond to broadcaster messages.
======================================================================================*/

void CNamePropertiesWindow::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch ( inMessage )
	{

		case paneID_ConferencePopup:
			SetConferenceText();
			break;

		default:
			Inherited::ListenToMessage(inMessage, ioParam);
			break;
	}
}


/*======================================================================================
	Update the name properties to the current values in the dialog fields.
======================================================================================*/

void CNamePropertiesWindow::UpdateBackendToUI(DIR_Server *inDir, ABook *inABook) {

		LView *view;
		StPersonEntry personEntry;
		PersonEntry *entry = personEntry.GetEntry();
		
		// General
		view = USearchHelper::FindViewSubview(this, paneID_GeneralView);
		
		P2CStr(mFirstNameField->GetDescriptor((StringPtr) entry->pGivenName));
		P2CStr(mLastNameField->GetDescriptor((StringPtr) entry->pFamilyName));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_EMail)->GetDescriptor((StringPtr) entry->pEmailAddress));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_Nickname)->GetDescriptor((StringPtr) entry->pNickName));
		((CSearchEditField *) USearchHelper::FindViewEditField(view, paneID_Notes))->GetText(entry->pInfo);
		entry->HTMLmail = !(USearchHelper::FindViewControl(view, paneID_PrefersHTML)->GetValue() == 0);
		P2CStr(USearchHelper::FindViewEditField(view, paneID_Company)->GetDescriptor((StringPtr) entry->pCompanyName));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_Title)->GetDescriptor((StringPtr) entry->pTitle));
		// Contact
		view = USearchHelper::FindViewSubview(this, paneID_ContactView);
		
	
		P2CStr(USearchHelper::FindViewEditField(view, paneID_Address)->GetDescriptor((StringPtr) entry->pAddress));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_City)->GetDescriptor((StringPtr) entry->pLocality));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_State)->GetDescriptor((StringPtr) entry->pRegion));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_ZIP)->GetDescriptor((StringPtr) entry->pZipCode));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_Country)->GetDescriptor((StringPtr) entry->pCountry));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_WorkPhone)->GetDescriptor((StringPtr) entry->pWorkPhone));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_FaxPhone)->GetDescriptor((StringPtr) entry->pFaxPhone));
		P2CStr(USearchHelper::FindViewEditField(view, paneID_HomePhone)->GetDescriptor((StringPtr) entry->pHomePhone));
		
		// Conference
		view = USearchHelper::FindViewSubview( this, paneID_CooltalkView );
		P2CStr(USearchHelper::FindViewEditField( view, paneID_ConferenceAddress)->GetDescriptor((StringPtr) entry->pCoolAddress) );
		entry->UseServer =short( (USearchHelper::FindSearchPopup( view, paneID_ConferenceServer )->GetValue())-1);// entries are 0 based
		// need to do CSID
		MWContext* context= CAddressBookWindow::GetMailContext();
		INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
		entry->WinCSID = INTL_GetCSIWinCSID(c);
	if ( mEntryID != CAddressBookPane::eNewEntryID ) // Existing entry
	{
		// Update BE
		CAddressBookManager::FailAddressError( AB_ModifyUser(inDir, inABook, mEntryID, entry) );
	} 
	else // new entry
	{
		ABID entryID;
		CAddressBookManager::FailAddressError( AB_AddUser( inDir, CAddressBookManager::GetAddressBook(), entry, &entryID) );
	}
}
// Update Caption text in Conference
void CNamePropertiesWindow::SetConferenceText( )
{
	short serverType = USearchHelper::FindSearchPopup( this, paneID_ConferenceServer )->GetValue();
	LStr255 exampleString(kConferenceExampleStr, serverType );
	LCaption* text = dynamic_cast<LCaption*>(this->FindPaneByID('CoES'));
	Assert_(text);
	if( text )	
		text->SetDescriptor( exampleString);
	LView *view = (LView*)USearchHelper::FindViewEditField( this, paneID_ConferenceAddress);
	if( view )
	{
		if( serverType == 1 )
		{
			view->SetDescriptor("\p");
			view->Disable();
		}
		else
			view->Enable();
	}

}

/*======================================================================================
	Update the dialog fields to the current values of the name properties.
======================================================================================*/

void CNamePropertiesWindow::UpdateUIToBackend(DIR_Server *inDir, ABook *inABook,
													 ABID inEntryID) {

	if ( mEntryID == inEntryID ) return;

	
	// Turn off listening
	
	StValueChanger<Boolean> change(mIsListening, false);
	
	LView *view = USearchHelper::FindViewSubview(this, paneID_GeneralView);

	char value[1024];

	// General
	
	value[0] = 0; AB_GetGivenName(inDir, inABook, inEntryID, value);
	mFirstNameField->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetFamilyName(inDir, inABook, inEntryID, value);
	mLastNameField->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetEmailAddress(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_EMail)->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetNickname(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_Nickname)->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetInfo(inDir, inABook, inEntryID, value);
	((CSearchEditField *) USearchHelper::FindViewEditField(view, paneID_Notes))->SetText(value);

	XP_Bool prefersHTML; AB_GetHTMLMail(inDir, inABook, inEntryID, &prefersHTML);
	USearchHelper::FindViewControl(view, paneID_PrefersHTML)->SetValue(prefersHTML ? 1 : 0);
	
	value[0] = 0; AB_GetCompanyName(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_Company)->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetTitle(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_Title)->SetDescriptor(C2PStr(value));

	// Contact
	view = USearchHelper::FindViewSubview(this, paneID_ContactView);
	
	
	value[0] = 0; AB_GetStreetAddress(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_Address)->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetLocality(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_City)->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetRegion(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_State)->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetZipCode(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_ZIP)->SetDescriptor(C2PStr(value));
	
	value[0] = 0; AB_GetCountry(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_Country)->SetDescriptor(C2PStr(value));
	
	value[0] = 0; AB_GetWorkPhone(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_WorkPhone)->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetFaxPhone(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_FaxPhone)->SetDescriptor(C2PStr(value));

	value[0] = 0; AB_GetHomePhone(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_HomePhone)->SetDescriptor(C2PStr(value));

	// Conference
	view = USearchHelper::FindViewSubview(this, paneID_CooltalkView);
	value[0] = 0; AB_GetCoolAddress(inDir, inABook, inEntryID, value);
	USearchHelper::FindViewEditField(view, paneID_ConferenceAddress)->SetDescriptor(C2PStr(value));
	
	short serverType;
	AB_GetUseServer( inDir,inABook,  inEntryID, &serverType);
	serverType++; // Popups and resources are 1 based
	USearchHelper::FindSearchPopup( view, paneID_ConferenceServer )->SetValue( serverType );
	
	SetConferenceText( );
	//

	mEntryID = inEntryID;
	UpdateTitle();
}

void CNamePropertiesWindow::UpdateUIToPerson( PersonEntry* person, ABID inEntryID )
{
	
	LView *view = USearchHelper::FindViewSubview(this, paneID_GeneralView);

	// General

	mFirstNameField->SetDescriptor( CStr255( person->pGivenName ));

	mLastNameField->SetDescriptor( CStr255(  person->pFamilyName ));

	USearchHelper::FindViewEditField(view, paneID_EMail)->SetDescriptor(CStr255( person->pEmailAddress  ));

	mEntryID = inEntryID;
#if 0 // Currently this is only called from new card which leaves the rest of these fields nil pointers
	USearchHelper::FindViewEditField(view, paneID_Nickname)->SetDescriptor(CStr255( person->pNickName ));
	if( person->pInfo )
		((CSearchEditField *) USearchHelper::FindViewEditField(view, paneID_Notes))->SetText( person->pInfo );

	USearchHelper::FindViewControl(view, paneID_PrefersHTML)->SetValue( person->HTMLmail ? 1 : 0);
	
	// Contact
	view = USearchHelper::FindViewSubview(this, paneID_ContactView);
	
	USearchHelper::FindViewEditField(view, paneID_Company)->SetDescriptor(CStr255( person->pCompanyName ));

	USearchHelper::FindViewEditField(view, paneID_Title)->SetDescriptor(CStr255( person->pTitle ));

	USearchHelper::FindViewEditField(view, paneID_Address)->SetDescriptor(CStr255( person->pAddress ));

	USearchHelper::FindViewEditField(view, paneID_City)->SetDescriptor(CStr255( person->pLocality ));

	USearchHelper::FindViewEditField(view, paneID_State)->SetDescriptor(CStr255( person->pRegion));

	USearchHelper::FindViewEditField(view, paneID_ZIP)->SetDescriptor(CStr255( person->pZipCode ));

	USearchHelper::FindViewEditField(view, paneID_WorkPhone)->SetDescriptor(CStr255( person->pWorkPhone));

	USearchHelper::FindViewEditField(view, paneID_FaxPhone)->SetDescriptor(CStr255( person->pFaxPhone ));

	USearchHelper::FindViewEditField(view, paneID_HomePhone)->SetDescriptor(CStr255( person->pHomePhone));
#endif
	// Conference
	view = USearchHelper::FindViewSubview(this, paneID_CooltalkView);
	USearchHelper::FindViewEditField(view, paneID_ConferenceAddress)->SetDescriptor(CStr255( person->pCoolAddress ));
	
	USearchHelper::FindSearchPopup( view, paneID_ConferenceServer )->SetValue( person->UseServer+1 );
	
	SetConferenceText( );
	
	UpdateTitle();

}

/*======================================================================================
	Update the window title.
======================================================================================*/

void CNamePropertiesWindow::UpdateTitle() {

	CStr255 title;
	{
		CStr255 first, last;
		mFirstNameField->GetDescriptor(first);
		mLastNameField->GetDescriptor(last);
		if ( last[0] ) first += " ";
		first += last;
	
		::GetIndString( title, 8903, 1);
		::StringParamText( title, first);
	}
	SetDescriptor( title );
}


#pragma mark -

/*======================================================================================
	Construct the list window.
======================================================================================*/

CListPropertiesWindow::CListPropertiesWindow(LStream *inStream) : 
							 		  	   	 CAddressBookChildWindow(inStream, WindowType_AddressList),
							 		  	   	 mAddressBookListTable(nil),
							 		  	   	 mTitleField(nil) {

	SetPaneID(pane_ID);
	
}


/*======================================================================================
	Dispose of the list window.
======================================================================================*/

CListPropertiesWindow::~CListPropertiesWindow() {
//	Boolean canRotate = false;
//	if ( IsVisible() ) {
//		USearchHelper::FindWindowTabGroup(&mSubCommanders)->SetRotateWatchValue(&canRotate);
//	}
//	delete mAddressBookListTable;
	mAddressBookListTable = nil;
}

/*======================================================================================
	Load and display the specified address book list.
======================================================================================*/

void CListPropertiesWindow::UpdateUIToBackend(DIR_Server *inDir, ABook *inABook, ABID inEntryID) {

	AssertFail_(mAddressBookListTable != nil);
	
	// Update any currently pending data
	
	// Turn off listening
	
	StValueChanger<Boolean> change(mIsListening, false);
	if( inEntryID !=  CAddressBookPane::eNewEntryID )
		mEntryID = inEntryID;	// Needs to be set here
	else 
		mEntryID = 0;
	mAddressBookListTable->LoadAddressBookList( mEntryID );
	
	if ( inEntryID != CAddressBookPane::eNewEntryID )
	{
		char value[1024];

		// Set edit fields
		value[0] = 0; AB_GetFullName(inDir, inABook, inEntryID, value);
		mTitleField->SetDescriptor(CStr255(value));

		value[0] = 0; AB_GetNickname(inDir, inABook, inEntryID, value);
		USearchHelper::FindViewEditField(this, paneID_Nickname)->SetDescriptor(CStr255(value));

		value[0] = 0; AB_GetInfo(inDir, inABook, inEntryID, value);
		USearchHelper::FindViewEditField(this, paneID_Description)->SetDescriptor(CStr255(value));

	}
	else
	{

		CStr255 defaultName;
		USearchHelper::AssignUString(uStr_ListDefaultName, defaultName);
		mTitleField->SetDescriptor(defaultName);
	}

	UpdateTitle();
}

/*======================================================================================
	Update the list properties to the current values in the dialog fields.
======================================================================================*/

void CListPropertiesWindow::UpdateBackendToUI(DIR_Server *inDir, ABook *inABook) {

#pragma unused (inDir)
#pragma unused (inABook)
	StMailingListEntry listEntry;
	MailingListEntry *entry = listEntry.GetEntry();
	P2CStr(mTitleField->GetDescriptor((StringPtr) entry->pFullName));
	P2CStr(USearchHelper::FindViewEditField(this, paneID_Nickname)->GetDescriptor((StringPtr) entry->pNickName));
	P2CStr(USearchHelper::FindViewEditField(this, paneID_Description)->GetDescriptor((StringPtr) entry->pInfo));
	// Need to copy in the win CSID
	MWContext* context= CAddressBookWindow::GetMailContext();
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	entry->WinCSID = INTL_GetCSIWinCSID(c);
	
	MSG_ViewIndex index = 0;
	CAddressBookManager::FailAddressError( AB_ModifyMailingListAndEntriesWithChecks( 
	 						(MLPane *)mAddressBookListTable->GetMessagePane(), entry, &index, 0 ) );
}


/*======================================================================================
	Initialize the window.
======================================================================================*/

void CListPropertiesWindow::FinishCreateSelf() {

	mAddressBookListTable = 
		dynamic_cast<CAddressBookListTableView *>(USearchHelper::FindViewSubview(this, paneID_AddressBookListTable));
	FailNILRes_(mAddressBookListTable);
	mTitleField = USearchHelper::FindViewEditField(this, paneID_Name);

	Inherited::FinishCreateSelf();	// Call CMediatedWindow for now since we need to
											// create a different context than CMailNewsWindow
	CAddressBookChildWindow::FinishCreateSelf(); // Link OK Cancel Button
	// Call inherited method
	FinishCreateWindow();
	
	// Lastly, link to our broadcasters, need to link to allow title update 
	USearchHelper::LinkListenerToBroadcasters(this, this);
	
}



/*======================================================================================
	Draw the window.
======================================================================================*/

void CListPropertiesWindow::DrawSelf() {
	Rect frame;
	
	if ( mAddressBookListTable->CalcPortFrameRect(frame) && ::RectInRgn(&frame, mUpdateRgnH) ) {
			
		{	
			StExcludeVisibleRgn excludeRgn(mAddressBookListTable);
			Inherited::DrawSelf();
		}

		StColorPenState::Normalize();
		::EraseRect(&frame);
	} else {
		Inherited::DrawSelf();
	}
	
	USearchHelper::RemoveSizeBoxFromVisRgn(this);
}


/*======================================================================================
	Update the window title.
======================================================================================*/

void CListPropertiesWindow::UpdateTitle() {

	CStr255 title;
	{
		CStr255 name;
		mTitleField->GetDescriptor(name);
		::GetIndString( title, 8903, 2);
		::StringParamText( title, name);
	}
	SetDescriptor( title );
}

//-----------------------------------
Boolean CListPropertiesWindow::ObeyCommand(CommandT inCommand, void *ioParam)
//-----------------------------------
{
	Boolean rtnVal = true;

	switch ( inCommand ) {
	
		case cmd_NewMailMessage:
			mAddressBookListTable->ComposeMessage();
			break;
		default:
			rtnVal = Inherited::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return rtnVal;
}

#pragma mark -

/*======================================================================================
	Allocate and initialize the person entry data record.
======================================================================================*/

StPersonEntry::StPersonEntry() :
							 mPerson(nil) {

	UInt32 allocSize = sizeof(PersonEntry) + (kMaxNameLength + kMaxNameLength + kMaxNameLength + kMaxNameLength +
											  kMaxCompanyLength + kMaxLocalityLength + kMaxRegionLength + kMaxEmailAddressLength +
											  kMaxInfo + kMaxTitle + kMaxAddress + kMaxZipCode + kMaxPhone + kMaxPhone + kMaxPhone +
											  1 + kMaxCoolAddress+ kMaxCountryLength);
	
	mPerson = (PersonEntry *) XP_CALLOC(1, allocSize);
	FailNIL_(mPerson);
	
	char *currentString = ((char *) mPerson) + sizeof(PersonEntry);
	
	mPerson->pNickName = currentString; currentString += kMaxNameLength;
	mPerson->pGivenName = currentString; currentString += kMaxNameLength;
	mPerson->pMiddleName = currentString; currentString += kMaxNameLength;
	mPerson->pFamilyName = currentString; currentString += kMaxNameLength;
	mPerson->pCompanyName = currentString; currentString += kMaxCompanyLength;
	mPerson->pLocality = currentString; currentString += kMaxLocalityLength;
	mPerson->pRegion = currentString; currentString += kMaxRegionLength;
	mPerson->pEmailAddress = currentString; currentString += kMaxEmailAddressLength;
	mPerson->pInfo = currentString; currentString += kMaxInfo;
	mPerson->pTitle = currentString; currentString += kMaxTitle;
	mPerson->pAddress = currentString; currentString += kMaxAddress;
	mPerson->pZipCode = currentString; currentString += kMaxZipCode;
	mPerson->pCountry = currentString; currentString+= kMaxCountryLength;
	mPerson->pWorkPhone = currentString; currentString += kMaxPhone;
	mPerson->pFaxPhone = currentString; currentString += kMaxPhone;
	mPerson->pHomePhone = currentString; currentString += kMaxPhone;
	mPerson->pDistName = currentString; currentString += 1;	//???
	mPerson->pCoolAddress = currentString; currentString += kMaxCoolAddress;
	
	AssertFail_(currentString == (((char *) mPerson) + allocSize));

	mPerson->Security = 0;
	mPerson->UseServer = 1;
	mPerson->HTMLmail = 1;
	mPerson->WinCSID = 0;
	
}


#pragma mark -

/*======================================================================================
	Allocate and initialize the person entry data record.
======================================================================================*/

StMailingListEntry::StMailingListEntry() :
							 		   mMailingList(nil) {

	UInt32 allocSize = sizeof(MailingListEntry) + (kMaxFullNameLength + kMaxNameLength + kMaxInfo + 1);
	
	mMailingList = (MailingListEntry *) XP_CALLOC(1, allocSize);
	FailNIL_(mMailingList);
	
	char *currentString = ((char *) mMailingList) + sizeof(MailingListEntry);
	
	mMailingList->pFullName = currentString; currentString += kMaxFullNameLength;
	mMailingList->pNickName = currentString; currentString += kMaxNameLength;
	mMailingList->pInfo = currentString; currentString += kMaxInfo;
	mMailingList->pDistName = currentString; currentString += 1;

	AssertFail_(currentString == (((char *) mMailingList) + allocSize));
	mMailingList->WinCSID = 0;

}

// CAddressBookController
void	CAddressBookController::FinishCreateSelf()
{
	mAddressBookTable = dynamic_cast<CAddressBookTableView *>( FindPaneByID( paneID_AddressBookTable ));
	mAddressBookTable->AddListener( this );
	FailNILRes_(mAddressBookTable);
	
	mDividedView = dynamic_cast<LDividedView *>(FindPaneByID( paneID_DividedView ));
	FailNILRes_(mDividedView);
	
	LWindow* window = dynamic_cast<LWindow*>( LWindow::FetchWindowObject(GetMacPort()) );
	mProgressCaption = dynamic_cast<CPatternProgressCaption*>(window->FindPaneByID(kMailNewsStatusPaneID));
	FailNILRes_(mProgressCaption);
	
	mSearchButton = dynamic_cast<LGAPushButton *>(FindPaneByID( paneID_Search));
	FailNILRes_(mSearchButton);
	
	mStopButton = dynamic_cast<LGAPushButton *>(FindPaneByID( paneID_Stop));
	FailNILRes_(mStopButton);
	
	mTypedownName = dynamic_cast<CSearchEditField *>(FindPaneByID( paneID_TypedownName) );
	FailNILRes_(mTypedownName);
	mTypedownName->AddListener(this);
	SwitchTarget( mTypedownName );
	UReanimator::LinkListenerToControls(this, this, 8920);
	PopulateDirectoryServers();
	
	// Frame Highlighting
	CTargetFramer* framer = new CTargetFramer();
	mAddressBookTable->AddAttachment(framer);
	framer = new CTargetFramer();
	mTypedownName->AddAttachment(framer);
}

void	CAddressBookController::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch ( inMessage )
	{
		case CSearchEditField::msg_UserChangedText:
			// User changed the typedeown text
			if ( mNextTypedownCheckTime == eDontCheckTypedown ) {
				mNextTypedownCheckTime = LMGetTicks() + eCheckTypedownInterval;
			}
			break;
		case CAddressBookTableView::cmd_NewAddressCard:
		case CAddressBookTableView::cmd_NewAddressList:
		case CAddressBookTableView::cmd_EditProperties:
		case CAddressBookTableView::cmd_DeleteEntry:
		case CAddressBookTableView::cmd_ComposeMailMessage:	
		case cmd_SearchAddresses:
			mAddressBookTable->ButtonMessageToCommand(inMessage);	
			break;
			
		case paneID_DirServers:
			SelectDirectoryServer(nil, GetServerIndexFromPopup());
			break;
			
		case CAddressBookTableView::cmd_ConferenceCall:
			mAddressBookTable->ConferenceCall();
			break;
		
		// Status messages	
		case msg_NSCAllConnectionsComplete:
			MessageWindStop(false);				// line order..
			mProgressCaption->SetSeamless();	// ...does matter
			Refresh(); // ?
			break;
			
		case msg_NSCProgressEnd:
			break;

		case msg_NSCProgressMessageChanged:
			if ( ioParam != nil ) {
				mProgressCaption->SetDescriptor(TString<Str255>((const char*)ioParam));
			} else {
				mProgressCaption->SetDescriptor("\p");
			}
			break;
			
		case msg_NSCProgressPercentChanged:
			mProgressCaption->SetValue(*(Int32*)ioParam);
			break;
		
		case paneID_Search:
			MessageWindSearch();
			break;
			
		case paneID_Stop:
			MessageWindStop(true);
			break;	
		
		case MSG_PaneDirectoriesChanged:
			UpdateToDirectoryServers();
			break;
			
		default:
			// No superclass method
			break;
	}
	
}

Boolean	CAddressBookController::ObeyCommand(CommandT inCommand, void *ioParam)
{
	Boolean cmdHandled = true;
	switch ( inCommand )
	{
		case paneID_Search:
			MessageWindSearch();
			break;
			
		case paneID_Stop:
			MessageWindStop(true);
			break;
		default:
			cmdHandled = LCommander::ObeyCommand(inCommand, ioParam);
			break;
	}
	return cmdHandled;
}

void	CAddressBookController::ReadStatus(LStream *inStatusData)
{
	mDividedView->RestorePlace(inStatusData);
	mAddressBookTable->GetTableHeader()->ReadColumnState(inStatusData);
}

void	CAddressBookController::WriteStatus(LStream *outStatusData)
{
	
	mDividedView->SavePlace(outStatusData);
	mAddressBookTable->GetTableHeader()->WriteColumnState(outStatusData);
}


//-----------------------------------
void CAddressBookController::SpendTime(const EventRecord &inMacEvent)
//	Check to see if the user has typed in text that needs to be searched.
//-----------------------------------
{
	if ( CAddressBookTableView::CurrentBookIsPersonalBook() ) {
		if ( inMacEvent.when >= mNextTypedownCheckTime ) {
			Assert_(mTypedownName);
			Assert_(mAddressBookTable);
		
			Str255 typedownText;
			mTypedownName->GetDescriptor(typedownText);
			
			mAddressBookTable->UpdateToTypedownText(typedownText);
			mNextTypedownCheckTime = eDontCheckTypedown;
		}
	} else if ( !IsLDAPSearching() ) {
		USearchHelper::EnableDisablePane(mSearchButton, !IsLDAPSearching()/*mTypedownName->GetTextLength() > 0*/, true);
	}
}



//-----------------------------------
void CAddressBookController::PopulateDirectoryServers()
//	Populate the directory server popup menu with the current servers.
//-----------------------------------
{

	CSearchPopupMenu *popup = USearchHelper::FindSearchPopup(this, paneID_DirServers);
	popup->ClearMenu();	// Remove any current items
	
	XP_List *serverList = CAddressBookManager::GetDirServerList();
	if ( !serverList ) return;
	for (Int32 i = 1; i <= XP_ListCount(serverList); i++) {
		DIR_Server *server = (DIR_Server *) XP_ListGetObjectNum(serverList, i);
		if ( (server->dirType == LDAPDirectory) || (server->dirType == PABDirectory) ) {
			CStr255 string(server->description);
			if ( string.Length() == 0 ) string += "????";	// Bug in string generation!
			popup->AppendMenuItemCommand(i, string, true);
		}
	}
	popup->SetValue(1);
} // CAddressBookWindow::PopulateDirectoryServers

//-----------------------------------
void CAddressBookController::SelectDirectoryServer(DIR_Server *inServer, Int32 inServerIndex)
//	Select the specified directory server into the address book table view. If inServer
//	is not nil, select that server,; otherwise use the 1-based index given by
//	inServerIndex to select the directory server at that index in the list returned
//	by CAddressBookManager::GetDirServerList(). If the specified server cannot be
//	found or is not a valid server that can be displayed, do nothing.
//-----------------------------------
{
	mProgressCaption->SetDescriptor("\p");
	AssertFail_(mAddressBookTable != nil);
	SetCursor(*GetCursor(watchCursor));
	XP_List *serverList = CAddressBookManager::GetDirServerList();
	if ( !serverList ) return;
	
	if ( inServer == nil )
		inServer = (DIR_Server *) XP_ListGetObjectNum(serverList, inServerIndex);
	else
	{
		DIR_Server *foundServer = nil;
		for (Int32 i = 1; i <= (XP_ListCount(serverList) + 1); i++)
		{
			foundServer = (DIR_Server *) XP_ListGetObjectNum(serverList, i);
			if ( foundServer == inServer ) break;
		}
		inServer = foundServer;
	}
	// Validate server
	if ( (inServer == nil) || 
		 !((inServer->dirType == LDAPDirectory) || (inServer->dirType == PABDirectory)) )
		 return;	// Can't find it
	// Load server into address book table

	if ( !mAddressBookTable->LoadAddressBook(inServer) ) return;
	
	const Boolean isLDAPServer = (inServer->dirType == LDAPDirectory);
	USearchHelper::ShowHidePane(mSearchButton, isLDAPServer);
	USearchHelper::EnableDisablePane(mSearchButton, mTypedownName->GetTextLength() > 0, true);
	USearchHelper::ShowHidePane(mStopButton, false);
	USearchHelper::SelectEditField(mTypedownName);
	mAddressBookTable->SetColumnHeaders(inServer);

	UpdatePort();
} // CAddressBookWindow::SelectDirectoryServer



//-----------------------------------
void CAddressBookController::MessageWindSearch( char* text)
//	React to search message.
//-----------------------------------
{

	if ( CAddressBookTableView::CurrentBookIsPersonalBook() ||
		 IsLDAPSearching() ) return;


	

	MessageT message = paneID_Search;
	if( text == NULL )
	{
		StDialogHandler handler(8980, nil);
		// Select the "URL" edit field
		CLDAPQueryDialog* dialog = dynamic_cast< CLDAPQueryDialog*>( handler.GetDialog() );
		Assert_( dialog );

		dialog->Setup(  mAddressBookTable->GetMessagePane(), mAddressBookTable->GetCurrentBook() );
		
		
		Boolean doSearch = false;
		// Run the dialog
		
				
		do {
			message = handler.DoDialog();
		} while (message != paneID_Search && message != msg_Cancel);
	}
	if ( message == paneID_Search )
	{
		CAddressBookManager::FailAddressError(AB_SearchDirectory((ABPane *) mAddressBookTable->GetMessagePane(), 
						 										 text ) );

		//USearchHelper::FindViewSubpane(this, paneID_SearchEnclosure)->Disable();
		USearchHelper::ShowHidePane(mSearchButton, false);
		USearchHelper::ShowHidePane(mStopButton, true);
		// UpdatePort();

	}
}

/*======================================================================================
	Update the window to the current directory servers.
======================================================================================*/

void CAddressBookController::UpdateToDirectoryServers() {

	CAddressBookWindow *addressWindow = dynamic_cast<CAddressBookWindow *>(
				CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Address));
	
	if ( addressWindow == nil ) return;
	
	PopulateDirectoryServers();
}

//-----------------------------------
Int32 CAddressBookController::GetServerIndexFromPopup()
//-----------------------------------
{
	CSearchPopupMenu *popup = USearchHelper::FindSearchPopup(this, paneID_DirServers);
	if ( !popup ) return -1;
	return popup->GetCurrentItemCommandNum();
}

//-----------------------------------
void CAddressBookController::SetPopupFromServerIndex(Int32 inServerIndex)
//-----------------------------------
{
	CSearchPopupMenu *popup = USearchHelper::FindSearchPopup(this, paneID_DirServers);
	if ( !popup ) return;
	popup->SetCurrentItemByCommand(inServerIndex);
}

//-----------------------------------
void CAddressBookController::MessageWindStop(Boolean inUserAborted)
//	React to stop message.
//-----------------------------------
{

#pragma unused (inUserAborted)
	if ( CAddressBookTableView::CurrentBookIsPersonalBook() ||
		 !IsLDAPSearching() ) return;

	AB_FinishSearch((ABPane *) mAddressBookTable->GetMessagePane(), CAddressBookWindow::GetMailContext());

	//USearchHelper::FindViewSubpane(this, paneID_SearchEnclosure)->Enable();
	USearchHelper::ShowHidePane(mStopButton, false);
	USearchHelper::ShowHidePane(mSearchButton, true);
	USearchHelper::SelectEditField(mTypedownName);
	UpdatePort();
}

//-----------------------------------
Boolean CAddressBookController::HandleKeyPress(const EventRecord &inKeyEvent)
//-----------------------------------
{
	Int16 theKey = inKeyEvent.message & charCodeMask;
	
	if (((theKey == char_Enter) || (theKey == char_Return)) && !IsLDAPSearching())
	{		
		CStr255 typedownText;
		mTypedownName->GetDescriptor(typedownText);
		if (typedownText.Length() > 0 )
			MessageWindSearch( typedownText );
	}
	else if ( (((theKey == char_Escape) && ((inKeyEvent.message & keyCodeMask) == vkey_Escape)) ||
				 UKeyFilters::IsCmdPeriod(inKeyEvent)) && IsLDAPSearching() )
	{
		mStopButton->SimulateHotSpotClick(kControlButtonPart);
		return true;
	}
	return false;
}


#if 1
void CAddressBookManager::DoPickerDialog(  CComposeAddressTableView* initialTable )
{
	StDialogHandler handler ( 8990, nil  );
	CAddressPickerWindow* dialog = dynamic_cast< CAddressPickerWindow* >( handler.GetDialog() );
	dialog->Setup( initialTable );
	dialog->Show();
	MessageT message;
	do {
		message = handler.DoDialog();
	} while ( message != CAddressPickerWindow::eOkayButton &&
			  message != CAddressPickerWindow::eCancelButton ) ;
}

// Mail News address picker
#pragma mark -

void CAddressPickerWindow::FinishCreateSelf()
{
	mPickerAddresses = dynamic_cast< CComposeAddressTableView* >( FindPaneByID( 'Addr' ) );
	FailNILRes_( mPickerAddresses );
	
	mAddressBookTable = dynamic_cast<CAddressBookTableView *>( FindPaneByID( paneID_AddressBookTable ));
	FailNILRes_(mAddressBookTable);
	mAddressBookTable->AddListener( this );
	UReanimator::LinkListenerToControls(this, this,CAddressPickerWindow:: res_ID);
	// Adjust button state
	ListenToMessage (  CStandardFlexTable::msg_SelectionChanged, nil );
	
	CAddressBookWindow::FinishCreateSelf();

}

void	CAddressPickerWindow::Setup( CComposeAddressTableView* initialTable )
{
	// Copy the old table to the new one
	Assert_( initialTable );
	Assert_( mPickerAddresses );
	
	TableIndexT numRows;
	mInitialTable = initialTable;
	initialTable->GetNumRows(numRows);
	STableCell cell;
	Uint32 size;
	char* address = NULL;
	for ( int32 i = 1; i <= numRows; i++ )
	{
		EAddressType type = initialTable->GetRowAddressType( i );
		cell.row = i;
		cell.col = 2;
		size = sizeof(address);
		initialTable->GetCellData(cell, &address, size);
		mPickerAddresses->InsertNewRow( type, address, false );
	}
}

void 	CAddressPickerWindow::ListenToMessage(MessageT inMessage, void *ioParam)
{
#pragma unused (ioParam)
	switch( inMessage )
	{
		case eOkayButton:
			CComposeAddressTableStorage* oldTableStorage  =dynamic_cast< CComposeAddressTableStorage*> (mInitialTable->GetTableStorage() );
			mPickerAddresses->EndEditCell();
			mInitialTable->SetTableStorage( mPickerAddresses->GetTableStorage() );
			mPickerAddresses->SetTableStorage( oldTableStorage );
			break;
	
		case eHelp:
			ShowHelp( HELP_SELECT_ADDRESSES );
			break;
			
		case eToButton:
			AddSelection ( eToType );
			break;
			
		case eCcButton:
			AddSelection ( eCcType );
			break;
			
		case eBccButton:
			AddSelection( eBccType );
			break;
			
		case ePropertiesButton:
			break;
		case CStandardFlexTable::msg_SelectionChanged:
			Boolean enable = mAddressBookTable->GetSelectedRowCount() >0;
			
			USearchHelper::EnableDisablePane( USearchHelper::FindViewControl( this ,eToButton ), enable, true );
			USearchHelper::EnableDisablePane( USearchHelper::FindViewControl( this ,eCcButton ), enable, true );
			USearchHelper::EnableDisablePane( USearchHelper::FindViewControl( this ,eBccButton ), enable, true );
			USearchHelper::EnableDisablePane( USearchHelper::FindViewControl( this ,ePropertiesButton ), enable, true );
			break;
		
		default:
			break;
	}


}

void CAddressPickerWindow::AddSelection( EAddressType inAddressType )
{
	
	char* address = NULL;
	//Uint32 size= sizeof(address);
	TableIndexT row = 0;

	while ( mAddressBookTable->GetNextSelectedRow( row )  )
	{
		mAddressBookTable->GetFullAddress( row, &address );		
		mPickerAddresses->InsertNewRow( inAddressType, address, false );
	}
}

#endif
#endif // MOZ_NEWADDR
