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

// CAddressBookWindows.cp

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/
#define DEBUGGER_ASSERTIONS

 

#include "CAddressBookWindows.h"
#ifdef MOZ_NEWADDR

#include "SearchHelpers.h"
#include "CSizeBox.h"
#include "MailNewsgroupWindow_Defines.h"
#include "Netscape_Constants.h"
#include "resgui.h"
#include "CMailNewsWindow.h"
#include "UGraphicGizmos.h"
#include "uprefd.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "CGATabBox.h"
#include "URobustCreateWindow.h"
#include "UStdDialogs.h"
#include "macutil.h"
#include "UStClasses.h"
#include <LGAPushButton.h>
#include <LGAIconButton.h>
#include <LDragAndDrop.h>
#include "CTargetFramer.h"
// get string constants
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS

#include "dirprefs.h"
#include "prefapi.h"
#include "UProcessUtils.h"
#include "CAppleEventHandler.h"
#include "secnav.h"
#include "CTableKeyAttachment.h"
#include "intl_csi.h"
#include "xp_help.h"
#include "CAddressBookViews.h"
#include "CLDAPQueryDialog.h"
// #include "CAddressPickerWindow.h"
#include "MailNewsMediators.h"
#include "CABContainerDialogs.h"
class CNamePropertiesWindow;
class CListPropertiesWindow;

const ResIDT kConferenceExampleStr 		= 8901;
const ResIDT kAddressbookErrorStrings	= 8902;

// Junk to allow linking. When libaddr is update these should be removed
#include "abdefn.h"
#include "addrbook.h"
typedef struct ABook ABook;
ABook *FE_GetAddressBook(MSG_Pane * /*pane*/) {

	return NULL;
}


int FE_ShowPropertySheetFor (MWContext* /* context */, ABID /* entryID */, PersonEntry* /* pPerson */)
{
	
	return 0;
}
 // End of obsolete

#pragma mark -

const Int32 kNumPersonAttributes = 22;
const Int32	kMaxPersonSize		= 4096; //Really should recalc this


#pragma mark -
//------------------------------------------------------------------------------
//	¥	XP Callbacks
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//	¥	FE_GetDirServers
//------------------------------------------------------------------------------
//	
XP_List *FE_GetDirServers() {
	return CAddressBookManager::GetDirServerList();
}


//------------------------------------------------------------------------------
//	¥	FE_GetAddressBookContext
//------------------------------------------------------------------------------
//	Returns the address book context, creating it if necessary.  A mail pane is passed in, 
//	on the unlikely chance that it might be useful for the FE.  If "viewnow" is TRUE, 
//	then present the address book window to the user; otherwise, don't (unless it is 
//	already visible).
//
MWContext *FE_GetAddressBookContext(MSG_Pane * /*pane*/, XP_Bool viewnow) {

	if ( viewnow ) {
		CAddressBookManager::ShowAddressBookWindow();
	}
	
	return CAddressBookWindow::GetMailContext();
}

//------------------------------------------------------------------------------
//	¥	FE_ShowPropertySheeetForAB2
//------------------------------------------------------------------------------
//	The XP layer is requesting that the FE show a property window
//	return AB_SUCCESS if the window was brought up
// 	return AB_Failure if the window couldn't be brought up
//
int MacFe_ShowModelessPropertySheetForAB2( MSG_Pane * pane,  MWContext* /* inContext */)
{
	// Get the Resource Id for the property window 
	ResID	resId = 0;
	
	MSG_PaneType type = MSG_GetPaneType( pane );
	switch( type )
	{
		case	AB_MAILINGLISTPANE:
			resId = CListPropertiesWindow::res_ID;
			break;
		
		case	AB_PERSONENTRYPANE:
			resId = CNamePropertiesWindow::res_ID;
			break;
		
		default:
			assert( 0 );
			return AB_FAILURE;
	}

	// Create the new property windows.
	try
	{

		CAddressBookWindow *addressWindow = dynamic_cast<CAddressBookWindow *>(
				CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Address));
		AssertFail_(addressWindow != nil);	// Must be around to create name properties window
		
		LWindow* window = URobustCreateWindow::CreateWindow( resId,  addressWindow->GetSuperCommander() );
		CAddressBookChildWindow* propertiesWindow = dynamic_cast<CAddressBookChildWindow *>( window );

		propertiesWindow->UpdateUIToBackend( pane );
		propertiesWindow->Show();
		
		return AB_SUCCESS;
	} catch( ... )
	{
	
	}
	return AB_FAILURE;
}

int MacFE_ShowPropertySheetForDir( 
			DIR_Server* server, MWContext* /* context */, MSG_Pane * srcPane, XP_Bool  /* newDirectory */)
{
	Boolean isPAB = PABDirectory == server->dirType;
	if( isPAB )
	{
		CPABPropertyDialogManager dialogManager(  server, srcPane );
	}
	else
	{
		CLDAPPropertyDialogManager dialogManager(  server, srcPane );
	}
	return AB_SUCCESS;
}


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

XP_List *CAddressBookManager::sDirServerList = nil;
Boolean CAddressBookManager::sDirServerListChanged = false;


//------------------------------------------------------------------------------
//	¥	OpenAddressBookManager
//------------------------------------------------------------------------------
//	Open the address book at application startup. This method sets the initial address
//	book to the local personal address book for the user (creates one if it doesn't
//	exist already).
//	I think this whole function is obsolete
//	as we are no longer using AB_InitializeAddressBook
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
		
		// Get the Akbar (v3.0) address book.  It's in HTML format (addressbook.html)
		spec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
		::GetIndString(spec.name, 300, htmlAddressBook);	// Get address book name from prefs
		oldName = CFileMgr::GetURLFromFileSpec(spec);
		FailNIL_(oldName);
		#if 0
		CAddressBookManager::FailAddressError(
			AB_InitializeAddressBook(GetPersonalBook(), &sAddressBook,
				oldName + XP_STRLEN("file://"))
			);
		#endif // OBsolete?
		XP_FREE(oldName);
	}
	Catch_(inErr) {
		CAddressBookManager::CloseAddressBookManager();
		XP_FREEIF(oldName);
	}
	EndCatch_
	
	//PREF_RegisterCallback("ldap_1", DirServerListChanged, NULL);
}



//------------------------------------------------------------------------------
//	¥	CloseAddressBookManager
//------------------------------------------------------------------------------
//	Closes any open resources used by the address book manager.
//
void CAddressBookManager::CloseAddressBookManager()
{
	DIR_ShutDown();
}

//------------------------------------------------------------------------------
//	¥	ImportLDIF
//------------------------------------------------------------------------------
//	We come here in response to an odoc event with an LDIF file.
//	Currently no error result.  Assuming the back end will put up an alert.
//
void CAddressBookManager::ImportLDIF(const FSSpec& inFileSpec)
{
	char* path = CFileMgr::EncodedPathNameFromFSSpec(
					inFileSpec, true /*wantLeafName*/);
	if (path)
	{
		int result = AB_ImportData(
			nil, // no container, create new
			path,
			strlen(path),
			AB_Filename	// FIX ME: need bit that tells the BE to delete the file.
			);
		if (result != AB_SUCCESS)
			SysBeep(1);
		XP_FREE(path);
	}
}


//------------------------------------------------------------------------------
//	¥	ShowAddressBookWindow
//------------------------------------------------------------------------------
//	Show the search dialog by bringing it to the front if it is not already. Create it
//	if needed.
//
CAddressBookWindow * CAddressBookManager::ShowAddressBookWindow()
{

	// Find out if the window is already around
	CAddressBookWindow *addressWindow = dynamic_cast<CAddressBookWindow *>(
			CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Address));

	if ( addressWindow == nil )
	{
		// Search dialog has not yet been created, so create it here and display it.
		addressWindow = dynamic_cast<CAddressBookWindow *>(
				URobustCreateWindow::CreateWindow(CAddressBookWindow::res_ID, 
												  LCommander::GetTopCommander()));
		AssertFail_(addressWindow != nil);
	}
	
	addressWindow->Show();
	addressWindow->Select();
	return addressWindow;
}


//------------------------------------------------------------------------------
//	¥	GetDirServerList
//------------------------------------------------------------------------------
//	Return the list of directory servers for the application. Why are these
//	method and variable here? Can also call the BE method FE_GetDirServers() to return
//	the same result.
//
XP_List *CAddressBookManager::GetDirServerList()
{
	return DIR_GetDirServers();
}


//------------------------------------------------------------------------------
//	¥	SetDirServerList
//------------------------------------------------------------------------------
//	This method should be called to set the current directory servers list. The old list
//	is destroyed and is replaced with inList; the caller does NOT dispose of inList, since
//	it is managed by this class after calling this method. This method also calls the
//	BE method DIR_SaveServerPreferences() if inSavePrefs is true.
//
void CAddressBookManager::SetDirServerList(XP_List *inList, Boolean inSavePrefs)
{

	AssertFail_(inList != nil);

	XP_List *tempDirServerList = sDirServerList;
	sDirServerList = inList;	// This needs to be set correctly for the callback to work.
	if ( inSavePrefs ) {
		Int32 error = DIR_SaveServerPreferences(inList);	// No listed return error codes, 
															// who knows what they are!
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


//------------------------------------------------------------------------------
//	¥	GetPersonalBook
//------------------------------------------------------------------------------
//	Return the local personal address book.
//
DIR_Server *CAddressBookManager::GetPersonalBook()
{
	AssertFail_((sDirServerList != nil));
	DIR_Server *personalBook = nil;
	DIR_GetPersonalAddressBook(sDirServerList, &personalBook);
	AssertFail_(personalBook != nil);
	return personalBook;
}



//------------------------------------------------------------------------------
//	¥	FailAddressError
//------------------------------------------------------------------------------
//	
void CAddressBookManager::FailAddressError(Int32 inError)
{
	if ( inError == AB_SUCCESS )
		return;
	switch (	inError )
	{
		case AB_FAILURE:
			// Should be throwing
			break;
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


//------------------------------------------------------------------------------
//	¥	RegisterAddressBookClasses
//------------------------------------------------------------------------------
//	Register all classes associated with the address book window.
//
void CAddressBookManager::RegisterAddressBookClasses() {

	RegisterClass_(CAddressBookWindow);
	RegisterClass_(CAddressBookTableView);
	RegisterClass_(CMailingListTableView);
	RegisterClass_(CSearchPopupMenu);

	RegisterClass_(CNamePropertiesWindow);
	RegisterClass_(CListPropertiesWindow);

	RegisterClass_(CAddressBookController);
	RegisterClass_(CLDAPQueryDialog);
	RegisterClass_(CAddressBookContainerView);
	CGATabBox::RegisterTabBoxClasses();
}


#pragma mark -

//------------------------------------------------------------------------------
//	¥	~CAddressBookWindow
//------------------------------------------------------------------------------
//	
//
CAddressBookWindow::~CAddressBookWindow()
{

}


//------------------------------------------------------------------------------
//	¥	GetMailContext
//------------------------------------------------------------------------------
//	Get the mail context for the address book. We must have an address book window to 
//	access the context, so this method creates the window (but doesn't show it) if the
//	window is not yet around.
//
MWContext *CAddressBookWindow::GetMailContext()
{
	CAddressBookWindow *addressWindow = dynamic_cast<CAddressBookWindow *>(
				CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Address));
	
	if ( addressWindow == nil ) {
		// AddressBookWindow dialog has not yet been created, so create it here
		addressWindow = dynamic_cast<CAddressBookWindow *>(
					URobustCreateWindow::CreateWindow(CAddressBookWindow::res_ID, 
													  LCommander::GetTopCommander()));
		AssertFail_(addressWindow != nil);
	}

	return *(addressWindow->GetWindowContext());
}


//------------------------------------------------------------------------------
//	¥	FinishCreateSelf
//------------------------------------------------------------------------------
//	
void CAddressBookWindow::FinishCreateSelf()
{
	mAddressBookController = 
		dynamic_cast< CAddressBookController* >(FindPaneByID( paneID_AddressBookController) ) ;
	FailNILRes_(mAddressBookController);
	
	// Create the context
	Inherited::FinishCreateSelf();	
	mMailNewsContext->SetWinCSID(INTL_DocToWinCharSetID(mMailNewsContext->GetDefaultCSID()));
	
	// Have the AddressBook Controller listen to the toolbar and context
	UReanimator::LinkListenerToControls(mAddressBookController, this, res_ID);
	mMailNewsContext->AddListener(mAddressBookController);
	mAddressBookController->SetContext( *mMailNewsContext );
	mAddressBookController->GetAddressBookContainerView()->Setup( *mMailNewsContext );
	
	// Stop is in the toolbar. Could do this in constructor but then would have to duplicate the container
	LGAPushButton* stopButton = dynamic_cast<LGAPushButton *>(FindPaneByID( paneID_Stop));
	if ( stopButton )
			USearchHelper::ShowHidePane(stopButton, false);

	// Update the window title 
	CStr255 windowTitle;
	GetUserWindowTitle(4, windowTitle);
	SetDescriptor(windowTitle);	
}


//------------------------------------------------------------------------------
//	¥	CreateContext
//------------------------------------------------------------------------------
//	
CNSContext* CAddressBookWindow::CreateContext() const
{
	CNSContext* result = new CNSContext( MWContextAddressBook);
	FailNIL_(result);
	return result;
} 


//------------------------------------------------------------------------------
//	¥	GetActiveTable
//------------------------------------------------------------------------------
// The active table is the one with the current keyboard focus. If the keyboard focus is in 
// the editfield, the AddressBookTable has focus
//
CMailFlexTable*	CAddressBookWindow::GetActiveTable()
{
	CMailFlexTable* result = nil;
	Assert_( mAddressBookController );
	result = dynamic_cast<CMailFlexTable* >( mAddressBookController->GetAddressBookContainerView() );
	if (! (result && result->IsOnDuty() ) )
		 result = dynamic_cast<CMailFlexTable* >( mAddressBookController->GetAddressBookTable() ); 
	return result;
}


//------------------------------------------------------------------------------
//	¥	ReadWindowStatus
//------------------------------------------------------------------------------
//	Adjust the window to stored preferences.
//
void CAddressBookWindow::ReadWindowStatus(LStream *inStatusData)
{
	Inherited::ReadWindowStatus(inStatusData);
	if ( inStatusData != nil )
	{
		mAddressBookController->ReadStatus( inStatusData );
	}
	
}


//------------------------------------------------------------------------------
//	¥	WriteWindowStatus
//------------------------------------------------------------------------------
//	Write window stored preferences.
//
void CAddressBookWindow::WriteWindowStatus(LStream *outStatusData)
{
	Inherited::WriteWindowStatus(outStatusData);
	mAddressBookController->WriteStatus(outStatusData);
}


#pragma mark -
//------------------------------------------------------------------------------
//	¥	ListenToMessage
//------------------------------------------------------------------------------
//
void CAddressBookChildWindow::ListenToMessage(MessageT inMessage, void *ioParam)
{
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
				UpdateBackendToUI(  );
				
				AB_ClosePane( mMSGPane );
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
			AB_ClosePane( mMSGPane );
			DoClose();
			break;

		default:
			Inherited::ListenToMessage( inMessage, ioParam );
			break;
	}
}


#pragma mark -
//------------------------------------------------------------------------------
//	¥	PaneChanged
//------------------------------------------------------------------------------
//	
void CMailWindowCallbackListener::PaneChanged( MSG_Pane* /* inPane */, MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode, int32 /* value */)
{
	switch (inNotifyCode)
	{
		case MSG_PaneClose:
			mWindow->DoClose();
			break;
		default:	
			break;
	}
}

#pragma mark -
//------------------------------------------------------------------------------
//	¥	CNamePropertiesWindow
//------------------------------------------------------------------------------
//	
//
CNamePropertiesWindow::CNamePropertiesWindow(LStream *inStream) : 
						 CAddressBookChildWindow(inStream),
						mFirstNameField(nil),	mLastNameField(nil)
{
}


//------------------------------------------------------------------------------
//	¥	FinishCreateSelf
//------------------------------------------------------------------------------
//	
//
void CNamePropertiesWindow::FinishCreateSelf()
{

	mFirstNameField = USearchHelper::FindViewEditField(this, paneID_FirstName);
	mLastNameField = USearchHelper::FindViewEditField(this, paneID_LastName);
	
	Inherited::FinishCreateSelf();	
	
	UReanimator::LinkListenerToControls(this, this, res_ID );
	
	// need to listen to first/last name fields to update title
	USearchHelper::LinkListenerToBroadcasters(USearchHelper::FindViewSubview(this, paneID_GeneralView), this);
	
	// Need Cooltalk since we listen to the popup
	USearchHelper::LinkListenerToBroadcasters(USearchHelper::FindViewSubview(this, paneID_CooltalkView), this); 

}



//------------------------------------------------------------------------------
//	¥	ListenToMessage
//------------------------------------------------------------------------------
//	
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


//------------------------------------------------------------------------------
//	¥	SetConferenceText
//------------------------------------------------------------------------------
// Update Caption text in Conference
// The edit field is disable if Netscape DLS server is selected
//
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




//------------------------------------------------------------------------------
//	¥	GetPaneAndAttribID
//------------------------------------------------------------------------------
//	Given a Index return the associated Pane and attribute id's. Also returns the
//	number of attributes
//
int32 CNamePropertiesWindow::GetPaneAndAttribID( TableIndexT index, PaneIDT& outPaneID, AB_AttribID	&outAttrib   )
{
	Int32	personAttributes[][2] =
	{ 		 
     			paneID_Nickname		,	AB_attribNickName
     		,	paneID_FirstName	, 	AB_attribGivenName           
      		,	paneID_LastName		,	AB_attribFamilyName 
      		,	paneID_Company		,	AB_attribCompanyName 
      		,	paneID_State		,	AB_attribRegion
      		,	paneID_EMail		,	AB_attribEmailAddress 
      		,	paneID_Notes		,	AB_attribInfo
      		,	paneID_PrefersHTML	,	AB_attribHTMLMail
      		,	paneID_Title		,	AB_attribTitle 
      		,	paneID_Address		,	AB_attribStreetAddress
      		,	paneID_ZIP			,	AB_attribZipCode 
      		,	paneID_Country		,	AB_attribCountry 
      		,	paneID_WorkPhone	,	AB_attribWorkPhone 
      		,	paneID_FaxPhone		,	AB_attribFaxPhone 
      		,	paneID_HomePhone	,	AB_attribHomePhone 
      		,	paneID_ConferenceAddress,	AB_attribCoolAddress 
      		,	paneID_ConferenceServer,	AB_attribUseServer
	  		,	paneID_PagerPhone		,	AB_attribPager
	  		,	paneID_DisplayName		,	AB_attribDisplayName
	  		,	paneID_CelluarPhone , AB_attribCellularPhone
	  		,	paneID_Department,	AB_attribDistName
    };
    outPaneID = paneID_None;
    Int32 numEntries =  ( sizeof( personAttributes )/ 8 );
    if ( index < numEntries )
    {
    	outPaneID =  PaneIDT( personAttributes[index][0] );
    	outAttrib = AB_AttribID( personAttributes[index][1] );
	}
	
	return	numEntries;
};

//------------------------------------------------------------------------------
//	¥	FindPaneForAttribute
//------------------------------------------------------------------------------
//	Given a Index return the associated Pane and attribute id's. Also returns the
//	number of attributes
//
PaneIDT CNamePropertiesWindow::FindPaneForAttribute ( AB_AttribID inAttrib )
{
	Int32 numEntries = 0;
	PaneIDT pane = 0;
	AB_AttribID attrib;
	numEntries = GetPaneAndAttribID ( 0, pane, attrib );
	for ( int32 i = 0 ; i< numEntries ; i++ )
	{
		numEntries = GetPaneAndAttribID ( i, pane, attrib );
		if ( attrib == inAttrib )
			return pane;
	}
	return paneID_None;
}


//------------------------------------------------------------------------------
//	¥	UpdateBackendToUI
//------------------------------------------------------------------------------
//	Update the name properties to the current values in the dialog fields and other settings.
//
void CNamePropertiesWindow::UpdateBackendToUI(  )
{
	char charData[ kMaxPersonSize ];
	Int32 currentTextOffset = 0;
	AB_AttribID attribID ;
	PaneIDT paneID = paneID_None;
	int32 numControls =  kNumPersonAttributes - 1 ;
	
	AB_AttributeValue* attribute = new AB_AttributeValue[ kNumPersonAttributes];
	
	// Convert fill array with control values
	for ( Int32 i = 0; i< numControls; i++ )
	{
		GetPaneAndAttribID ( i, paneID, attribID ); 
		
		attribute[ i ].attrib = attribID;
		
		LPane* control = FindPaneByID( paneID );
		if ( control )
		{
			CSearchEditField* editField = NULL;
			LGAPopup* popup = NULL;
			LGACheckbox* checkbox = NULL;
			if( ( editField = dynamic_cast< CSearchEditField* >( control ) ) != 0)
			{
				editField->GetText( &charData[ currentTextOffset] );
				Int32 length = XP_STRLEN( &charData[ currentTextOffset]  );
								
				Assert_( length + currentTextOffset < kMaxPersonSize );
				attribute[i].u.string = &charData[ currentTextOffset ];
				currentTextOffset+= length+1 ; 
				
			}
			else if( ( popup = dynamic_cast< LGAPopup* >( control ) ) != 0)
			{
				attribute[i].u.shortValue = popup->GetValue();
			}
			else if ( ( checkbox = dynamic_cast< LGACheckbox *>(control ) )!= 0 )
			{
				attribute[i].u.boolValue = checkbox->GetValue();
			}
		}
	}
	
	// WinCSID -- I love special cases.
	
	MWContext* context= CAddressBookWindow::GetMailContext();
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	attribute[ numControls ].attrib = AB_attribWinCSID;
	attribute [ numControls ].u.shortValue = INTL_GetCSIWinCSID(c);

	// Commit Changes
	CAddressBookManager::FailAddressError( AB_SetPersonEntryAttributes ( mMSGPane, attribute, kNumPersonAttributes ) );
	CAddressBookManager::FailAddressError( AB_CommitChanges( mMSGPane ) );

	delete [] attribute;
}


//------------------------------------------------------------------------------
//	¥	UpdateUIToBackend
//------------------------------------------------------------------------------
//	Update the dialog fields to the current values of the name properties.
//
void CNamePropertiesWindow::UpdateUIToBackend( MSG_Pane* inPane )
{
	Assert_( inPane );
	mMSGPane = inPane;
	MSG_SetFEData( inPane, CMailCallbackManager::Get());
	
	// Get the person Attribute
	AB_AttribID personAttribs [ kNumPersonAttributes ];
	int32 numPanes = 0;
	AB_AttribID attrib;
	PaneIDT  paneID = 0;
	numPanes = GetPaneAndAttribID( 0, paneID, attrib );
	Assert_( numPanes <= kNumPersonAttributes );
	
	for( int32 i = 0; i< numPanes; i++ )
	{
		GetPaneAndAttribID( i, paneID, attrib );
		personAttribs[ i ] = attrib;
	}
	uint16 numberAttributes = numPanes;
	
	// Allocate storage for the attribute values
	AB_AttributeValue* attribute;
	CAddressBookManager::FailAddressError (AB_GetPersonEntryAttributes ( mMSGPane, personAttribs, &attribute, &numberAttributes ) );
	
	for ( Int32 i = 0; i < numberAttributes; i++ )
	{
		PaneIDT pane = FindPaneForAttribute( attribute[i].attrib );
		if( pane != paneID_None )
		{
			switch(  attribute[i].attrib )
			{
				case AB_attribUseServer:
					USearchHelper::FindSearchPopup( this, pane )->SetValue( attribute[i].u.shortValue + 1 ); // Popups are 1 based
					break;
				
				case  AB_attribHTMLMail:
					USearchHelper::FindViewControl( this , pane )->SetValue( attribute[i].u.boolValue);
					break;
					
				case AB_attribWinCSID:
					break;
				
				default:
					CSearchEditField* editfield = dynamic_cast<CSearchEditField*>( FindPaneByID( pane ) );
					if( editfield )
						editfield->SetText ( attribute[i].u.string );
					break;
			};
		}
	}
	AB_FreeEntryAttributeValues( attribute , numberAttributes);
	
	SetConferenceText( );
}


//------------------------------------------------------------------------------
//	¥	UpdateTitle
//------------------------------------------------------------------------------
//	Update the window title.
//
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


//------------------------------------------------------------------------------
//	¥	CListPropertiesWindow
//------------------------------------------------------------------------------
//	Construct the list window.
//
CListPropertiesWindow::CListPropertiesWindow(LStream *inStream) : 
							 	CAddressBookChildWindow(inStream),
							 		  	   	 mAddressBookListTable(nil),
							 		  	   	 mTitleField(nil)
{
	SetPaneID(pane_ID);
}


//------------------------------------------------------------------------------
//	¥	~CListPropertiesWindow
//------------------------------------------------------------------------------
//	Dispose of the list window.
//
inline CListPropertiesWindow::~CListPropertiesWindow()
{

	mAddressBookListTable = nil;
}


//------------------------------------------------------------------------------
//	¥	UpdateUIToBackend
//------------------------------------------------------------------------------
//	Load and display the specified address book mailing listlist
//
void CListPropertiesWindow::UpdateUIToBackend(MSG_Pane* inPane)
{
	AssertFail_(mAddressBookListTable != nil);
	Assert_( inPane );
	mMSGPane = inPane;
	
	mAddressBookListTable->LoadMailingList( inPane );
	
	// Set edit fields
	AB_AttribID mailingListAttributes[] = { AB_attribFullName, AB_attribNickName, AB_attribInfo };
	uint16 numItems = 3;
	// Can't use New since XP code will be freeing the values
	AB_AttributeValue*  mailingListValues =( AB_AttributeValue*) XP_ALLOC( sizeof( AB_AttributeValue) *  numItems  );
	if( !mailingListValues )
		Throw_(memFullErr); 
	CAddressBookManager::FailAddressError(
		AB_GetMailingListAttributes( mMSGPane,  mailingListAttributes, &mailingListValues, &numItems ) ); 
	
	mTitleField->SetText(  mailingListValues[0].u.string  );
	dynamic_cast< CSearchEditField *>( USearchHelper::FindViewEditField(this, paneID_Nickname) )->SetText(  mailingListValues[1].u.string );
	dynamic_cast< CSearchEditField *>( USearchHelper::FindViewEditField(this, paneID_Description) )->SetText(  mailingListValues[2].u.string );
	CAddressBookManager::FailAddressError(AB_FreeEntryAttributeValues( mailingListValues,  3  ) );  
	UpdateTitle();	
}


//------------------------------------------------------------------------------
//	¥	UpdateBackendToUI
//------------------------------------------------------------------------------
//	Update the list properties to the current values in the dialog fields.
//
void CListPropertiesWindow::UpdateBackendToUI(  )
{
	char mValue[ kMaxPersonSize ]; 
	AB_AttributeValue mailingListValues[ 4 ];
	
	mailingListValues[0].attrib = AB_attribFullName;
	mailingListValues[0].u.string= mTitleField->GetText( &mValue[0] );
	
	mailingListValues[1].attrib = AB_attribNickName;
	mailingListValues[1].u.string = ( dynamic_cast< CSearchEditField *>
						( USearchHelper::FindViewEditField(this, paneID_Nickname) ) )->GetText( &mValue[256] );

	mailingListValues[2].attrib = AB_attribInfo;
	mailingListValues[2].u.string = ( dynamic_cast< CSearchEditField *>
					(USearchHelper::FindViewEditField(this, paneID_Description) ) )->GetText( &mValue[512] );
	// Need to copy in the win CSID

	MWContext* context= CAddressBookWindow::GetMailContext();
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	mailingListValues[3].attrib = AB_attribWinCSID;
	mailingListValues[3].u.shortValue= INTL_GetCSIWinCSID(c);
	
	CAddressBookManager::FailAddressError( AB_SetMailingListAttributes( mMSGPane, mailingListValues,  3 ) ); 
	CAddressBookManager::FailAddressError( AB_CommitChanges( mMSGPane ) );
}



//------------------------------------------------------------------------------
//	¥	FinishCreateSelf
//------------------------------------------------------------------------------
//	Initialize the window.
//
void CListPropertiesWindow::FinishCreateSelf() {

	mAddressBookListTable = 
		dynamic_cast<CMailingListTableView *>(USearchHelper::FindViewSubview(this, paneID_AddressBookListTable));
	FailNILRes_(mAddressBookListTable);
	mTitleField = dynamic_cast< CSearchEditField *> ( USearchHelper::FindViewEditField(this, paneID_Name) );

	Inherited::FinishCreateSelf();	// Call CMediatedWindow for now since we need to
											// create a different context than CMailNewsWindow
	
	UReanimator::LinkListenerToControls(this, this, res_ID );
	// Lastly, link to our broadcasters, need to link to allow title update 
	USearchHelper::LinkListenerToBroadcasters(this, this);
	
}


//------------------------------------------------------------------------------
//	¥	DrawSelf
//------------------------------------------------------------------------------
//		Draw the window.
//
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


//------------------------------------------------------------------------------
//	¥	UpdateTitle
//------------------------------------------------------------------------------
//	Update the window title.
//
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
#endif

