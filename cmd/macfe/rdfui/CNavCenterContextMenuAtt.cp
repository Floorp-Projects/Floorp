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
// A version of the contextual menu attachment that generates the menu from 
// the RDF backend.
//

#include "CNavCenterContextMenuAtt.h"
#include "CHyperTreeFlexTable.h"
#include "Netscape_constants.h"
#include "CNavCenterSelectorPane.h"


//
// ConvertHTCommandToPPCommand
//
// The HyperTree has its own constants that represent the commands supplied by the
// backend. In order to make the HT commands live in a powerplant world, we have to 
// convert them.  This (simply) involves adding a base constant to the HT command to make the 
// appropriate PP command.
//
CommandT 
CNavCenterContextMenuAttachment :: ConvertHTCommandToPPCommand ( HT_MenuCmd inCmd )
{
	return inCmd + cmd_NavCenterBase;
	
} // ConvertHTCommandToPPCommand


//
// BuildMenu
//
// Overridden to create the menu from the RDF backend instead of from a resource.
//
LMenu*
CNavCenterContextMenuAttachment :: BuildMenu ( ) 
{
	LMenu* menu = new LMenu(32001);		// load empty menu to fill in
	HT_Cursor curs = NewHTContextMenuCursor();

	if ( !curs || !menu )
		return NULL;
		
	HT_MenuCmd menuItem;
	short after = 0;
	while ( HT_NextContextMenuItem(curs, &menuItem) ) {
		LStr255 itemName;
		if ( menuItem == HT_CMD_SEPARATOR )
			itemName = "\p-";
		else
			itemName = HT_GetMenuCmdName(menuItem);
		menu->InsertCommand ( itemName, ConvertHTCommandToPPCommand(menuItem),
								after );
		after++;		
	}
	HT_DeleteContextMenuCursor ( curs );
	
	return menu;
	
} // BuildMenu


//
// NewHTContextMenuCursor
//
// Create the HT context menu cursor that is appropriate for this tree view
//
HT_Cursor
CNavCenterContextMenuAttachment :: NewHTContextMenuCursor ( )
{
	PRBool clickInBackground = PR_TRUE;
	CHyperTreeFlexTable* table = dynamic_cast<CHyperTreeFlexTable*>(mOwnerHost);
	Assert_(table != NULL);
	if ( table ) {
		TableIndexT selectedRow = 0;
		if ( table->GetNextSelectedRow(selectedRow) )
			clickInBackground = PR_FALSE;
	}
		
	return HT_NewContextualMenuCursor ( table->GetHTView(), PR_FALSE, clickInBackground );

} // NewHTContextMenuCursor


//
// PruneMenu
//
// Override to not do anything, since HT already prunes the list for us. The return
// value is used as the default item, and we don't care about a default (as long as
// this value is > 0).
//
UInt16
CNavCenterContextMenuAttachment :: PruneMenu ( LMenu* /*inMenu*/ )
{
	return 1;

} // PruneMenu


#pragma mark -- class CNavCenterSelectorContextMenuAttachment --


//
// NewHTContextMenuCursor
//
// Create the HT context menu cursor that is appropriate for this view
//
HT_Cursor
CNavCenterSelectorContextMenuAttachment :: NewHTContextMenuCursor ( )
{
	CNavCenterSelectorPane* selectorPane = dynamic_cast<CNavCenterSelectorPane*>(mOwnerHost);
	Assert_(selectorPane != NULL);
	
	// check if mouse is in background
	Point where;
	::GetMouse(&where);
	bool inBackground = selectorPane->FindSelectorForPoint(where) == NULL;
	
	HT_View currView = NULL;
	if ( !inBackground )
		currView = selectorPane->GetActiveWorkspace();
	return HT_NewContextualMenuCursor ( currView, PR_TRUE, inBackground ? PR_TRUE : PR_FALSE );

} // NewHTContextMenuCursor

