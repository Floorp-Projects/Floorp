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
#include "CNavCenterSelectorPane.h"

class CHyperTreeFlexTable;


class CRDFCoordinator :	public LView,
						public LListener, public LCommander,
						public CRDFNotificationHandler,
						public LDragAndDrop
{
public:
	enum { class_ID = 'RCoo', pane_ID = 'RCoo' };
	
	static const char* Pref_EditWorkspace;
	static const char* Pref_ShowNavCenterSelector;
	static const char* Pref_ShowNavCenterShelf;
		
					CRDFCoordinator(LStream* inStream);
	virtual			~CRDFCoordinator();

		// save/restore user preferences
	virtual void	SavePlace ( LStream* outStreamData ) ;
	virtual void	RestorePlace ( LStream* outStreamData ) ;
	
		// Set the current workspace to a particular kind of workspace
	virtual void SelectView ( HT_ViewType inPane ) ;
		
		// access to the two shelves that comprise the NavCenter. These wrapper classes
		// allow you to easily slide in/out the shelves or check if they are open.
	CShelf& NavCenterShelf() const { return *mNavCenter; } ;		// tree view
	CShelf& NavCenterSelector() const { return *mSelector; } ;		// selector widget

		// register/unregister this NavCenter for SiteMap updates, etc
	void RegisterNavCenter ( MWContext* inContext ) ;
	void UnregisterNavCenter ( ) ;

protected:

		// PowerPlant overrides
	virtual	void	FinishCreateSelf();
	virtual Boolean	ObeyCommand ( CommandT inCommand, void* ioParam );
	virtual Boolean HandleKeyPress ( const EventRecord &inKeyEvent ) ;
		
		// change the currently selected workspace
	virtual void	SelectView(HT_View view);
	
	virtual void	ExpandNode(HT_Resource node);
	virtual void	CollapseNode(HT_Resource node);

		// messaging and notifications
	virtual	void	HandleNotification( HT_Notification	notifyStruct, HT_Resource node, HT_Event event);
	virtual void	ListenToMessage( MessageT inMessage, void *ioParam);

	PaneIDT					mSelectorPaneID;	// for the selector shelf
	CNavCenterSelectorPane*	mSelectorPane;
	CShelf*					mSelector;

	PaneIDT					mTreePaneID;		// for the tree view shelf
	CHyperTreeFlexTable*	mTreePane;
	CShelf* 				mNavCenter;
	
	HT_Pane			mHTPane;					// the HT pane containing all the workspaces
	
	bool			mIsInChrome;				// are we embedded in chrome?
	
}; // CRDFCoordinator