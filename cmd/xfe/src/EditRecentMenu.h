/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "structs.h"
#include "xp_core.h"
#include "ntypes.h"
#include <Xm/Xm.h>

#ifndef _xfe_edit_recent_menu_h_
#define _xfe_edit_recent_menu_h_


class XFE_Frame;

// This class can be used with a DYNA_CASCADEBUTTON or DYNA_MENUITEMS.
class XFE_EditRecentMenu
{
public:

	virtual ~XFE_EditRecentMenu();

	// this function occupies the generateCallback slot in a menuspec.
	static void generate(Widget			cascade,
						 XtPointer		data,
						 XFE_Frame *	frame);

private:
	
	XFE_EditRecentMenu(XFE_Frame *	frame,
					   Widget		cascade,
					   XP_Bool    openPage);

	// the toplevel component -- the thing we dispatch our events to.
	XFE_Frame *		_frame;

	// the cascade button we're tied to.
	Widget			_cascade;

	// the row column we're in.
	Widget			_submenu;

	// do we need an "Open Page..." menu item at the top of the submenu?...
	XP_Bool		    _openPage;

	// the number of menu items in the current submenu...
	int32			_itemCount;
	
	// Cascade callbacks
	static void cascadingCB			(Widget, XtPointer, XtPointer);
	static void cascadeDestroyCB	(Widget, XtPointer, XtPointer);

	// Menu item callbacks
	static void itemActivateCB		(Widget, XtPointer, XtPointer);
	static void itemArmCB			(Widget, XtPointer, XtPointer);
	static void itemDisarmCB		(Widget, XtPointer, XtPointer);
	static void itemDestroyCB		(Widget, XtPointer, XtPointer);

	void			cascading			();

	void			itemActivate		(char*	pUrl);
	void			itemArm				(char*	pUrl);
	void			itemDisarm			();

	void			destroyItems		();

	void			fillSubmenu			();

	void			addItem				(int	position,
										 char*  name,
										 char*	pUrl);
};

#endif /* _xfe_edit_recent_menu_h_ */
