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
   ABDirPropertyDlg.h -- class definition for XFE_ABDirPropertyDlg
   Created: Tao Cheng <tao@netscape.com>, 10-nov-97
 */

#ifndef _ABDIRPROPERTYDLG_H_
#define _ABDIRPROPERTYDLG_H_

#include "PropertySheetDialog.h"
#include "addrbook.h"

typedef void (*ABDirPropertyCBProc)(DIR_Server *dir,
									void       *callData);

class XFE_ABDirPropertyDlg: public XFE_PropertySheetDialog {
public:
	XFE_ABDirPropertyDlg(Widget    parent,
						 char     *name,
						 Boolean   modal,
						 MWContext *context);
	
	virtual ~XFE_ABDirPropertyDlg();

#if defined(USE_ABCOM)
	void    setPane(MSG_Pane *pane);
#endif /* USE_ABCOM */
	virtual void setDlgValues(DIR_Server* dir);
	DIR_Server*  getDir() { return m_dir;}
	void         registerCB(ABDirPropertyCBProc proc, void* callData);

protected:
	virtual void apply();

private:
	DIR_Server          *m_dir;
    ABDirPropertyCBProc  m_proc;
	void                *m_callData;
#if defined(USE_ABCOM)
	MSG_Pane            *m_pane;
#endif /* USE_ABCOM */
}; /* XFE_ABDirPropertyDlg */

/* C API
 */
extern "C" void fe_showABDirPropertyDlg(Widget               parent,
										MWContext           *context,
										DIR_Server          *dir,
										ABDirPropertyCBProc  proc,
										void                *callData);

#endif /* _ABDIRPROPERTYDLG_H_ */
