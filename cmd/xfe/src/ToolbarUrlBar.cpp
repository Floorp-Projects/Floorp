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
// Name:        ToolbarUrlBar.cpp                                       //
//                                                                      //
// Description:	XFE_ToolbarUrlBar class implementation.                 //
//              A toolbar url bar combo box.                            //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "ToolbarUrlBar.h"
#include "IconGroup.h"
#include "Button.h"

#include <Xfe/FancyBox.h>
#include <Xfe/ToolBar.h>

//////////////////////////////////////////////////////////////////////////
//
// XFE_ToolbarUrlBar notifications
//
//////////////////////////////////////////////////////////////////////////
const char *
XFE_ToolbarUrlBar::doCommandNotice = "XFE_ToolbarUrlBar::doCommandNotice";

const char *
XFE_ToolbarUrlBar::navigateToUrlNotice = "XFE_ToolbarUrlBar::navigateToUrlNotice";

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarUrlBar::XFE_ToolbarUrlBar(XFE_Frame *		frame,
									 Widget				parent,
									 const String		name) :
	XFE_ToolbarItem(frame,parent,name),
	m_proxyIcon(NULL),
	m_proxyIconDragSite(NULL)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarUrlBar::~XFE_ToolbarUrlBar()
{
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Initialize
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarUrlBar::initialize()
{
    Widget uw = createBaseWidget(getParent(),getName());

	setBaseWidget(uw);

    installDestroyHandler();

	createProxyIcon(m_widget,"proxyIcon");
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Widget creation interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ Widget
XFE_ToolbarUrlBar::createBaseWidget(Widget			parent,
									const String	name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );
	
	Widget url_bar;
	
	url_bar = XtVaCreateWidget(name,
							   xfeFancyBoxWidgetClass,
							   parent,
							   XmNforceDimensionToMax,	False,
							   XmNcomboBoxType,			XmCOMBO_BOX_EDITABLE,
							   XmNwidth,				400,
							   XmNusePreferredWidth,	False,
// 							   XmNtraversalOn,			False,
// 							   XmNhighlightThickness,	0,
							   NULL);

    XtAddCallback(url_bar,
				  XmNtextActivateCallback,
				  XFE_ToolbarUrlBar::textActivateCB,
				  (XtPointer) this);
	
	return url_bar;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Tool tip support
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarUrlBar::tipStringObtain(XmString *	stringReturn,
								   Boolean *	needToFreeString)
{
	XP_ASSERT( isAlive() );
	
	*stringReturn = getTipStringFromAppDefaults();
	*needToFreeString = False;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarUrlBar::docStringObtain(XmString *	stringReturn,
								   Boolean *	needToFreeString)
{
	XP_ASSERT( isAlive() );
	
	*stringReturn = getDocStringFromAppDefaults();
	*needToFreeString = False;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarUrlBar::docStringSet(XmString /* string */)
{
	XFE_Frame * frame = (XFE_Frame *) getToplevel();

	XP_ASSERT( frame != NULL );

// 	frame->notifyInterested(Command::commandArmedCallback,
// 							(void *) getCommand());
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarUrlBar::docStringClear(XmString /* string */)
{
	XFE_Frame * frame = (XFE_Frame *) getToplevel();

	XP_ASSERT( frame != NULL );

// 	frame->notifyInterested(Command::commandDisarmedCallback,
// 							(void *) getCommand());
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
/* virtual */ void	
XFE_ToolbarUrlBar::textActivate()
{
	String text = XfeComboBoxGetTextString(m_widget);

	if (text != NULL)
	{
//		printf("textActivate(%s)\n",text);

		URL_Struct * url = NET_CreateURLStruct(text,NET_DONT_RELOAD);

		notifyInterested(XFE_ToolbarUrlBar::navigateToUrlNotice,
						 (void *) url);
		
		
		XtFree(text);
	}

	XFE_Frame * frame = (XFE_Frame *) getToplevel();

	fe_NeutralizeFocus(frame->getContext());
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarUrlBar::createProxyIcon(Widget			parent,
								   const String		name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	XFE_Frame * frame = (XFE_Frame *) getToplevel();

	XP_ASSERT( frame != NULL );

	// Proxy icon
	m_proxyIcon = new XFE_Button(frame,
								 parent,
								 name,
								 &LocationProxy_group);
	
	m_proxyIcon->show();

	m_proxyIconDragSite = new XFE_LocationDrag(m_proxyIcon->getBaseWidget());
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private callbacks
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarUrlBar::textActivateCB(Widget		w,
								  XtPointer		clientData,
								  XtPointer		callData)
{
	XFE_ToolbarUrlBar *			urlbar = (XFE_ToolbarUrlBar*) clientData;

	XP_ASSERT( urlbar != NULL );

	urlbar->textActivate();
}
//////////////////////////////////////////////////////////////////////////
