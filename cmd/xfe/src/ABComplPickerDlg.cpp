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

/* -*- Mode: C++; tab-width: 4 -*-
   Created: Tao Cheng <tao@netscape.com>, 16-mar-98
 */

#include "ABComplPickerDlg.h"
#include "ABComplPickerView.h"

#include "Frame.h"
#include "ViewGlue.h"

#include <Xm/Form.h>
#include <Xm/DialogS.h>

#include "xpgetstr.h"
#include "xp_mem.h"

#include "libi18n.h"
#include "intl_csi.h"
#include "felocale.h"

extern int XFE_AB_NAME;
extern int XFE_AB_TITLE;
extern int XFE_AB_DEPARTMENT;

XFE_ABComplPickerDlg::XFE_ABComplPickerDlg(Widget     parent,
										   char      *name,
										   Boolean    modal,
										   MWContext *context,
										   MSG_Pane  *pane,
										   MWContext *pickerContext,
										   NCPickerExitFunc func,
										   void      *callData):
#if defined(GLUE_COMPO_CONTEXT)
  XFE_ViewDashBDlg(parent, name, context,
				   True, /* ok */
				   True, /* cancel */
				   True, /* help */
				   False, /* apply; remove */
				   modal)
#else
	XFE_ViewDialog((XFE_View *) 0, parent, name,
				   context,
				   True, /* ok */
				   True, /* cancel */
				   True, /* help */
				   False, /* apply; remove */
				   False, /* separator */
				   modal)
#endif /* GLUE_COMPO_CONTEXT */
{
	
  m_func = func;
  m_cData = callData;
  m_okToDestroy = False;

  Arg av [20];
  int ac = 0;

  /* Form: m_chrome is the dialog 
   */
  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNresizePolicy, XmRESIZE_NONE); ac++;
  Widget form = XmCreateForm (m_chrome, "pickerParentForm", av, ac);
#if defined(GLUE_COMPO_CONTEXT)
  m_aboveButtonArea = form;
#endif /* GLUE_COMPO_CONTEXT */
  XtManageChild (form);
  XtVaSetValues(m_chrome, /* the dialog */
				// XmNallowShellResize, FALSE,
				// XmNnoResize, False,
				XmNdefaultButton, NULL,
				NULL);

  /* picker pane
   */
  MSG_Pane *pickerPane = 0;
  if (!pane || !pickerContext) {
	  m_pickerContext = cloneCntxtNcreatePane(context, &pickerPane);
  }/* pane */
  else {
	  m_pickerContext = pickerContext;
	  pickerPane = pane;
  }/* else */

  /* The list view
   */
  XFE_ABComplPickerView *pickerView = 
	  new XFE_ABComplPickerView(this,
								form,
								NULL,
								m_pickerContext,
								pickerPane);
  setView(pickerView);
  //
  pickerView->registerInterest(XFE_ABComplPickerView::confirmed,
							   this,
							   (XFE_FunctionNotification)confirmed_cb);
#if defined(GLUE_COMPO_CONTEXT)
  if (m_dashboard && pickerView)
	  m_dashboard->connect2Dashboard(pickerView);
  // 
  attachView();
#endif /* GLUE_COMPO_CONTEXT */
}

XFE_ABComplPickerDlg::~XFE_ABComplPickerDlg()
{
#if defined(GLUE_COMPO_CONTEXT)
	if (m_view && m_dashboard)
		m_dashboard->disconnectFromDashboard(m_view);
#endif
	m_view->unregisterInterest(XFE_ABComplPickerView::confirmed,
							   this,
							   (XFE_FunctionNotification)confirmed_cb);
	 m_okToDestroy = True;
}

XFE_CALLBACK_DEFN(XFE_ABComplPickerDlg, confirmed)(XFE_NotificationCenter */*obj*/, 
								void */*clientData*/, 
								void */* callData */)
{
  ok();
}

void
XFE_ABComplPickerDlg::selectItem(int i)
{
	XFE_Outliner *outliner = ((XFE_MNListView *) m_view)->getOutliner();
	if (i < outliner->getTotalLines())
		outliner->selectItemExclusive(i);
}

MWContext* 
XFE_ABComplPickerDlg::cloneCntxtNcreatePane(MWContext  *context, 
											MSG_Pane  **pane)
{
	MWContext *pickerContext = XP_NewContext();
	if (!pickerContext)
		return NULL;
	  

#if defined(GLUE_COMPO_CONTEXT)
	fe_ContextData *fec = XP_NEW_ZAP (fe_ContextData);
	XP_ASSERT(fec);
	CONTEXT_DATA((pickerContext)) = fec;
	pickerContext->funcs = fe_BuildDisplayFunctionTable();
	// hack to identify parent type
	XFE_Frame *f = ViewGlue_getFrame(context);
	ViewGlue_addMapping(f, (void *)pickerContext);
#else
	fe_ContextData *fec = XP_NEW_ZAP (fe_ContextData);
	XP_ASSERT(fec);
	CONTEXT_DATA((pickerContext)) = fec;
	XFE_Frame *f = ViewGlue_getFrame(context);
	ViewGlue_addMapping(f, (void *)pickerContext);
	pickerContext->funcs = fe_BuildDisplayFunctionTable();
	CONTEXT_WIDGET(pickerContext) = CONTEXT_WIDGET(context);
	// fe_InitRemoteServer (XtDisplay (widget));
#endif /* GLUE_COMPO_CONTEXT */

	// Initialize doc_csid
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(pickerContext);
	INTL_SetCSIDocCSID(c, fe_LocaleCharSetID);
	INTL_SetCSIWinCSID(c, INTL_DocToWinCharSetID(INTL_GetCSIDocCSID(c)));

	//
	int error = AB_CreateABPickerPane(pane,
									  pickerContext,
									  fe_getMNMaster(), 
									  8);
	XP_ASSERT(*pane);
	return pickerContext;
}

void 
XFE_ABComplPickerDlg::cancel()
{
  hide();
}

void 
XFE_ABComplPickerDlg::ok()
{
  cancel();
  if (m_func)
	  m_func(((XFE_ABComplPickerView *)m_view)->getSelection(), 
			 m_cData);
}

/* C API
 */
extern "C"  XFE_ABComplPickerDlg*
fe_showComplPickerDlg(Widget     toplevel, 
					  MWContext *context,
					  MSG_Pane  *pane,
					  MWContext *pickerContext,
					  NCPickerExitFunc func,
					  void      *callData)
{
	XFE_ABComplPickerDlg *dlg =  
		new XFE_ABComplPickerDlg(toplevel, 
								 "CompletionPickerDlg",
								 True,
								 context,
								 pane,
								 pickerContext,
								 func,
								 callData);
#if 1
	return dlg;
#else
	dlg->show();
#endif
}

