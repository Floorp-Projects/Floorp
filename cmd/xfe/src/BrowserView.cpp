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
   BrowserView.cpp -- class definition for the browser view  class
   Created: Radha Kulkarni <radha@netscape.com>, 23-Feb-1998
 */

#include "BrowserView.h"

#include <Xfe/Pane.h>

#if DEBUG_radha
#define D(x) x
#else
#define D(x)
#endif

//////////////////////////////////////////////////////////////////////////
XFE_BrowserView::XFE_BrowserView(XFE_Component *	toplevel_component,
								 Widget				parent,
								 XFE_View *			parent_view,
								 MWContext *		context)
	: XFE_View(toplevel_component, parent_view, context),
	  _htmlView(NULL),
	  _navCenterView(NULL)
{
	Widget pane;

	// Create a horizontal pane to hold the Navigation Center on the left
	// and an HTML View on the right.
	pane = XtVaCreateWidget("browserViewPane", 
							xfePaneWidgetClass,
							parent,
							XmNorientation,		XmHORIZONTAL,
							NULL);
	
	// create the HTML view
	_htmlView = new XFE_HTMLView(toplevel_component, 
								 pane, 
								 this, 
								 context);

	// Place the view to the right (child two)
	XtVaSetValues(_htmlView->getBaseWidget(),
				  XmNpaneChildType,			XmPANE_CHILD_WORK_AREA_TWO,
				  NULL);
	
	// Add _htmlview to the sub-view list of browser view
	addView(_htmlView);

	// show the html view only to begin with
	_htmlView->show();

	// Manage the pane after its children
	XtManageChild(pane);

	// Set the pane as the base widget
	setBaseWidget(pane);
}

#ifdef UNDEF
Boolean XFE_BrowserView::isCommandEnabled(CommandType  cmd,  void * calldata, XFE_CommandInfo * i) 
{


}

Boolean XFE_BrowserView::handlesCommand(CommandType cmd, void *calldata = NULL,
				 XFE_CommandInfo* i = NULL)
{


}

char* XFE_BrowserView::commandToString(CommandType cmd, void *calldata = NULL,
				 XFE_CommandInfo* i = NULL)
{
}

XP_Bool XFE_BrowserView::isCommandSelected(CommandType cmd, void *calldata = NULL,
			     	 XFE_CommandInfo* = NULL)
{
}
#endif

//////////////////////////////////////////////////////////////////////////
void
XFE_BrowserView::showNavCenter()
{
	// Create the nav center view for the first time
	if (!_navCenterView)
	{
		_navCenterView = new XFE_NavCenterView(m_toplevel, 
											   m_widget, 
											   this, 
											   m_contextData);

		// Add _navCenterView to the sub-view list of browser view
		addView(_navCenterView);
	}

	// Show the nav center
	_navCenterView->show();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BrowserView::hideNavCenter()
{
	XP_ASSERT( _navCenterView != NULL );

	if (_navCenterView)
	{
		_navCenterView->hide();
	}
}
//////////////////////////////////////////////////////////////////////////
Boolean
XFE_BrowserView::isNavCenterShown(void) 
{
	return (_navCenterView && 
			_navCenterView->isAlive() &&
			_navCenterView->isShown());
}
//////////////////////////////////////////////////////////////////////////
XFE_BrowserView::~XFE_BrowserView()
{
	// XFE_Frame cleans up views in its dtor() which should take care
	// of cleaning up the base widget, or at least I think that -re.
	//
#ifdef IM_NOT_SURE
	XtDestroyWidget(m_widget);

	m_widget = NULL;
#endif
}
//////////////////////////////////////////////////////////////////////////
XFE_NavCenterView * XFE_BrowserView::getNavCenterView() 
{
	// Callers better know what they are doing
	XP_ASSERT( _htmlView != NULL );

	return _navCenterView;
}
//////////////////////////////////////////////////////////////////////////
XFE_HTMLView * XFE_BrowserView::getHTMLView() 
{
	// Callers better know what they are doing
	XP_ASSERT( _htmlView != NULL );

	return _htmlView;
}
//////////////////////////////////////////////////////////////////////////
