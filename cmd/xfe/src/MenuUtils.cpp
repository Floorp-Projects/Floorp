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
// Name:        MenuUtils.cpp                                           //
//                                                                      //
// Description:	Utilities for creating and manipulatin menus and and    //
//              menu items.                                             //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "MenuUtils.h"

#include "xpassert.h"

#include <Xfe/Xfe.h>		// For XfeIsAlive()

#include <Xm/CascadeB.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>

#include <Xm/CascadeBG.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>

#include <Xfe/BmButton.h>
#include <Xfe/BmCascade.h>

#include <Xm/RowColumn.h>

#define CASCADE_WC(fancy) \
( (fancy) ? xfeBmCascadeWidgetClass : xmCascadeButtonWidgetClass )

//////////////////////////////////////////////////////////////////////////
//
// Menu items
//
//////////////////////////////////////////////////////////////////////////
/* static */ Widget	
XFE_MenuUtils::createPushButton(Widget				parent,
								const String		name,
								Boolean				gadget,
								Boolean				fancy,
								XtCallbackProc		activate_cb,
								XtCallbackProc		arm_cb,
								XtCallbackProc		disarm_cb,
								XtPointer			client_data,
								ArgList				av,
								Cardinal			ac)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	WidgetClass wc = xmPushButtonWidgetClass;

	if (gadget)
	{
		wc = xmPushButtonGadgetClass;
	}
	else if(fancy)
	{
		wc = xfeBmButtonWidgetClass;
	}

	return XFE_MenuUtils::createItem(parent,
									 name,
									 wc,
									 activate_cb,
									 arm_cb,
									 disarm_cb,
									 NULL,
									 client_data,
									 av,
									 ac);
}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//
// More button
//
//////////////////////////////////////////////////////////////////////////
/* static */ Widget	
XFE_MenuUtils::createMoreButton(Widget			menu,
								const String	name,
								const String	paneName,
								Boolean			fancy)
{
	XP_ASSERT( XfeIsAlive(menu) );
    XP_ASSERT( XmIsRowColumn(menu) );
	XP_ASSERT( name != NULL );
	XP_ASSERT( paneName != NULL );

    Widget   cascade = NULL;
    Widget   pulldown = NULL;

    // Create a pulldown pane (cascade + pulldown)
    XfeMenuCreatePulldownPane(menu,
                              menu,
                              name,
                              paneName,
                              CASCADE_WC(fancy),
                              False,
                              NULL,
                              0,
                              &cascade,
                              &pulldown);

#if 0
	// Cant use a NULL entry...hmmm...

    // Configure the more button
	XFE_RDFUtils::configureMenuCascadeButton(cascade,NULL);
#endif

    return cascade;
}
//////////////////////////////////////////////////////////////////////////
/* static */ Widget	
XFE_MenuUtils::getLastMoreMenu(Widget			menu,
							   const String		name,
							   const String		paneName,
							   Boolean			fancy)
{
    XP_ASSERT( XfeIsAlive(menu) );
    XP_ASSERT( XmIsRowColumn(menu) );

    // Find the last more... menu
    Widget last_more_menu = XfeMenuFindLastMoreMenu(menu,name);

    XP_ASSERT( XfeIsAlive(last_more_menu) );

    // Check if the last menu is full
    if (XfeMenuIsFull(last_more_menu))
    {
        // Look for the More... button for the last menu
        Widget more_button = XfeMenuGetMoreButton(last_more_menu,name);

        // If no more button, create one plus a submenu
        if (!more_button)
        {
            more_button = XFE_MenuUtils::createMoreButton(last_more_menu,
														  name,
														  paneName,
														  fancy);

            XtManageChild(more_button);
        }

        // Set the last more menu to the submenu of the new more button
        last_more_menu = XfeCascadeGetSubMenu(more_button);
    }

    return last_more_menu;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private
//
//////////////////////////////////////////////////////////////////////////
/* static */ Widget	
XFE_MenuUtils::createItem(Widget				parent,
						  const String			name,
						  WidgetClass			wc,
						  XtCallbackProc		activate_cb,
						  XtCallbackProc		arm_cb,
						  XtCallbackProc		disarm_cb,
						  XtCallbackProc		cascading_cb,
						  XtPointer				client_data,
						  ArgList				av,
						  Cardinal				ac)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );
	XP_ASSERT( wc != NULL );

	Widget item = NULL;

	item = XtCreateWidget((String) name,wc,parent,av,ac);

	if (activate_cb != NULL)
	{
		XtAddCallback(item,XmNactivateCallback,activate_cb,client_data);
	}

	if (arm_cb != NULL)
	{
		XtAddCallback(item,XmNarmCallback,arm_cb,client_data);
	}

	if (disarm_cb != NULL)
	{
		XtAddCallback(item,XmNdisarmCallback,disarm_cb,client_data);
	}

	if (cascading_cb != NULL)
	{
		XtAddCallback(item,XmNcascadingCallback,cascading_cb,client_data);
	}

	return item;
}
//////////////////////////////////////////////////////////////////////////
