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

#include <Xfe/Button.h>

//////////////////////////////////////////////////////////////////////////
//
// XFE_ToolbarButton notifications
//
//////////////////////////////////////////////////////////////////////////
const char *
XFE_ToolbarButton::doCommandNotice = "XFE_ToolbarButton::doCommandNotice";

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarButton::XFE_ToolbarButton(XFE_Frame *		frame,
									 Widget				parent,
                                     HT_Resource		htResource,
									 const String		name) :
	XFE_ToolbarItem(frame,parent,htResource,name),
	m_command(NULL),
	m_callData(NULL)
{
	// The default value for the command is the widget name
	m_command = Command::intern((char *) getName());

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
    Widget bw = createBaseWidget(getParent(),getName());

	setBaseWidget(bw);

    installDestroyHandler();
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
// Command interface interface
//
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarButton::setCommand(CommandType command)
{
	m_command = command;
}
//////////////////////////////////////////////////////////////////////////
CommandType
XFE_ToolbarButton::getCommand()
{
	return m_command;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarButton::setCallData(void * callData)
{
	m_callData = callData;
}
//////////////////////////////////////////////////////////////////////////
void *
XFE_ToolbarButton::getCallData()
{
	return m_callData;
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
							  NULL);

    XtAddCallback(button,
				  XmNactivateCallback,
				  XFE_ToolbarButton::activateCB,
				  (XtPointer) this);
	
	return button;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Tool tip support
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::tipStringObtain(XmString *	stringReturn,
								   Boolean *	needToFreeString)
{
	XP_ASSERT( isAlive() );
	
	*stringReturn = getTipStringFromAppDefaults();
	*needToFreeString = False;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::docStringObtain(XmString *	stringReturn,
								   Boolean *	needToFreeString)
{
	XP_ASSERT( isAlive() );
	
	*stringReturn = getDocStringFromAppDefaults();
	*needToFreeString = False;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::docStringSet(XmString /* string */)
{
	getAncestorFrame()->notifyInterested(Command::commandArmedCallback,
										 (void *) getCommand());
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::docStringClear(XmString /* string */)
{
	getAncestorFrame()->notifyInterested(Command::commandDisarmedCallback,
										 (void *) getCommand());
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private callbacks
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarButton::activateCB(Widget		w,
							  XtPointer		clientData,
							  XtPointer		callData)
{
	XFE_ToolbarButton *			button = (XFE_ToolbarButton*) clientData;
	XfeButtonCallbackStruct *	cd = (XfeButtonCallbackStruct *) callData;
	CommandType					cmd = button->getCommand();
	void *						cmdCallData = button->getCallData();

	// The command info - only widget and event available
	XFE_CommandInfo				info(XFE_COMMAND_BUTTON_ACTIVATE,
									 w,
									 cd->event);
	// Command arguments
	XFE_DoCommandArgs			cmdArgs(cmd,
										cmdCallData,
										&info);

	// Send a message that will perform an action.
	button->notifyInterested(XFE_ToolbarButton::doCommandNotice,
							 (void *) & cmdArgs);
}
//////////////////////////////////////////////////////////////////////////
