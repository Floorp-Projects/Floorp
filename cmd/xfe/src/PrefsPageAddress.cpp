/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
   PrefsPageAddress.cpp - Mail/News Addressing Preference Pane
   Created: Alec Flett <alecf@netscape.com>
 */

#include "felocale.h"
#include "prefapi.h"
#include "PrefsPageAddress.h"

XFE_PrefsPageAddress::XFE_PrefsPageAddress(XFE_PrefsDialog *dialog)
    : XFE_PrefsPage(dialog),
      m_pLdapServers(0),
      m_dirList(0),
      m_dirCount(0),
      m_dirSelected(0)
{


    // get ready to fill up LDAP Directory combo
    // taken from WinFE
    char location[256];
    int nLen = sizeof(location);
    
    m_pLdapServers=XP_ListNew();
    
    PREF_GetDefaultCharPref("browser.addressbook_location", location, &nLen);
    DIR_GetServerPreferences(&m_pLdapServers, location);

    int count=XP_ListCount(m_pLdapServers);
    // put list into a format DtComboBox can understand (array of pointers)
    m_dirList = (DIR_Server**)XP_CALLOC(count, sizeof(DIR_Server*));
    m_dirNames= (XmString *)  XP_CALLOC(count, sizeof(XmString));
    XP_List *pList=m_pLdapServers;
    DIR_Server *pServer;

    // build up the array of XmStrings, and save the name of the
    // one used for selection
    int i=0;
    while (pServer = (DIR_Server *)XP_ListNextObject(pList)) {
        // only use LDAP servers
        if ((!pServer->description) ||
            (pServer->dirType != LDAPDirectory))
            continue;

        m_dirList[i]=pServer;
        m_dirNames[i]=XmStringCreateLocalized(pServer->description);
        if (DIR_TestFlag(pServer, DIR_AUTO_COMPLETE_ENABLED))
            m_dirSelected=m_dirNames[i];
        i++;
        
    }
    
    m_dirCount=i;

}

XFE_PrefsPageAddress::~XFE_PrefsPageAddress()
{

    DIR_DeleteServerList(m_pLdapServers);
    int i;
    for (i=0; i<m_dirCount; i++)
        XtFree((char *)m_dirNames[i]);
    
}


void XFE_PrefsPageAddress::create()
{
    Arg av[5];
    int ac;

    Widget address_form;

    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;

    m_wPage = address_form =
        XmCreateForm(m_wPageForm, "mailnewsAddress", av,ac);
    XtManageChild(address_form);

    Widget message_frame = createMessageFrame(address_form, NULL);
    Widget sort_frame = createSortFrame(address_form, message_frame);

    setCreated(TRUE);
}


Widget XFE_PrefsPageAddress::createMessageFrame(Widget parent,
                                                Widget attachTo)
{

    Widget kids[10];
    int i;
    Arg av[10];
    int ac;

    XP_ASSERT(parent);

    Widget message_frame;
    ac=0;
    if (attachTo==NULL) {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    }
    else {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
        XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    }
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    message_frame =
        XmCreateFrame(parent,"messageFrame",av,ac);
  
    Widget message_form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    message_form =
        XmCreateForm(message_frame,"messageForm",av,ac);

    Widget message_label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    message_label =
        XmCreateLabelGadget(message_frame, "messageLabel",av,ac);

    // we need this because XmLists don't know thier own visuals, etc
    Visual *v=0;
    Colormap cmap=0;
    Cardinal depth=0;
    XtVaGetValues(getPrefsDialog()->getPrefsParent(),
                  XtNvisual, &v,
                  XtNcolormap, &cmap,
                  XtNdepth, &depth,
                  0);
    ac=0;
    XtSetArg(av[ac], XtNvisual, v); ac++;
    XtSetArg(av[ac], XtNcolormap, cmap); ac++;
    XtSetArg(av[ac], XtNdepth, depth); ac++;
    XtSetArg(av[ac], XmNarrowType, XmMOTIF); ac++;

    i=0;
    Widget complete_label;
    Widget conflict_label;
    Widget only_label;
    Widget conflict_radiobox;
    
    kids[i++] = complete_label =
        XmCreateLabelGadget(message_form, "completeLabel", NULL, 0);
    
    kids[i++] = m_complete_ab_toggle =
        XmCreateToggleButtonGadget(message_form, "completeABToggle", NULL, 0);
    
    kids[i++] = m_complete_dir_toggle =
        XmCreateToggleButtonGadget(message_form, "completeDirToggle", NULL, 0);

    // av holds the visual/colormap/depth stuff
    kids[i++] = m_complete_dir_combo =
        DtCreateComboBox(message_form, "completeDirCombo", av, ac);

    kids[i++] = conflict_label =
        XmCreateLabelGadget(message_form, "conflictLabel", NULL, 0);
    
    kids[i++] = conflict_radiobox =
        XmCreateRadioBox(message_form, "conflictRadioBox", NULL, 0);
    m_conflict_show_toggle =
        XmCreateToggleButtonGadget(conflict_radiobox,"conflictShowToggle",NULL,0);
    m_conflict_accept_toggle =
        XmCreateToggleButtonGadget(conflict_radiobox,"conflictAcceptToggle",NULL,0);

    kids[i++] = only_label =
        XmCreateLabelGadget(message_form, "onlyMatchLabel", NULL, 0);

    kids[i++] = m_only_ab_toggle =
        XmCreateToggleButtonGadget(message_form, "onlyMatchToggle", NULL, 0);



    // place the widgets
    XtVaSetValues(complete_label,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(m_complete_ab_toggle,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, complete_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(m_complete_dir_toggle,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_complete_ab_toggle,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);

    XtVaSetValues(m_complete_dir_combo,
                  XmNitems, m_dirNames,
                  XmNitemCount, m_dirCount,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_complete_dir_toggle,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);    
    XtVaSetValues(conflict_label,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_complete_dir_combo,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);    
    XtVaSetValues(conflict_radiobox,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, conflict_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    
    XtVaSetValues(only_label,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, conflict_radiobox,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(m_only_ab_toggle,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, only_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    
    XtManageChildren(kids,i);
    XtManageChild(m_conflict_show_toggle);
    XtManageChild(m_conflict_accept_toggle);
    XtManageChild(message_frame);
    XtManageChild(message_form);
    XtManageChild(message_label);

    return message_frame;
}

Widget XFE_PrefsPageAddress::createSortFrame(Widget parent, Widget attachTo) {

    Widget kids[3];
    int i;
    Arg av[10];
    int ac;

    XP_ASSERT(parent);

    Widget sort_frame;
    ac=0;
    if (attachTo==NULL) {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    }
    else {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
        XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    }
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    sort_frame =
        XmCreateFrame(parent,"sortFrame",av,ac);
  
    Widget sort_form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    sort_form =
        XmCreateForm(sort_frame,"sortForm",av,ac);
    
    Widget sort_label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    sort_label =
        XmCreateLabelGadget(sort_frame, "sortLabel",av,ac);

    ac=0;
    i=0;

    Widget sort_radiobox;

    sort_radiobox =
        XmCreateRadioBox(sort_form, "sortRadioBox", NULL, 0);

    kids[i++] = m_sort_first_toggle =
        XmCreateToggleButton(sort_radiobox, "sortFirstToggle", NULL, 0);
    kids[i++] = m_sort_last_toggle =
        XmCreateToggleButton(sort_radiobox, "sortLastToggle", NULL, 0);

    XtVaSetValues(sort_radiobox,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  NULL);
    XtManageChild(sort_radiobox);

    XtManageChildren(kids,i);
    XtManageChild(sort_form);
    XtManageChild(sort_frame);
    XtManageChild(sort_label);

    return sort_frame;
}



void XFE_PrefsPageAddress::init() {
    Bool boolval;
    Bool locked;

    PREF_GetBoolPref("ldap_1.autoComplete.useAddressBooks", &boolval);
    locked=PREF_PrefIsLocked("ldap_1.autoComplete.useAddressBooks");
    XtVaSetValues(m_complete_ab_toggle,
                  XmNset, boolval,
                  XmNsensitive, !locked,
                  NULL);

    PREF_GetBoolPref("ldap_1.autoComplete.useDirectory", &boolval);
    locked=PREF_PrefIsLocked("ldap_1.autoComplete.useDirectory");
    XtVaSetValues(m_complete_dir_toggle,
                  XmNset, boolval,
                  XmNsensitive, !locked,
                  NULL);

    if (m_dirSelected) 
        DtComboBoxSelectItem(m_complete_dir_combo, m_dirSelected);
    
    PREF_GetBoolPref("ldap_1.autoComplete.showDialogForMultipleMatches",
                     &boolval);
    locked=
        PREF_PrefIsLocked("ldap_1.autoComplete.showDialogForMultipleMatches");
    XtVaSetValues(m_conflict_show_toggle,
                  XmNset, boolval,
                  XmNsensitive, !locked,
                  NULL);
    XtVaSetValues(m_conflict_accept_toggle,
                  XmNset, !boolval,
                  XmNsensitive, !locked,
                  NULL);
    
    PREF_GetBoolPref("ldap_1.autoComplete.skipDirectoryIfLocalMatchFound",
                     &boolval);
    locked=
        PREF_PrefIsLocked("ldap_1.autoComplete.skipDirectoryIfLocalMatchFound");
    XtVaSetValues(m_only_ab_toggle,
                  XmNset, boolval,
                  XmNsensitive, !locked,
                  NULL);


    PREF_GetBoolPref("mail.addr_book.lastnamefirst", &boolval);
    locked=PREF_PrefIsLocked("mail.addr_book.lastnamefirst");
    XtVaSetValues(m_sort_first_toggle,
                  XmNset, !boolval,
                  XmNsensitive, !locked,
                  NULL);
    XtVaSetValues(m_sort_last_toggle,
                  XmNset, boolval,
                  XmNsensitive, !locked,
                  NULL);
    setInitialized(TRUE);
}

void XFE_PrefsPageAddress::install() {

}

void XFE_PrefsPageAddress::save() {
    Bool boolval;
    int32 intval;

    boolval = XmToggleButtonGadgetGetState(m_complete_ab_toggle);
    PREF_SetBoolPref("ldap_1.autoComplete.useAddressBooks", boolval);
    boolval = XmToggleButtonGadgetGetState(m_complete_dir_toggle);
    PREF_SetBoolPref("ldap_1.autoComplete.useDirectory", boolval);

    // find index of selected server
    XtVaGetValues(m_complete_dir_combo,
                  XmNselectedPosition, &intval,
                  NULL);

    // get corresponding DIR_Server*
    DIR_Server *pServer=m_dirList[intval];
    
    DIR_SetAutoCompleteEnabled(m_pLdapServers, pServer, boolval);
    DIR_SaveServerPreferences(m_pLdapServers);

    boolval = XmToggleButtonGadgetGetState(m_conflict_show_toggle);
    PREF_SetBoolPref("ldap_1.autoComplete.showDialogForMultipleMatches",
                     boolval);
    boolval = XmToggleButtonGadgetGetState(m_only_ab_toggle);
    PREF_SetBoolPref("ldap_1.autoComplete.skipDirectoryIfLocalMatchFound",
                     boolval);

    boolval = XmToggleButtonGadgetGetState(m_sort_last_toggle);
    PREF_SetBoolPref("mail.addr_book.lastnamefirst", boolval);
    
}

Boolean XFE_PrefsPageAddress::verify() {

    return True;
}

