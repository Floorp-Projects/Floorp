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

#include "HTMLView.h"			// For XFE_HTMLView::newURLLoading

#include <Xfe/FancyBox.h>
#include <Xfe/ToolBar.h>

//////////////////////////////////////////////////////////////////////////
//
// XFE_ToolbarUrlBar notifications
//
//////////////////////////////////////////////////////////////////////////
const char *
XFE_ToolbarUrlBar::urlBarTextActivatedNotice = "XFE_ToolbarUrlBar::urlBarTextActivatedNotice";

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarUrlBar::XFE_ToolbarUrlBar(XFE_Frame *		frame,
									 Widget				parent,
									 HT_Resource		htResource,
									 const String		name) :
	XFE_ToolbarItem(frame,parent,htResource,name),
	m_proxyIcon(NULL),
	m_proxyIconDragSite(NULL)
{
	getAncestorFrame()->registerInterest(XFE_HTMLView::newURLLoading, 
										 this,
										 newPageLoadingNotice_cb);
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarUrlBar::~XFE_ToolbarUrlBar()
{
	getAncestorFrame()->unregisterInterest(XFE_HTMLView::newURLLoading, 
										   this,
										   newPageLoadingNotice_cb);
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
    Widget urlbar = createBaseWidget(getParent(),getName());

	setBaseWidget(urlbar);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Configure
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarUrlBar::configure()
{
	XP_ASSERT( isAlive() );

	createProxyIcon(m_widget,"proxyIcon");
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// addCallbacks
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarUrlBar::addCallbacks()
{
	XP_ASSERT( isAlive() );

    XtAddCallback(m_widget,
				  XmNtextActivateCallback,
				  XFE_ToolbarUrlBar::textActivateCB,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Text string methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarUrlBar::setTextStringFromURL(URL_Struct * url)
{
	XP_ASSERT( isAlive() );
	XP_ASSERT( url != NULL );

	// Update the proxy icon
	if (m_proxyIconDragSite != NULL)
	{
        m_proxyIconDragSite->setDragDataFromURL(url);
	}

	// Lots of munging and sanitization need to happen here.  See
	// URLBar.cpp for the insanity

	XfeComboBoxSetTextString(m_widget,url->address);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarUrlBar::setTextString(const String str)
{
	XP_ASSERT( isAlive() );
	XP_ASSERT( str != NULL );

	// Update the proxy icon
	if (m_proxyIconDragSite != NULL)
	{
        m_proxyIconDragSite->setDragDataFromString(str);
	}

	// Lots of munging and sanitization need to happen here.  See
	// URLBar.cpp for the insanity

	XfeComboBoxSetTextString(m_widget,str);
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
	
	Widget urlbar;
	
	urlbar = XtVaCreateWidget(name,
							  xfeFancyBoxWidgetClass,
							  parent,
							  XmNforceDimensionToMax,	False,
							  XmNcomboBoxType,			XmCOMBO_BOX_EDITABLE,
							  XmNwidth,					400,
							  XmNusePreferredWidth,		False,
// 							   XmNtraversalOn,			False,
// 							   XmNhighlightThickness,	0,
							  NULL);

    fe_HackTranslations(getAncestorContext(), urlbar);

	return urlbar;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Urlbar callback interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void	
XFE_ToolbarUrlBar::textActivate()
{
	String text = XfeComboBoxGetTextString(m_widget);

	if (text != NULL)
	{
//		printf("textActivate(%s)\n",text);

		URL_Struct * url = NET_CreateURLStruct(text,NET_DONT_RELOAD);

		notifyInterested(XFE_ToolbarUrlBar::urlBarTextActivatedNotice,
						 (void *) url);
		
		XtFree(text);
	}

	fe_NeutralizeFocus(getAncestorContext());
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

	// Proxy icon
	m_proxyIcon = new XFE_Button(getAncestorFrame(),
								 parent,
								 name,
								 &LocationProxy_group);
	

	m_proxyIconDragSite = new XFE_LocationDrag(m_proxyIcon->getBaseWidget());

	m_proxyIcon->show();
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private callbacks
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarUrlBar::textActivateCB(Widget		/* w */,
								  XtPointer		clientData,
								  XtPointer		/* callData */)
{
	XFE_ToolbarUrlBar *			urlbar = (XFE_ToolbarUrlBar*) clientData;

	XP_ASSERT( urlbar != NULL );

	urlbar->textActivate();
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_ToolbarUrlBar, newPageLoadingNotice)
    (XFE_NotificationCenter *    /* obj */, 
     void *                      clientData, 
     void *                      callData)
{
	URL_Struct * url = (URL_Struct *) callData;

	XP_ASSERT( url != NULL );

	if (url == NULL)
	{
		setTextString("Carajo, ahora si que estamos jodidos.");
	}
	else
	{
		setTextStringFromURL(url);
	}
}
//////////////////////////////////////////////////////////////////////////
