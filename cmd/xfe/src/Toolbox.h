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
/* Name:		Toolbox.h												*/
/* Description:	XFE_Toolbox component header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#ifndef _xfe_toolbox_h
#define _xfe_toolbox_h

//#include "xp_core.h"

#include "Component.h"
#include "IconGroup.h"
#include "icons.h"

class XFE_ToolboxItem;

class XFE_Toolbox : public XFE_Component
{
public:  

	Widget getToolBoxWidget		();
	Widget getLogoWidget		();

	XFE_Toolbox(XFE_Component * toplevel_component,Widget parent);

	virtual ~XFE_Toolbox();

	void		addDragDescendant		(Widget);
	void		removeDragDescendant	(Widget);
	void		toggleItemState			(Widget);

	void		setItemPosition			(Widget item,int position);
	void		setItemOpen				(Widget item,XP_Bool open);

	XP_Bool		stateOfItem				(Widget);
	int			positionOfItem			(Widget);
	Widget		tabOfItem				(Widget item,XP_Bool opened);

	// Item methods
	Cardinal				getNumItems					();
	WidgetList				getItems					();

	XFE_ToolboxItem *		getItemAtIndex				(Cardinal index);
	XFE_ToolboxItem *		firstOpenItem				();
	XFE_ToolboxItem *		firstManagedItem			();
	XFE_ToolboxItem *		firstOpenAndManagedItem		();

	XP_Bool isNeeded			();

	// Invoked when one of the toolbars is closed
	static const char *toolbarClose;

	// Invoked when one of the toolbars is opened
	static const char *toolbarOpen;

	// Invoked when one of the toolbars is snapped
	static const char *toolbarSnap;

	// Invoked when one of the toolbars is swapped
	static const char *toolbarSwap;

protected:

	Widget			m_toolbox;

private:

	void	createMain		(Widget parent);

	static void			closeCallback		(Widget,XtPointer,XtPointer);
	static void			openCallback		(Widget,XtPointer,XtPointer);
	static void			newItemCallback		(Widget,XtPointer,XtPointer);
	static void			snapCallback		(Widget,XtPointer,XtPointer);
	static void			swapCallback		(Widget,XtPointer,XtPointer);
};

#endif /* _xfe_toolbox_h */
