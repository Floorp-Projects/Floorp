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

#include "ListUtils.h"
void SynchronizeRowsTo( ListHandle list, int newRows )
{
	int		rows;
	
	if ( !list || !*list )
		return;
		
	rows = (**list).dataBounds.bottom - 1;

	if ( rows < 0 )
		rows = 0;
		
	if ( rows < newRows )
		::LAddRow( newRows - rows, 1, list );
	if ( rows > newRows )
		::LDelRow( rows - newRows, 1, list );
}

void SynchronizeColumnsTo( ListHandle list, int newColumns )
{
	int		columns;
	
	if ( !list || !*list )
		return;
		
	columns = (**list).dataBounds.right - 1;

	if ( columns < 0 )
		columns = 0;
		
	if ( columns < newColumns )
		::LAddColumn( newColumns - columns, 1, list );
	if ( columns > newColumns )
		::LDelColumn( columns - newColumns, 1, list );
}

void SetMenuSize( LGAPopup* popup, short shouldBe )
{
	MenuHandle		menu;
	short			count;
	
	Assert_( popup );
	menu = popup->GetMacMenuH();
	count = ::CountMItems( menu );
	
	while ( shouldBe > count )
	{	
		InsertMenuItem( menu, "\p ", count );
		count++;
	}
	
	while ( shouldBe < count )
	{
		DeleteMenuItem( menu, count );
		count--;
	}
	
	popup->SetMinValue( 1 );
	popup->SetMaxValue( shouldBe );
}

Boolean SetPopupToNamedItem( LGAPopup* whichMenu, const LStr255& itemText )
{
	MenuHandle		menuH;
	short			menuSize;
	Str255			fontName;
	
	Assert_( whichMenu );
	menuH = whichMenu->GetMacMenuH();
	Assert_( menuH );

	menuSize = CountMItems( menuH );
	
	for ( short i = 1; i <= menuSize; i++ )
	{
		::GetMenuItemText( menuH, i, fontName );
		if ( itemText == (LStr255)fontName )
		{
			whichMenu->SetValue( i );
			return true;
		}
	}
	return false;
}

