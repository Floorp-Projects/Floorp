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
// Name:        ToolbarNavigation.cpp                                   //
//                                                                      //
// Description:	XFE_ToolbarNavigation class implementation.             //
//              The Back/Forward toolbar buttons.                       //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "ToolbarNavigation.h"
#include "RDFUtils.h"

#include "BackForwardMenu.h"

#include <Xfe/Cascade.h>

#include "prefapi.h"
#include "xpgetstr.h"			// for XP_GetString()

extern int XFE_UNTITLED;

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarNavigation::XFE_ToolbarNavigation(XFE_Frame *	frame,
											 Widget			parent,
											 HT_Resource	htResource,
											 const String	name,
											 int			forward) :
	XFE_ToolbarButton(frame,parent,htResource,name),
	m_submenu(NULL),
	m_forward(forward)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarNavigation::~XFE_ToolbarNavigation()
{
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Initialize
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarNavigation::initialize()
{
    Widget navigation = createBaseWidget(getParent(),getName());

	setBaseWidget(navigation);

    XtVaGetValues(navigation,XmNsubMenuId,&m_submenu,NULL);

	XP_ASSERT( XfeIsAlive(m_submenu) );
	
	XFE_BackForwardMenu::generate(m_widget,
								  (XtPointer) m_forward,
								  getAncestorFrame());
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Accessors
//
//////////////////////////////////////////////////////////////////////////
Widget
XFE_ToolbarNavigation::getSubmenu()
{
	return m_submenu;
}
//////////////////////////////////////////////////////////////////////////
int
XFE_ToolbarNavigation::getForward()
{
	return m_forward;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Widget creation interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ Widget
XFE_ToolbarNavigation::createBaseWidget(Widget			parent,
										const String	name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	Widget navigation;

	navigation = XtVaCreateWidget(name,
								  xfeCascadeWidgetClass,
								  parent,
                                  XmNinstancePointer,	this,
								  NULL);
	return navigation;
}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//
// ToolTip interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarNavigation::tipStringObtain(XmString *	stringReturn,
									   Boolean *	needToFreeString)
{
	entryStringObtain(stringReturn,needToFreeString);

	if (*stringReturn == NULL)
	{
		XFE_ToolbarItem::tipStringObtain(stringReturn,needToFreeString);
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarNavigation::docStringObtain(XmString *	stringReturn,
									   Boolean *	needToFreeString)
{
	entryStringObtain(stringReturn,needToFreeString);

	if (*stringReturn == NULL)
	{
		XFE_ToolbarItem::docStringObtain(stringReturn,needToFreeString);
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarNavigation::entryStringObtain(XmString *	stringReturn,
										 Boolean *	needToFreeString)
{
	MWContext * context = getAncestorContext();

	XP_ASSERT( context != NULL );

	// Get the session history list
	XP_List* list = SHIST_GetList(context);

	XP_ASSERT( list != NULL );

	if (list != NULL)
	{
		// Get the pointer to the current history entry
		History_entry * current_entry = context->hist.cur_doc_ptr;

		XP_List * current = XP_ListFindObject(list, current_entry);

		if (current != NULL)
		{
			// Find the target node
			XP_List * target = m_forward ? current->next : current->prev;
			
			if (target != NULL)
			{
				// Find the target entry
				History_entry * entry = 
					(History_entry *) (target ? target->object : NULL);

				// Determine the label
				char * label = NULL;
				
				if (entry != NULL)
				{
					if (entry->title != NULL)
					{
						label = entry->title;
					}
					else if (entry->address != NULL)
					{
						label = entry->address;
					}
					else 
					{
						label = XP_GetString(XFE_UNTITLED);
					}
				}
				
				if (label != NULL)
				{
					*stringReturn = XmStringCreateLtoR(label,
													   XmFONTLIST_DEFAULT_TAG);

					*needToFreeString = True;

					return;
				}

			} // (target != NULL)
		} // (current != NULL)
	} // (list != NULL)

	*stringReturn = NULL;
	*needToFreeString = False;
}
//////////////////////////////////////////////////////////////////////////
