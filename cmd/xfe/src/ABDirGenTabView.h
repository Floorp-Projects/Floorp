/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   ABDirGenTabView.h -- class definition for XFE_ABDirGenTabView
   Created: Tao Cheng <tao@netscape.com>, 10-nov-97
 */

#ifndef _ABDIRGENTABVIEW_H_
#define _ABDIRGENTABVIEW_H_

#include "PropertyTabView.h"
#include "addrbook.h"

class XFE_ABDirGenTabView: public XFE_PropertyTabView {
public:
	XFE_ABDirGenTabView(XFE_Component *top,
						XFE_View      *view/* the parent view */);
	virtual ~XFE_ABDirGenTabView();
	
	virtual void setDlgValues();
	
	enum {ABDIR_DESCRIPTION = 0,
		  ABDIR_LDAPSERVER,
		  ABDIR_SEARCHROOT,
		  ABDIR_PORTNUMBER,
		  ABDIR_MAXHITS,
		  ABDIR_LAST
	} GEN_TEXTF;

	enum {ABDIR_SECUR = 0,
		  ABDIR_USEPASSWD,
		  ABDIR_SAVEPASSWD,
		  ABDIR_SECLAST
	} GEN_TOGGLE;

	static void usePasswdCallback(Widget w, 
								  XtPointer clientData, XtPointer callData);
protected:
	virtual void usePasswdCB(Widget w, XtPointer callData);

	virtual void apply(){};
	virtual void getDlgValues();

private:
	/* m_widget is the tab form
	 */
	
	/* widgets in this tab
	 */
	Widget m_textFs[ABDIR_LAST+1];
	Widget m_labels[ABDIR_LAST+1];
#if defined(MOZ_MAIL_NEWS)
	Widget m_toggles[ABDIR_SECLAST];
#else
	Widget m_toggles[3];
#endif /* MOZ_MAIL_NEWS */
}; /* XFE_ABDirGenTabView */

#endif /* _ABDIRGENTABVIEW_H_ */
