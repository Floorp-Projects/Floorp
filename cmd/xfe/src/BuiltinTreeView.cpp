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

XFE_BuiltinTreeView::XFE_BuiltinTreeView(XFE_Component *toplevel_component, 
										 Widget         parent, 
										 XFE_View      *parent_view,
										 MWContext     *context,
										 LO_BuiltinStruct *builtin_struct)
	: XFE_RDFBase()
{
	printf("XFE_BuiltinTreeView::XFE_BuiltinTreeView()\n");

	// Create our form to live in.
	mainForm = XtVaCreateManagedWidget("builtinTreeViewMainForm",
									   xmFormWidgetClass,
									   parent,
									   NULL);
	
#if 1
	
	// Hunt the builtin_struct.
	classId = LO_GetBuiltInAttribute(builtin_struct, "classid");
	url     = LO_GetBuiltInAttribute(builtin_struct, "data");
	target  = LO_GetBuiltInAttribute(builtin_struct, "target");

	// New HT pane.
#ifdef OJI	
	newPaneFromURL(url, builtin_struct->attributes.n, 
				   builtin_struct->attributes.names, 
				   builtin_struct->attributes.values);
#else
	newPaneFromURL(url, builtin_struct->attribute_cnt,
				   builtin_struct->attribute_list,
				   builtin_struct->value_list);

#endif
	// Create an RDF View instance and give it our form.
	m_view = new XFE_RDFTreeView(toplevel_component,
								 mainForm,
								 parent_view, 
								 context);

	m_view->setBaseWidget(mainForm);

	m_view->show();
#else
	// Test, I'm not seeing a tree yet.  Please delete this soon :-)
	{
		Widget testHack = XmCreatePushButton(mainForm, "RDFTreeView", NULL, 0);
		XtManageChild(testHack);
	}
#endif


	// Register the MWContext instance in the XP list.
	XP_SetLastActiveContext(context);
}

XFE_BuiltinTreeView::~XFE_BuiltinTreeView()
{
	if(m_view) {
		delete m_view;
	}
}


Widget
XFE_BuiltinTreeView::getBaseWidget()
{
	return mainForm;
}
