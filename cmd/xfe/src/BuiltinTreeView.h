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
   BuiltinTreeView.h - Display the builtin tag.
   Created: mcafee@netscape.com  Thu Sep  3 17:43:59 PDT 1998
 */

#ifndef _xfe_builtintreeview_h
#define _xfe_builtintreeview_h

#include "RDFTreeView.h"

class XFE_BuiltinTreeView : public XFE_RDFTreeView
{
 public:
	XFE_BuiltinTreeView(XFE_Component *toplevel_component, 
						Widget         parent, 
						XFE_View      *parent_view,
						MWContext     *context,
						LO_BuiltinStruct *builtin_struct);
	virtual ~XFE_BuiltinTreeView();

    // Override RDFBase method
    virtual void          notify      (HT_Resource n, HT_Event whatHappened);

 protected:
    XFE_RDFTreeView *    _rdftree;
	Widget               mainForm;
	char                 *url;
	char                 *target;
	char                 *classId;
};


#endif /* XFE_BuiltinTreeView.h */
