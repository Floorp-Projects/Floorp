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
/* Name:		ToolboxItem.cpp											*/
/* Description:	XFE_ToolboxItem component source.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include "ToolboxItem.h"
#include "Logo.h"
#include "Toolbox.h"

#include <Xfe/ToolBox.h>

//
// AbstractToolbar class
//
//////////////////////////////////////////////////////////////////////////
XFE_ToolboxItem::XFE_ToolboxItem(XFE_Component *	top,
										 XFE_Toolbox *		parentToolbox) : 
	XFE_Component(top),
	m_logo(NULL),
	m_showLogo(False),
	m_parentToolbox(parentToolbox)
{
	XP_ASSERT( m_parentToolbox != NULL );
}
//////////////////////////////////////////////////////////////////////////
int
XFE_ToolboxItem::getPosition()
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XfeIsToolBox(XtParent(m_widget)) );

	return 	m_parentToolbox->positionOfItem(m_widget);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolboxItem::setPosition(int position)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( m_parentToolbox != NULL );

// 	if (XFE_ToolboxItem::getPosition() != position)
// 	{
// 		printf("setPosition(%s,%d)\n",XtName(m_widget),position);
// 	}

	m_parentToolbox->setItemPosition(m_widget,position);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolboxItem::showLogo()
{
	XP_ASSERT( m_logo != NULL );

	m_showLogo = True;

	configureLogo();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolboxItem::hideLogo()
{
	XP_ASSERT( m_logo != NULL );

	m_showLogo = False;

	configureLogo();
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_ToolboxItem::isLogoShown()
{
	return m_showLogo;
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_ToolboxItem::isOpen()
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( m_parentToolbox != NULL );

	return 	m_parentToolbox->stateOfItem(m_widget);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolboxItem::setOpen(XP_Bool open)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( m_parentToolbox != NULL );


	m_parentToolbox->setItemOpen(m_widget,open);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolboxItem::setShowing(XP_Bool showing)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( m_parentToolbox != NULL );

	if (showing)
	{
		show();
	}
	else
	{
		hide();
	}
}
//////////////////////////////////////////////////////////////////////////
XFE_Logo *
XFE_ToolboxItem::getLogo()
{
    return m_logo;
}
//////////////////////////////////////////////////////////////////////////
XFE_Toolbox *
XFE_ToolboxItem::getParentToolbox()
{
    return m_parentToolbox;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolboxItem::addDragWidget(Widget item)
{
    XP_ASSERT( m_parentToolbox != NULL );
    XP_ASSERT( XfeIsAlive(item) );

	m_parentToolbox->addDragDescendant(item);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolboxItem::configureLogo()
{
	// Make sure the logo exists
	if (!m_logo)
	{
		return;
	}

	// Logo shown
	if (m_showLogo)
	{
		m_logo->show();
	}
	// Logo hidden
	else
	{
		m_logo->hide();
	}
}
//////////////////////////////////////////////////////////////////////////


