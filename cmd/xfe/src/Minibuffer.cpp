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
/* 
   Minibuffer.cpp -- silly epoch easter egg.
   Created: Chris Toshok <toshok@netscape.com>, 19-Feb-97
 */



#include "Minibuffer.h"
#include "ViewGlue.h"

#include <Xm/Text.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include "felocale.h"

XFE_Minibuffer::XFE_Minibuffer(XFE_Frame *toplevel, Widget parent)
	: XFE_Component(toplevel)
{
	m_parentFrame = toplevel;
	m_textfield = XtVaCreateWidget("minibuffertext",
								   xmTextFieldWidgetClass,
								   parent,
								   NULL);

	XtAddCallback(m_textfield, XmNactivateCallback, textActivate_cb, this);
	
	setBaseWidget(m_textfield);
}

XFE_Minibuffer::~XFE_Minibuffer()
{
	// nothing to do here.
}

void
XFE_Minibuffer::textActivate()
{
	char *text;

	text = fe_GetTextField(m_textfield);

	// This needs to be smart -- checking for parameters and parsing them into
	// a CommandInfo struct.
	m_parentFrame->doCommand(Command::intern(text));

	XtFree(text);
}

void
XFE_Minibuffer::textActivate_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_Minibuffer *obj = (XFE_Minibuffer*)clientData;

	obj->textActivate();
}

void
FE_ShowMinibuffer(MWContext *context)
{
	XFE_Frame *frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

	if (!frame)
		return;

	frame->doCommand(xfeCmdOpenMinibuffer);
}
