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

//
// CPersonalToolbarManager.h
//
// Interface to the class that manages all of the personal toolbars in browser windows. We
// need a central manager so that we can keep all the windows in sync when things change
// (say when the user adds a bookmark to one window). This reads all its info from RDF,
// not from the outdated BM UI. 
//

#pragma once

#include <string>
#include "htrdf.h"
#include "ntypes.h"
#include "CRDFNotificationHandler.h"

class CPersonalToolbarTable;
class CBookmarksContext;


//
// CUserButtonInfo
//
class CUserButtonInfo 
{
public:

	CUserButtonInfo( const string & pName, const string & pURL, Uint32 nBitmapID, Uint32 nBitmapIndex, 
						bool bIsFolder);
	~CUserButtonInfo();

	const string & GetName(void) const 				{ return mName; }
	const string & GetURL(void) const 				{ return mURL; }
	
	bool IsResourceID(void) const 					{ return mIsResourceID; }
	Uint32 GetBitmapID(void) const 					{ return mBitmapID; }
	Uint32 GetBitmapIndex(void) const 				{ return mBitmapIndex; }
	bool IsFolder(void) const 						{ return mIsFolder;}

private:
	const string mName;
	const string mURL;
	bool	mIsResourceID;
	Uint32	mBitmapID;
	Uint32	mBitmapIndex;
	string	mBitmapFile;
	bool	mIsFolder;

}; // CUserButtonInfo


//
// CPersonalToolbarManager
//
// Nothing really needs to be static because there will be only one of these and it
// can be easily accessed from a static variable in CFrontApp
//
class CPersonalToolbarManager : public LBroadcaster, public CRDFNotificationHandler
{
	public:
		//--- some constants and messages
		enum { kMinPersonalToolbarChars = 15, kMaxPersonalToolbarChars = 30 };
		enum MessageT { k_PTToolbarChanged = 'TbCh' } ;
		
		CPersonalToolbarManager ( ) ;
		virtual ~CPersonalToolbarManager ( ) ;
	
		//--- create, add, delete, etc operations on the toolbars
		void RegisterNewToolbar ( CPersonalToolbarTable* inBar );
		void ToolbarChanged();
		void AddButton ( HT_Resource inBookmark, Uint32 inIndex) ;
		void AddButton ( const string & inURL, const string & inTitle, Uint32 inIndex ) ;
		void RemoveButton ( Uint32 inIndex );
		
		//--- utilities
		Uint32 GetMaxToolbarButtonChars() const { return mMaxToolbarButtonChars; }
		Uint32 GetMinToolbarButtonChars() const { return mMinToolbarButtonChars; }
		void SetMaxToolbarButtonChars(Uint32 inNewMax) { mMaxToolbarButtonChars = inNewMax; }
		void SetMinToolbarButtonChars(Uint32 inNewMin) { mMinToolbarButtonChars = inNewMin; }
		
		LArray & GetButtons ( ) { return mUserButtonInfoArray; }
		HT_View GetHTView() const { return mToolbarView; }

	protected:		
	
		HT_View		mToolbarView;
		HT_Resource	mToolbarRoot;
		Uint32		mMaxToolbarButtonChars;
		Uint32		mMinToolbarButtonChars;
		LArray		mUserButtonInfoArray;
		
		static const char* kMaxButtonCharsPref;
		static const char* kMinButtonCharsPref;
		
		void InitializeButtonInfo();
		void RemoveToolbarButtons();
		void HandleNotification( HT_Notification notifyStruct, HT_Resource node, HT_Event event);
	
}; // CPersonalToolbarManager
