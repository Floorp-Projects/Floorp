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
   SubscribeDialog.h -- 4.x Subscribe UI dialog.
   Created: Chris Toshok <toshok@netscape.com>, 16-Oct-96.
 */



#ifndef _xfe_subscribedialog_h
#define _xfe_subscribedialog_h

#if defined(GLUE_COMPO_CONTEXT)
#include "ViewDashBDlg.h"
#else
#include "ViewDialog.h"
#endif /* GLUE_COMPO_CONTEXT */

#include "SubscribeView.h"
#include "Dashboard.h"

#if defined(GLUE_COMPO_CONTEXT)
class XFE_SubscribeDialog: public XFE_ViewDashBDlg
#else
class XFE_SubscribeDialog : public XFE_ViewDialog
#endif /* GLUE_COMPO_CONTEXT */
{
public:
	XFE_SubscribeDialog(char *name, XFE_NotificationCenter *toplevel, 
						Widget parent, MWContext *context, MSG_Host *host);
	
	virtual ~XFE_SubscribeDialog();

	void show();

#if !defined(GLUE_COMPO_CONTEXT)
    XFE_CALLBACK_DECL(allConnectionsComplete)
#endif /* GLUE_COMPO_CONTEXT */

protected:
#if !defined(GLUE_COMPO_CONTEXT)
	Widget createButtonArea(Widget parent,
							Boolean ok, Boolean cancel,
							Boolean help, Boolean apply);
#endif /* GLUE_COMPO_CONTEXT */
    virtual void ok();
private:

#if !defined(GLUE_COMPO_CONTEXT)
	XFE_Dashboard *m_dashboard;
	
	XFE_NotificationCenter *m_oldForwarder;
#endif /* GLUE_COMPO_CONTEXT */

	XFE_NotificationCenter *m_toplevelNotifier;
	XFE_SubscribeView *m_subscribeview;
};

extern "C" void fe_showSubscribeDialog(XFE_NotificationCenter *toplevel,
									   Widget parent, MWContext *context,
									   MSG_Host *host = NULL);

#endif /* _xfe_subscribedialog_h */
