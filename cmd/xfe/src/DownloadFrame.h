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
   DownloadFrame.h -- class definition for download frames.
   Created: Chris Toshok <toshok@netscape.com>, 21-Feb-197
 */



#ifndef _xfe_downloadframe_h
#define _xfe_downloadframe_h

#include "Frame.h"
#include "View.h"

class XFE_DownloadFrame : public XFE_Frame
{
public:

	XFE_DownloadFrame(Widget toplevel, XFE_Frame *parent_frame);

	virtual ~XFE_DownloadFrame();

	virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
	virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
	virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

	void setAddress(char *address);
	void setDestination(char *address);

	virtual void allConnectionsComplete(MWContext  *context = NULL);

 	virtual XFE_Logo *	getLogo			();
 	virtual void		configureLogo	();

private:
	Widget m_stopButton;

	static void activate_cb(Widget, XtPointer, XtPointer);

};

class XFE_DownloadView : public XFE_View
{
public:
	XFE_DownloadView(XFE_Component *toplevel_component, Widget parent, 
					 XFE_View *parent_view, MWContext *context);
	virtual ~XFE_DownloadView();

	void setAddress(char *address);
	void setDestination(char *destination);

	XFE_Logo * getLogo();

private:

	XFE_Logo * m_logo;

	Widget m_url_value;
	Widget m_saving_value;
};

extern "C" MWContext* fe_showDownloadWindow(Widget, XFE_Frame *parent_frame);

#endif /* _xfe_downloadframe_h */
