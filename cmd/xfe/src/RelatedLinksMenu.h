/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		RelatedLinksMenu.h										*/
/* Description:	XFE_RelatedLinksMenu component header file.				*/
/*																		*/
/* Created:		Ramiro Estrugo <ramiro@netscape.com>					*/
/* Date:		Fri Jan 30 01:13:11 PST 1998							*/
/*																		*/
/*----------------------------------------------------------------------*/

//#include "NotificationCenter.h"
#include "Frame.h"
#include "ntypes.h"
#include "../../lib/xp/rl.h"		/* For related links */

#ifndef _xfe_related_links_menu_h_
#define _xfe_related_links_menu_h_

// This class can be used with a DYNA_CASCADEBUTTON or
// DYNA_MENUITEMS.

class XFE_RelatedLinksMenu// : public XFE_NotificationCenter
{
public:

	virtual ~XFE_RelatedLinksMenu();

	// this function occupies the generateCallrelated slot in a menuspec.
	static void generate(Widget			cascade,
						 XtPointer		data,
						 XFE_Frame *	frame);

	void		setRelatedLinksAddress		(char * address);

private:
	
	XFE_RelatedLinksMenu(XFE_Frame *	frame,
						 Widget			cascade);

	// the toplevel component -- the thing we dispatch our events to.
	XFE_Frame *		_frame;

	// the cascade button we're tied to.
	Widget			_cascade;

	// the row column we're in.
	Widget			_submenu;

	RL_Window		_rl_window;

	// Cascade callrelateds
	static void cascadingCB			(Widget, XtPointer, XtPointer);
	static void cascadeDestroyCB	(Widget, XtPointer, XtPointer);

	// Menu item callrelateds
	static void itemActivateCB		(Widget, XtPointer, XtPointer);
	static void itemArmCB			(Widget, XtPointer, XtPointer);
	static void itemDisarmCB		(Widget, XtPointer, XtPointer);
	static void itemDestroyCB		(Widget, XtPointer, XtPointer);

	void			cascading			();

	void			itemActivate		(RL_Item rl_item,XP_Bool new_window);
	void			itemArm				(RL_Item rl_item);
	void			itemDisarm			();

	void			destroyItems		();

	void			addItems			(Widget submenu,RL_Window rl_window);

	void			addItem				(Widget submenu,RL_Item rl_item);
	void			addContainer		(Widget submenu,RL_Item rl_item);
	void			addSeparator		(Widget submenu,RL_Item rl_item);
	void			addLink				(Widget submenu,RL_Item rl_item,XP_Bool new_window);
};

#endif /* _xfe_related_links_menu_h_ */
