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

#pragma once

#ifndef CRECENTEDITMENUATTACHMENT_H_
#define CRECENTEDITMENUATTACHMENT_H_

#include <LAttachment.h>
#include <LMenu.h>
#include "ntypes.h"

/***********************************************************************************
 * CRecentEditMenuAttachment
 * Processes Recent-Edited files menu commands -- should be attached to application
 * Currently, this menu is only used in the Editor window
 ***********************************************************************************/
class CRecentEditMenuAttachment: public LAttachment
{
public:
	enum { menu_ID = 24, cmd_ID_toSearchFor = 'Rece' };
	
	// ¥¥ constructors
						CRecentEditMenuAttachment();
	// ¥¥Êevents
	virtual void		ExecuteSelf( MessageT inMessage, void* ioParam );

	static LMenu*		GetMenu();
	static void			UpdateMenu();

	static void			RemoveMenus();
	static void			InstallMenus();
	
protected:
	static MWContext*	GetTopWindowContext();

	static LMenu*		sMenu;
};

#endif
