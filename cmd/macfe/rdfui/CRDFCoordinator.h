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
// The RDFCoordiator suite of classes handles interactions between HT (the HyperTree interface
// to RDF) and the front end. The Coordinator's big job is funneling HT events to the right UI
// element (tree, title bar, etc). Right now, there are 4 basic types of Coordiators:
//   - CDockedRDFCoordinator
//       Tree is docked in the left of the window (handles opening/closing/etc of shelf)
//   - CShackRDFCoordinator
//       Tree is embedded in HTML content
//   - CWindowRDFCoordinator
//       Tree is in a standalone window
//   - CPopupRDFCoordinator
//       Tree is popped up from a container on the toolbar
//

#pragma once

#include <PP_Types.h>
#include <LView.h>
#include <LListener.h>
#include "auto_HT_Pane.h"

#include "CRDFNotificationHandler.h"
#include "CShelfMixin.h"


class CHyperTreeFlexTable;
class CNavCenterTitle;
class CNavCenterCommandStrip;
class CBrowserView;
class CBrowserContext;


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

//
// class CRDFCoordinator
//
// The base-class of all coordinators. Don't instantiate one of these, only one of it's
// subclasses. The creation of the HT_Pane is left up to each subclass since they all vary
// in how the pane is initialized (url, HT_Resource, or nothing).
// 

class CRDFCoordinator :	public LView,
						public LListener, public LCommander, public LBroadcaster,
						public CRDFNotificationHandler
{
protected:
		// protected to prevent anyone from instantiating one of these by itself.
					CRDFCoordinator(LStream* inStream);
	virtual			~CRDFCoordinator();

public:
	enum { class_ID = 'RCoo' };
	enum {
		msg_ActiveSelectorChanged = 'selc'		// broadcast when selector changes
	};

		// Set the current workspace to a particular kind of workspace
	virtual void SelectView ( HT_ViewType inPane ) ;
		
		// register/unregister this NavCenter for SiteMap updates, etc
	void RegisterNavCenter ( const MWContext* inContext ) const ;
	void UnregisterNavCenter ( ) const ;
	
		// because sometimes you just need to get to the top-level HT pane....
	const HT_Pane HTPane ( ) const { return mHTPane.get(); }
	HT_Pane HTPane ( ) { return mHTPane.get(); }
	void HTPane ( auto_ptr<_HT_PaneStruct> inNewPane ) { mHTPane = inNewPane; }
	
		// get/set the frame to which urls are dispatched. It's ok not to set
		// this as the default will be the top-most HTML view.
	virtual void	SetTargetFrame ( const char* inFrame ) ;
	
protected:
	enum {
		kScrollerPaneID = 'HyTC'				// pane id of the scroller containing tree/headers/etc
	};
	
		// handle showing/hiding column headers. Should be called after the HT_Pane has
		// been created.
	virtual void	ShowOrHideColumnHeaders ( ) ;
	virtual void	ShowColumnHeaders ( ) ;
	virtual void	HideColumnHeaders ( ) ;
	
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

	PaneIDT					mTreePaneID;		// hyperTree pane
	PaneIDT					mColumnHeaderID;	// column header pane
	PaneIDT					mTitleStripID;		// title strip pane
	PaneIDT					mTitleCommandID;	// title command pane

	CHyperTreeFlexTable*	mTreePane;
	LPane*					mColumnHeaders;
	CNavCenterTitle*		mTitleStrip;
	CNavCenterCommandStrip*	mTitleCommandArea;
	
private:

	auto_ptr<_HT_PaneStruct>	mHTPane;		// the HT pane containing all the workspaces

		// pass-by-value not allowed for a coordinator because it corresponds 1-to-1
		// with an element in the UI.
	CRDFCoordinator( const CRDFCoordinator& );				// DON'T IMPLEMENT
	CRDFCoordinator& operator=( const CRDFCoordinator& );	// DON'T IMPLEMENT
	
}; // CRDFCoordinator


#pragma mark class CDockedRDFCoordinator


//
// class CDockedRDFCoordinator
//
// Handles everything involved with having the tree docked in the browser window on the left,
// especially opening/closing the spring-open shelf. 
//

class CDockedRDFCoordinator : public CRDFCoordinator
{
public:
	enum { class_ID = 'RCoE', pane_ID = 'RCoE', adSpacePane_ID = 'spam', mainHTMLPane_ID = 'html' };
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
	CShelf& NavCenterShelf() { return *mNavCenter; } ;
	const CShelf& NavCenterShelf() const { return *mNavCenter; } ;

		// access to the shelf comprising the HTML ad space
	CShelf& AdSpaceShelf() { return *mAdSpace; } ;
	const CShelf& AdSpaceShelf() const { return *mAdSpace; } ;
		
		// create the pane with |inNode| as the root of the view an can be fed info from |inContext|
	virtual void	BuildHTPane ( HT_Resource inNode, MWContext* inContext  ) ;
	
protected:

	virtual void	FinishCreateSelf ( ) ;
	virtual void	ListenToMessage( MessageT inMessage, void *ioParam);

private:

	virtual	void	HandleNotification( HT_Notification	notifyStruct, HT_Resource node, HT_Event event, void *token, uint32 tokenType);

	void SetAdSpaceToCorrectSize ( HT_View inView ) ;
	void ShowAdSpace ( ) ;

	CShelf* 		mNavCenter;
	CShelf*			mAdSpace;
	
	CBrowserView*	mAdSpaceView;

		// pass-by-value not allowed for a coordinator because it corresponds 1-to-1
		// with an element in the UI.
	CDockedRDFCoordinator( const CDockedRDFCoordinator& );				// DON'T IMPLEMENT
	CDockedRDFCoordinator& operator=( const CDockedRDFCoordinator& );	// DON'T IMPLEMENT
	
}; // CDockedRDFCoordinator


#pragma mark class CShackRDFCoordinator


//
// class CShackRDFCoordinator
//
// Use this when the tree is to be embedded in the HTML content area (code name: Shack).
// Really the only difference here is that it creates its HT_Pane from a url provided in
// the HTML data.
//

class CShackRDFCoordinator : public CRDFCoordinator
{
public:
	enum { class_ID = 'RCoS', pane_ID = 'RCoS' };
	enum { res_ID = 9503 } ;

					CShackRDFCoordinator(LStream* inStream);
	virtual			~CShackRDFCoordinator();

	void			BuildHTPane ( const char* inURL, unsigned int inCount, 
									char** inParamNames, char** inParamValues ) ;

private:
		// pass-by-value not allowed for a coordinator because it corresponds 1-to-1
		// with an element in the UI.
	CShackRDFCoordinator( const CShackRDFCoordinator& );				// DON'T IMPLEMENT
	CShackRDFCoordinator& operator=( const CShackRDFCoordinator& );		// DON'T IMPLEMENT
	
}; // CShackRDFCoordinator


#pragma mark class CWindowRDFCoordinator


//
// class CWindowRDFCoordinator
//
// Use this when the tree is in a standalone window. All this does differently than the
// default is create the HT_Pane w/out any special arguments. It's not really specific to
// being in a window, but until there are other things that init this way as well, the
// name is descriptive enough.
//

class CWindowRDFCoordinator : public CRDFCoordinator
{
public:
	enum { class_ID = 'RCoW', pane_ID = 'RCoW' };

					CWindowRDFCoordinator(LStream* inStream);
	virtual			~CWindowRDFCoordinator();

	void			BuildHTPane ( HT_Resource inNode ) ;
	void			BuildHTPane ( RDF_Resource inNode ) ;

protected:

	void			FinishCreateSelf ( ) ;

private:
		// pass-by-value not allowed for a coordinator because it corresponds 1-to-1
		// with an element in the UI.
	CWindowRDFCoordinator( const CWindowRDFCoordinator& );				// DON'T IMPLEMENT
	CWindowRDFCoordinator& operator=( const CWindowRDFCoordinator& );	// DON'T IMPLEMENT

}; // CWindowRDFCoordinator


#pragma mark class CWindowRDFCoordinator


//
// class CPopdownRDFCoordinator
//
// Use this when the tree is popped down from a container on the toolbars. This knows
// how to create itself from an HT_Resource.
//

class CPopdownRDFCoordinator : public CRDFCoordinator
{
public:
	enum { class_ID = 'RCoP', pane_ID = 'RCoP' };
	enum { res_ID = 9504 } ;
	enum { msg_ClosePopdownTree = 'clos' };

					CPopdownRDFCoordinator(LStream* inStream);
	virtual			~CPopdownRDFCoordinator();

		// create the pane with |inNode| as the root of the view an can be fed info from |inContext|
	virtual void	BuildHTPane ( HT_Resource inNode, MWContext* inContext ) ;

protected:

	virtual void	DrawSelf();
	virtual void	FinishCreateSelf();
	virtual	void	ListenToMessage( MessageT inMessage, void *ioParam);
	virtual void	FindCommandStatus(
									CommandT inCommand,
									Boolean	&outEnabled,
									Boolean	&outUsesMark,
									Char16	&outMark,
									Str255	outName);

private:
		// pass-by-value not allowed for a coordinator because it corresponds 1-to-1
		// with an element in the UI.
	CPopdownRDFCoordinator( const CPopdownRDFCoordinator& );				// DON'T IMPLEMENT
	CPopdownRDFCoordinator& operator=( const CPopdownRDFCoordinator& );		// DON'T IMPLEMENT
	
}; // CPopdownRDFCoordinator
