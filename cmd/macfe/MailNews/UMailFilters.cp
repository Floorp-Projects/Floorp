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



// UMailFilters.cp

////////////////////////////////////////////////////////////////////////////////////////
//
//	File containing key filters for Mail/News
//
//	Usage
//	-----
//
//	LEditField*	edit = dynamic_cast<LEditField*>( theWindow->FindPaneByID('XyWz') );
//	edit->SetKeyFilter( UMailFilters::HeaderField );
//	
//
////////////////////////////////////////////////////////////////////////////////////////

#include "UKeyFilters.h"
#include "UMailFilters.h"
	

EKeyStatus	
UMailFilters::CustomHeaderFilter(TEHandle /*inMacTEH*/, Char16 inKeyCode, Char16 &ioCharCode, SInt16 /*inModifiers*/)
{	
	if( IsTEDeleteKey( inKeyCode ) ) 		// need to trap this before the printing characters
		return keyStatus_Input;
		
	if( IsActionKey( inKeyCode ) ) 		// need to trap this before the printing characters
		return keyStatus_PassUp;
		
	if ( !IsPrintingChar( ioCharCode ) ) 	// ASCII 23 to 126
		return keyStatus_Reject;
	
	if ( ioCharCode == ':' || ioCharCode == ' ' ) // ':' and SPACE
	{
		::SysBeep(32);
		return keyStatus_Reject;
	}
		
	return keyStatus_Input;
}



// ---------------------------------------------------------------------------
//		* ServerNameKeyFilter									[static]
// ---------------------------------------------------------------------------

EKeyStatus
UMailFilters::ServerNameKeyFilter(TEHandle /*inMacTEH*/, Char16 inKeyCode, Char16 &ioCharCode, SInt16 /*inModifiers*/)
{
	EKeyStatus	theKeyStatus = keyStatus_PassUp;
	
	if (IsTEDeleteKey(inKeyCode))
		theKeyStatus = keyStatus_TEDelete;
	else if (IsTECursorKey(inKeyCode))
		theKeyStatus = keyStatus_TECursor;
	else if (IsExtraEditKey(inKeyCode))
		theKeyStatus = keyStatus_ExtraEdit;
	else if (ioCharCode == ',' || ioCharCode == ' ' || ioCharCode == '/' || ioCharCode == ':' || ioCharCode == ';')
		theKeyStatus = keyStatus_Reject;
	else if (IsPrintingChar(ioCharCode))
		theKeyStatus = keyStatus_Input;
	
	return theKeyStatus;
}
