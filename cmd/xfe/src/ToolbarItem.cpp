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
// Name:        ToolbarItem.cpp                                         //
//                                                                      //
// Description:	XFE_ToolbarItem class implementation.                   //
//              Superclass for anything that goes in a toolbar.         //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "ToolbarItem.h"

#include <Xfe/ToolTip.h>

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarItem::XFE_ToolbarItem(XFE_Frame *	frame,
								 Widget			parent,
                                 HT_Resource	htResource,
								 const String	name) :
	XFE_Component(frame),
	m_name(NULL),
	m_parent(NULL),
    m_ancestorFrame(frame),
	m_htResource(htResource)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( frame != NULL );
	XP_ASSERT( name != NULL );

	m_name = (String) XtNewString((char *) name);
	m_parent = parent;
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarItem::~XFE_ToolbarItem()
{
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Accessors
//
//////////////////////////////////////////////////////////////////////////
const String
XFE_ToolbarItem::getName()
{
	return m_name;
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_ToolbarItem::getParent()
{
	return m_parent;
}
//////////////////////////////////////////////////////////////////////////
XFE_Frame *
XFE_ToolbarItem::getAncestorFrame()
{
	XP_ASSERT( m_ancestorFrame != NULL );

	return m_ancestorFrame;
}
//////////////////////////////////////////////////////////////////////////
MWContext *
XFE_ToolbarItem::getAncestorContext()
{
	XP_ASSERT( getAncestorFrame()->getContext() != NULL );

	return getAncestorFrame()->getContext();
}
//////////////////////////////////////////////////////////////////////////
HT_Resource
XFE_ToolbarItem::getHtResource()
{
    return m_htResource;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarItem::setBaseWidget(Widget w)
{
//	printf("XFE_ToolbarItem::setBaseWidget(%s)\n",XtName(w));

	XP_ASSERT( XfeIsAlive(w) );

	XFE_Component::setBaseWidget(w);

	XP_ASSERT( isAlive() );
	
	// Add destroy callback to collect memory allocated for m_name
	XtAddCallback(m_widget,
				  XmNdestroyCallback,
				  XFE_ToolbarItem::freeNameCB,
				  (XtPointer) m_name);

	// Add tooltip support
	addToolTipSupport();

	// Configure the item
    configure();

    // Install destroy handler (magic garbage collection for the item)
    installDestroyHandler();
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Sensitive interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarItem::setSensitive(Boolean state)
{
	XP_ASSERT( isAlive() );

	XtSetSensitive(m_widget,state);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ Boolean
XFE_ToolbarItem::isSensitive()
{
	XP_ASSERT( isAlive() );
	
	return XtIsSensitive(m_widget);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Tool tip support
//
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarItem::addToolTipSupport()
{
	XP_ASSERT( isAlive() );

	// Add tip string support
	XfeTipStringAdd(m_widget);

	XfeTipStringSetObtainCallback(m_widget,
								  XFE_ToolbarItem::tipStringObtainCB,
								  (XtPointer) this);

	// Add doc string support
	XfeDocStringAdd(m_widget);

	XfeDocStringSetObtainCallback(m_widget,
								  XFE_ToolbarItem::docStringObtainCB,
								  (XtPointer) this);
    
	XfeDocStringSetCallback(m_widget,
							XFE_ToolbarItem::docStringCB,
							(XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
XmString
XFE_ToolbarItem::getTipStringFromAppDefaults()
{
	XP_ASSERT( isAlive() );

	return XfeTipStringGetFromAppDefaults(m_widget);
}
//////////////////////////////////////////////////////////////////////////
XmString
XFE_ToolbarItem::getDocStringFromAppDefaults()
{
	XP_ASSERT( isAlive() );

	return XfeDocStringGetFromAppDefaults(m_widget);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private callbacks
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarItem::tipStringObtainCB(Widget		/* w */,
								   XtPointer	clientData,
								   XmString *	stringReturn,
								   Boolean *	needToFreeString)
{
	XFE_ToolbarItem * item = (XFE_ToolbarItem *) clientData;

	XP_ASSERT( item != NULL );

	item->tipStringObtain(stringReturn,needToFreeString);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarItem::docStringObtainCB(Widget		/* w */,
								   XtPointer	clientData,
								   XmString *	stringReturn,
								   Boolean *	needToFreeString)
{
	XFE_ToolbarItem * item = (XFE_ToolbarItem *) clientData;

	XP_ASSERT( item != NULL );

	item->docStringObtain(stringReturn,needToFreeString);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarItem::docStringCB(Widget			/* w */,
							 XtPointer		clientData,
							 unsigned char	reason,
							 XmString		string)
{
	XFE_ToolbarItem * item = (XFE_ToolbarItem *) clientData;

	XP_ASSERT( item != NULL );

	if (reason == XfeDOC_STRING_SET)
	{
 		item->docStringSet(string);
	}
	else if (reason == XfeDOC_STRING_CLEAR)
	{
 		item->docStringClear(string);
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarItem::freeNameCB(Widget			/* w */,
							XtPointer		clientData,
							XtPointer		/* callData */)
{
	char * name = (char *) clientData;

	XP_ASSERT( name != NULL );

	XtFree(name);
}
//////////////////////////////////////////////////////////////////////////
