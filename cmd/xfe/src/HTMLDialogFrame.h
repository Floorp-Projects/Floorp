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
   HTMLDialogFrame.h -- class definition for HTML dialogs
   Created: Chris Toshok <toshok@netscape.com>, 7-Mar-97
 */



#ifndef _xfe_htmldialogframe_h
#define _xfe_htmldialogframe_h

#include "Frame.h"
#include "HTMLView.h"

class XFE_HTMLDialogFrame : public XFE_Frame
{
public:
	XFE_HTMLDialogFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chrome_spec);

	virtual ~XFE_HTMLDialogFrame();
};

extern "C" MWContext* fe_showHTMLDialog(Widget, XFE_Frame *parent_frame, Chrome *chromespec);

#endif /* _xfe_htmldialogframe_h */
