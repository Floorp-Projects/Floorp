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
   PropertySheetDialog.cpp -- class definition for PropertySheetDialog
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */



#include "PropertySheetDialog.h"
#include "PropertySheetView.h"

//
// This is the dialog it self
//
XFE_PropertySheetDialog::XFE_PropertySheetDialog(XFE_View  *view, 
										 Widget     parent,
										 char      *name,
										 MWContext *context,
										 Boolean    ok, 
										 Boolean    cancel,
										 Boolean    help, 
										 Boolean    apply, 
										 Boolean    separator, 
										 Boolean    modal):
	XFE_ViewDialog(view, 
				   parent, 
				   name,
				   context,
				   ok,
				   cancel,
				   help,
				   apply,
				   separator,
				   modal)
{
  
  /* 1. Dialog frame is created in XFE_ViewDialog::XFE_ViewDialog
   */
  m_clientData = 0;

  /* 2. Create a folder view. m_chrome is the dialog widget
   */
  XFE_PropertySheetView *folderview = 
    new XFE_PropertySheetView(this /* topComponent */, m_chrome, 
			  0, 0);

  folderview->show();

  //
  // we don't want a default value, since return does other stuff for
  // us.
  XtVaSetValues(m_chrome, /* the dialog */
		XmNdefaultButton, NULL,
		NULL);

  /* set and show view 
   */
  setView(folderview);


}

XFE_PropertySheetDialog::~XFE_PropertySheetDialog()
{
	/* 100892: Caldera: address book problem causes continuous loop 
	 * reset flag to leave fe_eventloop()
	 */
	if (m_clientData)
		*m_clientData = (ANS_t) eCANCEL;
	
}

void XFE_PropertySheetDialog::setDlgValues()
{
  if (m_view)
    ((XFE_PropertySheetView *) m_view)->setDlgValues();
#if defined(DEBUG_tao_)
  else
    printf("\n***[XFE_PropertySheetDialog::setDlgValues] m_view is not set!");
#endif
}

void XFE_PropertySheetDialog::getDlgValues()
{
  if (m_view)
    ((XFE_PropertySheetView *) m_view)->getDlgValues();
#if defined(DEBUG_tao_)
  else
    printf("\n***[XFE_PropertySheetDialog::setDlgValues] m_view is not set!");
#endif
}

void XFE_PropertySheetDialog::cancel()
{
  // default setting
  if (m_clientData)
	  *m_clientData = (ANS_t) eCANCEL;
  else
	  hide();
}

void XFE_PropertySheetDialog::apply()
{

  // default setting
  if (m_clientData)
	  *m_clientData = (ANS_t) eAPPLY;

  if (m_view)
    ((XFE_PropertySheetView *) m_view)->apply();
}

void XFE_PropertySheetDialog::ok()
{
  // default setting
  if (m_clientData)
	  *m_clientData = (ANS_t) eAPPLY;

  apply();

  if (!m_clientData && m_okToDestroy)
	  hide();
}
