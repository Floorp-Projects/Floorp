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
   ABNameConTabView.cpp -- class definition for ABNameConTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#include "PropertySheetView.h"
#include "ABNameConTab.h"
#include "ABNameFolderDlg.h"
#include "AddrBookView.h"

#include <Xm/Form.h>
#include <Xm/TextF.h> 
#include <Xm/Text.h> 
#include <Xm/Separator.h>

#include <Xm/ToggleB.h>
#include <Xm/LabelG.h> 

#include "felocale.h"
#include "xpgetstr.h"

extern int XFE_AB_NAME_CONTACT_TAB;
extern int XFE_AB_ADDRESS;
extern int XFE_AB_CITY;
extern int XFE_AB_STATE;
extern int XFE_AB_ZIP;
extern int XFE_AB_COUNTRY;
extern int XFE_AB_WORKPHONE;
extern int XFE_AB_FAX;
extern int XFE_AB_HOMEPHONE;
extern int XFE_AB_PAGER;
extern int XFE_AB_CELLULAR;

XFE_ABNameConTabView::XFE_ABNameConTabView(XFE_Component *top,
					   XFE_View *view):
  XFE_PropertyTabView(top, view, XFE_AB_NAME_CONTACT_TAB)
{

  int ac, numForms = AB_LAST;
  Arg    av[20];
  Widget topForm = getBaseWidget(),
         label,
         stripForm[AB_LAST];
  Dimension width;
  char *genTabLabels[AB_LAST];

  genTabLabels[AB_ADDRESS] = XP_GetString(XFE_AB_ADDRESS);
  genTabLabels[AB_CITY] = XP_GetString(XFE_AB_CITY);
  genTabLabels[AB_STATE] = XP_GetString(XFE_AB_STATE);
  genTabLabels[AB_ZIP] = XP_GetString(XFE_AB_ZIP);
  genTabLabels[AB_COUNTRY] = XP_GetString(XFE_AB_COUNTRY);

  genTabLabels[AB_WORKPHONE] = XP_GetString(XFE_AB_WORKPHONE);
  genTabLabels[AB_FAXPHONE] = XP_GetString(XFE_AB_FAX);
  genTabLabels[AB_HOMEPHONE] = XP_GetString(XFE_AB_HOMEPHONE);
  genTabLabels[AB_PAGER] = XP_GetString(XFE_AB_PAGER);
  genTabLabels[AB_CELLULAR] = XP_GetString(XFE_AB_CELLULAR);

  int i; // for use in multiple for loops... without breaking newer ANSI C++ rules
  for (i=0; i < numForms; i++) {
    ac = 0;
    stripForm[i] = XmCreateForm(topForm, "strip", av, ac);

    /* Create labels
     */
    label = XtVaCreateManagedWidget(genTabLabels[i],
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
	
	if (i == 0) {
		/* Text
		 */
		ac = 0;
		XtSetArg (av[ac], XmNheight, 60); ac++;
		XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
		m_textFs[i] = fe_CreateText(stripForm[i], "longAddress", av, ac);
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
	}/* else */

    if (i == 0) {
		XtVaSetValues(stripForm[i], 
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_FORM,
					  0);
#if 0 /* out for 5.0 
	   */
		ac = 0;
		addr2TextFForm = XmCreateForm(topForm, "strip", av, ac);

		/* TextF
		 */
		ac = 0;
		m_addr2TextF = fe_CreateTextField(addr2TextFForm, 
										"addr2TextF",
										av, ac);
		XtVaSetValues(m_addr2TextF,
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNbottomAttachment, XmATTACH_FORM,
					  0);
		XtManageChild(m_addr2TextF);
		XtVaSetValues(addr2TextFForm,
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_WIDGET,
					  XmNtopWidget, stripForm[i],
					  0);
		XtManageChild(addr2TextFForm);
#endif
	}
    else
      XtVaSetValues(stripForm[i], 
					XmNleftAttachment, XmATTACH_FORM,
					XmNrightAttachment, XmATTACH_FORM,
					XmNtopAttachment, XmATTACH_WIDGET,
					XmNtopWidget, 
					stripForm[i-1],
					XmNtopOffset, (i == AB_WORKPHONE)?15:3,
					0);

    XtManageChild(stripForm[i]);
  }/* for i*/

  for (i=0; i < AB_LAST; i++) {
	  XtVaSetValues(m_labels[i], 
					XmNwidth, m_labelWidth,
					0);
  }/* for i */
#if 0 
  XtVaSetValues(m_addr2TextF, 
				XmNleftOffset, m_labelWidth,
				0);
#endif
}

XFE_ABNameConTabView::~XFE_ABNameConTabView()
{
}

void XFE_ABNameConTabView::setDlgValues()
{
  XFE_ABNameFolderDlg *dlg = (XFE_ABNameFolderDlg *)getToplevel();
#if defined(USE_ABCOM)
  XFE_PropertySheetView *folderView = (XFE_PropertySheetView *) getParent();
  MSG_Pane *pane = folderView->getPane();

  uint16 numItems = AB_LAST;
  AB_AttribID * attribs = (AB_AttribID *) XP_CALLOC(numItems, 
													sizeof(AB_AttribID));
  attribs[AB_ADDRESS] = AB_attribStreetAddress;
  attribs[AB_CITY] = AB_attribLocality;
  attribs[AB_STATE] = AB_attribRegion;
  attribs[AB_ZIP] = AB_attribZipCode;
  attribs[AB_COUNTRY] = AB_attribCountry;
  attribs[AB_WORKPHONE] = AB_attribWorkPhone;
  attribs[AB_FAXPHONE] = AB_attribFaxPhone;
  attribs[AB_HOMEPHONE] = AB_attribHomePhone;
  attribs[AB_PAGER] = AB_attribPager;
  attribs[AB_CELLULAR] = AB_attribCellularPhone; // cellular

  AB_AttributeValue *values = NULL;
  int error = AB_GetPersonEntryAttributes(pane, 
										  attribs,
										  &values, 
										  &numItems);
  char *tmp = NULL;

  for (int i=0; i < numItems; i++) {
	  switch (values[i].attrib) {
	  case AB_attribStreetAddress:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_ADDRESS], 
						  tmp?tmp:"");
		  break;

	  case AB_attribLocality:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_CITY], 
						  tmp?tmp:"");
		  break;

	  case AB_attribRegion:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_STATE], 
						  tmp?tmp:"");
		  break;

	  case AB_attribZipCode:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_ZIP], 
						  tmp?tmp:"");
		  break;

	  case AB_attribCountry:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_COUNTRY], 
						  tmp?tmp:"");
		  break;

	  case AB_attribWorkPhone:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_WORKPHONE], 
						  tmp?tmp:"");
		  break;

	  case AB_attribFaxPhone:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_FAXPHONE], 
						  tmp?tmp:"");
		  break;

	  case AB_attribHomePhone:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_HOMEPHONE], 
						  tmp?tmp:"");
		  break;

	  case AB_attribPager:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_PAGER], 
						  tmp?tmp:"");
		  break;

	  case AB_attribCellularPhone:
		  tmp = values[i].u.string;
		  fe_SetTextField(m_textFs[AB_CELLULAR], 
						  tmp?tmp:"");
		  break;
	  }/* switch */
  }/* for i */

  XP_FREEIF(attribs);
  AB_FreeEntryAttributeValues(values, numItems);
#else
  PersonEntry& entry = dlg->getPersonEntry();

  /* We switch the sequence of displaying these two fields
   * so that it won't appear in reverse order in vcard
   */
  fe_SetTextField(m_textFs[AB_ADDRESS],   entry.pPOAddress?entry.pPOAddress:"");
  //fe_SetTextField(m_addr2TextF,           entry.pAddress?entry.pAddress:"");
  fe_SetTextField(m_textFs[AB_CITY],      entry.pLocality?entry.pLocality:"");
  fe_SetTextField(m_textFs[AB_STATE],     entry.pRegion?entry.pRegion:"");
  fe_SetTextField(m_textFs[AB_ZIP],       entry.pZipCode?entry.pZipCode:"");
  fe_SetTextField(m_textFs[AB_COUNTRY],   entry.pCountry?entry.pCountry:"");
  fe_SetTextField(m_textFs[AB_WORKPHONE], entry.pWorkPhone?entry.pWorkPhone:"");
  fe_SetTextField(m_textFs[AB_FAXPHONE],  entry.pFaxPhone?entry.pFaxPhone:"");
  fe_SetTextField(m_textFs[AB_HOMEPHONE], entry.pHomePhone?entry.pHomePhone:"");
#endif /* USE_ABCOM */
}

void XFE_ABNameConTabView::apply()
{
}

void XFE_ABNameConTabView::getDlgValues()
{
  XFE_ABNameFolderDlg *dlg = (XFE_ABNameFolderDlg *)getToplevel();
#if defined(USE_ABCOM)
  XFE_PropertySheetView *folderView = (XFE_PropertySheetView *) getParent();
  MSG_Pane *pane = folderView->getPane();

  uint16 numItems = AB_LAST;
  AB_AttribID * attribs = (AB_AttribID *) XP_CALLOC(numItems, 
													sizeof(AB_AttribID));
  AB_AttributeValue *values = 
	  (AB_AttributeValue *) XP_CALLOC(numItems, 
									  sizeof(AB_AttributeValue));
  char *tmp = NULL;

  //
  tmp = fe_GetTextField(m_textFs[AB_ADDRESS]);
  values[AB_ADDRESS].attrib = AB_attribStreetAddress;
  if (tmp && strlen(tmp))
	  values[AB_ADDRESS].u.string = tmp;
  else
	  values[AB_ADDRESS].u.string = XP_STRDUP("");


  //
  tmp = fe_GetTextField(m_textFs[AB_CITY]);
  values[AB_CITY].attrib = AB_attribLocality;
  if (tmp && strlen(tmp))
	  values[AB_CITY].u.string = tmp;
  else
	  values[AB_CITY].u.string = XP_STRDUP("");

  //
  tmp = fe_GetTextField(m_textFs[AB_STATE]);
  values[AB_STATE].attrib = AB_attribRegion;
  if (tmp && strlen(tmp))
	  values[AB_STATE].u.string = tmp;
  else
	  values[AB_STATE].u.string = XP_STRDUP("");


  //
  tmp = fe_GetTextField(m_textFs[AB_ZIP]);
  values[AB_ZIP].attrib = AB_attribZipCode;
  if (tmp && strlen(tmp))
	  values[AB_ZIP].u.string = tmp;
  else
	  values[AB_ZIP].u.string = XP_STRDUP("");


  //
  tmp = fe_GetTextField(m_textFs[AB_COUNTRY]);
  values[AB_COUNTRY].attrib = AB_attribCountry;
  if (tmp && strlen(tmp))
	  values[AB_COUNTRY].u.string = tmp;
  else
	  values[AB_COUNTRY].u.string = XP_STRDUP("");


  //
  tmp = fe_GetTextField(m_textFs[AB_WORKPHONE]);
  values[AB_WORKPHONE].attrib = AB_attribWorkPhone;
  if (tmp && strlen(tmp))
	  values[AB_WORKPHONE].u.string = tmp;
  else
	  values[AB_WORKPHONE].u.string = XP_STRDUP("");


  //
  tmp = fe_GetTextField(m_textFs[AB_FAXPHONE]);
  values[AB_FAXPHONE].attrib = AB_attribFaxPhone;
  if (tmp && strlen(tmp))
	  values[AB_FAXPHONE].u.string = tmp;
  else
	  values[AB_FAXPHONE].u.string = XP_STRDUP("");


  //
  tmp = fe_GetTextField(m_textFs[AB_HOMEPHONE]);
  values[AB_HOMEPHONE].attrib = AB_attribHomePhone;
  if (tmp && strlen(tmp))
	  values[AB_HOMEPHONE].u.string = tmp;
  else
	  values[AB_HOMEPHONE].u.string = XP_STRDUP("");


  //
  tmp = fe_GetTextField(m_textFs[AB_PAGER]);
  values[AB_PAGER].attrib = AB_attribPager;
  if (tmp && strlen(tmp))
	  values[AB_PAGER].u.string = tmp;
  else
	  values[AB_PAGER].u.string = XP_STRDUP("");


  //
  tmp = fe_GetTextField(m_textFs[AB_CELLULAR]);
  values[AB_CELLULAR].attrib = AB_attribCellularPhone;
  if (tmp && strlen(tmp))
	  values[AB_CELLULAR].u.string = tmp;
  else
	  values[AB_CELLULAR].u.string = XP_STRDUP("");


  int error = AB_SetPersonEntryAttributes(pane, 
										  values, 
										  numItems);

  AB_FreeEntryAttributeValues(values, numItems);
#else
  PersonEntry& entry = dlg->getPersonEntry();

  /* setting up the defaults 
   */
  char *tmp;
  /* We switch the sequence of displaying these two fields
   * so that it won't appear in reverse order in vcard
   */
  tmp = fe_GetTextField(m_textFs[AB_ADDRESS]);
  if (tmp && strlen(tmp))
	  entry.pPOAddress = tmp;
  else
	  entry.pPOAddress = XP_STRDUP("");
  tmp = fe_GetTextField(m_textFs[AB_CITY]);
  if (tmp && strlen(tmp))
	  entry.pLocality = tmp;
  else
	  entry.pLocality = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_STATE]);
  if (tmp && strlen(tmp))
	  entry.pRegion = tmp;
  else
	  entry.pRegion = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_ZIP]);
  if (tmp && strlen(tmp))
	  entry.pZipCode = tmp;
  else
	  entry.pZipCode = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_COUNTRY]);
  if (tmp && strlen(tmp))
	  entry.pCountry = tmp;
  else
	  entry.pCountry = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_WORKPHONE]);
  if (tmp && strlen(tmp))
	  entry.pWorkPhone = tmp;
  else
	  entry.pWorkPhone = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_FAXPHONE]);
  if (tmp && strlen(tmp))
	  entry.pFaxPhone = 
		  fe_GetTextField(m_textFs[AB_FAXPHONE]);
  else
	  entry.pFaxPhone = XP_STRDUP("");

  tmp = fe_GetTextField(m_textFs[AB_HOMEPHONE]);
  if (tmp && strlen(tmp))
	  entry.pHomePhone = tmp;
  else
	  entry.pHomePhone = XP_STRDUP("");

#endif /* USE_ABCOM */
}

