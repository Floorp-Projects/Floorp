/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   ABMListDlg.cpp -- class definition for ABMListDlg
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
   Revised: Tao Cheng <tao@netscape.com>, 19-nov-96
 */

#include "AddrBookView.h"
#include "ABMListDlg.h"
#include "ABMListView.h"
#include "MNView.h"

#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/TextF.h> 
#include <Xm/LabelG.h> 
#include "felocale.h"

#include "xpgetstr.h"
extern "C" {
#include "xfe.h"
};

extern int XFE_AB_MLIST_TITLE;
extern int XFE_AB_MLIST_LISTNAME;
extern int XFE_AB_MLIST_NICKNAME;
extern int XFE_AB_MLIST_DESCRIPTION;
extern int XFE_AB_MLIST_PROMPT;
extern int XFE_AB_REMOVE;

#define MAX_LISTNAME_LEN 31

XFE_ABMListDlg::XFE_ABMListDlg(XFE_View *view, /* the parent view */
			       Widget    parent,
			       char     *name,
			       Boolean   modal,
			       MWContext *context):
  XFE_ViewDialog((XFE_View *) 0, parent, name,
		 context,
		 True, /* ok */
		 True, /* cancel */
		 True, /* help */
		 True, /* apply ; remove */
		 False, /* separator */
		 modal),
  m_mListPane(NULL)
  
{
  m_abView = (XFE_AddrBookView *) view;
  m_AddrBook = m_abView->getAddrBook();
  m_dir = m_abView->getDir();
  m_mailListEntry.Initialize();
  createUI();
}

#if defined(USE_ABCOM)
XFE_ABMListDlg::XFE_ABMListDlg(MSG_Pane  *pane,
							   MWContext *context):
	XFE_ViewDialog((XFE_View *) 0, 
				   CONTEXT_WIDGET(context), 
				   "abMListProperties",
				   context,
				   True, /* ok */
				   True, /* cancel */
				   True, /* help */
				   True, /* apply ; remove */
				   False, /* separator */
				   True),
	m_pane(pane)
{
	/*
	 */
  createUI();
}
#endif /* USE_ABCOM */

XFE_ABMListDlg::~XFE_ABMListDlg() 
{
	m_mailListEntry.CleanUp();
}/* XFE_ABMListDlg() */

void 
XFE_ABMListDlg::createUI()
{
  Arg av[8];
  int ac;

  /* Form: m_chrome is the dialog 
   */
  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  Widget form = XmCreateForm (m_chrome, "mailListForm", av, ac);
  XtManageChild (form);

  /* Frame: 
   */
  XtSetArg (av [ac], XmNwidth, 500); ac++;
  Widget frame =  XmCreateFrame(form, "mailListFrame", av, ac);
  XtManageChild (frame);

  Widget labelTitle;
  labelTitle= XtVaCreateManagedWidget(XP_GetString(XFE_AB_MLIST_TITLE),
				      xmLabelGadgetClass, frame,
				      XmNchildType, XmFRAME_TITLE_CHILD,
				      XmNalignment, XmALIGNMENT_CENTER, 
				      NULL);
  ac = 0;
  XtSetArg (av[ac], XmNchildType, XmFRAME_WORKAREA_CHILD); ac++;
  Widget workForm = XmCreateForm(frame, "workAreaForm", av, ac);
  XtManageChild(workForm);

  /* Inside the frame
   */
  char *genTabLabels[4];

  genTabLabels[0] = XP_GetString(XFE_AB_MLIST_LISTNAME);
  genTabLabels[1] = XP_GetString(XFE_AB_MLIST_NICKNAME);
  genTabLabels[2] = XP_GetString(XFE_AB_MLIST_DESCRIPTION);
  genTabLabels[3] = XP_GetString(XFE_AB_MLIST_PROMPT);

  Widget stripForm[4], 
         label;
  for (int i = 0; i < 4; i++) {
    ac = 0;
    stripForm[i] = XmCreateForm(workForm, "lisNameForm", av, ac);


    /* Label/ attachment
     */
    label = XtVaCreateManagedWidget (genTabLabels[i],
				     xmLabelGadgetClass, stripForm[i],
				     XmNalignment, 
				     (i < 3)?
				     XmALIGNMENT_END:
				     XmALIGNMENT_BEGINNING, 
				     NULL);
    if (i != 3) {
      XtVaSetValues(label, 
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_NONE,
		    XmNbottomAttachment, XmATTACH_FORM,
		    0);
    }/* if */
    else {
      XtVaSetValues(label, 
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_NONE,
		    0);
    }/* else */


    /* textF / attachment
     */
     if (i != 3) {
      /* TextF
       */
      ac = 0;
      m_textFs[i] = fe_CreateTextField(stripForm[i], (char *) genTabLabels[i],
				      av, ac);
      XtVaSetValues(m_textFs[i], 
		    XmNleftAttachment, XmATTACH_WIDGET,
		    XmNleftWidget, label,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_FORM,
		    0);
      XtManageChild(m_textFs[i]);

      // callbacks
	  if (i == 0)
		  XtAddCallback(m_textFs[i], 
						XmNvalueChangedCallback, 
						XFE_ABMListDlg::entryTTYValChgCallback, 
						this);


    }/* if */
    else {
      /* TextF
       */
      ac = 0;
      //XtSetArg (av [ac], XmNwidth, 180); ac++;
      m_textFs[i] = fe_CreateTextField(stripForm[i], (char *) genTabLabels[i],
				      av, ac);
      XtVaSetValues(m_textFs[i], 
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNtopWidget, label,
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_NONE,
		    0);
      XtManageChild(m_textFs[i]);
  
      /* mailing list view
       */
      XFE_ABMListView *listView = new XFE_ABMListView((XFE_Component *)this, 
						      stripForm[i],
						      m_abView->getDir(), 
						      m_abView->getAddrBook(),
						      NULL,
						      m_abView->getContext(),
						      fe_getMNMaster());
      XtVaSetValues(listView->getBaseWidget(),
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNtopWidget, m_textFs[i],
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_FORM,
		    NULL);

      setView(listView);
      listView->show();

      // callbacks
      XtAddCallback(m_textFs[i], 
		    XmNvalueChangedCallback, 
		    XFE_ABMListView::entryTTYValChgCallback, 
		    listView);

      XtAddCallback(m_textFs[i], 
		    XmNactivateCallback, 
		    XFE_ABMListView::entryTTYActivateCallback, 
		    listView);
    }/* else */
     
    /* form attachment
     */
    if (!i)
      XtVaSetValues(stripForm[i],
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_NONE,
		    NULL);
    else
      XtVaSetValues(stripForm[i],
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNtopWidget, stripForm[i-1],
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_NONE,
		    NULL);

    XtManageChild(stripForm[i]);
  }/* for i */

  //
  // we don't want a default value, since return does other stuff for
  // us.
  XtVaSetValues(m_chrome, /* the dialog */
		XmNdefaultButton, NULL,
		NULL);


  // Set Apply to Remove
  fe_SetString(m_applyButton, XmNlabelString, XP_GetString(XFE_AB_REMOVE));
  
}/* createUI */

void XFE_ABMListDlg::cancel()
{
  hide();
}

void XFE_ABMListDlg::apply()
{
  /* remove 
   */
  ((XFE_ABMListView *)m_view)->remove();
}

void XFE_ABMListDlg::ok()
{
	getDlgValues();

	if (!(((XFE_ABMListView *)m_view)->apply(m_mailListEntry.pNickName, 
											 m_mailListEntry.pFullName, 
											 m_mailListEntry.pInfo))) {
		m_abView->changeEntryCount();
		m_okToDestroy = TRUE;
		cancel();
	}/* if */
	m_okToDestroy = FALSE;
}

void 
XFE_ABMListDlg::setDlgValues()
{
}

void XFE_ABMListDlg::setDlgValues(ABID entry, Boolean newList)
{
  m_newList = newList;
  m_entry = entry;

  /* The dialog
   */
  if (entry != MSG_VIEWINDEXNONE && !newList) {
    char        a_line[AB_MAX_STRLEN];

    a_line[0] = '\0';
    if (AB_GetFullName(m_dir, m_AddrBook, entry, a_line) != MSG_VIEWINDEXNONE) 
      m_mailListEntry.pFullName = XP_STRDUP(a_line);

    a_line[0] = '\0';
    if (AB_GetNickname(m_dir, m_AddrBook, entry, a_line)!= MSG_VIEWINDEXNONE)
      m_mailListEntry.pNickName = XP_STRDUP(a_line);

    a_line[0] = '\0';
    if (AB_GetInfo(m_dir, m_AddrBook, entry, a_line)!= MSG_VIEWINDEXNONE)
      m_mailListEntry.pInfo = XP_STRDUP(a_line);
  }/* if */
  else {
    /* New list
     */
    //Initialize();
    m_mailListEntry.pFullName = XP_STRDUP("New Mailing List");
  }/* else */

  fe_SetTextField(m_textFs[0],
		m_mailListEntry.pFullName?m_mailListEntry.pFullName:"");
  fe_SetTextField(m_textFs[1],
		m_mailListEntry.pNickName?m_mailListEntry.pNickName:"");
  fe_SetTextField(m_textFs[2],
		m_mailListEntry.pInfo?m_mailListEntry.pInfo:"");

  /* the view
   */
  /* mlist entry
   */
  m_entry = ((XFE_ABMListView *)m_view)->setValues(newList?0:m_entry);
  m_mListPane = ((XFE_ABMListView *)m_view)->getMListPane();
}

void XFE_ABMListDlg::getDlgValues()
{
  char *tmp;
  tmp = fe_GetTextField(m_textFs[0]);
  if (tmp && strlen(tmp))
	  m_mailListEntry.pFullName = tmp;
  else
	  m_mailListEntry.pFullName = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[1]);
  if (tmp && strlen(tmp))
	  m_mailListEntry.pNickName = tmp;
  else
	  m_mailListEntry.pNickName = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[2]);
  if (tmp && strlen(tmp))
	  m_mailListEntry.pInfo = tmp;
  else
	  m_mailListEntry.pInfo = XP_STRDUP("");
}

void XFE_ABMListDlg::Initialize()
{
  XP_STRCPY(m_mailListEntry.pFullName, "");
  XP_STRCPY(m_mailListEntry.pNickName, "");
  XP_STRCPY(m_mailListEntry.pInfo, "");
  XP_STRCPY(m_mailListEntry.pDistName, "");
}/* XFE_ABMListDlg::Initialize() */

void XFE_ABMListDlg::entryTTYValChgCallback(Widget w, 
											XtPointer clientData, 
											XtPointer callData)
{
  XFE_ABMListDlg *obj = (XFE_ABMListDlg *) clientData;
  obj->entryTTYValChgCB(w, callData);
}

void
XFE_ABMListDlg::entryTTYValChgCB(Widget w, XtPointer /* callData */)
{
  char *str;
  str = fe_GetTextField(w);
  if (str && XP_STRLEN(str) > MAX_LISTNAME_LEN) {
	  str[MAX_LISTNAME_LEN] = '\0';
	  fe_SetTextField(w, str);
	  XmTextFieldSetCursorPosition(w, MAX_LISTNAME_LEN);
	  XBell(XtDisplay(w), 100);
  }/* if */
}

#if defined(USE_ABCOM)
extern "C" int
fe_ShowPropertySheetForMList(MSG_Pane *pane, MWContext *context)
{
	XFE_ABMListDlg* listDlg = 
		new XFE_ABMListDlg(pane, context);
	listDlg->setDlgValues();
	listDlg->show();
	return 1;
}
#endif
/* XFE_ABListSearchView::popupListPropertyWindow() */
