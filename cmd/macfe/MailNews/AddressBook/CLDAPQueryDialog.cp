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


// CLDAPQueryDialog.cp

#include "CLDAPQueryDialog.h"
#include "pascalstring.h"
#include "LGAEditField.h"
#include "prefapi.h"
#include "SearchHelpers.h"
#include "resgui.h"
#include "xp_help.h"
//------------------------------------------------------------------------------
//	¥	CLDAPBasicHandler
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//	¥	Setup
//------------------------------------------------------------------------------
//
void CLDAPBasicHandler::Setup( MSG_Pane *inSearchPane, DIR_Server *inServer, LView *inView )
{
	Assert_( inSearchPane != NULL );
	Assert_( inServer != NULL );
	Assert_(inView != NULL );

	mView = inView;
	int maxItems = 4;
	
	// Get Basic Search attributes
	MSG_SearchError error = MSG_GetBasicLdapSearchAttributes (
			 inServer, &mSearchAttributes[0], &maxItems); 
	
		
	// Update the captions to reflect their new attributes
	for( int32 captionNumber = 0; captionNumber < maxItems; captionNumber++ )
	{
		LCaption* caption = dynamic_cast< LCaption*>( mView->FindPaneByID (eBasicSearchCaption+ captionNumber ) );
		Assert_( caption );
		caption->SetDescriptor ( CStr255(mSearchAttributes[ captionNumber ].name ));
	}

}


//------------------------------------------------------------------------------
//	¥	GetSearchParameters
//------------------------------------------------------------------------------
//	Get the current search parameters. outSearchParams must be able to hold at least
//	4 elements. Strings are output as c-strings.
//
Int32	CLDAPBasicHandler::GetSearchParameters(SearchLevelParamT* levelParam )
{
	CStr255 valueString; 
	MSG_SearchOperator op = (MSG_SearchOperator) opContains;;	
	int maxItems = 4;
	Int32 currentLevel = 0;
	
	for( int32 i = 0; i < maxItems; i++ )
	{
		LGAEditField* editField = dynamic_cast< LGAEditField*>( mView->FindPaneByID (eBasicSearchTerm+ i ) );
		Assert_( editField );
		editField->GetDescriptor ( valueString );
		if ( valueString.Length() )
		{
			levelParam[ currentLevel ].val.attribute = MSG_SearchAttribute( mSearchAttributes[ i ].attrib );
			XP_STRCPY( levelParam[ currentLevel ].val.u.string, valueString );
			levelParam[ currentLevel ].op = op;
			levelParam[ currentLevel ].boolOp = true;
			currentLevel++;
		}
	}
	
	return currentLevel;
}


//------------------------------------------------------------------------------
//	¥	SetSearchParameters
//------------------------------------------------------------------------------
// This works by taking the first occurence of an Param whose attribute
// which matches a basic search criteria attribute.
//
void CLDAPBasicHandler::SetSearchParameters ( SearchLevelParamT* levelParam , int32 numLevels )
{
	int maxItems = 4;
	Int32 currentLevel = 0;
	MSG_SearchAttribute attribute;
	for( int32 basic = 0; basic < maxItems; basic++ )
	{
		attribute = MSG_SearchAttribute( mSearchAttributes[ basic ].attrib ); 
		for (	int32 currentLevel = 0; currentLevel<numLevels; currentLevel++ )
		{
			if ( levelParam[ currentLevel ].val.attribute == attribute )
			{
				LGAEditField* editField = dynamic_cast< LGAEditField*>( mView->FindPaneByID (eBasicSearchTerm+ basic ) );
				Assert_( editField );
				editField->SetDescriptor( CStr255( levelParam[ currentLevel ].val.u.string) );
				break;
			}
		}
	}
}


//------------------------------------------------------------------------------
//	¥	Setup
//------------------------------------------------------------------------------
// Advanced Search Routines
// Most of the work is done by delagation to  the Search Manager
//
void CLDAPAdvancedHandler::Setup( MSG_Pane *inSearchPane, DIR_Server *inServer, LView *inView )
{
	Assert_( inSearchPane != NULL );
	Assert_( inServer != NULL );
	Assert_(inView != NULL );
	
	mSearchFolders.InsertItemsAt(1, LArray::index_First, &inServer);
	mSearchManager.InitSearchManager( inView, NULL, scopeLdapDirectory, & mSearchFolders );
	
}


//------------------------------------------------------------------------------
//	¥	GetSearchParameters
//------------------------------------------------------------------------------
//
Int32	CLDAPAdvancedHandler::GetSearchParameters ( SearchLevelParamT* levelParam )
{
	mSearchManager.GetSearchParameters( levelParam );
	return mSearchManager.GetNumVisibleLevels();
}


//------------------------------------------------------------------------------
//	¥	SetSearchParameters
//------------------------------------------------------------------------------
//
void CLDAPAdvancedHandler::SetSearchParameters ( SearchLevelParamT* levelParam , int32 numLevels )
{
	mSearchManager.SetSearchParameters ( numLevels, levelParam );
}

//------------------------------------------------------------------------------
//	¥	~CLDAPQueryDialog
//------------------------------------------------------------------------------
//
CLDAPQueryDialog::~CLDAPQueryDialog()
{ 
	if( mMSGPane ) // If the search manager isn't initialized don't save the window data
	{
		PREF_SetBoolPref("mail.addr_book.useAdvancedSearch", mAdvancedSearch);
		CSaveWindowStatus::SaveStatusInfo();
	}
}


//------------------------------------------------------------------------------
//	¥	BuildQuery
//------------------------------------------------------------------------------
//
Boolean CLDAPQueryDialog::BuildQuery()
{
	// Initial Search setup
	Assert_( mMSGPane );
	MSG_SearchFree ( mMSGPane );
	MSG_SearchAlloc ( mMSGPane );
	
	
	if( MSG_AddLdapScope( mMSGPane, mDirServer ) != SearchError_Success )
		return false;
	
	if( AddParametersToSearch(  ) != SearchError_Success)
		return false;
	
	return true;

};


//------------------------------------------------------------------------------
//	¥	AddParametersToSearch
//------------------------------------------------------------------------------
//
MSG_SearchError CLDAPQueryDialog::AddParametersToSearch( )
{
	Assert_(mMSGPane != nil);
	MSG_SearchError error = SearchError_Success;
	
	// Get the search parameters
	StSearchDataBlock searchParams( mMaxLevels, StSearchDataBlock::eAllocateStrings);
	SearchLevelParamT *curLevel = searchParams.GetData();
	int32 numLevels = mHandler[ mAdvancedSearch ]->GetSearchParameters( curLevel );
	
	// Add parameters to the search
	for ( Int32 i=0; i< numLevels; i++ )
	{
		#ifdef FE_IMPLEMENTS_BOOLEAN_OR
			error = MSG_AddSearchTerm(mMSGPane, curLevel[i].val.attribute, curLevel[i].op,
										  &curLevel[i].val, curLevel[i].boolOp, NULL ) ;
		#else
			error = MSG_AddSearchTerm(mMSGPane, curLevel[i].val.attribute, curLevel[i].op,
										  &curLevel[i].val) ;
		#endif
		if ( error != SearchError_Success )
			break;
	} 
	return error;
}


//------------------------------------------------------------------------------
//	¥	Setup
//------------------------------------------------------------------------------
//
void	CLDAPQueryDialog::Setup( MSG_Pane* inPane, DIR_Server* inServer )
{
	mMSGPane = inPane;
	mDirServer = inServer;
	
	// Set the name of the group box
	LView* box = dynamic_cast<LView* >( FindPaneByID('ScBx') );
	Assert_( box);
	CStr255 name;
	box->GetDescriptor( name );
	name+= mDirServer->description;
	box->SetDescriptor( name );
	
	// Setup the Handlers
	mBasicView = dynamic_cast<LView*>( box->FindPaneByID(  eBasicEnclosure ) );
	Assert_( mBasicView );
	mHandler[ eBasic ]->Setup( inPane, inServer,  mBasicView );
	USearchHelper::LinkListenerToBroadcasters( mBasicView, this );
	 
	mAdvancedView = dynamic_cast<LView*>( box->FindPaneByID(  eAdvancedEnclosure ) );
	Assert_( mAdvancedView );
	mHandler[ eAdvanced ]->Setup( inPane, inServer, mAdvancedView );
	
	CLDAPAdvancedHandler* advancedLDAP = dynamic_cast<CLDAPAdvancedHandler*>(mHandler[ eAdvanced ]);
	Assert_( advancedLDAP );
	mSearchManager = advancedLDAP->GetSearchManager();
	mSearchManager->AddListener( this );
	
	XP_Bool isAdvanced = false;
	if (PREF_GetBoolPref("mail.addr_book.useAdvancedSearch", &isAdvanced)== PREF_NOERROR )
		mAdvancedSearch = isAdvanced; 
	CSaveWindowStatus::FinishCreateWindow();	
	
	mIsModified = false;
	
	ShowHandler();
	Show();
	
}


//------------------------------------------------------------------------------
//	¥	ListenToMessage
//------------------------------------------------------------------------------
//
void CLDAPQueryDialog::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch( inMessage )
	{
		case paneID_Search:
			BuildQuery();
			break;
			
		case  CSearchManager::msg_SearchParametersResized:
			ResizeWindowBy( 0, *((Int16 *) ioParam) );
			break;
			
		case CSearchEditField::msg_UserChangedText:
			mIsModified = true;
			break;
		
		case msg_Help:
			if ( mAdvancedSearch )
				ShowHelp( HELP_SEARCH_LDAP_ADVANCED );
			else
				ShowHelp( HELP_SEARCH_LDAP_BASIC ); 
			break;
			
		case paneID_Advanced:
		case paneID_Basic:
			if ( !PREF_PrefIsLocked( "mail.addr_book.useAdvancedSearch") )
			mAdvancedSearch = !mAdvancedSearch;
			ShowHandler();
			break;
	};

}


//------------------------------------------------------------------------------
//	¥	FinishCreateSelf
//------------------------------------------------------------------------------
//
void	CLDAPQueryDialog::FinishCreateSelf()
{
	mHandler[ eBasic ] = new CLDAPBasicHandler;
	mHandler[ eAdvanced ] = new CLDAPAdvancedHandler;
	UReanimator::LinkListenerToControls( this, this, CLDAPQueryDialog::res_ID );
	Inherited::FinishCreateSelf();
}


//------------------------------------------------------------------------------
//	¥	ShowHandler
//------------------------------------------------------------------------------
//
void CLDAPQueryDialog::ShowHandler()
{
	Assert_ ( mSearchManager );
	
	Int32	windowHeight = 130; 
	if ( mAdvancedSearch )
	{
		Int16 deltaLevels = 5 - mSearchManager->GetNumVisibleLevels();
		windowHeight =  220 -  deltaLevels * 23 ;
	}
	
	ResizeWindowTo ( 500, windowHeight );
	// modify contents, always do when going from advanced to basic
	// Only do when going from basic to advanced if there has been a modification
	if ( mIsModified || !mAdvancedSearch)
	{
	 	StSearchDataBlock searchParams( mMaxLevels, StSearchDataBlock::eAllocateStrings);
		SearchLevelParamT *curLevel = searchParams.GetData();
		int32 numLevels = mHandler[ !mAdvancedSearch ]->GetSearchParameters( curLevel ); 
		mHandler[ mAdvancedSearch ]->SetSearchParameters( curLevel, numLevels );
		mIsModified = false;
	}
	
	// Swap the Buttons
	USearchHelper::ShowHidePane( FindPaneByID(paneID_Advanced), !mAdvancedSearch);
	USearchHelper::ShowHidePane( FindPaneByID( paneID_Basic ), mAdvancedSearch);
	
	// Swap the panes
	USearchHelper::ShowHidePane( mBasicView, !mAdvancedSearch);
	USearchHelper::ShowHidePane( mAdvancedView, mAdvancedSearch);
}


//------------------------------------------------------------------------------
//	¥	ReadWindowStatus
//------------------------------------------------------------------------------
//
void CLDAPQueryDialog::ReadWindowStatus(LStream *inStatusData)
{
	CSaveWindowStatus::ReadWindowStatus(inStatusData);

	mSearchManager->ReadSavedSearchStatus(inStatusData);
}


//------------------------------------------------------------------------------
//	¥	WriteWindowStatus
//------------------------------------------------------------------------------
//
void CLDAPQueryDialog::WriteWindowStatus(LStream *outStatusData)
{
	CSaveWindowStatus::WriteWindowStatus(outStatusData);
	if( mAdvancedSearch == 0 )
	{
		mAdvancedSearch = true;
		ShowHandler();
	}											   
	mSearchManager->WriteSavedSearchStatus(outStatusData);

}
