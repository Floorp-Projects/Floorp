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
/* Name:		BackForwardMenu.h										*/
/* Description:	XFE_BackForwardMenu component header file.				*/
/*				These are the menu items that appear at the end of the	*/
/*																		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/


/* Date:		Sun Mar  2 01:34:13 PST 1997							*/
/*																		*/
/*----------------------------------------------------------------------*/

#include "structs.h"
#include "xp_core.h"
#include "ntypes.h"
#include <Xm/Xm.h>

#ifndef _xfe_back_forward_menu_h_
#define _xfe_back_forward_menu_h_


class XFE_Frame;

// This class can be used with a DYNA_CASCADEBUTTON or DYNA_MENUITEMS.
class XFE_BackForwardMenu
{
public:

	virtual ~XFE_BackForwardMenu();

	// this function occupies the generateCallback slot in a menuspec.
	static void generate(Widget			cascade,
						 XtPointer		data,
						 XFE_Frame *	frame);

private:
	
	XFE_BackForwardMenu(XFE_Frame *	frame,
						Widget		cascade,
						int			forward);

	// the toplevel component -- the thing we dispatch our events to.
	XFE_Frame *		_frame;

	// the cascade button we're tied to.
	Widget			_cascade;

	// the row column we're in.
	Widget			_submenu;
	
	// True = Forward, False = Backward
	XP_Bool			_forward;
	
	// Cascade callbacks
	static void cascadingCB			(Widget, XtPointer, XtPointer);
	static void cascadeDestroyCB	(Widget, XtPointer, XtPointer);

	// Menu item callbacks
	static void itemActivateCB		(Widget, XtPointer, XtPointer);
	static void itemArmCB			(Widget, XtPointer, XtPointer);
	static void itemDisarmCB		(Widget, XtPointer, XtPointer);
	static void itemDestroyCB		(Widget, XtPointer, XtPointer);

	void			cascading			();

	void			itemActivate		(History_entry *	entry);
	void			itemArm				(History_entry *	entry);
	void			itemDisarm			();

	void			destroyItems		();

	void			addItems			(XP_List *			list,
										 XP_Bool			forward);

	void			fillSubmenu			(XP_Bool forward);

	void			addItem				(int				position,
										 History_entry *	entry);
};

#endif /* _xfe_back_forward_menu_h_ */
