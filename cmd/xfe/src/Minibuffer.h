/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   Minibuffer.h -- silly emacs easter egg.
   Created: Chris Toshok <toshok@netscape.com>, 19-Feb-97.
 */



#ifndef _xfe_minibuffer_h
#define _xfe_minibuffer_h

#include "Frame.h"
#include "Component.h"

class XFE_Minibuffer : public XFE_Component
{
public:
	XFE_Minibuffer(XFE_Frame *toplevel_component,
				   Widget parent);
	virtual ~XFE_Minibuffer();

private:
	XFE_Frame *m_parentFrame;
	Widget m_textfield;

	void textActivate();
	static void textActivate_cb(Widget, XtPointer, XtPointer);
};

extern "C" void FE_ShowMinibuffer(MWContext *context);

#endif /* _xfe_minibuffer_h */
