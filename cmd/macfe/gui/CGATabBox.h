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

#ifndef __H_CGATabBox
#define __H_CGATabBox
#pragma once

/*======================================================================================
	AUTHOR:			Ted Morris - 18 NOV 96

	DESCRIPTION:	Implements a view displaying tabbed panes.
	
					The user con is used to specify the initial tab in Constructor.
					The GetTextTraitsID() specifies the font for a selected tab, 
					GetTextTraitsID() + 1 for an unselected tab.
					
					The message value of the tabs (CGATabBoxTab) in this view are
					used to specified the PaneIDT for the pane that is displayed
					when the tab is clicked, or 0 if no pane is to be displayed.
					
	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "LGABox_fixes.h"

class CGATabBoxTab;
class CGATabBox;
class LTabGroup;


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/

// Message structures

typedef struct {
	CGATabBox		*box;		// The view
	Boolean			canChange;	// Initially set to true, set to false if cannot change
} TabBoxCanChangeT;


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark -

class CGATabBox : public LGABox_fixes,
				  protected LListener,
				  protected LBroadcaster {
				  
public:
						// Broadcast messages

						enum { 
							msg_TabViewCanChange = 'CnCh'		// TabBoxCanChangeT *, tab view is about 
																// to change, return true if this is 
																// alright
						,	msg_TabViewChanged = 'TvCh'			// this, the current view was just
																// changed but is not yet visibly updated, 
																// react as necessary
						};

						enum { class_ID = 'TbOx' };

	static void			RegisterTabBoxClasses(void);

						CGATabBox(LStream *inStream);
						
	PaneIDT				GetCurrentTabID(void) {
							return mCurrentTabID;
						}
	PaneIDT				GetCurrentViewID(void) {
							return mCurrentPaneID;
						}
	void				SetCurrentTabID(PaneIDT inTabID);

	virtual void		CalcBorderRect(Rect &outRect);

protected:
	
	virtual void		FinishCreateSelf(void);
	virtual void		DrawBoxBorder(void);
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam);
	
	void				SetupTabs(void);
	Boolean				StoreLatentTabCommander(LView *inTabView);
	Boolean				RestoreLatentTabCommander(LView *inTabView);
	void				CalcCurrentTabRect(Rect &outRect);
	CGATabBoxTab		*GetTab(PaneIDT inID);
	LTabGroup			*FindTabGroup(void);
	
	// Instance variables
	
	PaneIDT				mCurrentTabID;			// Currently selected tab
	PaneIDT				mCurrentPaneID;			// Currently displayed pane
	LArray				mTabLatentCommanders;	// Latent commanders for each tab	
};


#endif // __H_CGATabBox

