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
   ABNameGenTabView.cpp -- class definition for ABNameGenTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#include "PropertySheetView.h"
#include "ABNameFolderDlg.h"
#include "ABNameGenTab.h"
#include "AddrBookView.h"

#include <Xm/Form.h>
#include <Xm/TextF.h> 
#include <Xm/Text.h> 
#include <Xm/Separator.h>

#include <Xm/ToggleB.h>
#include <Xm/LabelG.h> 

#include "felocale.h"
#include "xpgetstr.h"

extern int XFE_AB_NAME_CARD_FOR;
extern int XFE_AB_NAME_NEW_CARD;

extern int XFE_AB_NAME_GENERAL_TAB;
extern int XFE_AB_DISPLAYNAME;
extern int XFE_AB_FIRSTNAME;
extern int XFE_AB_LASTNAME;
extern int XFE_AB_COMPANY;
extern int XFE_AB_TITLE;
extern int XFE_AB_EMAIL;
extern int XFE_AB_NICKNAME;
extern int XFE_AB_NOTES;
extern int XFE_AB_PREFHTML;

XFE_ABNameGenTabView::XFE_ABNameGenTabView(XFE_Component *top,
					   XFE_View *view):
  XFE_PropertyTabView(top, view, XFE_AB_NAME_GENERAL_TAB)
{

  int ac, numForms = AB_LAST+2;
  Arg    av[20];
  Widget topForm = getBaseWidget(),
         label = NULL,
         stripForm[AB_LAST+2];
  char *genTabLabels[AB_LAST+1];
  Dimension width;

  genTabLabels[AB_FIRST_NAME] = XP_GetString(XFE_AB_FIRSTNAME);
  genTabLabels[AB_LAST_NAME] = XP_GetString(XFE_AB_LASTNAME);
  genTabLabels[AB_DISPLAY_NAME] = XP_GetString(XFE_AB_DISPLAYNAME);

  genTabLabels[AB_EMAIL] = XP_GetString(XFE_AB_EMAIL);
  genTabLabels[AB_NICKNAME] = XP_GetString(XFE_AB_NICKNAME);

  genTabLabels[AB_TITLE] = XP_GetString(XFE_AB_TITLE);
  genTabLabels[AB_COMPANY_NAME] = XP_GetString(XFE_AB_COMPANY);
  genTabLabels[AB_LAST] = XP_GetString(XFE_AB_NOTES);

  int i; // for use in multiple for loops... 
         // without breaking newer ANSI C++ rules
  for (i=0; i < numForms; i++) {
    ac = 0;

    stripForm[i] = XmCreateForm(topForm, "strip", av, ac);
    if (i < (AB_LAST+1)) {
      /* Create labels
       */
      label = XtVaCreateManagedWidget (genTabLabels[i],
									   xmLabelGadgetClass, stripForm[i],
									   XmNalignment, XmALIGNMENT_END, 
									   NULL);
	  m_labels[i] = label;
      XtVaSetValues(label, 
					XmNleftAttachment, XmATTACH_FORM,
					XmNtopAttachment, XmATTACH_FORM,
					XmNrightAttachment, XmATTACH_NONE,
					XmNbottomAttachment, XmATTACH_FORM,
					0);
	  XtVaGetValues(label, 
					XmNwidth, &width,
					0);

	  m_labelWidth = (m_labelWidth < width)?width:m_labelWidth;

      /* TextF / Text
       */
      if (i < AB_LAST) {
		  /* TextF
		   */
		  ac = 0;
		  m_textFs[i] = fe_CreateTextField(stripForm[i], 
										  (char *) genTabLabels[i],
										  av, ac);
		  XtVaSetValues(m_textFs[i], 
						XmNleftAttachment, XmATTACH_WIDGET,
						XmNleftWidget, label,
						XmNtopAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_FORM,
						XmNbottomAttachment, XmATTACH_FORM,
						0);
		  XtManageChild(m_textFs[i]);
      }/* if */
      else {
		  /* Text
		   */
		  ac = 0;
		  XtSetArg (av[ac], XmNheight, 100); ac++;
		  XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
		  m_notesTxt = fe_CreateText(stripForm[i], "notesText", av, ac);
		  XtVaSetValues(m_notesTxt, 
						XmNleftAttachment, XmATTACH_WIDGET,
						XmNleftWidget, label,
						XmNtopAttachment, XmATTACH_FORM,
						XmNrightAttachment, XmATTACH_FORM,
						XmNbottomAttachment, XmATTACH_FORM,
						0);
		  XtManageChild(m_notesTxt);
		  
      }/* else */
    }/* if */
    else {
		/* the toggle */
		XmString xmstr;
		xmstr = XmStringCreate(XP_GetString(XFE_AB_PREFHTML),
							   XmFONTLIST_DEFAULT_TAG);
		ac = 0;
		XtSetArg (av[ac], XmNlabelString, xmstr); ac++;
		m_prefHTMLTog = XmCreateToggleButton(stripForm[i], "prefHTMLTog", 
											 av, ac);
		XtVaSetValues(m_prefHTMLTog, 
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNleftOffset, m_labelWidth,
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNrightAttachment, XmATTACH_NONE,
					  XmNbottomAttachment, XmATTACH_FORM,
					  0);
		XtManageChild(m_prefHTMLTog);
		
    }/* else */
    if (i == 0)
		XtVaSetValues(stripForm[i], 
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNtopOffset, 10,
					  0);
    else
		XtVaSetValues(stripForm[i], 
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_WIDGET,
					  XmNtopWidget, stripForm[i-1],
					  XmNtopOffset, (i == AB_TITLE || 
									 i == AB_EMAIL || 
									 i == AB_LAST)?10:3,
									 0);
					  XtManageChild(stripForm[i]);
  }/* for i */
  for (i=0; i < (AB_LAST+1); i++) {
	  XtVaSetValues(m_labels[i], 
					XmNwidth, m_labelWidth,
					0);
  }/* for i */

}

XFE_ABNameGenTabView::~XFE_ABNameGenTabView()
{
}

void XFE_ABNameGenTabView::setDlgValues()
{
  /* Get mode, entryid, and ab_view
   */
  XFE_ABNameFolderDlg *dlg = (XFE_ABNameFolderDlg *)getToplevel();
#if defined(USE_ABCOM)
  XFE_PropertySheetView *folderView = (XFE_PropertySheetView *) getParent();
  MSG_Pane *pane = folderView->getPane();

  uint16 numItems = AB_LAST+2;
  AB_AttribID * attribs = (AB_AttribID *) XP_CALLOC(numItems, 
													sizeof(AB_AttribID));
  attribs[AB_FIRST_NAME] = AB_attribGivenName;
  attribs[AB_LAST_NAME] = AB_attribFamilyName;
  attribs[AB_DISPLAY_NAME] = AB_attribDisplayName;
  attribs[AB_EMAIL] = AB_attribEmailAddress;
  attribs[AB_NICKNAME] = AB_attribNickName;
  attribs[AB_TITLE] = AB_attribTitle;
  attribs[AB_COMPANY_NAME] = AB_attribCompanyName;
  attribs[AB_LAST] = AB_attribInfo;
  attribs[AB_LAST+1] = AB_attribHTMLMail;

  AB_AttributeValue *values = NULL;
  int error = AB_GetPersonEntryAttributes(pane, 
										  attribs,
										  &values, 
										  &numItems);
  char *tmp = NULL;
  for (int i=0; i < numItems; i++) {
	  switch (values[i].attrib) {
	  case AB_attribGivenName:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_FIRST_NAME], 
						  tmp?tmp:"");
		  break;

	  case AB_attribFamilyName:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_LAST_NAME], 
					  tmp?tmp:"");
		  break;
		  
	  case AB_attribDisplayName:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_DISPLAY_NAME], 
						  tmp?tmp:"");
		  break;
		  
	  case AB_attribEmailAddress:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_EMAIL], 
						  tmp?tmp:"");
		  break;
		  
	  case AB_attribNickName:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_NICKNAME], 
						  tmp?tmp:"");
		  break;

	  case AB_attribTitle:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_TITLE], 
						  tmp?tmp:"");
		  break;
		  
	  case AB_attribCompanyName:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_COMPANY_NAME], 
						  tmp?tmp:"");
		  break;
		  
	  case AB_attribInfo:
		  // AB_attribInfo
		  tmp = values[i].u.string;
		  fe_SetTextField(m_notesTxt, 
						  tmp?tmp:"");
		  break;
		  
	  case AB_attribHTMLMail:
		  XmToggleButtonSetState(m_prefHTMLTog, 
								 values[i].u.boolValue, FALSE);
		  break;
		  
	  }/* switch */
  }/* for i */

  XP_FREEIF(attribs);
  AB_FreeEntryAttributeValues(values, numItems);

#else
  PersonEntry& entry = dlg->getPersonEntry();
  fe_SetTextField(m_textFs[AB_FIRST_NAME], 
		entry.pGivenName?entry.pGivenName:"");
  fe_SetTextField(m_textFs[AB_LAST_NAME], 
		entry.pFamilyName?entry.pFamilyName:"");
  fe_SetTextField(m_textFs[AB_COMPANY_NAME], 
		entry.pCompanyName?entry.pCompanyName:"");
  fe_SetTextField(m_textFs[AB_TITLE], entry.pTitle?entry.pTitle:"");
  fe_SetTextField(m_textFs[AB_EMAIL], 
		entry.pEmailAddress?entry.pEmailAddress:"");
  fe_SetTextField(m_textFs[AB_NICKNAME], entry.pNickName?entry.pNickName:"");
  fe_SetTextField(m_notesTxt, entry.pInfo?entry.pInfo:""); 
  XmToggleButtonSetState(m_prefHTMLTog, entry.HTMLmail, FALSE);
#endif /* USE_ABCOM */
}

void XFE_ABNameGenTabView::getDlgValues()
{
#if defined(DEBUG_tao_)
	printf("\n XFE_ABNameGenTabView::getDlgValues \n");
#endif
  XFE_ABNameFolderDlg *dlg = (XFE_ABNameFolderDlg *)getToplevel();
#if defined(USE_ABCOM)
  XFE_PropertySheetView *folderView = (XFE_PropertySheetView *) getParent();
  MSG_Pane *pane = folderView->getPane();

  uint16 numItems = AB_LAST+1;
  AB_AttributeValue *values = 
	  (AB_AttributeValue *) XP_CALLOC(numItems, 
									  sizeof(AB_AttributeValue));
  char *tmp = NULL;

  //
  tmp = fe_GetTextField(m_textFs[AB_FIRST_NAME]);
  values[AB_FIRST_NAME].attrib = AB_attribGivenName;
  if (tmp && strlen(tmp))
	  values[AB_FIRST_NAME].u.string = tmp;
  else
	  values[AB_FIRST_NAME].u.string = XP_STRDUP("");

  //
  tmp = fe_GetTextField(m_textFs[AB_LAST_NAME]);
  values[AB_LAST_NAME].attrib = AB_attribFamilyName;
  if (tmp && strlen(tmp))
	  values[AB_LAST_NAME].u.string = tmp;
  else
	  values[AB_LAST_NAME].u.string = XP_STRDUP("");

  // AB_attribInfo
  tmp = fe_GetTextField(m_notesTxt);
  values[AB_DISPLAY_NAME].attrib = AB_attribInfo;
  if (tmp && strlen(tmp))
	  values[AB_DISPLAY_NAME].u.string = tmp;
  else
	  values[AB_DISPLAY_NAME].u.string = XP_STRDUP("");

  //
  tmp = fe_GetTextField(m_textFs[AB_EMAIL]);
  values[AB_EMAIL].attrib = AB_attribEmailAddress;
  if (tmp && strlen(tmp))
	  values[AB_EMAIL].u.string = tmp;
  else
	  values[AB_EMAIL].u.string = XP_STRDUP("");

  //
  tmp = fe_GetTextField(m_textFs[AB_NICKNAME]);
  values[AB_NICKNAME].attrib = AB_attribNickName;
  if (tmp && strlen(tmp))
	  values[AB_NICKNAME].u.string = tmp;
  else
	  values[AB_NICKNAME].u.string = XP_STRDUP("");

  //
  tmp = fe_GetTextField(m_textFs[AB_TITLE]);
  values[AB_TITLE].attrib = AB_attribTitle;
  if (tmp && strlen(tmp))
	  values[AB_TITLE].u.string = tmp;
  else
	  values[AB_TITLE].u.string = XP_STRDUP("");

  //
  tmp = fe_GetTextField(m_textFs[AB_COMPANY_NAME]);
  values[AB_COMPANY_NAME].attrib = AB_attribCompanyName;
  if (tmp && strlen(tmp))
	  values[AB_COMPANY_NAME].u.string = tmp;
  else
	  values[AB_COMPANY_NAME].u.string = XP_STRDUP("");

  //
  values[AB_LAST].u.boolValue = XmToggleButtonGetState(m_prefHTMLTog);
  values[AB_LAST].attrib = AB_attribHTMLMail;

  //set values 
  int error = AB_SetPersonEntryAttributes(pane, 
										  values, 
										  numItems);
  AB_FreeEntryAttributeValues(values, numItems);

#else
  PersonEntry& entry = dlg->getPersonEntry();

  /* setting up the defaults 
   */
  char *tmp;

  tmp = fe_GetTextField(m_textFs[AB_NICKNAME]);
  if (tmp && strlen(tmp))
	  entry.pNickName = tmp;
  else
	  entry.pNickName = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_FIRST_NAME]);
  if (tmp && strlen(tmp))
	  entry.pGivenName = tmp;
  else
	  entry.pGivenName = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_LAST_NAME]);
  if (tmp && strlen(tmp))
	  entry.pFamilyName = tmp;
  else
	  entry.pFamilyName = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_COMPANY_NAME]);
  if (tmp && strlen(tmp))
	  entry.pCompanyName = tmp;
  else
	  entry.pCompanyName = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_TITLE]);
  if (tmp && strlen(tmp))
	  entry.pTitle = tmp;
  else
	  entry.pTitle = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_EMAIL]);
  if (tmp && strlen(tmp))
	  entry.pEmailAddress = 
		  fe_GetTextField(m_textFs[AB_EMAIL]);
  else
	  entry.pEmailAddress = XP_STRDUP("");

  tmp = fe_GetTextField(m_notesTxt);
  if (tmp && strlen(tmp))
	  entry.pInfo = tmp;
  else
	  entry.pInfo = XP_STRDUP("");

  entry.HTMLmail = XmToggleButtonGetState(m_prefHTMLTog);
#endif /* USE_ABCOM */

  // set title
  if (tmp = dlg->getFullname()) {
	  char tmp2[AB_MAX_STRLEN];
	  XP_SAFE_SPRINTF(tmp2, sizeof(tmp2),
					  XP_GetString(XFE_AB_NAME_CARD_FOR),
					  tmp);
	  dlg->setCardName(tmp2);
	  XP_FREE((char *) tmp);
  }/* if */
  else 
	  dlg->setCardName(XP_GetString(XFE_AB_NAME_NEW_CARD));
}
