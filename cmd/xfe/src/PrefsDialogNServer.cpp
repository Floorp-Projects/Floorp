/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* -*- Mode: C++; tab-width: 4 -*- */
/*
   PrefsDialogMServer.cpp -- Multiple mail server preferences dialog
   Created: Alec Flett <alecf@netscape.com>
 */

#include "rosetta.h"
#include "PrefsDialogNServer.h"
#include "felocale.h"
#include "prprf.h"
#include "msgcom.h"
#include "Xfe/Geometry.h"

#include "xfe.h"

#include <Xm/Xm.h>
#include <Xm/LabelG.h>
#include <Xm/ToggleB.h>
#include <Xm/ToggleBG.h>
#include <Xm/Text.h>


XFE_PrefsNServerDialog::XFE_PrefsNServerDialog(Widget parent)
    : XFE_Dialog(parent, "NewsServerInfo", 
                 TRUE, TRUE, TRUE, FALSE, TRUE, TRUE),
      m_server_text(0),
      m_port_text(0),
      HG71661
      m_password_toggle(0)
{
    

}

void
XFE_PrefsNServerDialog::create() {
    Widget kids[7];
    int i=0;
    
    Widget form = XmCreateForm(m_chrome, "form", NULL, 0);
    Widget server_label = kids[i++] =
        XmCreateLabelGadget(form, "serverLabel", NULL, 0);
    m_server_text = kids[i++] =
        XmCreateText(form, "serverText", NULL, 0);
    
    Widget port_label = kids[i++] =
        XmCreateLabelGadget(form, "portLabel", NULL, 0);
    m_port_text = kids[i++] =
        XmCreateText(form, "portText", NULL, 0);

    HG90177
    m_password_toggle = kids[i++] =
        XmCreateToggleButtonGadget(form, "passwordToggle", NULL, 0);

    int max_height1 = XfeVaGetTallestWidget(server_label, m_server_text, NULL);
    int max_height2 = XfeVaGetTallestWidget(port_label, m_port_text, NULL);
    // now arrange the widgets
    XtVaSetValues(server_label,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
    XtVaSetValues(m_server_text,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, server_label,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, server_label,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);
    XtVaSetValues(port_label,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, server_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
    XtVaSetValues(m_port_text,
                  XmNheight, max_height2,
                  XmNcolumns,5,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, port_label,
                  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNleftWidget, m_server_text,
                  NULL);
    HG79299
    XtVaSetValues(m_password_toggle,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, HG21897,
                  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNleftWidget, HG17661,
                  NULL);

    XtManageChildren(kids,i);
    XtManageChild(form);

    XtAddCallback(m_chrome, XmNokCallback, cb_ok, this);
    XtAddCallback(m_chrome, XmNcancelCallback, cb_cancel, this);
    HG18261

    setPort(NEWS_PORT);
}

void
XFE_PrefsNServerDialog::init(const char *server, int32 port,
                             XP_Bool xxx, XP_Bool password)
{

    if (m_server_text) fe_SetTextField(m_server_text, server);

    setPort(port);

    HG19877
    if (m_password_toggle)
        XmToggleButtonSetState(m_password_toggle, password, False);

}

void
XFE_PrefsNServerDialog::cb_cancel(Widget, XtPointer closure, XtPointer)
{
    XFE_PrefsNServerDialog *theDialog =
        (XFE_PrefsNServerDialog *)closure;

    theDialog->hide();
    theDialog->m_retVal=FALSE;
    theDialog->m_doneWithLoop=TRUE;
}

void
XFE_PrefsNServerDialog::cb_ok(Widget, XtPointer closure, XtPointer)
{
    XFE_PrefsNServerDialog *theDialog =
        (XFE_PrefsNServerDialog *)closure;

    theDialog->hide();
    theDialog->m_retVal=TRUE;
    theDialog->m_doneWithLoop=TRUE;
}

HG19871



XP_Bool
XFE_PrefsNServerDialog::prompt()
{

    m_doneWithLoop=False;
    show();

    while (!m_doneWithLoop)
        fe_EventLoop();

    return m_retVal;
}

void XFE_PrefsNServerDialog::setPort(int32 port)
{
    HG19773

    char *charval=PR_smprintf("%d",port);
    fe_SetTextField(m_port_text, charval);
    XP_FREE(charval);
}

char *
XFE_PrefsNServerDialog::getServerName()
{
    return fe_GetTextField(m_server_text);
}

int32
XFE_PrefsNServerDialog::getPort()
{
    char *charval = fe_GetTextField(m_port_text);
    int32 port = XP_ATOI(charval);
    XP_FREE(charval);

    return port;
}



XP_Bool
XFE_PrefsNServerDialog::getPassword()
{
    XP_ASSERT(m_password_toggle);
    if (!m_password_toggle) return FALSE;
    return (XmToggleButtonGetState(m_password_toggle)==True) ? TRUE : FALSE;
}
