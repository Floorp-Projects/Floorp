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

#ifndef CFONTMENUATTACHMENT_H_
#define CFONTMENUATTACHMENT_H_

#include <LAttachment.h>
#include "CPatternButtonPopupText.h"
#include "ntypes.h"	// MWContext


class LMenu;

/***********************************************************************************
 * CFontMenuAttachment
 * Processes Font menu commands -- should be attached to application
 * Currently, this menu is only used in the Editor and mail compose window
 ***********************************************************************************/
class CFontMenuAttachment: public LAttachment
{
public:
	enum { menu_ID = 13, PERM_FONT_ITEMS = 3, cmd_ID_toSearchFor = 'FONT' };

	// ¥¥ constructors
						CFontMenuAttachment();
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



class CFontMenuPopup : 	public CPatternButtonPopupText
{
	public:
		enum { class_ID = 'Fpop' };
		
		static	void*	CreateCFontMenuPopupStream( LStream *inStream ) {return( new CFontMenuPopup (inStream ));};
						CFontMenuPopup( LStream *inStream );		//	¥ Constructor
						~CFontMenuPopup();							//	¥ Destructor

						// ¥ drawing
		void			FinishCreateSelf(void);
};

#endif
