/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* -*- Mode: C++; tab-width: 4 -*-
   ViewDashBDlg.h -- View dialog with a dashboard.
   Created: Tao Cheng <tao@netscape.com>, 27-apr-98.
 */



#ifndef _VIEWDASHBDLG_H_
#define _VIEWDASHBDLG_H_

#include "ViewDialog.h"
#include "Dashboard.h"

class XFE_ViewDashBDlg : public XFE_ViewDialog
{
public:
	XFE_ViewDashBDlg(Widget     parent, 
					 char      *name, 
					 MWContext *context,
					 Boolean    ok     = True, 
					 Boolean    cancel = True,
					 Boolean    help   = False, 
					 Boolean    apply  = False, 
					 Boolean    modal  = False);
	
	virtual ~XFE_ViewDashBDlg();


protected:
	static Widget create_chrome_widget(Widget   parent, 
									   char    *name,
									   Boolean  ok, 
									   Boolean  cancel,
									   Boolean  help, 
									   Boolean  apply,
									   Boolean  separator, 
									   Boolean  modal);

	Widget createButtonArea(Widget parent,
							Boolean ok, Boolean cancel,
							Boolean help, Boolean apply);

	virtual void attachView();

	XFE_Dashboard *m_dashboard;
	Widget         m_aboveButtonArea;
	Widget         m_okBtn;
	
private:
	Widget         m_buttonArea;
};

#endif /* _VIEWDASHBDLG_H_ */
