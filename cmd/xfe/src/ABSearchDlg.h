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
   ABSearchDlg.h -- class definition for XFE_ABSearchDlg
   Created: Tao Cheng <tao@netscape.com>, 22-oct-97
 */

#ifndef _ABSEARCHDLG_H_
#define _ABSEARCHDLG_H_

#include "ViewDialog.h"
#include "ABSearchView.h"

class XFE_ABSearchDlg: public XFE_ViewDialog 
{

public:

	XFE_ABSearchDlg(Widget          parent,
					char           *name,
					Boolean         modal,
					MWContext      *context,
					ABSearchInfo_t *info);

	virtual ~XFE_ABSearchDlg();

	void            setParams(ABSearchInfo_t *info);

	virtual void cancel();

	//
	XFE_CALLBACK_DECL(dlgClose)

protected:
	virtual void ok();

};

/* C API
 */
extern "C"  void
fe_showABSearchDlg(Widget          toplevel, 
				   MWContext      *context,
				   ABSearchInfo_t *info);

#endif /* _ABSEARCHDLG_H_ */
