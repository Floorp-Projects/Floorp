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
	char                 *url;
	char                 *target;
	char                 *classId;
};


#endif /* XFE_BuiltinTreeView.h */
