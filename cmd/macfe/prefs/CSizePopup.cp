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

#include "CSizePopup.h"

#include "PascalString.h"
#include "uerrmgr.h"
#include "resgui.h"
#include "macutil.h"
#include "UModalDialogs.h"


//-----------------------------------
CSizePopup::CSizePopup(LStream* inStream)
//-----------------------------------
:	mFontNumber(0)
,	LPopupButton(inStream)
{
}

//-----------------------------------
Int32 CSizePopup::GetMenuSize() const
//-----------------------------------
{
	Int32		size = 0;
	MenuHandle	menuH = const_cast<CSizePopup*>(this)->GetMacMenuH();
	
	if (menuH)
	{
		size = ::CountMItems(menuH);
	}
	
	return size;
}

//-----------------------------------
void CSizePopup::SetUpCurrentMenuItem(MenuHandle	inMenuH, Int16 inCurrentItem )
//-----------------------------------
{
	
	// ¥ If the current item has changed then make it so, this
	// also involves removing the mark from any old item
	if (inMenuH)
	{
		CStr255		sizeString;
		CStr255		itemString;
		Handle		ellipsisHandle = nil;
		char		ellipsisChar;	
		short		menuSize = ::CountMItems(inMenuH);
		
		ThrowIfNil_(ellipsisHandle = ::GetResource('elps', 1000));

		ellipsisChar = **ellipsisHandle;
		::ReleaseResource(ellipsisHandle);

		if (inCurrentItem == ::CountMItems(inMenuH))
		{
			itemString = ::GetCString(OTHER_FONT_SIZE);
			::StringParamText(itemString, (SInt32) GetFontSize(), 0, 0, 0);
		}
		else
		{
			itemString = ::GetCString(OTHER_RESID);
			itemString += ellipsisChar;
		}
		
		::SetMenuItemText(inMenuH, menuSize, itemString);

		// ¥ Get the current value
		Int16 oldItem = GetValue();
		
		// ¥ Remove the current mark
		::SetItemMark(inMenuH, oldItem, 0);
		
		// ¥ Always make sure item is marked
		::SetItemMark(inMenuH, inCurrentItem,  checkMark);
	}
	
}

//-----------------------------------
Int32 CSizePopup::GetFontSizeFromMenuItem(Int32 inMenuItem) const
//-----------------------------------
{
	Str255		sizeString;
	Int32		fontSize = 0;

	::GetMenuItemText(const_cast<CSizePopup*>(this)->GetMacMenuH(), inMenuItem, sizeString);
	
	myStringToNum(sizeString, &fontSize);
	return fontSize;
}

//-----------------------------------
void CSizePopup::SetFontSize(Int32 inFontSize)
//-----------------------------------
{
	mFontSize = inFontSize;
	if (inFontSize)
	{
		MenuHandle sizeMenu = GetMacMenuH();
		short menuSize = ::CountMItems(sizeMenu);
		Boolean isOther = true;
		for (int i = 1; i <= menuSize; ++i)
		{
			CStr255 sizeString;
			::GetMenuItemText(sizeMenu, i, sizeString);
			Int32 fontSize = 0;
			myStringToNum(sizeString, &fontSize);
			if (fontSize == inFontSize)
			{
				SetValue(i);
				if (i != menuSize)
					isOther = false;
				break;
			}
		}		
		if (isOther)
			SetValue(menuSize);
	}
}


//-----------------------------------
void CSizePopup::SetValue(Int32 inValue)
//-----------------------------------
{
	// ¥ We intentionally do not guard against setting the value to the 
	//		same value the popup current has. This is so that the other
	//		size stuff works correctly.
	MenuHandle menuH = GetMacMenuH();
	if (inValue < mMinValue) {		// Enforce min/max range
		inValue = mMinValue;
	} else if (inValue > mMaxValue) {
		inValue = mMaxValue;
	}

	// handle the "other" command if the user picked the last element of the menu
	if ( inValue == ::CountMItems( menuH ))
	{
		StDialogHandler handler(Wind_OtherSizeDialog, nil);
		LWindow* dialog = handler.GetDialog();
		LEditField	*sizeField = dynamic_cast<LEditField*>(dialog->FindPaneByID('SIZE'));
		Assert_(sizeField);
		sizeField->SetValue(GetFontSize());
		sizeField->SelectAll();

		// Run the dialog
		MessageT message = msg_Nothing;
		do
		{
			message = handler.DoDialog();
		} while (message == msg_Nothing);

		if (message == 1501)
			mFontSize = sizeField->GetValue();
	}
	else
		mFontSize = GetFontSizeFromMenuItem(inValue);

	// ¥ Get the current item setup in the menu
	if ( menuH )
		SetUpCurrentMenuItem( menuH, inValue );

	mValue = inValue;			//   Store new value
	BroadcastValueMessage();	//   Inform Listeners of value change

	// ¥ Now we need to get the popup redrawn so that the change
	// will be seen
	Draw( nil );
	
}	//	CSizePopup::SetValue


//-----------------------------------
void CSizePopup::MarkRealFontSizes(LPopupButton *fontPopup)
//-----------------------------------
{
	CStr255	fontName;
	::GetMenuItemText(	fontPopup->GetMacMenuH(),
						fontPopup->GetValue(),
						fontName);
	GetFNum(fontName, &mFontNumber);
	MarkRealFontSizes(mFontNumber);
}

//-----------------------------------
void CSizePopup::MarkRealFontSizes(short fontNum)
//-----------------------------------
{
	Str255			itemString;
	MenuHandle		sizeMenu;
	short			menuSize;
	
	sizeMenu = GetMacMenuH();
	menuSize = CountMItems(sizeMenu);
	
	for (short menuItem = 1; menuItem <= menuSize; ++menuItem)
	{
		Int32	fontSize;
		::GetMenuItemText(sizeMenu, menuItem, itemString);
		fontSize = 0;
		myStringToNum(itemString, &fontSize);

		Style	theSyle;

		if (fontSize && RealFont(fontNum, fontSize))
		{
			theSyle = outline;
		}
		else
		{
			theSyle = normal;
		}
		::SetItemStyle(	sizeMenu,
						menuItem,
						theSyle);
	}
}
