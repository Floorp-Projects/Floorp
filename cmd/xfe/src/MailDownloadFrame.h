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
   MailDownloadFrame.h -- class definition for the mail download window.
   Created: Chris Toshok <toshok@netscape.com>, 22-Jan-97
 */



#ifndef _xfe_maildownloadframe_h
#define _xfe_maildownloadframe_h

#include "Frame.h"
#include "MNView.h"

class XFE_Logo;

class XFE_MailDownloadFrame : public XFE_Frame
{
public:
	XFE_MailDownloadFrame(Widget toplevel, XFE_Frame *parent_frame, MSG_Pane *parent_pane);
	virtual ~XFE_MailDownloadFrame();

	virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
	virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
	virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

	virtual void show();

	virtual void startDownload();
	virtual void startNewsDownload();
    virtual void cleanUpNews();

	virtual void app_delete_response();

	virtual void allConnectionsComplete(MWContext  *context = NULL);

 	virtual XFE_Logo *	getLogo			();
 	virtual void		configureLogo	();

private:
	Widget m_stopButton;

	static void activate_cb(Widget, XtPointer, XtPointer);

	XP_Bool m_being_deleted;

    XFE_CALLBACK_DECL(pastPassword)
    XFE_CALLBACK_DECL(progressDone)
};

class XFE_MailDownloadView : public XFE_MNView
{
public:
	static const char *pastPasswordCheck;
	static const char *progressDone;

	XFE_MailDownloadView(XFE_Component *toplevel_component, Widget parent, 
						 XFE_View *parent_view, MWContext *context, 
						 MSG_Pane *parent_pane,
						 MSG_Pane *p = NULL);

	virtual ~XFE_MailDownloadView();

	void startDownload();
	void startNewsDownload();
    void cleanUpNews();

	virtual void paneChanged(XP_Bool asynchronous, MSG_PANE_CHANGED_NOTIFY_CODE notify_code, int32 value);

	XFE_Logo * getLogo();

private:
	XFE_Logo *m_logo;
    Widget m_label;
};


#endif /* _xfe_maildownloadframe_h */
