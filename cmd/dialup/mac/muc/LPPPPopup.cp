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

#include "LPPPPopup.h"
#include "ListUtils.h"

// ¥ LPPPPopup
void* LPPPPopup::CreatePPPPopup( LStream* inStream )
{
	return new LPPPPopup( inStream );
}

LPPPPopup::LPPPPopup( LStream* inStream ): LGAPopup( inStream )
{
	mFunction = NULL;
}

void LPPPPopup::SetPPPFunction( TraversePPPListFunc p )
{
	mFunction = p;
}

void LPPPPopup::UpdateList()
{
	if ( mFunction )
	{
		MenuHandle		menuH;
		menuH = this->GetMacMenuH();

		OSErr			err;
		Str255*			list;
		int				number;
		
		err = ( *mFunction )( &list );
		number = ::GetPtrSize( (Ptr)list ) / sizeof ( Str255 );
		SetMenuSize( this, number );

		for ( short i = 1; i <= number; i++ )
		{
			LStr255		text;
			text = *list;
			::SetMenuItemText( menuH, i, text );
			list++;
		}
		if ( list )
			DisposePtr( (Ptr)list );
	}
}	

void LPPPPopup::GetNameValue( LStr255& value )
{
	MenuHandle		menuH;
	menuH = this->GetMacMenuH();
	
	::GetMenuItemText( menuH, this->GetValue(), value );
}
  
Boolean LPPPPopup::SetToNamedItem( const LStr255& name )
{
	return SetPopupToNamedItem( this, name );
}

