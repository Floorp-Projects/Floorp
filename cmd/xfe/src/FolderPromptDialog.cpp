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
   FolderPromptDialog.cpp -- "New Folder" dialog
   Created: Akkana Peck <akkana@netscape.com>, 11-Dec-97.
 */



#include "FolderPromptDialog.h"

#include "felocale.h"

#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>

#include "FolderDropdown.h"

// we need this in the post() method.
extern "C" void fe_EventLoop();

XFE_FolderPromptDialog::XFE_FolderPromptDialog(Widget parent, char *name,
                                               XFE_Frame *frame,
                                               XFE_Component* toplevel)
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
    m_toplevel = toplevel;

    Widget form = XtCreateManagedWidget("form",
                                        xmFormWidgetClass,
                                        m_chrome,
                                        NULL, 0);

    m_text = XtVaCreateManagedWidget("folderText",
                                     xmTextFieldWidgetClass,
                                     form,
                                     XmNtopAttachment, XmATTACH_FORM,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     0);
    XtVaCreateManagedWidget("name", xmLabelGadgetClass, form,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNrightAttachment, XmATTACH_WIDGET,
                            XmNrightWidget, m_text,
                            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                            XmNbottomWidget, m_text,
                            0);
    Widget w = XtVaCreateManagedWidget("subFolderOf", xmLabelGadgetClass, form,
                                       XmNtopAttachment, XmATTACH_WIDGET,
                                       XmNtopWidget, m_text,
                                       XmNleftAttachment, XmATTACH_FORM,
                                       0);

    m_folderDropDown = new XFE_FolderDropdown(getToplevel(),
                                              form,
                                              TRUE, /* allowServerSelection */
                                              FALSE /* showNewsgroups */);
    XtVaSetValues(m_folderDropDown->getBaseWidget(),
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, w,
                  XmNrightAttachment, XmATTACH_FORM,
                  0);
    m_folderDropDown->setPopupServer(False);
    m_folderDropDown->show();

    XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);
    XtAddCallback(m_chrome, XmNcancelCallback, cancel_cb, this);
}

XFE_FolderPromptDialog::~XFE_FolderPromptDialog()
{
    //delete m_folderDropDown;
}

MSG_FolderInfo*
XFE_FolderPromptDialog::prompt(MSG_FolderInfo* finfo)
{
    m_folderDropDown->selectFolder(finfo);

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
XFE_FolderPromptDialog::ok()
{
    m_doneWithLoop = True;
    char* name = fe_GetTextField(m_text);
    m_retVal = 0;

    //
    // Supposedly, MSG_CreateMailFolder() is obsoleted by
    // MSG_CreateMailFolderWithPane(); but we don't necessarily have
    //  a pane here, e.g. we might have been called from the prefs win.
    //
    MSG_Master* master = FE_GetMaster();
    if (MSG_CreateMailFolder(master,
                             m_folderDropDown->getSelectedFolder(),
                             name) == 0)
    {
#if 0
        // NOTE!  This is fraught with problems!  MSG_CreateMailFolder()
        // doesn't return a pointer to the folder it just created; so we
        // search by name, but there's no guarantee that the folder found
        // by the folder dropdown is the same folder that we just created,
        // only that it has the same name. :-(
        m_folderDropDown->selectFolder(name);
        m_retVal = m_folderDropDown->getSelectedFolder();
#endif
        int32 numchildren = 
            MSG_GetFolderChildren(master,
                                  m_folderDropDown->getSelectedFolder(),
                                  NULL, 0);
        if (numchildren > 0)
        {
            MSG_FolderInfo** result = new MSG_FolderInfo* [ numchildren ];
            MSG_GetFolderChildren(master,
                                  m_folderDropDown->getSelectedFolder(),
                                  result,
                                  sizeof result / sizeof *result);
            for (int i=0; i<numchildren; ++i)
            {
                char* childname = fe_Basename(MSG_GetFolderNameFromID(result[i]));
                printf("'%s' vs '%s'\n", name, childname);
                if (!XP_STRCMP(name, childname))
                {
                    m_retVal = result[i];
                    printf("matched!\n");
                    break;
                }
            }
            delete[] result;
        }
    }

    hide();
}

void
XFE_FolderPromptDialog::cancel()
{
    m_doneWithLoop = True;
    m_retVal = 0;

    hide();
}

void 
XFE_FolderPromptDialog::ok_cb(Widget, XtPointer clientData, XtPointer)
{
    XFE_FolderPromptDialog *obj = (XFE_FolderPromptDialog*)clientData;

    obj->ok();
}

void
XFE_FolderPromptDialog::cancel_cb(Widget, XtPointer clientData, XtPointer)
{
    XFE_FolderPromptDialog *obj = (XFE_FolderPromptDialog*)clientData;

    obj->cancel();
}

