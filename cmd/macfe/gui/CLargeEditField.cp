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

#include "CLargeEditField.h"
#include "xp_mem.h"

// returns a c-string
char * CLargeEditField::GetLongDescriptor()
{
	CharsHandle	theText = ::TEGetText(mTextEditH);
	long	len = ::GetHandleSize(theText);
	if ( len <= 0 )
		return NULL;
	
	char *returnValue = (char *)XP_ALLOC( len + 1 );
	if ( returnValue )
	{
		::BlockMoveData(*theText, returnValue, len);
		returnValue[ len ] = 0;
	}
	
	return returnValue;
}


// takes a c-string
void CLargeEditField::SetLongDescriptor( char *inDescriptor )
{
	::TESetText(inDescriptor, strlen(inDescriptor), mTextEditH);
	Refresh();
}
