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



#if DEBUG_slamm
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

XFE_PopupMenuBase::XFE_PopupMenuBase(String name, Widget parent)
{
	// Create the popup menu
	m_popup_menu = XFE_PopupMenuBase::CreatePopupMenu(parent,name,NULL,0);
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

XFE_PopupMenuBase::~XFE_PopupMenuBase()
{
	//
	// we need to destroy the shell parent of our base widget, which
	// happens to be the row column.
	// In case the popup has submenus, walk the tree following
	// cascade/sub-menu linkages.
	// 
	fe_WidgetTreeWalk(XtParent(m_popup_menu), fe_popup_destroy_mappee, NULL);
}

void
XFE_PopupMenuBase::position(XEvent *event)
{
  XP_ASSERT(event->type == ButtonPress);
  XP_ASSERT( XfeIsAlive(m_popup_menu) );
  
  XmMenuPosition(m_popup_menu, (XButtonPressedEvent*)event);
}

/* static */ Widget
XFE_PopupMenuBase::CreatePopupMenu(Widget pw,String name,ArgList av,Cardinal ac)
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
XFE_PopupMenuBase::removeLeftRightTranslations()
{
	WidgetList	children;
	Cardinal	num_children;
	Cardinal	i;

	XP_ASSERT( XfeIsAlive(m_popup_menu) );
	XP_ASSERT( XmIsRowColumn(m_popup_menu) );

	if (!XfeIsAlive(m_popup_menu))
	{
		return;
	}
	
	XfeChildrenGet(m_popup_menu,&children,&num_children);

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
XFE_PopupMenuBase::raise()
{
	XP_ASSERT( XfeIsAlive(m_popup_menu) );

	XRaiseWindow(XtDisplay(XtParent(m_popup_menu)),
				 XtWindow(XtParent(m_popup_menu)));
}

//////////////////////////////////////////////////////////////////////
XFE_PopupMenu::XFE_PopupMenu(String			name,
							 XFE_Frame *	parent_frame,
							 Widget			parent,
							 MenuSpec *		spec)
    : XFE_Menu(parent_frame,NULL,NULL), XFE_PopupMenuBase(name,parent)
{
    setBaseWidget(m_popup_menu);
	setMenuSpec(spec);

	if (spec)
		createWidgets(spec);
}
#if 0
// Not sure if this is needed
XFE_PopupMenu::~XFE_PopupMenu()
{
	m_widget = NULL;
}
#endif
//////////////////////////////////////////////////////////////////////

XFE_SimplePopupMenu::XFE_SimplePopupMenu(String name, Widget parent)
	: XFE_PopupMenuBase(name,parent)
{
    ; // Nothing to do.
}

void
XFE_SimplePopupMenu::show()
{
      XtManageChild(m_popup_menu);
}

void 
XFE_SimplePopupMenu::addSeparator()
{
    Widget sep = XtCreateWidget("separator",
                                xmSeparatorGadgetClass,
                                m_popup_menu,
                                NULL, 0);

    XtManageChild(sep);
}

void 
XFE_SimplePopupMenu::addPushButton(String name, void *userData, Boolean isSensitive)
{
    Widget button = XtCreateWidget("push",
                                   xmPushButtonWidgetClass,
                                   m_popup_menu,
                                   NULL, 0);
				
    XmString str;
    str = XmStringCreate(name, XmFONTLIST_DEFAULT_TAG);

    XtVaSetValues(button,
                  XmNuserData, (XtPointer)userData,
                  XmNsensitive, isSensitive,
                  XmNlabelString, str,
                  NULL);

    XtAddCallback(button, XmNactivateCallback, pushb_activate_cb, this);

    XtManageChild(button);
}

void
XFE_SimplePopupMenu::pushb_activate_cb(Widget w,
                                       XtPointer clientData, XtPointer callData)
{
  XFE_SimplePopupMenu* obj = (XFE_SimplePopupMenu*)clientData;
  //XmPushButtonCallbackStruct* cd = (XmPushButtonCallbackStruct *)callData;

  XtPointer userData;
  XtVaGetValues(w,
                XmNuserData, &userData,
                NULL);

  obj->PushButtonActivate(w,userData);
}

void
XFE_SimplePopupMenu::PushButtonActivate(Widget w, XtPointer userData)
{
    ; // Do nothing for now.
}
