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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Name:        ToolbarWindowList.cpp                                   //
//                                                                      //
// Description:	XFE_ToolbarWindowList class implementation.             //
//              The Back/Forward toolbar buttons.                       //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "ToolbarWindowList.h"
#include "RDFUtils.h"

#include "WindowListMenu.h"

#include <Xfe/Cascade.h>

#include "prefapi.h"
#include "xpgetstr.h"			// for XP_GetString()

extern int XFE_UNTITLED;

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarWindowList::XFE_ToolbarWindowList(XFE_Frame *	frame,
											 Widget			parent,
											 HT_Resource	htResource,
											 const String	name) :
	XFE_ToolbarButton(frame,parent,htResource,name),
	m_submenu(NULL)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarWindowList::~XFE_ToolbarWindowList()
{
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Initialize
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarWindowList::initialize()
{
    Widget navigation = createBaseWidget(getParent(),getName());

	setBaseWidget(navigation);

    XtVaGetValues(navigation,XmNsubMenuId,&m_submenu,NULL);

	XP_ASSERT( XfeIsAlive(m_submenu) );
	
	XFE_WindowListMenu::generate(m_widget,NULL,getAncestorFrame());
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Accessors
//
//////////////////////////////////////////////////////////////////////////
Widget
XFE_ToolbarWindowList::getSubmenu()
{
	return m_submenu;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Widget creation interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ Widget
XFE_ToolbarWindowList::createBaseWidget(Widget			parent,
										const String	name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	Widget navigation;

	navigation = XtVaCreateWidget(name,
								  xfeCascadeWidgetClass,
								  parent,
								  NULL);
	return navigation;
}
//////////////////////////////////////////////////////////////////////////
