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
// Name:        ToolbarSeparator.cpp                                    //
//                                                                      //
// Description:	XFE_ToolbarSeparator class implementation.              //
//              A toolbar item separator.                               //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "ToolbarSeparator.h"

#include <Xfe/Xfe.h>
#include <Xm/Separator.h>

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarSeparator::XFE_ToolbarSeparator(XFE_Frame *		frame,
										   Widget			parent,
										   HT_Resource		htResource,
										   const String		name) :
	XFE_ToolbarItem(frame,parent,htResource,name)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarSeparator::~XFE_ToolbarSeparator()
{
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Initialize
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarSeparator::initialize()
{
	Widget separator = createBaseWidget(getParent(),getName());

	setBaseWidget(separator);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Widget creation interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ Widget
XFE_ToolbarSeparator::createBaseWidget(Widget		parent,
									   const String	name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	Widget separator;

	separator = XtVaCreateWidget(name,
								 xmSeparatorWidgetClass,
								 parent,
								 NULL);

	return separator;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Configure
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarSeparator::configure()
{
	XP_ASSERT( isAlive() );

	XtVaSetValues(m_widget,
				  XmNorientation,	XmVERTICAL,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
