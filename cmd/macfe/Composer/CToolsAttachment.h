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

#ifndef CTOOLSATTACHMENT_H_
#define CTOOLSATTACHMENT_H_

#include <LAttachment.h>
#include "ntypes.h"

class LMenu;
class LArray;

/***********************************************************************************
 * CToolsAttachment
 * Processes Tools menu commands -- should be attached to application
 * Currently, this menu is only used in the Editor window
 ***********************************************************************************/
class CToolsAttachment: public LAttachment
{
public:
	// ¥¥ constructors
						CToolsAttachment();
	// ¥¥Êevents
	virtual void		ExecuteSelf( MessageT inMessage, void* ioParam );

	static LMenu*		GetMenu();
	static void			InvalidateMenu() { sInvalidMenu = true; }
	static void			UpdateMenu();

	static void			RemoveMenus();
	static void			InstallMenus();
	
protected:
	static void			FillMenu(
							int32 CategoryIndex,
							LMenu* newMenu,
							int& commandNum,
							int whichItem );
							
	static MWContext*	GetTopWindowContext();

	static LMenu*		sMenu;
	static Boolean		sInvalidMenu;
	static LArray		sMenusList;
};

#endif
