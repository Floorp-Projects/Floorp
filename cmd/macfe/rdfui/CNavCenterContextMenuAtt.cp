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
#include "URDFUtilities.h"
#include "CRDFToolbarItem.h"
#include "CRDFToolbar.h"


extern RDF_NCVocab gNavCenter;			// RDF vocab struct for NavCenter


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


#pragma mark -


//
// NewHTContextMenuCursor
//
// Create the HT context menu cursor that is appropriate for this tree view
//
HT_Cursor
CTreeViewContextMenuAttachment :: NewHTContextMenuCursor ( )
{
	PRBool clickInBackground = PR_TRUE;
	CHyperTreeFlexTable* table = dynamic_cast<CHyperTreeFlexTable*>(mOwnerHost);
	Assert_(table != NULL);
	if ( table ) {
		TableIndexT selectedRow = 0;
		if ( table->GetNextSelectedRow(selectedRow) )
			clickInBackground = PR_FALSE;
		
#if 0
// This will cause a crash if we return NULL because the context menu code assumes a menu will
// be created. Not sure what the right fix is right now, but leaving context menus on is not
// that bad (pinkerton)
		// only allow context menu if the "useSelection" flag is true for the current view
		if ( URDFUtilities::PropertyValueBool(HT_TopNode(table->GetHTView()), gNavCenter->useSelection, true) == false)
			return NULL;
#endif

	}
	
	return HT_NewContextualMenuCursor ( table->GetHTView(), PR_FALSE, clickInBackground );
	
} // NewHTContextMenuCursor


#pragma mark -


//
// NewHTContextMenuCursor
//
// Create the HT context menu cursor that is appropriate for this button. HT uses the
// selection to provide the correct context menu, so even though it sounds totally weird
// and is a hack, set the selection of the view to the button clicked on.
//
HT_Cursor
CToolbarButtonContextMenuAttachment :: NewHTContextMenuCursor ( )
{
	HT_Resource buttonNode = NULL;
	HT_View buttonView = NULL;
	CRDFToolbarItem* button = dynamic_cast<CRDFToolbarItem*>(mOwnerHost);
	Assert_(button != NULL);
	if ( button ) {
		buttonNode = button->HTNode();
		buttonView = HT_GetView(buttonNode);
		Assert_(buttonView != NULL);
		
		URDFUtilities::StHTEventMasking noEvents(HT_GetPane(buttonView), HT_EVENT_NO_NOTIFICATION_MASK);
		HT_SetSelectedView(HT_GetPane(buttonView), buttonView);
		HT_SetSelection(buttonNode);
	}
	
	return HT_NewContextualMenuCursor ( buttonView, PR_FALSE, PR_FALSE );
	
} // NewHTContextMenuCursor


#pragma mark -


//
// NewHTContextMenuCursor
//
// Create the HT context menu cursor that is appropriate for this toolbar. We
// have to make sure the selection is empty so HT gives us the right context menu.
//
HT_Cursor
CToolbarContextMenuAttachment :: NewHTContextMenuCursor ( )
{
	HT_View toolbarView = NULL;
	CRDFToolbar* bar = dynamic_cast<CRDFToolbar*>(mOwnerHost);
	Assert_(bar != NULL);
	if ( bar ) {
		toolbarView = bar->HTView();
		Assert_(toolbarView != NULL);
		
		URDFUtilities::StHTEventMasking noEvents(HT_GetPane(toolbarView), HT_EVENT_NO_NOTIFICATION_MASK);
		HT_SetSelectedView(HT_GetPane(toolbarView), toolbarView);
		HT_SetSelectionAll(toolbarView, PR_FALSE);
	}
	
	return HT_NewContextualMenuCursor ( toolbarView, PR_FALSE, PR_TRUE );
	
} // NewHTContextMenuCursor
