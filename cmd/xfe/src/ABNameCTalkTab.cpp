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
   PropertyTabView.cpp -- class definition for PropertyTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#include "PropertySheetView.h"
#include "ABNameFolderDlg.h"
#include "ABNameCTalkTab.h"

#include "abcom.h"

#include <Xm/Form.h>
#include <Xm/TextF.h> 
#include <Xm/Separator.h>

#include <Xm/ToggleB.h>
#include <Xm/LabelG.h> 

#include "DtWidgets/ComboBox.h"

#include "felocale.h"
#include "xpgetstr.h"
#include "xfe.h"

extern int XFE_AB_NAME_COOLTALK_TAB;
extern int XFE_AB_COOLTALK_INFO;
extern int XFE_AB_COOLTALK_DEF_SERVER;
extern int XFE_AB_COOLTALK_SERVER_IK_HOST;
extern int XFE_AB_COOLTALK_DIRECTIP;
extern int XFE_AB_COOLTALK_ADDR_LABEL;
extern int XFE_AB_COOLTALK_ADDR_EXAMPLE;

XFE_ABNameCTalkTabView::XFE_ABNameCTalkTabView(XFE_Component *top,
					       XFE_View *view):
  XFE_PropertyTabView(top, view, XFE_AB_NAME_COOLTALK_TAB)
{

  int ac, numForms = 3;
  Arg    av[20];
  Widget topForm = getBaseWidget(),
         label,
         stripForm[4];

  XmString xmstr;

  for (int i=0; i < numForms; i++) {
	  ac = 0;
	  stripForm[i] = XmCreateForm(topForm, "strip", av, ac);

	  if (i == 0) {
		  label = XtVaCreateManagedWidget("cTalkLabelInfo",
										  xmLabelGadgetClass, stripForm[i],
										  XmNalignment, XmALIGNMENT_BEGINNING, 
										  NULL);

		  xmstr = XmStringCreateLtoR(XP_GetString(XFE_AB_COOLTALK_INFO),
									XmFONTLIST_DEFAULT_TAG);
		  XtVaSetValues(label,
						XmNlabelString, xmstr,
						XmNleftAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_FORM,
						XmNtopAttachment, XmATTACH_FORM,
						XmNbottomAttachment, XmATTACH_NONE,
						0);
		  XmStringFree(xmstr);

		  XtVaSetValues(stripForm[i],
						XmNleftAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_FORM,
						XmNtopAttachment, XmATTACH_FORM,
						XmNbottomAttachment, XmATTACH_NONE,
						XmNtopOffset, 10,
						0);
      
	  }/* if */
	  else if (i == 1) {
		  /* the select prompt label
		   */
		  xmstr = XmStringCreateLtoR(XP_GetString(XFE_AB_COOLTALK_ADDR_LABEL),
									 XmFONTLIST_DEFAULT_TAG);
		  label = XtVaCreateManagedWidget("cTalkServerPrompt",
										  xmLabelGadgetClass, stripForm[i],
										  XmNlabelString, xmstr,
										  XmNalignment, XmALIGNMENT_BEGINNING, 
										  NULL);
		  XmStringFree(xmstr);

		  /* The combo select
		   */
		  Visual   *v = 0;
		  Colormap  cmap = 0;
		  Cardinal  depth = 0;
		  XtVaGetValues (getToplevel()->getBaseWidget(), 
						 XmNvisual, &v, 
						 XmNcolormap, &cmap,
						 XmNdepth, &depth, 
						 NULL);


		  /* Create a combobox for storing directories 
		   */
		  XtSetArg (av[ac], XmNvisual, v); ac++;
		  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
		  XtSetArg (av[ac], XmNdepth, depth); ac++;
		  XtSetArg (av[ac], XmNmoveSelectedItemUp, False); ac++;
		  XtSetArg (av[ac], XmNtype, XmDROP_DOWN_COMBO_BOX); ac++;
		  XtSetArg (av[ac], XmNvisibleItemCount, 3); ac++; 

		  m_serverCombo = DtCreateComboBox(stripForm[i], 
										   "serverCombo", av,ac);
		  XtVaGetValues(m_serverCombo,
						XmNtextField, &(m_textFs[0]), 
						NULL);
		  XtVaSetValues(m_textFs[0],
						XmNeditable, False,
						0);

		  // Add fake items to comboBox
		  int nServers = 3;
		  XmString xmstr;
		  for (int j = 0; j < nServers; j++) {
			  xmstr = XmStringCreateLtoR(XP_GetString(XFE_AB_COOLTALK_DEF_SERVER+j), 
										 XmSTRING_DEFAULT_CHARSET);
			  DtComboBoxAddItem(m_serverCombo, xmstr, 1, True );
			  XmStringFree(xmstr);
		  }/* for j */
		  // attachment
		  XtVaSetValues(label,
						XmNleftAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_NONE,
						XmNtopAttachment, XmATTACH_FORM,
						XmNbottomAttachment, XmATTACH_FORM,
						0);

		  XtVaSetValues(m_serverCombo,
						XmNleftAttachment, XmATTACH_WIDGET,
						XmNleftWidget, label,
						XmNrightAttachment, XmATTACH_FORM,
						XmNtopAttachment, XmATTACH_FORM,
						XmNbottomAttachment, XmATTACH_FORM,
						0);

		  XtManageChild(m_serverCombo);
		  XtAddCallback(m_serverCombo, 
						XmNselectionCallback, 
						XFE_ABNameCTalkTabView::comboSelCallback, 
						this);

		  //
		  XtVaSetValues(stripForm[i], 
						XmNleftAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_FORM,
						XmNtopAttachment, XmATTACH_WIDGET,
						XmNtopWidget, stripForm[i-1],
						XmNtopOffset, 20,
						0);
		  XtVaGetValues(label, 
						XmNwidth, &m_labelWidth,
						0);

		  m_labelWidth += 10;
		  XtVaSetValues(label,
						XmNwidth, m_labelWidth,
						0);
	  }/* else if */
	  else if (i == 2) {
		  /* type in text
		   */
		  ac = 0;
		  m_textFs[1] = fe_CreateTextField(stripForm[i], "serverName",
										  av, ac);
		  /* example label
		   */
		  xmstr = XmStringCreateLtoR(" ",
									 XmFONTLIST_DEFAULT_TAG);
		  m_serverExample = XtVaCreateManagedWidget("cTalkServerPrompt",
													xmLabelGadgetClass, stripForm[i],
													XmNlabelString, xmstr,
													XmNalignment, XmALIGNMENT_BEGINNING,
													NULL);
		  XmStringFree(xmstr);

		  //
		  XtVaSetValues(m_textFs[1],
						XmNleftAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_FORM,
						XmNtopAttachment, XmATTACH_FORM,
						XmNbottomAttachment, XmATTACH_NONE,
						XmNleftOffset, m_labelWidth,
						0);
		  XtVaSetValues(m_serverExample,
						XmNleftAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_FORM,
						XmNtopAttachment, XmATTACH_WIDGET,
						XmNtopWidget, m_textFs[1],
						XmNleftOffset, m_labelWidth,
						XmNbottomAttachment, XmATTACH_FORM,
						0);
		  XtVaSetValues(stripForm[i], 
						XmNleftAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_FORM,
						XmNtopAttachment, XmATTACH_WIDGET,
						XmNtopWidget, stripForm[i-1],
						XmNtopOffset, 10,
						XmNbottomOffset, 10,
						0);
		  XtManageChild(m_textFs[1]);
		  XtSetSensitive(stripForm[i], False);
		  m_typeinTextForm = stripForm[i];
	  }/* else */

	  XtManageChild(stripForm[i]);
  }/* for i */
  // setServerState(0, 0);
}

XFE_ABNameCTalkTabView::~XFE_ABNameCTalkTabView()
{
}


void XFE_ABNameCTalkTabView::comboSelCallback(Widget w, 
											  XtPointer clientData, 
											  XtPointer callData)
{
	XFE_ABNameCTalkTabView *obj = (XFE_ABNameCTalkTabView *) clientData;
	obj->comboSelCB(w, callData);
}

void
XFE_ABNameCTalkTabView::comboSelCB(Widget /* w */, XtPointer callData)
{
  DtComboBoxCallbackStruct *cbData = (DtComboBoxCallbackStruct *)callData;
  if (cbData->reason == XmCR_SELECT ) {
	  char* text = 0;
	  XmString str = cbData->item_or_text;
	  
	  XmStringGetLtoR(str, XmSTRING_DEFAULT_CHARSET, &text);
	  if (*text ) {
		  XmString xmstr;
		  char tmp[128];

		  tmp[0] = '\0';
		  if (XP_STRCMP(text, "Netscape Conference DLS Server") == 0) {
			  XP_SAFE_SPRINTF(tmp, sizeof(tmp),
							  "%s",
							  "");
			  m_serverType = kDefaultDLS;
			  XtSetSensitive(m_typeinTextForm, False);
		  }/* if */
		  else if (XP_STRCMP(text, "Specific DLS Server") == 0) {
			  XP_SAFE_SPRINTF(tmp, sizeof(tmp),
							  XP_GetString(XFE_AB_COOLTALK_ADDR_EXAMPLE),
							  "servername.domain.com");

			  m_serverType = kSpecificDLS;
			  XtSetSensitive(m_typeinTextForm, True);
		  }/* if */
		  else if (XP_STRCMP(text, "Hostname or IP Address") == 0) {
			  XP_SAFE_SPRINTF(tmp, sizeof(tmp),
							  XP_GetString(XFE_AB_COOLTALK_ADDR_EXAMPLE),
							  "name.domain.com or 123.45.678.90");

			  m_serverType = kHostOrIPAddress;
			  XtSetSensitive(m_typeinTextForm, True);
		  }/* if */
		  xmstr = XmStringCreateLtoR(tmp,
									 XmFONTLIST_DEFAULT_TAG);
		  XtVaSetValues(m_serverExample,
						XmNlabelString, xmstr,
						NULL);
		  XmStringFree(xmstr);

		  XtFree(text);
	  }/* if text */
  }/* if */
}

void XFE_ABNameCTalkTabView::setDlgValues()
{
  XFE_ABNameFolderDlg *dlg = (XFE_ABNameFolderDlg *)getToplevel();
#if defined(USE_ABCOM)
  XFE_PropertySheetView *folderView = (XFE_PropertySheetView *) getParent();
  MSG_Pane *pane = folderView->getPane();

  uint16 numItems = 2;
  AB_AttribID * attribs = (AB_AttribID *) XP_CALLOC(numItems, 
													sizeof(AB_AttribID));
  attribs[0] = AB_attribUseServer;
  attribs[1] = AB_attribCoolAddress;

  AB_AttributeValue *values = NULL;
  int error = AB_GetPersonEntryAttributes(pane, 
										  attribs,
										  &values, 
										  &numItems);
  int16 shortValue = kDefaultDLS;
  char *tmp = NULL;
  for (int i=0; i < numItems; i++) {
	  switch (values[i].attrib) {
	  case AB_attribUseServer:
		  shortValue = values[i].u.shortValue;
		  break;

	  case AB_attribCoolAddress:
		  tmp = values[i].u.string;
		  break;

	  default:
		  XP_ASSERT(0);
		  break;
	  }/* switch */
  }/* for i */

  setServerState(shortValue, 
				 tmp);

  XP_FREEIF(attribs);
  AB_FreeEntryAttributeValues(values, numItems);
#else
  PersonEntry& entry = dlg->getPersonEntry();
  setServerState(entry.UseServer, 
				 entry.pCoolAddress);
#endif /* USE_ABCOM */
}

void XFE_ABNameCTalkTabView::apply()
{
}

void XFE_ABNameCTalkTabView::getDlgValues()
{
  XFE_ABNameFolderDlg *dlg = (XFE_ABNameFolderDlg *)getToplevel();
#if defined(USE_ABCOM)
  XFE_PropertySheetView *folderView = (XFE_PropertySheetView *) getParent();
  MSG_Pane *pane = folderView->getPane();

  uint16 numItems = 2;
  AB_AttributeValue *values = 
	  (AB_AttributeValue *) XP_CALLOC(numItems, 
									  sizeof(AB_AttributeValue));
  char *tmp = NULL;

  values[0].attrib = AB_attribUseServer;
  values[0].u.shortValue = m_serverType;

  tmp = fe_GetTextField(m_textFs[1]);
  values[1].attrib = AB_attribCoolAddress;
  if (tmp && strlen(tmp))
	  values[1].u.string = tmp;
  else
	  values[1].u.string = XP_STRDUP("");

  int error = AB_SetPersonEntryAttributes(pane, 
										  values, 
										  numItems);

  AB_FreeEntryAttributeValues(values, numItems);
#else
  PersonEntry& entry = dlg->getPersonEntry();

  char *tmp;
  tmp = fe_GetTextField(m_textFs[1]);
  if (tmp && strlen(tmp))
	  entry.pCoolAddress = tmp;
  else
	  entry.pCoolAddress = XP_STRDUP("");
  entry.UseServer = m_serverType;
#endif /* USE_ABCOM */
}

void XFE_ABNameCTalkTabView::setServerState(short serverType, char* serverName)
{
	XmString xmstr;
	if (serverType == kDefaultDLS) {
		fe_SetTextField(m_textFs[1], "");
		xmstr = XmStringCreateLtoR(XP_GetString(XFE_AB_COOLTALK_DEF_SERVER), 
								   XmSTRING_DEFAULT_CHARSET);
		XtSetSensitive(m_typeinTextForm, False);
	}/* if */
	else if (serverType == kSpecificDLS) {
		fe_SetTextField(m_textFs[1], serverName?serverName:"");
		xmstr = XmStringCreateLtoR(XP_GetString(XFE_AB_COOLTALK_SERVER_IK_HOST), 
								   XmSTRING_DEFAULT_CHARSET);
		XtSetSensitive(m_typeinTextForm, True);
	}/* if */
	else {
		fe_SetTextField(m_textFs[1], serverName?serverName:"");
		xmstr = XmStringCreateLtoR(XP_GetString(XFE_AB_COOLTALK_DIRECTIP), 
								   XmSTRING_DEFAULT_CHARSET);
		XtSetSensitive(m_typeinTextForm, True);
	}/* else */
	DtComboBoxSelectItem(m_serverCombo, xmstr);
	XmStringFree(xmstr);
	m_serverType = serverType;
}
