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
/* 
   PopupMenu.cpp -- implementation file for PopupMenu
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#if DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#include "Frame.h"
#include "PopupMenu.h"
#include "DisplayFactory.h"

#include <Xm/RowColumn.h>
#include <Xfe/Xfe.h>

MenuSpec XFE_PopupMenu::title_spec[] = {
  //  { "title", LABEL },
  //MENU_SEPARATOR,
  { NULL }
};

extern "C" 
{
	// Needed to determine the colormap used by the floating shell colormap
	Colormap fe_getColormap(fe_colormap *colormap);
}

XFE_PopupMenu::XFE_PopupMenu(String			name,
							 XFE_Frame *	parent_frame,
							 Widget			parent,
							 MenuSpec *		spec) :
	XFE_Menu(parent_frame,
			 NULL,
			 NULL)
{
	// Create the popup menu
	Widget popup_menu = XFE_PopupMenu::CreatePopupMenu(parent,name,NULL,0);

	setBaseWidget(popup_menu);

	setMenuSpec(spec);

	if (spec)
		createWidgets(spec);
}

static XtPointer
fe_popup_destroy_mappee(Widget widget, XtPointer data)
{
    Widget menu;

    if (XtIsSubclass(widget, xmCascadeButtonGadgetClass)) {
		XtVaGetValues(widget, XmNsubMenuId, &menu, 0);
		if (menu != 0)
			fe_WidgetTreeWalk(menu, fe_popup_destroy_mappee, NULL);
    }
	
    XtDestroyWidget(widget);

    return 0;
}

XFE_PopupMenu::~XFE_PopupMenu()
{
	//
	// we need to destroy the shell parent of our base widget, which
	// happens to be the row column.
	// In case the popup has submenus, walk the tree following
	// cascade/sub-menu linkages.
	// 
	fe_WidgetTreeWalk(XtParent(m_widget), fe_popup_destroy_mappee, NULL);

	m_widget = NULL;
}

void
XFE_PopupMenu::position(XEvent *event)
{
  XP_ASSERT(event->type == ButtonPress);
  
  XmMenuPosition(m_widget, (XButtonPressedEvent*)event);
}

void
XFE_PopupMenu::show()
{
  update();

  XFE_Menu::show();
}

/* static */ Widget
XFE_PopupMenu::CreatePopupMenu(Widget pw,String name,ArgList av,Cardinal ac)
{
	Widget			popup;
	ArgList			new_av;
	Cardinal		new_ac;
	Cardinal		i;

	XP_ASSERT( XfeIsAlive(pw) );
	XP_ASSERT( name != NULL );

	new_ac = ac + 3;

	new_av = (ArgList) XtMalloc(sizeof(Arg) * new_ac);

	for (i = 0; i < ac; i++)
	{
		XtSetArg(new_av[i],av[i].name,av[i].value);
	}

	// Make sure the display factory is running
	fe_startDisplayFactory(FE_GetToplevelWidget());

	fe_colormap *	colormap;
	Colormap		x_colormap;
	Visual *		visual;
	int				depth;

	// Obtain the colormap / depth / and visual 
	colormap	= XFE_DisplayFactory::theFactory()->getSharedColormap();
	x_colormap	= fe_getColormap(colormap);
	depth		= XFE_DisplayFactory::theFactory()->getVisualDepth();
	visual		= XFE_DisplayFactory::theFactory()->getVisual();

	XtSetArg(new_av[ac + 0],XmNcolormap,x_colormap);
	XtSetArg(new_av[ac + 1],XmNvisual,visual);
	XtSetArg(new_av[ac + 2],XmNdepth,depth);

	popup = XmCreatePopupMenu(pw,name,new_av,new_ac);

	XtFree((char *) new_av);

	return popup;
}

// Remove the osfLeft and osfRight translations of the popup's children.
// For bug 71620.  See the bug description for details.  Basically, for
// some strange reason, motif assumes that either a left or a right 
// naviation widget exists for this popup menu and core dumps trying 
// to dereference a NULL widget.

static char _leftRightTranslations[] = "\
:<Key>osfLeft:			\n\
:<Key>osfRight:			";

void
XFE_PopupMenu::removeLeftRightTranslations()
{
	WidgetList	children;
	Cardinal	num_children;
	Cardinal	i;

	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XmIsRowColumn(m_widget) );

	if (!XfeIsAlive(m_widget))
	{
		return;
	}
	
	XfeChildrenGet(m_widget,&children,&num_children);

	if (!children || !num_children)
	{
		return;
	}
	
	for(i = 0; i < num_children; i++)
	{
		if (XmIsPrimitive(children[i]))
		{
			XfeOverrideTranslations(children[i],_leftRightTranslations);
		}
	}
}

void
XFE_PopupMenu::raise()
{
	XP_ASSERT( isAlive() );

	XRaiseWindow(XtDisplay(XtParent(m_widget)),
				 XtWindow(XtParent(m_widget)));
}
