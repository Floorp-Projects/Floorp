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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CWindowMenu.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CWindowMenu.h"
#include "CWindowMediator.h"
#include "UMenuUtils.h"
//#include "UAEGizmos.h"

//#include <LCommander.h>
#include <LArrayIterator.h>
#include <UAppleEventsMgr.h>

CWindowMenu* CWindowMenu::sWindowMenu = NULL;

//-----------------------------------
CWindowMenu::CWindowMenu(ResIDT inMenuID)
//-----------------------------------
	:	LMenu(inMenuID)
	,	mWindowList(sizeof(const CMediatedWindow*))
	,	mDirty(true)
{
	MenuHandle macMenu = GetMacMenuH();
	
	if (macMenu)
		UMenuUtils::ConvertToIconMenu(macMenu, 15550);
	
	mLastNonDynamicItem = ::CountMenuItems(macMenu);
	sWindowMenu = this;
}

//-----------------------------------
CWindowMenu::~CWindowMenu()
//-----------------------------------
{
	sWindowMenu = NULL;
}

//-----------------------------------
void CWindowMenu::ListenToMessage(MessageT inMessage, void *ioParam)
//-----------------------------------
{
	switch (inMessage)
		{
		case msg_WindowCreated:
			{
			const CMediatedWindow* window = (CMediatedWindow*)ioParam;
			AddWindow(window);
			}
			break;
			
		case msg_WindowDisposed:
			{
			const CMediatedWindow* window = (CMediatedWindow*)ioParam;
			RemoveWindow(window);
			}
			break;
					
		case msg_WindowDescriptorChanged:
			{
			LCommander::SetUpdateCommandStatus(true);
			SetMenuDirty(true);
			}
			break;
		}
}

//-----------------------------------
void CWindowMenu::AddWindow(const CMediatedWindow* inWindow)
//-----------------------------------
{
	// Check if a window is already there (needed now that we call remove on
	// both hide/dispose and add on both show/add)
	ArrayIndexT	index = mWindowList.FetchIndexOf(&inWindow);
	if (index == LArray::index_Bad)
	{
		mWindowList.InsertItemsAt(1, LArray::index_Last, &inWindow);
		LCommander::SetUpdateCommandStatus(true);
		SetMenuDirty(true);
	}
}

//-----------------------------------
void CWindowMenu::RemoveWindow(const CMediatedWindow* inWindow)
//-----------------------------------
{
	ArrayIndexT	index = mWindowList.FetchIndexOf(&inWindow);
	if (index != LArray::index_Bad)
	{
		mWindowList.Remove(&inWindow);
		LCommander::SetUpdateCommandStatus(true);
		SetMenuDirty(true);
	}
}

//-----------------------------------
void CWindowMenu::Update()
//----------------------------------
{
	if (!sWindowMenu || !sWindowMenu->IsMenuDirty())
		return;
	sWindowMenu->UpdateSelf();
}

//-----------------------------------
void CWindowMenu::UpdateSelf()
//----------------------------------
{
	if (!sWindowMenu || !sWindowMenu->IsMenuDirty())
		return;
		
	MenuHandle theMacMenu = GetMacMenuH();
	Assert_(theMacMenu != NULL);

	UMenuUtils::PurgeMenuItems(theMacMenu, mLastNonDynamicItem);

	TString<Str255> windowName;
	CMediatedWindow* window;
	Boolean	bFirstTime = true;

	LArrayIterator theIterator(mWindowList, LArrayIterator::from_Start);
	while (theIterator.Next(&window))
	{

		if (bFirstTime)
		{
			UMenuUtils::AppendSeparator(theMacMenu);
			bFirstTime = false;
		}

//		if (window->GetWindowType() == WindowType_Browser)
		{
			window->GetDescriptor(windowName);
			Int16 theAppendIndex = UMenuUtils::AppendMenuItem(theMacMenu, windowName);
		}
	}

	SetMenuDirty(false);
}

//-----------------------------------
Boolean CWindowMenu::ObeyWindowCommand(CommandT inCommand)
//-----------------------------------
{
	if (!sWindowMenu)
		return false;
	ResIDT menuID;
	Int16 menuItem;
	if (LCommander::IsSyntheticCommand(inCommand, menuID, menuItem))
		return sWindowMenu->ObeyWindowCommand(menuID, menuItem);
	return false;
}

//-----------------------------------
Boolean CWindowMenu::ObeyWindowCommand(Int16 inMenuID, Int16 inMenuItem)
//-----------------------------------
{
	if (inMenuID != GetMenuID())
		return false;
	inMenuItem -= (mLastNonDynamicItem + 1); // 1 for the separator
	if (mLastNonDynamicItem <= 0)
		return false;
	const CMediatedWindow* window = NULL;
	if (!mWindowList.FetchItemAt(inMenuItem, &window) || !window)
		return false;
	//AppleEvent theSelectEvent;
	//LAEStream theEventStream(kAEMiscStandards, kAESelect);
	//StAEDescriptor windowDesc;
	//window->MakeSpecifier(windowDesc.mDesc);		
	//theEventStream.WriteKeyDesc(keyDirectObject, windowDesc);
	//theEventStream.Close(&theSelectEvent);
	//UAppleEventsMgr::SendAppleEvent(theSelectEvent);
	return true;
}

// called from CFrontApp to remove commands from the Window menu;
// updates the item count if necessary.
void CWindowMenu::RemoveCommand(CommandT inCommand)
{
	if (inCommand > 0) {
		LMenu::RemoveCommand(inCommand);
		mLastNonDynamicItem--;
	}
}

