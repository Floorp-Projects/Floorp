/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/MenuUtil.h>										*/
/* Description:	Menu/RowColum misc utilities header.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeMenuUtil_h_							/* start MenuUtil.h		*/
#define _XfeMenuUtil_h_

#include <Xm/Xm.h>								/* Motif public defs	*/

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Menus																*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeMenuPositionXY				(Widget			menu,
								 Position		x_root,
								 Position		y_root);
/*----------------------------------------------------------------------*/
extern int
XfeMenuItemPositionIndex		(Widget			item);
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuItemAtPosition			(Widget			menu,
								 int			position);
/*----------------------------------------------------------------------*/
extern Boolean
XfeMenuIsFull					(Widget			menu);
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuFindLastMoreMenu			(Widget			menu,
								 String			more_button_name);
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuGetMoreButton			(Widget			menu,
								 String			more_button_name);
/*----------------------------------------------------------------------*/
extern Widget
XfeCascadeGetSubMenu			(Widget			w);
/*----------------------------------------------------------------------*/
extern unsigned char
XfeMenuType						(Widget			menu);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Option menus															*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeMenuIsOptionMenu				(Widget			menu);
/*----------------------------------------------------------------------*/
extern void
XfeOptionMenuSetItem			(Widget			menu,
								 Cardinal		i);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Menu item functions.													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuItemNextItem				(Widget			item);
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuItemPreviousItem			(Widget			item);

/*----------------------------------------------------------------------*/
/*																		*/
/* Public accent drawing functions.										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeMenuItemDrawAccent			(Widget			item,
								 unsigned char	accent_type,
								 Dimension		offset_left,
								 Dimension		offset_right,
								 Dimension		shadow_thickness,
								 Dimension		accent_thickness);
/*----------------------------------------------------------------------*/
extern void
XfeMenuItemEraseAccent			(Widget			item,
								 unsigned char	accent_type,
								 Dimension		offset_left,
								 Dimension		offset_right,
								 Dimension		shadow_thickness,
								 Dimension		accent_thickness);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Display grabbed access.												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeDisplayIsUserGrabbed			(Widget				w);
/*----------------------------------------------------------------------*/
extern void
XfeDisplaySetUserGrabbed		(Widget				w,
								 Boolean			grabbed);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Destruction															*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeDestroyMenuWidgetTree		(WidgetList		children,
								 int			num_children,
								 Boolean		skip_private_components);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end MenuUtil.h		*/
