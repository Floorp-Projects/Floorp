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
   NewsServerDialog.h -- dialog for letting the user add a new news server.
   Created: Chris Toshok <toshok@netscape.com>, 24-Oct-96.
 */



#ifndef _xfe_newsserverdialog_h
#define _xfe_newsserverdialog_h

#include "xp_core.h"
#include "Dialog.h"
#include "Frame.h"

class XFE_NewsServerDialog: public XFE_Dialog
{
public:
	XFE_NewsServerDialog(Widget parent,
						 char *name,
						 XFE_Frame *frame);

	virtual ~XFE_NewsServerDialog();

	/* changes the delete response to UNMAP so we can keep the
	   dialog around after it's popped down, and returns True
	   if Ok was pressed, and False otherwise (either cancel or
	   the WM close button was pressed. 

	   This method calls fe_eventLoop so we can return with the
	   user's response from here. 

	   since we UNMAP instead of DESTROY, you need to delete the
	   instance after you're through with it.*/
	XP_Bool post();

	const char *getServer();
	int getPort();
	XP_Bool isSecure();
  
private:
	XP_Bool m_retVal;
	XP_Bool m_doneWithLoop;

	Widget m_serverText;
	Widget m_portText;
	Widget m_secureToggle;

	int m_portTextEdited;

	XFE_Frame *m_frame;

	void ok();
	void cancel();
	void port_edit();
	void toggle();

	XP_Bool verify();

	static void ok_cb(Widget, XtPointer, XtPointer);
	static void cancel_cb(Widget, XtPointer, XtPointer);
	static void toggle_cb(Widget, XtPointer, XtPointer);
	static void port_edit_cb(Widget, XtPointer, XtPointer);
};

#endif /* _xfe_newsserverdialog_h */
