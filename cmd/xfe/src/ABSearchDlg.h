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
