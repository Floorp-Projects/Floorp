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
   BookmarkWhatsNewDialog.cpp -- dialog for checking "What's New".
   Created: Stephen Lamm <slamm@netscape.com>, 28-May-97.
 */



#include "BookmarkWhatsNewDialog.h"
#include "bkmks.h"
#include "xfe.h"
#include "xpgetstr.h"

extern int XFE_LOOK_FOR_DOCUMENTS_THAT_HAVE_CHANGED_ON;
extern int XFE_ESTIMATED_TIME_REMAINING_CHECKED;
extern int XFE_ESTIMATED_TIME_REMAINING_CHECKING;
extern int XFE_DONE_CHECKING_ETC;

XFE_BookmarkWhatsNewDialog::XFE_BookmarkWhatsNewDialog(MWContext *context, Widget parent)
  : XFE_Dialog(parent, "bookmarksWhatsChanged",
               TRUE,   // ok
               TRUE,   // cancel
               FALSE,  // help
               TRUE,   // apply
               TRUE,   // Separator
               FALSE  // Modal
               )
{
  m_context = context;

  fe_UnmanageChild_safe (m_okButton);

  XtVaSetValues(m_chrome, XmNchildPlacement, XmPLACE_BELOW_SELECTION, NULL);

  m_text   = XmSelectionBoxGetChild (m_chrome, XmDIALOG_SELECTION_LABEL);

  XtManageChild(m_text);

  XtAddCallback(m_chrome, XmNdestroyCallback, destroy_cb, this);
  XtAddCallback(m_chrome, XmNcancelCallback, close_cb, this);
  XtAddCallback(m_chrome, XmNapplyCallback, apply_cb, this);
  XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);

  m_radioBox =
    XmVaCreateSimpleRadioBox(m_chrome, "radiobox", 0, NULL,
			     XmVaRADIOBUTTON, NULL, NULL, NULL, NULL,
			     XmVaRADIOBUTTON, NULL, NULL, NULL, NULL,
			     NULL);

  int numKids;
  Widget* kids;

  XtVaGetValues(m_radioBox, XmNnumChildren, &numKids, XmNchildren, &kids, 0);
  XP_ASSERT(numKids == 2);

  m_doAll      = kids[0];
  m_doSelected = kids[1];

  XtManageChildren(kids, numKids);

  XtManageChild(m_radioBox);

  XmString str = XmStringCreate( XP_GetString( XFE_LOOK_FOR_DOCUMENTS_THAT_HAVE_CHANGED_ON ), 
                                 XmFONTLIST_DEFAULT_TAG); 
  XtVaSetValues(m_text, XmNlabelString, str, NULL);
  XmStringFree(str);

  fe_HackDialogTranslations (m_chrome);
}

XFE_BookmarkWhatsNewDialog::~XFE_BookmarkWhatsNewDialog()
{
  close();
}

void
XFE_BookmarkWhatsNewDialog::destroy_cb(Widget /*widget*/,
                                   XtPointer closure, XtPointer /*call_data*/)
{
  XFE_BookmarkWhatsNewDialog* obj = (XFE_BookmarkWhatsNewDialog *)closure;

  obj->close();
}


void
XFE_BookmarkWhatsNewDialog::close()
{
  BM_CancelWhatsChanged(m_context);
}

void
XFE_BookmarkWhatsNewDialog::close_cb(Widget /*widget*/,
                                 XtPointer closure, XtPointer /*call_data*/)
{
  XFE_BookmarkWhatsNewDialog* obj = (XFE_BookmarkWhatsNewDialog *)closure;

  XtDestroyWidget(obj->getBaseWidget());
}

void
XFE_BookmarkWhatsNewDialog::apply()
{
  Boolean doselected = FALSE;
  XtVaGetValues(m_doSelected, XmNset, &doselected, NULL);
  BM_StartWhatsChanged(m_context, doselected);
}

void
XFE_BookmarkWhatsNewDialog::apply_cb(Widget /*widget*/,
                              XtPointer closure, XtPointer /*call_data*/)
{
  XFE_BookmarkWhatsNewDialog* obj = (XFE_BookmarkWhatsNewDialog *)closure;

  obj->apply();
}

void
XFE_BookmarkWhatsNewDialog::ok()
{
  ; // nothing more to do
}

void
XFE_BookmarkWhatsNewDialog::ok_cb(Widget /*widget*/,
                              XtPointer closure, XtPointer /*call_data*/)
{
  XFE_BookmarkWhatsNewDialog* obj = (XFE_BookmarkWhatsNewDialog *)closure;

  obj->ok();

  XtDestroyWidget(obj->getBaseWidget());
}

void
XFE_BookmarkWhatsNewDialog::updateWhatsChanged(const char* url,
                                               int32 done, int32 total,
                                               const char* totaltime)
{
  char buf[1024];
  XmString str;

  if (!m_chrome) return;

  if (m_radioBox) {
    fe_UnmanageChild_safe(m_radioBox);
    fe_UnmanageChild_safe(m_applyButton);
    m_radioBox = NULL;
  }

  if ( url )
    PR_snprintf(buf, sizeof(buf),
                XP_GetString( XFE_ESTIMATED_TIME_REMAINING_CHECKED ),
                url,
                total - done,
                total > 0 ? done * 100 / total : 0,
                totaltime);
  else
    PR_snprintf(buf, sizeof(buf),
                XP_GetString( XFE_ESTIMATED_TIME_REMAINING_CHECKING ),
                total - done,
                total > 0 ? done * 100 / total : 0,
                totaltime);

  str = XmStringCreateLtoR(buf, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(m_text, XmNlabelString, str, NULL);
  XmStringFree(str);
}

void 
XFE_BookmarkWhatsNewDialog::finishedWhatsChanged(int32 totalchecked,
                                                 int32 numreached, 
                                                 int32 numchanged)
{
  char buf[1024];
  XmString str;
  fe_UnmanageChild_safe(m_radioBox);
  fe_UnmanageChild_safe(m_cancelButton);
  XtManageChild(m_okButton);
  PR_snprintf(buf, sizeof(buf), XP_GetString( XFE_DONE_CHECKING_ETC ),
	      totalchecked, numreached, numchanged);

  str = XmStringCreateLtoR(buf, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(m_text, XmNlabelString, str, NULL);
  XmStringFree(str);
}
