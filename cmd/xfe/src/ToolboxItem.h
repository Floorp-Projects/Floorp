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
/*---------------------------------------*/
/*																		*/
/* Name:		ToolboxItem.h											*/
/* Description:	XFE_ToolboxItem component header.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#ifndef _xfe_toolbox_item_h_
#define _xfe_toolbox_item_h_

#include "Component.h"
#include "Chrome.h"
#include "IconGroup.h"
#include "Menu.h"

class XFE_Toolbox;
class XFE_Logo;

// Toolbar delay is ms
typedef enum
{
    XFE_TOOLBAR_DELAY_SHORT = 0,
    XFE_TOOLBAR_DELAY_LONG = 350
} EToolbarPopupDelay;


typedef struct ToolbarSpec 
{
	const char *toolbarButtonName;
	EChromeTag tag;
	
	IconGroup *iconGroup;
	IconGroup *iconGroup2;
	IconGroup *iconGroup3;
	IconGroup *iconGroup4;

	struct MenuSpec *submenu;

	dynamenuCreateProc generateProc;
	void* generateArg;

	EToolbarPopupDelay popupDelay;
} ToolbarSpec;

/* nice shortcut */
#define TOOLBAR_SEPARATOR	{ " ", SEPARATOR }

//
// An abstract toolbar is something that goes in a toolbox
//
class XFE_ToolboxItem : public XFE_Component
{
public:

	XFE_ToolboxItem(XFE_Component *,XFE_Toolbox *);

	virtual void		update() = 0;
	virtual void		setToolbarSpec(ToolbarSpec *) {};
	virtual void		hideButton(const char*, EChromeTag) {};
	virtual void		showButton(const char*, EChromeTag) {};
    virtual void		toggle();

	// Position index methods
	virtual int			getPosition		();
	virtual void		setPosition		(int);

	// Open methods
	virtual XP_Bool		isOpen			();
	virtual void		setOpen			(XP_Bool open);

	// Showing methods
	virtual void		setShowing		(XP_Bool showing);

	// Logo methods
	virtual void			showLogo			();
	virtual void			hideLogo			();

	virtual XP_Bool			isLogoShown			();
	virtual XFE_Logo *		getLogo				();
	virtual XFE_Toolbox *	getParentToolbox	();

protected:

	virtual void		addDragWidget		(Widget);
	virtual void		configureLogo		();

	XFE_Logo *			m_logo;				// the logo
	XP_Bool				m_showLogo;			// is that logo shown ?
	XFE_Toolbox *		m_parentToolbox;	// Parent must be a toolbox
};

#endif /* _xfe_toolbox_item_h_ */
