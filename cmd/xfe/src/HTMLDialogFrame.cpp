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
   HTMLDialogFrame.cpp -- class definition for HTML dialogs
   Created: Chris Toshok <toshok@netscape.com>, 7-Mar-97
 */



#include "HTMLDialogFrame.h"

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

extern "C" {
	void fe_set_scrolled_default_size(MWContext *context);
}

XFE_HTMLDialogFrame::XFE_HTMLDialogFrame(Widget toplevel,
										 XFE_Frame *parent_frame,
										 Chrome *chrome_spec)
	: XFE_Frame("Dialog", toplevel,
				parent_frame, 
				FRAME_HTML_DIALOG,
				chrome_spec,
				True, /* haveHTMLDisplay */
				False, /* haveMenuBar */
				False, /* haveToolbars */
				False, /* haveDashboard */
				True /* destroyOnClose */)
{
	XFE_HTMLView *htmlview;

	D(printf ("in XFE_HTMLDialogFrame::XFE_HTMLDialogFrame()\n");)

	htmlview = new XFE_HTMLView(this, getViewParent(), NULL, m_context);

	setView(htmlview);

	fe_set_scrolled_default_size(m_context);

	XtVaSetValues(htmlview->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	htmlview->show();

	respectChrome(chrome_spec);
}

XFE_HTMLDialogFrame::~XFE_HTMLDialogFrame()
{
	D(printf ("in XFE_HTMLDialogFrame::~XFE_HTMLDialogFrame()\n");)
}

MWContext*
fe_showHTMLDialog(Widget parent,
				  XFE_Frame *parent_frame,
				  Chrome *chromespec)
{
	XFE_HTMLDialogFrame *dialog = new XFE_HTMLDialogFrame(parent,
														  parent_frame,
														  chromespec);

	dialog->show();

	D(printf ("After calling dialog->show()\n");)

	return dialog->getContext();
}
