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

#include "CSelectionBroadcastingList.h"

#include <LStream.h>

// ---------------------------------------------------------------------------
//		¥ CreateFromStream [static]
// ---------------------------------------------------------------------------

void*
CSelectionBroadcastingList::CreateFromStream(
	LStream	*inStream)
{
	return (new CSelectionBroadcastingList(inStream));
}

// ---------------------------------------------------------------------------
//		¥ CSelectionBroadcastingList
// ---------------------------------------------------------------------------

CSelectionBroadcastingList::CSelectionBroadcastingList(
	LStream	*inStream)
	:	fAllowSelectionChange(true),
	
		super(inStream)
{
	inStream->ReadData(&fMessage, sizeof(fMessage));
}

// ---------------------------------------------------------------------------
//		¥ ~CSelectionBroadcastingList
// ---------------------------------------------------------------------------

CSelectionBroadcastingList::~CSelectionBroadcastingList()
{
}

// ---------------------------------------------------------------------------
//		¥ SetBroadcastMessage
// ---------------------------------------------------------------------------

void
CSelectionBroadcastingList::SetBroadcastMessage(MessageT inMessage)
{
	fMessage = inMessage;
}

// ---------------------------------------------------------------------------
//		¥ GetBroadcastMessage
// ---------------------------------------------------------------------------

MessageT
CSelectionBroadcastingList::GetBroadcastMessage()
{
	return fMessage;
}

// ---------------------------------------------------------------------------
//		¥ SetValue
// ---------------------------------------------------------------------------

void
CSelectionBroadcastingList::SetValue(Int32 inValue)
{
	if (fAllowSelectionChange)
	{
		super::SetValue(inValue);
		
		BroadcastMessage(fMessage, this);
	}
}

// ---------------------------------------------------------------------------
//		¥ DoNavigationKey
// ---------------------------------------------------------------------------

void
CSelectionBroadcastingList::DoNavigationKey(
	const EventRecord	&inKeyEvent)
{
	int before = NumCellsSelected();
	
	if (fAllowSelectionChange)
	{
		super::DoNavigationKey(inKeyEvent);
		
			// if num selected hasn't changed, then don't send message
		if (NumCellsSelected() != before)
			BroadcastMessage(fMessage, this);
	}
}	

// ---------------------------------------------------------------------------
//		¥ DoTypeSelection
// ---------------------------------------------------------------------------

void
CSelectionBroadcastingList::DoTypeSelection(
	const EventRecord& inKeyEvent)
{	
	if (fAllowSelectionChange)
	{
		super::DoTypeSelection(inKeyEvent);
		
		BroadcastMessage(fMessage, this);
	}
}

// ---------------------------------------------------------------------------
//		¥ SelectOneCell
// ---------------------------------------------------------------------------

void
CSelectionBroadcastingList::SelectOneCell(
	Cell	inCell)
{
	if (fAllowSelectionChange)
	{
		super::SelectOneCell(inCell);
		
		BroadcastMessage(fMessage, this);
	}
}

// ---------------------------------------------------------------------------
//		¥ ClickSelf
// ---------------------------------------------------------------------------

void
CSelectionBroadcastingList::ClickSelf(
	const SMouseDownEvent	&inMouseDown)
{
	int before = NumCellsSelected();

	if (fAllowSelectionChange)
	{
		super::ClickSelf(inMouseDown);
		
			// if num selected hasn't changed, then don't send message
		if (NumCellsSelected() != before)
			BroadcastMessage(fMessage, this);
	}
}

// ---------------------------------------------------------------------------
//		¥ DontAllowSelectionChange
// ---------------------------------------------------------------------------

void
CSelectionBroadcastingList::DontAllowSelectionChange()
{
	fAllowSelectionChange = false;
}

// ---------------------------------------------------------------------------
//		¥ AllowSelectionChange
// ---------------------------------------------------------------------------

void
CSelectionBroadcastingList::AllowSelectionChange()
{
	fAllowSelectionChange = true;
}

// ---------------------------------------------------------------------------
//		¥ NumCellsSelected
// ---------------------------------------------------------------------------

int
CSelectionBroadcastingList::NumCellsSelected()
{
	int		numSelected = 0;
	Cell	currentCell = {0, 0};
	Boolean	hasSelection = ::LGetSelect(true, &currentCell, mMacListH);
	
	if (hasSelection)
	{
		do
		{
			if (::LGetSelect(true, &currentCell, mMacListH))
				numSelected++;
		} while (::LNextCell(true, true, &currentCell, mMacListH));
	}
	
	return numSelected;
}
