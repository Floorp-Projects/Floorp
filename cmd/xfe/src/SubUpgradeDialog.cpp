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


#include "felocale.h"
#include "xfe.h"
#include "xpgetstr.h"
#include "SubUpgradeDialog.h"

extern "C" int XFE_SUBUPGRADE_AUTOSUBSCRIBE;

extern "C" MSG_IMAPUpgradeType
fe_promptIMAPSubscriptionUpgrade(MWContext *context,
                                    const char *hostName)
{
    MSG_IMAPUpgradeType retval;
    XFE_SubUpgradeDialog *sud = new XFE_SubUpgradeDialog(context,hostName);

    retval=sud->prompt();
    delete sud;

    return retval;
}

XFE_SubUpgradeDialog::XFE_SubUpgradeDialog(MWContext *context,
                                           const char *hostName)
    :XFE_Dialog(CONTEXT_WIDGET(context),
                "SubUpgradeDialog",TRUE, TRUE, FALSE, FALSE, FALSE), // ok and cancel
     m_retVal(MSG_IMAPUpgradeDont),
     m_doneWithLoop(0),
     m_hostname(hostName)
{
    create();
    init();
}
XFE_SubUpgradeDialog::~XFE_SubUpgradeDialog() {

}

void XFE_SubUpgradeDialog::create()
{

    Widget form;
    Widget paragraph_label;

    Widget selection_radiobox;

    form=XmCreateForm(m_chrome, "form", NULL,0);
    
    paragraph_label=XmCreateLabelGadget(form, "paragraphLabel", NULL,0);
    selection_radiobox=XmCreateRadioBox(form, "selectionBox", NULL,0);
    
    m_automatic_toggle=XmCreateToggleButtonGadget(selection_radiobox,"automaticToggle",NULL,0);
    m_custom_toggle=XmCreateToggleButtonGadget(selection_radiobox,"customToggle",NULL,0);

    char *automatic_toggle=
        PR_smprintf(XP_GetString(XFE_SUBUPGRADE_AUTOSUBSCRIBE), m_hostname);
    XmString automatic_toggle_string =
        XmStringCreateLocalized(automatic_toggle);
    
    XtVaSetValues(m_automatic_toggle,
                  XmNlabelString, automatic_toggle_string,
                  NULL);
    XP_FREE(automatic_toggle);
    XmStringFree(automatic_toggle_string);
    
    XtVaSetValues(paragraph_label,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNalignment, XmALIGNMENT_BEGINNING,
                  NULL);

    XtVaSetValues(selection_radiobox,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, paragraph_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  NULL);

    XtManageChild(m_custom_toggle);
    XtManageChild(m_automatic_toggle);
    XtManageChild(selection_radiobox);
    XtManageChild(paragraph_label);
    XtManageChild(form);

    XtAddCallback(m_chrome, XmNokCallback, cb_ok, this);
    XtAddCallback(m_chrome, XmNcancelCallback, cb_cancel, this);
    
}

void XFE_SubUpgradeDialog::init()
{
    XmToggleButtonGadgetSetState(m_automatic_toggle, True, True);
}

void XFE_SubUpgradeDialog::cb_ok(Widget, XtPointer closure, XtPointer)
{
    ((XFE_SubUpgradeDialog*)closure)->ok();
}



void XFE_SubUpgradeDialog::cb_cancel(Widget, XtPointer closure, XtPointer)
{
    ((XFE_SubUpgradeDialog*)closure)->cancel();
}

void XFE_SubUpgradeDialog::ok() {
    if (XmToggleButtonGadgetGetState(m_automatic_toggle))
        m_retVal=MSG_IMAPUpgradeAutomatic;
    else if (XmToggleButtonGadgetGetState(m_custom_toggle))
        m_retVal=MSG_IMAPUpgradeCustom;
    
    m_doneWithLoop=True;
    
}

void XFE_SubUpgradeDialog::cancel() {
    m_doneWithLoop=True;
}

MSG_IMAPUpgradeType XFE_SubUpgradeDialog::prompt() {
    m_doneWithLoop=False;
    show();
    while (!m_doneWithLoop)
        fe_EventLoop();

    hide();
    return m_retVal;
}
