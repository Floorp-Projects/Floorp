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
   ABAddressingDlg.cpp -- address book window stuff.
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */

#include "ABAddressingDlg.h"

#include "AB2PaneView.h"
#include "ABAddrSearchView.h"

#include "ABAddresseeView.h"
#include "AddressFolderView.h"

#include "Frame.h"
#include "ViewGlue.h"

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/TextF.h>
#include <Xm/DialogS.h>

extern "C" {
#include "xfe.h"

XP_List* FE_GetDirServers();
};

#include "xpgetstr.h"
#include "xp_mem.h"
extern int XFE_AB_ADDRESSEE_PROMPT;
extern int XFE_AB_SEARCH_PROMPT;

XFE_ABAddressingDlg::XFE_ABAddressingDlg(Widget                 parent,
					 char                  *name,
					 ABAddrMsgCBProc        proc, 
					 void                  *callData,
					 Boolean                modal,
					 MWContext *context):
#if defined(GLUE_COMPO_CONTEXT)
  XFE_ViewDashBDlg(parent, name, context,
				   True, /* ok */
				   True, /* cancel */
				   True, /* help */
				   False, /* apply; remove */
				   modal),
#else
  XFE_ViewDialog((XFE_View *) 0, parent, name,
		 context,
		 True, /* ok */
		 True, /* cancel */
		 True, /* help */
		 False, /* apply; remove */
		 False, /* separator */
		 modal),
#endif /* GLUE_COMPO_CONTEXT */
  m_cbProc(proc),
  m_callData(callData)
{
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
  Widget form = XmCreateForm (m_chrome, "addrMsgForm", av, ac);
#if defined(GLUE_COMPO_CONTEXT)
  m_aboveButtonArea = form;
#endif /* GLUE_COMPO_CONTEXT */

  XtManageChild (form);
  XtVaSetValues(m_chrome, /* the dialog */
				// XmNallowShellResize, FALSE,
				// XmNnoResize, False,
				XmNdefaultButton, NULL,
				NULL);
#if defined(DEBUG_tao)
  //hack!!
  fe_HackTranslations(context, m_chrome);
  if (context)
	  printf("\n---XFE_ABAddressingDlg:context->type=%d, w=0x%x\n",
			 context->type, m_chrome);
#endif /* DEBUG_tao */

  /* 2 sub forms
   */
  char genTabLabels[2][128];

  sprintf(genTabLabels[0], 
	  "%s", 
	   XP_GetString(XFE_AB_SEARCH_PROMPT));
  sprintf(genTabLabels[1], 
	  "%s", 
	  XP_GetString(XFE_AB_ADDRESSEE_PROMPT));

  Widget subForms[AB_LAST], 
         workForms[AB_LAST];

  for (int i=0; i < AB_LAST; i++) { 
    ac = 0;
    subForms[i] = XmCreateForm (form, "viewForm", av, ac);
    /* Frame
     */
    ac = 0;
	XtSetArg (av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    Widget frame = XmCreateFrame(subForms[i], "viewFrame", av, ac);
    XtManageChild(frame);

    /* frame title
     */
	if (i > 0) {
		Widget title;

		/* to satisfy gcc
		 */
		title = XtVaCreateManagedWidget(genTabLabels[i],
										xmLabelGadgetClass, frame,
										XmNchildType, XmFRAME_TITLE_CHILD,
										XmNalignment, XmALIGNMENT_CENTER, 
										NULL);
    }/* if */

    ac = 0;
	XtSetArg (av[ac], XmNchildType, XmFRAME_WORKAREA_CHILD); ac++;
    workForms[i] = XmCreateForm(frame, "workAreaForm", av, ac);
    XtManageChild(workForms[i]);
  }/* for i */

  XtVaSetValues(subForms[1],
			XmNheight, 200,
			XmNwidth, 640,
		    XmNtopAttachment, XmATTACH_NONE,
		    XmNbottomAttachment, XmATTACH_FORM,
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    NULL);
  XtVaSetValues(subForms[0],
			XmNheight, 300,
			XmNwidth, 640,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_WIDGET,
		    XmNbottomWidget, subForms[1],
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    NULL);
  XtManageChild (subForms[1]);
  XtManageChild (subForms[0]);

  /* Search UI
   */
  m_searchContext = XP_NewContext();
  if (!m_searchContext)
	  return;
#if defined(GLUE_COMPO_CONTEXT)
  fe_ContextData *fec = XP_NEW_ZAP (fe_ContextData);
  XP_ASSERT(fec);
  CONTEXT_DATA(m_searchContext) = fec;
  m_searchContext->funcs = fe_BuildDisplayFunctionTable();
  fe_InitRemoteServer (XtDisplay (m_widget));
#else
  /* use view 
   */
  XFE_Frame *f = ViewGlue_getFrame(context);
  fe_ContextData *fec = XP_NEW_ZAP (fe_ContextData);
  XP_ASSERT(fec);
  CONTEXT_DATA(m_searchContext) = fec;
  ViewGlue_addMapping(f, (void *)m_searchContext);
  m_searchContext->funcs = fe_BuildDisplayFunctionTable();
  CONTEXT_WIDGET(m_searchContext) = CONTEXT_WIDGET(m_context);
  fe_InitRemoteServer (XtDisplay (m_widget));
#endif /* GLUE_COMPO_CONTEXT */

  /* 2 pane
   */
  XFE_AB2PaneView  *TwoPaneView = 
	  new XFE_AB2PaneView(this, 
						  workForms[AB_SEARCH_VIEW], 
						  NULL, 
						  m_searchContext,
						  AB_PICKER);

  setView(TwoPaneView);

  m_addrSearchView = 
	  (XFE_AddrSearchView *) TwoPaneView->getEntriesListView();

#if defined(GLUE_COMPO_CONTEXT)
  /* use view
   */
  ViewGlue_addMappingForCompo(m_addrSearchView, (void *)m_searchContext);
  if (m_dashboard && m_addrSearchView)
	  m_dashboard->connect2Dashboard(m_addrSearchView);

  CONTEXT_WIDGET(m_searchContext) = m_addrSearchView->getBaseWidget();
#endif

  /* cmd group
   */
  ac = 0;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_NONE); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  Widget cmdGroupForm = XmCreateForm(workForms[AB_SEARCH_VIEW],
									  "cmdGroupForm", av, ac);

  Widget dummy = 
	  XtVaCreateManagedWidget("btnrowcol", 
							  xmRowColumnWidgetClass, cmdGroupForm,
							  XmNorientation, XmHORIZONTAL,
							  XmNpacking, XmPACK_COLUMN,
							  XmNleftAttachment, XmATTACH_FORM,
							  XmNtopAttachment, XmATTACH_NONE,
							  XmNrightAttachment, XmATTACH_NONE,
							  XmNbottomAttachment, XmATTACH_FORM,
							  NULL);
  /* to button
   */
  ac = 0;

  XtSetArg(av[ac], XtNsensitive, False); ac++;
  XtSetArg(av[ac], XmNwidth, 100); ac++;
  Widget toBtn = XmCreatePushButton(dummy, 
									"toBtn", 
									av, ac);
  XtManageChild(toBtn);
  m_addrSearchView->setToBtn(toBtn);

  /* cc button
   */
  ac = 0;
  XtSetArg(av[ac], XtNsensitive, False), ac++;
  XtSetArg (av[ac], XmNwidth, 100); ac++;
  Widget ccBtn = XmCreatePushButton(dummy, 
									"ccBtn", 
									av, ac);
  XtManageChild(ccBtn);
  m_addrSearchView->setCcBtn(ccBtn);

  /* bcc button
   */
  ac = 0;
  XtSetArg(av[ac], XtNsensitive, False), ac++;
  XtSetArg (av[ac], XmNwidth, 100); ac++;
  Widget bccBtn = XmCreatePushButton(dummy, 
									 "bccBtn", 
									 av, ac);
  XtManageChild(bccBtn);
  m_addrSearchView->setBccBtn(bccBtn);

  /* button holder
   */
  Widget dummy2 = 
	  XtVaCreateManagedWidget("btnrowcol2", 
							  xmRowColumnWidgetClass, cmdGroupForm,
							  XmNorientation, XmHORIZONTAL,
							  XmNleftAttachment, XmATTACH_NONE,
							  XmNtopAttachment, XmATTACH_NONE,
							  XmNrightAttachment, XmATTACH_FORM,
							  XmNbottomAttachment, XmATTACH_FORM,
							  NULL);
#if 0
  /* add to address button
   */
  ac = 0;
  XtSetArg(av[ac], XtNsensitive, False), ac++;
  Widget addToAddressBtn = XmCreatePushButton(dummy2, 
											  "addToAddressBtn", 
											  av, 
											  ac);
  XtManageChild(addToAddressBtn);
  m_addrSearchView->setAdd2ABBtn(addToAddressBtn);
#endif
  /* Properties
   */
  ac = 0;
  //XtSetArg(av[ac], XtNsensitive, False), ac++;
  Widget propertiesBtn = XmCreatePushButton(dummy2, 
											"propertiesBtn", 
											av, 
											ac);
  XtManageChild(propertiesBtn);
  m_addrSearchView->setPropertyBtn(propertiesBtn);

  /* outliner Attachment
   */
  XtVaSetValues(TwoPaneView->getBaseWidget(),
				XmNleftAttachment, XmATTACH_FORM,
				XmNtopAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNbottomAttachment, XmATTACH_WIDGET,
				XmNbottomWidget, cmdGroupForm,
				NULL);

  XtManageChild(cmdGroupForm);

  /*
   * Provide callbacks as for entries outside world
   */
  XtAddCallback(toBtn, 
				XmNactivateCallback, 
				XFE_AddrSearchView::toCallback, 
				m_addrSearchView);

  XtAddCallback(ccBtn, 
				XmNactivateCallback, 
				XFE_AddrSearchView::ccCallback, 
				m_addrSearchView);

  XtAddCallback(bccBtn, 
				XmNactivateCallback, 
				XFE_AddrSearchView::bccCallback, 
				m_addrSearchView);
#if 1
  XtAddCallback(propertiesBtn, 
				XmNactivateCallback, 
				XFE_AB2PaneView::propertiesCallback,
				TwoPaneView);
#else
  XtAddCallback(addToAddressBtn, 
				XmNactivateCallback, 
				XFE_AddrSearchView::addToAddressCallback, 
				m_addrSearchView);
#endif
  /* Addressee UI
   */
  m_addresseeView = 
    new XFE_AddresseeView(this,
			  workForms[AB_ADDRESSEE_VIEW],
			  NULL, m_context);
#if defined(GLUE_COMPO_CONTEXT)
  XtSetSensitive(m_okBtn, False);
  m_addresseeView->setOKBtn(m_okBtn);
#else
  Widget okBtn = XmSelectionBoxGetChild(m_chrome, XmDIALOG_OK_BUTTON);
  XtSetSensitive(okBtn, False);
  m_addresseeView->setOKBtn(okBtn);
#endif /* GLUE_COMPO_CONTEXT */

  m_addrSearchView->setAddressee(m_addresseeView);
  if (callData) {
	  /*  Assume the caller is XFE_AddressFolderView
	   */	  
	  ABAddrMsgCBProcStruc* clientData = 
		  ((XFE_AddressFolderView *)callData)->exportAddressees();
	  addAddressees(clientData);
  }/* if */
#if defined(GLUE_COMPO_CONTEXT)
  // 
  attachView();
#endif /* GLUE_COMPO_CONTEXT */
}

XFE_ABAddressingDlg::~XFE_ABAddressingDlg()
{
#if defined(GLUE_COMPO_CONTEXT)
	if (m_addrSearchView && m_dashboard)
		m_dashboard->disconnectFromDashboard(m_addrSearchView);
#endif
}

void XFE_ABAddressingDlg::cancel()
{
#if !defined(GLUE_COMPO_CONTEXT)
  XFE_ABListSearchView *searchView = (XFE_ABListSearchView *) m_view;
  searchView->unRegisterInterested();
#endif
  hide();
}

void XFE_ABAddressingDlg::ok()
{
  if (m_addresseeView && m_cbProc) {
    ABAddrMsgCBProcStruc *clientData = 
      (ABAddrMsgCBProcStruc *) XP_CALLOC(1, sizeof(ABAddrMsgCBProcStruc));
    clientData->m_pairs = m_addresseeView->getPairs(clientData->m_count); 
    m_cbProc(clientData, m_callData);
  }/* if */
  cancel();
}

void XFE_ABAddressingDlg::addAddressees(ABAddrMsgCBProcStruc *clientData)
{
	if (!m_addresseeView ||
		!clientData)
		return;

	int npairs = clientData->m_count;
	for (int i=0; i < npairs; i++)
		if (clientData->m_pairs[i]) {
			m_addresseeView->addEntry(clientData->m_pairs[i]);
		}/* if */
	XP_FREEIF(clientData->m_pairs);
	XP_FREEIF(clientData);
}


/* C API
 */
extern "C"  void
fe_showAddrMSGDlg(Widget toplevel, 
		  ABAddrMsgCBProc proc, void* callData, MWContext *context)
{
  XFE_ABAddressingDlg *dlg =  
    new XFE_ABAddressingDlg(toplevel, 
			    "AddreMsgWin",
			    proc,
			    callData,
			    True,
			    context);
  dlg->show();
}

