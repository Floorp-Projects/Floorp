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
   BookmarkPropDialog.cpp -- Dialog to edit bookmark properties.
   Created: Stephen Lamm <slamm@netscape.com>, 10-Mar-97.
 */



#include "BookmarkPropDialog.h"
#include "BookmarkView.h"

#include "bkmks.h"
#include "felocale.h"  // for fe_GetTextField()
#include "xfe.h"
#include "xfe2_extern.h"
#include "xpgetstr.h"

#include <Xm/Form.h>
#include <Xm/LabelG.h> 
#include <Xm/PushBG.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h> 

extern int XFE_BM_ALIAS;  // "This is an alias to the following Bookmark:"

XFE_BookmarkPropDialog::XFE_BookmarkPropDialog(MWContext *context, Widget parent)
  : XFE_Dialog(parent, "bookmarkProps",
               TRUE,   // ok
               TRUE,   // cancel
               FALSE,  // help
               FALSE,  // apply
               TRUE,   // Separator
               FALSE  // Modal
               )
{
  int ac;
  Arg av[20];
  Widget kids[20];
  int i;

  m_context = context;
  m_entry = NULL;

  Widget form = 0;
  Widget name_label = 0;
  Widget description_label = 0;
  Widget addedOn_label = 0;


  fe_UnmanageChild_safe (XmSelectionBoxGetChild (m_chrome,
                                                 XmDIALOG_SELECTION_LABEL));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (m_chrome,
                                                 XmDIALOG_TEXT));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (m_chrome,
                                                 XmDIALOG_APPLY_BUTTON));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (m_chrome,
                                                 XmDIALOG_HELP_BUTTON));

  XtAddCallback(m_chrome, XmNdestroyCallback, destroy_cb, this);
  XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);
  XtAddCallback(m_chrome, XmNcancelCallback, close_cb, this);

  ac = 0;
  form =              XmCreateForm (m_chrome, "form", av, ac);
  m_title =           XmCreateLabelGadget(form, "title", av, ac);
  m_aliasLabel =      XmCreateLabelGadget(form, "aliasLabel", av, ac);
  m_aliasButton =     XmCreatePushButtonGadget (form, "aliasButton", av, ac);
  name_label =        XmCreateLabelGadget(form, "nameLabel", av, ac);
  m_locationLabel =   XmCreateLabelGadget(form, "locationLabel", av, ac);
  description_label = XmCreateLabelGadget(form, "descriptionLabel", av, ac);

  if (m_context->type == MWContextBookmarks) {
    m_lastVisitedLabel = XmCreateLabelGadget(form, "lastvisitedLabel", av, ac);
    addedOn_label = XmCreateLabelGadget(form, "addedonLabel", av, ac);

  }

  XtAddCallback(m_aliasButton, XmNactivateCallback, selectalias_cb, this);

  ac = 0;
  m_name = fe_CreateTextField(form, "nameText", av, ac);
  m_location = fe_CreateTextField(form, "locationText", av, ac);

  ac = 0;
  XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
  m_description = fe_CreateText(form, "descriptionText", av, ac);
  
  ac = 0;
  m_lastVisited = XmCreateLabelGadget(form, "lastVisited", av, ac);
  m_addedOn = XmCreateLabelGadget(form, "addedOn", av, ac);
  
  XtVaSetValues(m_title,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		0);
  XtVaSetValues(name_label,
		XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNtopWidget, m_name,
		XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNbottomWidget, m_name,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_name,
		0);
  XtVaSetValues(m_locationLabel,
		XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNtopWidget, m_location,
		XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNbottomWidget, m_location,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_location,
		0);
  XtVaSetValues(description_label,
		XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNtopWidget, m_description,
		XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNbottomWidget, m_description,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_description,
		0);
  XtVaSetValues(m_lastVisitedLabel,
		XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNtopWidget, m_lastVisited,
		XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNbottomWidget, m_lastVisited,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_lastVisited,
		0);
  XtVaSetValues(addedOn_label,
		XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNtopWidget, m_addedOn,
		XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		XmNbottomWidget, m_addedOn,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_addedOn,
		0);
  ac = 0;
  kids[ac++] = m_title;
  kids[ac++] = m_name;
  kids[ac++] = m_location;
  kids[ac++] = m_description;
  kids[ac++] = m_lastVisited;
  kids[ac++] = m_addedOn;
  XtVaSetValues(kids[1],
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, kids[0],
		XmNleftAttachment, XmATTACH_NONE,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_NONE,
		0);
  for (i=2 ; i<ac ; i++) {
    XtVaSetValues(kids[i],
		  XmNtopAttachment, XmATTACH_WIDGET,
		  XmNtopWidget, kids[i-1],
		  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNleftWidget, kids[i-1],
		  XmNrightAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_NONE,
		  0);
  }
  XtVaSetValues(m_aliasLabel,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, kids[ac-1],
		XmNleftAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_aliasButton,
		0);
  XtVaSetValues(m_aliasButton,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, kids[ac-1],
		XmNleftAttachment, XmATTACH_NONE,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		0);

  fe_attach_field_to_labels(m_name, name_label, m_locationLabel,
			    description_label, m_lastVisitedLabel,
			    addedOn_label, 0);
  
  kids[ac++] = m_aliasLabel;
  kids[ac++] = m_aliasButton;
  kids[ac++] = name_label;
  kids[ac++] = m_locationLabel;
  kids[ac++] = description_label;
  kids[ac++] = m_lastVisitedLabel;
  kids[ac++] = addedOn_label;

  XtManageChildren(kids, ac);
  XtManageChild(form);

  fe_HackDialogTranslations (m_chrome);
}

XFE_BookmarkPropDialog::~XFE_BookmarkPropDialog() 
{
  close();
}

void
XFE_BookmarkPropDialog::selectAlias()
{
  if (m_entry)
    BM_SelectAliases(m_context, m_entry);
}

void
XFE_BookmarkPropDialog::selectalias_cb(Widget widget,
                                       XtPointer closure, XtPointer call_data)
{
  XFE_BookmarkPropDialog* obj = (XFE_BookmarkPropDialog *)closure;
  obj->selectAlias();
}

void
XFE_BookmarkPropDialog::destroy_cb(Widget widget,
                                   XtPointer closure, XtPointer call_data)
{
  XFE_BookmarkPropDialog* obj = (XFE_BookmarkPropDialog *)closure;

  obj->close();
}

void
XFE_BookmarkPropDialog::close()
{
  if (m_entry)
    {
      BM_CancelEdit(m_context, m_entry);
      m_entry = NULL;
    }
}

void
XFE_BookmarkPropDialog::close_cb(Widget widget,
                                 XtPointer closure, XtPointer call_data)
{
  XFE_BookmarkPropDialog* obj = (XFE_BookmarkPropDialog *)closure;

  obj->close();

  XtDestroyWidget(obj->getBaseWidget());
}

void
XFE_BookmarkPropDialog::ok()
{
  if (m_entry)
    {
      commitChanges();
      m_entry = NULL;
    }
}

void
XFE_BookmarkPropDialog::ok_cb(Widget widget,
                              XtPointer closure, XtPointer call_data)
{
  XFE_BookmarkPropDialog* obj = (XFE_BookmarkPropDialog *)closure;

  obj->ok();

  XtDestroyWidget(obj->getBaseWidget());
}


void
XFE_BookmarkPropDialog::editItem(BM_Entry *entry)
{
  const char* str;
  char* tmp;

  /* Raise the edit shell to view */
  if (XtIsManaged (CONTEXT_WIDGET (m_context))) {
    XRaiseWindow(XtDisplay(CONTEXT_WIDGET (m_context)),
		 XtWindow(CONTEXT_WIDGET (m_context)));
  }

  commitChanges();

  /* Alias stuff */
  if (BM_IsAlias(entry)) {
    fe_SetString(m_title, XmNlabelString, XP_GetString(XFE_BM_ALIAS));
    entry = BM_GetAliasOriginal(entry);
  }
  else
    fe_SetString(m_title, XmNlabelString, " ");

  m_entry = entry;

  if (BM_IsUrl(entry)) {
    str = BM_GetName(entry);
    fe_SetTextField(m_name, str?str:"");
    str = BM_GetAddress(entry);
    fe_SetTextField(m_location, str?str:"");
    XtVaSetValues(m_location, XmNsensitive, True, 0);
    XtVaSetValues(m_locationLabel, XmNsensitive, True, 0);
    str =  BM_GetDescription(entry);
    fe_SetTextField(m_description, str?str:"");
    tmp = BM_PrettyLastVisitedDate(entry);
    if (!tmp || !*tmp) tmp = " ";
    fe_SetString(m_lastVisited, XmNlabelString, tmp);
    XtVaSetValues(m_lastVisited, XmNsensitive, True, 0);
    XtVaSetValues(m_lastVisitedLabel, XmNsensitive, True, 0);
    fe_SetString(m_addedOn, XmNlabelString, BM_PrettyAddedOnDate(entry));
    fe_SetString(m_aliasLabel, XmNlabelString,
			BM_PrettyAliasCount(m_context, entry));
    if (BM_CountAliases(m_context, entry) > 0)
      XtVaSetValues(m_aliasButton, XmNsensitive, True, 0);
    else
      XtVaSetValues(m_aliasButton, XmNsensitive, False, 0);
  }
  else if (BM_IsHeader(entry)) {
    str = BM_GetName(entry);
    fe_SetTextField(m_name, str?str:"");
    fe_SetTextField(m_location, "");
    XtVaSetValues(m_location, XmNsensitive, False, 0);
    XtVaSetValues(m_locationLabel, XmNsensitive, False, 0);
    str =  BM_GetDescription(entry);
    fe_SetTextField(m_description, str?str:"");
    fe_SetString(m_lastVisited, XmNlabelString, " ");
    XtVaSetValues(m_lastVisited, XmNsensitive, False, 0);
    XtVaSetValues(m_lastVisitedLabel, XmNsensitive, False, 0);
    fe_SetString(m_addedOn, XmNlabelString, BM_PrettyAddedOnDate(entry));
    fe_SetString(m_aliasLabel, XmNlabelString, "" );
    XtVaSetValues(m_aliasButton, XmNsensitive, False, 0);
  }
}

void
XFE_BookmarkPropDialog::entryGoingAway(BM_Entry *entry)
{
  if (m_entry == entry) {
    m_entry = NULL;
    if (m_chrome) {
      fe_SetTextField(m_name, "");
      fe_SetTextField(m_location, "");
      fe_SetTextField(m_description, "");
      if (m_lastVisited) fe_SetString(m_lastVisited, XmNlabelString, " ");
      if (m_addedOn) fe_SetString(m_addedOn, XmNlabelString, " ");
    }
  }
}

char *
XFE_BookmarkPropDialog::getAndCleanText(Widget widget,
                                         Boolean new_lines_too_p)
{
  char	*str;

  str = fe_GetTextField(widget);
  if (!str) {
    return NULL;
  }
  fe_forms_clean_text(m_context, INTL_DefaultWinCharSetID(NULL), str,
                      new_lines_too_p);

  return str;
}


XP_Bool
XFE_BookmarkPropDialog::commitChanges()
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(m_context);
  char* ptr = 0;

  if (!m_entry) return(True);
  ptr = getAndCleanText(m_name, True);
  if (!ptr) {
    return(False);
  }

  // Have the view set the name so it can keep track
  // of the personal toolbar.
  if (view)
    view->setName(m_entry, ptr);
  XP_FREE(ptr);

  ptr = getAndCleanText(m_location, True);
  if (!ptr) {
    return(False);
  }
  BM_SetAddress(m_context, m_entry, ptr);
  XP_FREE(ptr);

  ptr = getAndCleanText(m_description, False);
  if (!ptr) {
    return(False);
  }
  BM_SetDescription(m_context, m_entry, ptr);
  XP_FREE(ptr);

  return(True);
}

