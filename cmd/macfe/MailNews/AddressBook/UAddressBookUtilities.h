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


// UAddressBookUtilities.h

#pragma once
#include "MailNewsAddressBook.h"
#include "abcom.h"
#ifdef MOZ_NEWADDR


class CMailFlexTable;
//------------------------------------------------------------------------------
//	¥	UAddressBookUtilites
//------------------------------------------------------------------------------
//
class UAddressBookUtilites
{
public:
	enum { 		// Command IDs
				cmd_NewAddressCard = 'NwCd'				// New Card
			,	cmd_NewAddressList = 'NwLs'				// New List
			, 	cmd_NewAddressBook = 'NwAb'			
			, 	cmd_NewDirectory = 'NwDr'
			,	cmd_HTMLDomains = 'HtDm'				// domains dialog.
			,	cmd_EditProperties = 'EdPr'				// Edit properties for a list or card
			,	cmd_DeleteEntry = 'DeLe'				// Delete list or card
			,	cmd_ComposeMailMessage = 'Cmps'			// Compose a new mail message
			, 	cmd_ConferenceCall	= 'CaLL'
			,	cmd_ViewMyCard = 'vmyC'
			,	invalid_command = 0xFFFFFFFF 
		};
	
	static	AB_CommandType GetABCommand( CommandT inCommand );
	static 	void			ABCommandStatus( 
									CMailFlexTable*	inTable,	AB_CommandType	inCommand,
									Boolean		&outEnabled,	Boolean		&outUsesMark,
									Char16		&outMark,		Str255		outName
									);
	static Boolean ABCommand( CMailFlexTable* inTable, AB_CommandType inCommand);
};


// The following stack based classes are designed to help with the memory managment 
// of attributes return from the backend for the Continer and Entry attributes

//------------------------------------------------------------------------------
//	¥	StABEntryAttribute
//------------------------------------------------------------------------------
//	Handles memory management for ABEntryAttributes
//
class StABEntryAttribute
{
public:
	StABEntryAttribute ( MSG_Pane* inPane, MSG_ViewIndex inIndex, AB_AttribID inAttrib)
	{
		 if(AB_GetEntryAttributeForPane ( inPane, inIndex-1, inAttrib , &mValue ) != AB_SUCCESS )
		 	mValue = NULL;
	}
	
	StABEntryAttribute()
	{
		if ( mValue )
			AB_FreeEntryAttributeValue ( mValue );
	}
	
	char* GetChar() const {  return ( mValue ? mValue->u.string : NULL ); }
	Int16 GetShort() const { return ( mValue ? mValue->u.shortValue : 0 ) ;}
	XP_Bool GetBoolean() const { return ( mValue ?  mValue->u.boolValue : false ); }
	AB_EntryType GetEntryType() const {  return( mValue ? mValue->u.entryType : AB_Person); }
	
private:
		AB_AttributeValue *mValue ;
};

//------------------------------------------------------------------------------
//	¥	StABContainerAttribute
//------------------------------------------------------------------------------
//	Handles memory management for ABEContainerAttributes
//
class StABContainerAttribute
{
public:
	StABContainerAttribute ( MSG_Pane* inPane, MSG_ViewIndex inIndex, AB_ContainerAttribute inAttrib)
	{
		
		if( AB_GetContainerAttributeForPane ( inPane, inIndex-1, inAttrib , &mValue ) != AB_SUCCESS )
			mValue = NULL;
	}
	
	StABContainerAttribute()
	{
		if ( mValue )
			AB_FreeContainerAttribValue ( mValue );
	}
	
	char* GetChar() const {return( mValue ?  mValue->u.string : NULL ); }
	Int32 GetNumber() const { return( mValue ? mValue->u.number: 0 ) ;}
	AB_ContainerInfo* GetContainerInfo() const { return( mValue ?  mValue->u.container : NULL ); }
	AB_ContainerType GetContainerType() const { return( mValue ?   mValue->u.containerType : AB_PABContainer ); }
	
private:
		AB_ContainerAttribValue *mValue ;
};

#endif
