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
   ABSearchDlg.cpp -- search dlg
   Created: Tao Cheng <tao@netscape.com>, 22-oct-97
 */

#include "ABSearchDlg.h"

#include <Xm/Form.h>

#include "xpgetstr.h"

#if 0
static XFE_ABSearchDlg *theDlg =  NULL;
#endif

XFE_ABSearchDlg::XFE_ABSearchDlg(Widget          parent,
								 char           *name,
								 Boolean         modal,
								 MWContext      *context,
								 ABSearchInfo_t *info):
	XFE_ViewDialog((XFE_View *) 0, parent, name,
				   context,
				   False, /* ok */
				   False, /* cancel */
				   False, /* help */
				   False, /* apply; remove */
				   False, /* separator */
				   modal)
{
	XP_ASSERT(info && info->m_dir);

	Arg av [20];
	int ac = 0;

	/* Form: m_chrome is the dialog 
	 */
	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	// XtSetArg (av [ac], XmNresizePolicy, XmRESIZE_NONE); ac++;
	Widget form = XmCreateForm(m_chrome, "abSearchDlgForm", av, ac);

	XtVaSetValues(m_chrome, /* the dialog */
				  // XmNallowShellResize, FALSE,
				  XmNnoResize, True,
				  XmNdefaultButton, NULL,
				  NULL);

	XFE_ABSearchView *view = new XFE_ABSearchView(this,
												  form,
												  context,
												  info);
	setView(view);
	view->show();
	view->registerInterest(XFE_ABSearchView::dlgClose,
						   this,
						   (XFE_FunctionNotification)dlgClose_cb);
}

XFE_ABSearchDlg::~XFE_ABSearchDlg()
{
	if (m_view)
		m_view->unregisterInterest(XFE_ABSearchView::dlgClose,
								   this,
								   (XFE_FunctionNotification)dlgClose_cb);
}

XFE_CALLBACK_DEFN(XFE_ABSearchDlg, dlgClose)(XFE_NotificationCenter */*obj*/, 
											  void */*clientData*/, 
											  void */* callData */)
{
	cancel();
}


void XFE_ABSearchDlg::setParams(ABSearchInfo_t *info)
{
	XP_ASSERT(m_view);
	((XFE_ABSearchView *) m_view)->setParams(info);
}

void XFE_ABSearchDlg::ok() 
{
	/* search button
	 */

	/* close */
	cancel();
}

void XFE_ABSearchDlg::cancel()
{
	XtDestroyWidget(getBaseWidget());
}

/* C API
 */
extern "C"  void
fe_showABSearchDlg(Widget          toplevel, 
				   MWContext      *context,
				   ABSearchInfo_t *info)
{
	XP_ASSERT(info);
#if 0
	if (!theDlg)
#else
		XFE_ABSearchDlg*
#endif
			theDlg = new XFE_ABSearchDlg(toplevel, 
										 "abSearchDlg",
										 True,
										 context,
										 info);
#if 0
	else
		theDlg->setParams(info);
#endif
	
	theDlg->show();
}

