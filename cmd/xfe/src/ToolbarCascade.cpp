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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Name:        ToolbarCascade.cpp                                      //
//                                                                      //
// Description:	XFE_ToolbarCascade class implementation.                //
//              A cascading toolbar push button.                        //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "ToolbarCascade.h"
#include "RDFUtils.h"

#include <Xfe/Cascade.h>

#include "prefapi.h"

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarCascade::XFE_ToolbarCascade(XFE_Frame *		frame,
									   Widget			parent,
									   HT_Resource		htResource,
									   const String		name) :
	XFE_ToolbarButton(frame,parent,htResource,name),
	m_submenu(NULL)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarCascade::~XFE_ToolbarCascade()
{
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Initialize
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarCascade::initialize()
{
    Widget cascade = createBaseWidget(getParent(),getName());

	setBaseWidget(cascade);

    XtVaGetValues(cascade,XmNsubMenuId,&m_submenu,NULL);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Accessors
//
//////////////////////////////////////////////////////////////////////////
Widget
XFE_ToolbarCascade::getSubmenu()
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
XFE_ToolbarCascade::createBaseWidget(Widget			parent,
									 const String	name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	Widget cascade;

	cascade = XtVaCreateWidget(name,
							  xfeCascadeWidgetClass,
							  parent,
							  NULL);
	return cascade;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Configure
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarCascade::configure()
{
	XP_ASSERT( isAlive() );

	// Chain
	XFE_ToolbarButton::configure();
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// addCallbacks
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarCascade::addCallbacks()
{
	XP_ASSERT( isAlive() );

	// Chain
	XFE_ToolbarButton::addCallbacks();

	// Add cascading callback
    XtAddCallback(m_widget,
				  XmNcascadingCallback,
				  XFE_ToolbarCascade::cascadingCB,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Button callback interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarCascade::cascading()
{
	printf("cascading(%s)\n",XtName(m_widget));
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private callbacks
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarCascade::cascadingCB(Widget		/* w */,
								XtPointer	clientData,
								XtPointer	/* callData */)
{
	XFE_ToolbarCascade * cascade = (XFE_ToolbarCascade *) clientData;

	XP_ASSERT( cascade != NULL );

	cascade->cascading();
}
//////////////////////////////////////////////////////////////////////////
