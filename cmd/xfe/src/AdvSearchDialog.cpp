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
   AdvSearchDialog.cpp -- dialog for specifying options to message search
   Created: Akkana Peck <akkana@netscape.com>, 21-Oct-97.
 */



#include "AdvSearchDialog.h"

#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h>
#include <Xm/ToggleBG.h>

#include "prefapi.h"

// we will need this in the post() method.
extern "C" void fe_EventLoop();

XFE_AdvSearchDialog::XFE_AdvSearchDialog(Widget parent, char *name,
                                         XFE_Frame *frame)
    : XFE_Dialog(parent, 
                 name, 
                 TRUE, // ok
                 TRUE, // cancel
                 FALSE, // help
                 FALSE, // apply
                 TRUE, // separator
                 TRUE // modal
        )
{
    m_frame = frame;

    Widget form = XtCreateManagedWidget("form",
                                        xmFormWidgetClass,
                                        m_chrome,
                                        NULL, 0);

    m_subfolderToggle = XtVaCreateManagedWidget("subfolderToggle",
                                                xmToggleButtonGadgetClass,
                                                form,
                                                XmNleftAttachment,
                                                  XmATTACH_FORM,
                                                XmNtopAttachment,
                                                  XmATTACH_FORM,
                                                0);
    XtAddCallback(m_subfolderToggle, XmNvalueChangedCallback, toggle_cb, this);

    Widget framew = XtVaCreateManagedWidget("frame",
                                           xmFrameWidgetClass,
                                           form,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNrightAttachment, XmATTACH_FORM,
                                           XmNtopAttachment, XmATTACH_WIDGET,
                                           XmNtopWidget, m_subfolderToggle,
                                           0);
    XtVaCreateManagedWidget("whenOnlineSearch",
                            xmLabelGadgetClass,
                            framew,
                            XmNchildType, XmFRAME_TITLE_CHILD,
                            0);
    Arg av[3];
    int ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_WORKAREA_CHILD); ++ac;
    XtSetArg(av[ac], XmNorientation, XmVERTICAL); ++ac;
    Widget rc = XmCreateRadioBox(framew, "radiobox", av, ac);

    m_searchLocalToggle = XtVaCreateManagedWidget("searchLocalToggle",
                                                  xmToggleButtonGadgetClass,
                                                  rc,
                                                  0);
    m_searchServerToggle = XtVaCreateManagedWidget("searchServerToggle",
                                                   xmToggleButtonGadgetClass,
                                                   rc,
                                                   0);
    XtManageChild(rc);

    XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);
    XtAddCallback(m_chrome, XmNcancelCallback, cancel_cb, this);
}

XFE_AdvSearchDialog::~XFE_AdvSearchDialog()
{
    // nothing needed (that I'm aware of) yet.
}

XP_Bool
XFE_AdvSearchDialog::post()
{
    // Set the toggle button states:
    // Note that we're assuming here that XP_Bool is the same as Boolean!
    XP_Bool searchSubfolders, searchServer;
    PREF_GetBoolPref("mailnews.searchSubFolders", &searchSubfolders);
    XmToggleButtonGadgetSetState(m_subfolderToggle, searchSubfolders, FALSE);
    PREF_GetBoolPref("mailnews.searchServer", &searchServer);
    if (searchServer)
        XmToggleButtonGadgetSetState(m_searchServerToggle, TRUE, TRUE);
    else
        XmToggleButtonGadgetSetState(m_searchLocalToggle, TRUE, TRUE);

    m_doneWithLoop = False;
  
    XtVaSetValues(m_chrome,
                  XmNdeleteResponse, XmUNMAP,
                  NULL);

    show();

    while(!m_doneWithLoop)
        fe_EventLoop();

    return m_retVal;
}

void
XFE_AdvSearchDialog::ok()
{
    m_doneWithLoop = True;
    m_retVal = True;

    Boolean searchSubfolders = XmToggleButtonGadgetGetState(m_subfolderToggle);
    Boolean searchServer = XmToggleButtonGadgetGetState(m_searchServerToggle);

    // Note that we're assuming here that XP_Bool is the same as Boolean!
    PREF_SetBoolPref("mailnews.searchSubFolders", searchSubfolders);
    PREF_SetBoolPref("mailnews.searchServer", searchServer);
			
    hide();
}

void
XFE_AdvSearchDialog::cancel()
{
    m_doneWithLoop = True;
    m_retVal = False;

    hide();
}

void
XFE_AdvSearchDialog::toggle()
{
    // do something with value of m_subfolderToggle here
}

void
XFE_AdvSearchDialog::toggle_cb(Widget, XtPointer clientData, XtPointer)
{
    XFE_AdvSearchDialog *obj = (XFE_AdvSearchDialog*)clientData;

    obj->toggle();
}

void 
XFE_AdvSearchDialog::ok_cb(Widget, XtPointer clientData, XtPointer)
{
    XFE_AdvSearchDialog *obj = (XFE_AdvSearchDialog*)clientData;

    obj->ok();
}

void
XFE_AdvSearchDialog::cancel_cb(Widget, XtPointer clientData, XtPointer)
{
    XFE_AdvSearchDialog *obj = (XFE_AdvSearchDialog*)clientData;

    obj->cancel();
}

