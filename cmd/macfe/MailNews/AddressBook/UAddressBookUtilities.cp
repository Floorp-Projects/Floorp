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


// UAddressBookUtilities.cp

#include "UAddressBookUtilities.h"
#ifdef MOZ_NEWADDR
#include "MailNewsgroupWindow_Defines.h"
#include "resgui.h"
#include "CMailFlexTable.h"
#include "UMailSelection.h"
#include "CAddressBookViews.h" // Need for pane ID's that should be fixed

//------------------------------------------------------------------------------
//	¥	GetABCommand
//------------------------------------------------------------------------------
//	Convert a powerplant command into a AB_CommandType
//
AB_CommandType 	UAddressBookUtilites::GetABCommand( CommandT inCommand )
{
	switch ( inCommand )
	{
	
		case	cmd_NewMailMessage:
		case	cmd_ComposeMailMessage: 				return AB_NewMessageCmd;
		case 	cmd_ImportAddressBook:					return AB_ImportCmd;	
		case 	cmd_SaveAs:								return  AB_SaveCmd;					
//		case 	cmd_Close: 						return  AB_CloseCmd;					

		case  	cmd_NewAddressBook:						return AB_NewAddressBook;
		case  	cmd_NewDirectory:							return  AB_NewLDAPDirectory;

 		case	cmd_Undo: 								return  AB_UndoCmd;
		case 	cmd_Redo:								return  AB_RedoCmd;
		
		case 	UAddressBookUtilites::cmd_DeleteEntry:	return  AB_DeleteCmd;	

//		case return  AB_LDAPSearchCmd,	

		case 	CAddressBookPane::eCol0:					return  AB_SortByColumnID0;		
		case 	CAddressBookPane::eCol1:					return  AB_SortByColumnID1;
		case 	CAddressBookPane::eCol2:					return  AB_SortByColumnID2;	
		case 	CAddressBookPane::eCol3:					return  AB_SortByColumnID3;		
		case 	CAddressBookPane::eCol4:					return  AB_SortByColumnID4;			
		case 	CAddressBookPane::eCol5:					return  AB_SortByColumnID5;			
		case 	CAddressBookPane::cmd_SortAscending: 		return  AB_SortAscending;				
		case	 CAddressBookPane::cmd_SortDescending: 		return  AB_SortDescending;			

		case	cmd_NewAddressCard:							return  AB_AddUserCmd;				
		case	cmd_NewAddressList:							return AB_AddMailingListCmd;		
		case	cmd_EditProperties:							return AB_PropertiesCmd;			
		case 	cmd_ConferenceCall:							return  AB_CallCmd;					
	//	case return  AB_ImportLdapEntriesCmd		
		default:											return AB_CommandType( invalid_command );
	}
}

//------------------------------------------------------------------------------
//	¥	ABCommandStatus
//------------------------------------------------------------------------------
//	Determine if a command should be enabled.
//	
//
void UAddressBookUtilites::ABCommandStatus( 
	CMailFlexTable*	inTable, 
	AB_CommandType			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
{
	if( inCommand != UAddressBookUtilites::invalid_command )
	{
		XP_Bool selectable;
		MSG_COMMAND_CHECK_STATE checkedState;
		const char* display_string = nil;
		XP_Bool plural;
	
		// Get table selection 
		CMailSelection selection;
		inTable->GetSelection(selection);
		
		// Check command status
		if ( AB_CommandStatusAB2(
			inTable->GetMessagePane(),
			inCommand,
			(MSG_ViewIndex*)selection.GetSelectionList(),
			selection.selectionSize,
			&selectable,
			&checkedState,
			&display_string,
			&plural)
		>= 0)
		{
			outEnabled = (Boolean)selectable;
			outUsesMark = true;
			if (checkedState == MSG_Checked)
				outMark = checkMark;
			else
				outMark = 0;
			
			if (display_string && *display_string)
						*(CStr255*)outName = display_string;
		}
	}	
}

//------------------------------------------------------------------------------
//	¥	ABCommand
//------------------------------------------------------------------------------
//	Execute a menu command
//	NOTE: this takes a PowerPlant Command
//
Boolean UAddressBookUtilites::ABCommand( CMailFlexTable* inTable, AB_CommandType inCommand)
{
	Assert_( inTable );
	Boolean enabled, usesMark;
	Char16	mark;
	Str255 name;
	if ( inCommand == invalid_command )
		return AB_INVALID_COMMAND;
	// Check to see if the command was enabled for this table
	UAddressBookUtilites::ABCommandStatus( inTable, inCommand, enabled, usesMark, mark, name );
	if ( !enabled )
		return AB_INVALID_COMMAND;
	
	// Get table Selection
	CMailSelection selection;
	inTable->GetSelection(selection);

	// Execute the command
	return AB_CommandAB2( inTable->GetMessagePane(), inCommand,
					 (MSG_ViewIndex*)selection.GetSelectionList(), selection.selectionSize );
}
#endif
