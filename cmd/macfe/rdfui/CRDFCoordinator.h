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
// Mike Pinkerton, Netscape Communications
//
// The view that contains the "selector widget" (an object that selects the different 
// RDF "views" stored in the RDF database) and the the "tree view" which displays the selected RDF
// view like the Finder list view.
//
// It's mainly responsible for coordinating the UI with the HT/RDF XP code.
//

#pragma once

#include <PP_Types.h>
#include <LView.h>
#include <LListener.h>

#include "CRDFNotificationHandler.h"
#include "CShelfMixin.h"


class CHyperTreeFlexTable;


//
// class ViewFEData
//
// Contains all the FE-specific data stuffed into an HT_View. This holds stuff about the icon on
// the selector bar for the view, columns associated with the view, etc...
//
#if 0
struct ViewFEData {
	ViewFEData ( );
	ViewFEData ( SelectorData* inSelector ); 
	~ViewFEData ( ) ;

	SelectorData* mSelector;
	// ColumnData* mColumnInfo;		// to come later...
};
#endif


#pragma mark class CRDFCoordinator

class CRDFCoordinator :	public LView,
						public LListener, public LCommander, LBroadcaster,
						public CRDFNotificationHandler
{
public:
	enum { class_ID = 'RCoo', pane_ID = 'RCoo' };

//	static const char* Pref_EditWorkspace;
//	static const char* Pref_ShowNavCenterSelector;
//	static const char* Pref_ShowNavCenterShelf;
		
					CRDFCoordinator(LStream* inStream);
	virtual			~CRDFCoordinator();

		// Set the current workspace to a particular kind of workspace
	virtual void SelectView ( HT_ViewType inPane ) ;
		
		// register/unregister this NavCenter for SiteMap updates, etc
	void RegisterNavCenter ( MWContext* inContext ) ;
	void UnregisterNavCenter ( ) ;
	
		// because sometimes you just need to get to the top-level HT pane....
	HT_Pane HTPane ( ) const { return mHTPane; } ;
	void HTPane ( HT_Pane inPane ) { mHTPane = inPane; } ;		// an imp for now...

protected:

		// PowerPlant overrides
	virtual	void	FinishCreateSelf();
	virtual Boolean	ObeyCommand ( CommandT inCommand, void* ioParam );
		
		// change the currently selected workspace
	virtual void	SelectView(HT_View view);
	
	virtual void	ExpandNode(HT_Resource node);
	virtual void	CollapseNode(HT_Resource node);

		// messaging and notifications
	virtual	void	HandleNotification( HT_Notification	notifyStruct, HT_Resource node, HT_Event event, void *token, uint32 tokenType);
	virtual void	ListenToMessage( MessageT inMessage, void *ioParam);

	PaneIDT					mTreePaneID;		// for the tree view shelf
	CHyperTreeFlexTable*	mTreePane;
	
	HT_Pane			mHTPane;					// the HT pane containing all the workspaces
	
}; // CRDFCoordinator


#pragma mark class CDockedRDFCoordinator

class CDockedRDFCoordinator : public CRDFCoordinator
{
public:
	enum { class_ID = 'RCoE', pane_ID = 'RCoE' };
	enum {
		msg_ShelfStateShouldChange	= 'shlf'		// broadcast when shelf should open/close
	};

					CDockedRDFCoordinator(LStream* inStream);
	virtual			~CDockedRDFCoordinator();

		// save/restore user preferences
	virtual void	SavePlace ( LStream* outStreamData ) ;
	virtual void	RestorePlace ( LStream* outStreamData ) ;

		// access to the shelf that comprised the NavCenter. This wrapper class
		// allows you to easily slide in/out the shelf or check if it is open.
	CShelf& NavCenterShelf() const { return *mNavCenter; } ;

protected:

	virtual void	FinishCreateSelf ( ) ;
	virtual void	ListenToMessage( MessageT inMessage, void *ioParam);
	

private:
	CShelf* 		mNavCenter;

}; // CDockedRDFCoordinator