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
//	CWindowMenu.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LMenu.h>
#include <LListener.h>
#include <LArray.h>

class CMediatedWindow;

//======================================
class CWindowMenu : public LMenu, public LListener
//======================================
{
	// API:
	public:
						CWindowMenu(ResIDT inMenuID);
		virtual			~CWindowMenu();
	
		static void		Update();
		static Boolean	ObeyWindowCommand(CommandT inCommand);

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);
		void			RemoveCommand(CommandT inCommand);

	protected:	
	
		void			UpdateSelf();
		Boolean			ObeyWindowCommand(Int16 inMenuID, Int16 inMenuItem);
		void			AddWindow(const CMediatedWindow* inWindow);
		void			RemoveWindow(const CMediatedWindow* inWindow);

		void			SetMenuDirty(Boolean inDirty);
		Boolean			IsMenuDirty() const;
	// Data:
	public:
		static CWindowMenu* sWindowMenu;

	protected:
				
		Int16			mLastNonDynamicItem;
		Boolean			mDirty;
		
		LArray			mWindowList;

};

inline void CWindowMenu::SetMenuDirty(Boolean inDirty)
	{	mDirty = inDirty;	}
	
inline Boolean CWindowMenu::IsMenuDirty() const
	{	return mDirty;		}
