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
// Name:        ToolbarButton.cpp                                       //
//                                                                      //
// Description:	XFE_ToolbarButton class implementation.                 //
//              A toolbar push button.                                  //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "ToolbarButton.h"
#include "RDFUtils.h"
#include "BrowserFrame.h"			// for fe_reuseBrowser()
#include "PopupMenu.h"

#include <Xfe/Button.h>

#include "prefapi.h"

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarButton::XFE_ToolbarButton(XFE_Frame *		frame,
									 Widget				parent,
                                     HT_Resource		htResource,
									 const String		name) :
	XFE_ToolbarItem(frame,parent,htResource,name),
    _popup(NULL)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarButton::~XFE_ToolbarButton()
{
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Initialize
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::initialize()
{
    Widget button = createBaseWidget(getParent(),getName());

	setBaseWidget(button);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Sensitive interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::setSensitive(Boolean state)
{
	XP_ASSERT( isAlive() );

	XfeSetPretendSensitive(m_widget,state);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ Boolean
XFE_ToolbarButton::isSensitive()
{
	XP_ASSERT( isAlive() );
	
	return XfeIsPretendSensitive(m_widget);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Widget creation interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ Widget
XFE_ToolbarButton::createBaseWidget(Widget			parent,
									const String	name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	Widget button;

	button = XtVaCreateWidget(name,
							  xfeButtonWidgetClass,
							  parent,
							  XmNinstancePointer,	this,
							  NULL);

	return button;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Configure
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::configure()
{
	XP_ASSERT( isAlive() );

	// TODO, all the toolbar item resizing magic
	// XtVaSetValues(m_widget,XmNforceDimensionToMax,False,NULL);

    // Set the item's label
    XFE_RDFUtils::setItemLabelString( m_widget,
									 getHtResource());

	// Set the item's style and layout
	HT_Resource		entry = getHtResource();
    int32			style = XFE_RDFUtils::getStyleForEntry(entry);
	unsigned char	layout = XFE_RDFUtils::getButtonLayoutForEntry(entry,
																   style);

    if (style == BROWSER_TOOLBAR_TEXT_ONLY)
	{
		XtVaSetValues(m_widget,
					  XmNpixmap,			XmUNSPECIFIED_PIXMAP,
					  XmNpixmapMask,		XmUNSPECIFIED_PIXMAP,
					  NULL);

		XtVaSetValues(m_widget, XmNbuttonLayout, layout, NULL);
	}
	else
	{
		Pixmap pixmap;
		Pixmap pixmapMask;

		XFE_RDFUtils::getPixmapsForEntry(m_widget,
										 getHtResource(),
										 &pixmap,
										 &pixmapMask,
										 NULL,
										 NULL);

        XtVaSetValues(m_widget,
                      XmNpixmap,		pixmap,
                      XmNpixmapMask,	pixmapMask,
                      XmNbuttonLayout,	layout,
                      NULL);
	}

}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// addCallbacks
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::addCallbacks()
{
	XP_ASSERT( isAlive() );

	// Add activate callback
    XtAddCallback(m_widget,
				  XmNactivateCallback,
				  XFE_ToolbarButton::activateCB,
				  (XtPointer) this);

	// Add popup callback
	XtAddCallback(m_widget,
				  XmNbutton3DownCallback,
				  XFE_ToolbarButton::popupCB,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Button callback interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::activate()
{
	HT_Resource		entry = getHtResource();

	XP_ASSERT( entry != NULL );

	// Check for xfe commands
	if (XFE_RDFUtils::ht_IsFECommand(entry)) 
	{
		// Convert the entry name to a xfe command
		CommandType cmd = XFE_RDFUtils::ht_GetFECommand(entry);

		XP_ASSERT( cmd != NULL );

		// Fill in the command info
		XFE_CommandInfo info(XFE_COMMAND_EVENT_ACTION,
							 m_widget, 
							 NULL /*event*/,
							 NULL /*params*/,
							 0    /*num_params*/);

		XFE_Frame * frame = getAncestorFrame();

		XP_ASSERT( frame != NULL );
		
		// If the frame handles the command and its enabled, execute it.
		if (frame->handlesCommand(cmd,NULL,&info) &&
			frame->isCommandEnabled(cmd,NULL,&info))
		{
			xfe_ExecuteCommand(frame,cmd,NULL,&info);
			
			//_frame->notifyInterested(Command::commandDispatchedCallback, 
			//callData);
		}
	}
	// Other URLs
	else if (!HT_IsContainer(entry))
	{
        XFE_RDFUtils::launchEntry(getAncestorContext(), entry);
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::popup(XEvent *event)
{
	if (_popup)
	{
        // Destroy old one
        delete _popup;
    }
	HT_Resource		entry = getHtResource();
    HT_View         view  = HT_GetView(entry);
    HT_Pane         pane  = HT_GetPane(view);

    //URDFUtilities::StHTEventMasking noEvents(pane, HT_EVENT_NO_NOTIFICATION_MASK);
    HT_SetSelectedView(pane, view);
    HT_SetSelection(entry);

    _popup = new XFE_RDFPopupMenu("popup",
                                   FE_GetToplevelWidget(),
                                   view,
                                   FALSE,   // isWorkspace
                                   FALSE); // background commands
		
	_popup->position(event);
	_popup->show();
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private callbacks
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarButton::activateCB(Widget		/* w */,
							  XtPointer		clientData,
							  XtPointer		/* callData */)
{
	XFE_ToolbarButton * button = (XFE_ToolbarButton *) clientData;

	XP_ASSERT( button != NULL );

	button->activate();
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarButton::popupCB(Widget			/* w */,
						   XtPointer		clientData,
						   XtPointer		callData)
{
	XFE_ToolbarButton * button = (XFE_ToolbarButton *) clientData;
    XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *) callData;

	XP_ASSERT( button != NULL );

	button->popup(cbs->event);
}
//////////////////////////////////////////////////////////////////////////
