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
   BuiltinTreeView.cpp - Display the builtin tag.
   Created: mcafee@netscape.com  Thu Sep  3 17:43:59 PDT 1998
 */

#include "View.h"
#include "RDFBase.h"
#include "RDFTreeView.h"
#include "RDFChromeTreeView.h"
#include "BuiltinTreeView.h"
#include "xp_ncent.h"
#include "xfe.h"

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif


XFE_BuiltinTreeView::XFE_BuiltinTreeView(XFE_Component *toplevel_component, 
										 Widget         parent, 
										 XFE_View      *parent_view,
										 MWContext     *context,
										 LO_BuiltinStruct *builtin_struct)
    : XFE_RDFTreeView(toplevel_component, parent, parent_view, context)
{
	printf("XFE_BuiltinTreeView::XFE_BuiltinTreeView()\n");

	createTree();

	doAttachments();

	// Hunt the builtin_struct.
	classId = LO_GetBuiltInAttribute(builtin_struct, "classid");
	url     = LO_GetBuiltInAttribute(builtin_struct, "data");
	target  = LO_GetBuiltInAttribute(builtin_struct, "target");

	// New HT pane.
	newPaneFromURL(context, url, 
                   builtin_struct->attributes.n, 
				   builtin_struct->attributes.names, 
				   builtin_struct->attributes.values);

    setHTView(HT_GetSelectedView(_ht_pane));

	// Register the MWContext instance in the XP list.
	XP_SetLastActiveContext(context);
}

XFE_BuiltinTreeView::~XFE_BuiltinTreeView()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BuiltinTreeView::notify(HT_Resource		n, 
                            HT_Event		whatHappened)
{
  D(debugEvent(n, whatHappened,"Builtin"););

  if (whatHappened == HT_EVENT_VIEW_SELECTED)
  {
      HT_View view = HT_GetView(n);
      
      if (view != HT_GetSelectedView(_ht_pane))
      {
          HT_SetSelectedView(_ht_pane, view);
      }
      
      _ht_view = view;
      setHTView(view);
  }
  XFE_RDFTreeView::notify(n, whatHappened);
}
//////////////////////////////////////////////////////////////////////
