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


// CLDAPQueryDialog.h
#pragma once

#include "LGADialogBox.h"
#include "CSearchManager.h"
#include "CSaveWindowStatus.h"
//------------------------------------------------------------------------------
//	¥	CLDAPHandlerInterface
//------------------------------------------------------------------------------
// I am defining a simple interface for getting Search Parameters
// an alternative would have been to subclass CSearchManager
// but that class does a lot more than just building queries
//
class CLDAPHandlerInterface
{
public:
	virtual	void Setup( MSG_Pane *inSearchPane, DIR_Server *inServer, LView *inView ) = 0;
	virtual Int32	GetSearchParameters ( SearchLevelParamT* levelParam ) = 0;
	virtual	void	SetSearchParameters ( SearchLevelParamT* levelParam , int32 numLevels ) = 0;
protected:
	LView * mView;
};

//------------------------------------------------------------------------------
//	¥	CLDAPBasicHandler
//------------------------------------------------------------------------------
// Handles building queries from basic search pane
//
class CLDAPBasicHandler: public CLDAPHandlerInterface
{
public:
	enum { eBasicSearchTerm = 8965, eBasicSearchCaption = 8961 };
	virtual void 	Setup( MSG_Pane *inSearchPane, DIR_Server *inServer, LView *inView );
	virtual Int32	GetSearchParameters ( SearchLevelParamT* levelParam );
	virtual	void	SetSearchParameters ( SearchLevelParamT* levelParam , int32 numLevels );

private:
	MSG_SearchMenuItem mSearchAttributes[4]; // The names and attributes of the 4 editfields
};

//------------------------------------------------------------------------------
//	¥	CLDAPAdvancedHandler
//------------------------------------------------------------------------------
// Handles building queries from the advanced pane
//
class CLDAPAdvancedHandler: public CLDAPHandlerInterface
{
public:
	virtual	void Setup( MSG_Pane *inSearchPane, DIR_Server *inServer, LView *inView ) ;
	virtual Int32	GetSearchParameters ( SearchLevelParamT* levelParam ) ;
	virtual	void	SetSearchParameters ( SearchLevelParamT* levelParam , int32 numLevels ) ;
	CSearchManager*	GetSearchManager()  { return &mSearchManager; }	
private:
	CSearchManager		mSearchManager;
	LArray				mSearchFolders;
};


static const UInt16 cLDAPSaveWindowStatusVersion = 	0x0214;
//------------------------------------------------------------------------------
//	¥	CLDAPQueryDialog
//------------------------------------------------------------------------------
// The query building dialog box
// if the user hits okay a query will be build for the given message pane
//
class CLDAPQueryDialog: public LGADialogBox, public CSaveWindowStatus
{
private:
	typedef LGADialogBox Inherited;
public:
	enum { class_ID = 'LdDb', res_ID = 8980 };
	enum { eBasic = 0, eAdvanced = 1 };
	enum { eBasicEnclosure = 'BaVw', eAdvancedEnclosure = 'PENC'};
	enum { paneID_Search = 'SRCH', paneID_Advanced = 'AdBt', paneID_Basic = 'BaBt'};
	
	CLDAPQueryDialog( LStream*	 inStream ): LGADialogBox( inStream ), CSaveWindowStatus ( this )
								,mMaxLevels(5), mAdvancedSearch( 0 ),mSearchManager( NULL ), 
								mMSGPane(NULL ), mDirServer(NULL ), mIsModified( false) {};
								
	~CLDAPQueryDialog();
						
	void	Setup( MSG_Pane* inPane, DIR_Server* inServer );
	Boolean BuildQuery();
	

protected:
	virtual void		FinishCreateSelf();
	virtual	void 		ListenToMessage(MessageT inMessage, void *ioParam);
// Status window	
	virtual ResIDT		GetStatusResID() const { return res_ID; }
	virtual UInt16		GetValidStatusVersion() const { return cLDAPSaveWindowStatusVersion; }
	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
private:
	void ShowHandler();
	MSG_SearchError AddParametersToSearch();

	CLDAPHandlerInterface* mHandler[2];
	LView* mBasicView;
	LView* mAdvancedView;
	
	Int32 	mAdvancedSearch;
	Boolean	mIsModified;
		
	MSG_Pane* mMSGPane;
	DIR_Server*	mDirServer;
	CSearchManager* mSearchManager;
	Int32	mMaxLevels;
};
