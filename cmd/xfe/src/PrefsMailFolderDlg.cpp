/* -*- Mode: C++; tab-width: 4 -*-
   PrefsMailFolderDlg.cpp -- Generic dialog for choosing a mail folder
   Copyright © 1998 Netscape Communications Corporation, all rights reserved.
   Created: Alec Flett <alecf@netscape.com>, 05-Mar-98
 */

#include "MozillaApp.h"
#include "xpgetstr.h"
#include "felocale.h"
#include "msgcom.h"
#include "msgmast.h"
#include "PrefsMailFolderDlg.h"
#include "FolderPromptDialog.h"
#include "ViewGlue.h"

#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xfe/Xfe.h>

extern int XFE_CHOOSE_FOLDER_INSTRUCT;
extern int XFE_FOLDER_ON_FORMAT;
extern int MK_MSG_SENT_L10N_NAME;

XFE_PrefsMailFolderDialog::XFE_PrefsMailFolderDialog(Widget parent,
                                                     MSG_Master *master,
                                                     MWContext *context) :
    XFE_Dialog(parent,"prefsMailFolderDialog",TRUE, TRUE, // ok and cancel
               FALSE, FALSE, FALSE, TRUE), // nothing else
    m_l10n_name(0),
    m_retVal(0)
{
    m_context=context;
    m_master=master;
}

XFE_PrefsMailFolderDialog::~XFE_PrefsMailFolderDialog()
{
    // should I be deleting FolderDropdowns?
    // the delete is also commented out on FolderPromptDialog.cpp, line 81
    // delete m_folderDropDown;
    // delete m_serverDropDown;
    delete m_l10n_name;
}

void XFE_PrefsMailFolderDialog::cb_ok(Widget, XtPointer closure, XtPointer)
{
    XFE_PrefsMailFolderDialog *theDialog =
        (XFE_PrefsMailFolderDialog *)closure;
    
    theDialog->hide();
    theDialog->m_retVal=theDialog->getFolder();
    theDialog->m_doneWithLoop=True;
}

void XFE_PrefsMailFolderDialog::cb_cancel(Widget, XtPointer closure, XtPointer)
{
    XFE_PrefsMailFolderDialog *theDialog =
        (XFE_PrefsMailFolderDialog *)closure;
    theDialog->hide();

    theDialog->m_doneWithLoop=True;
}

void XFE_PrefsMailFolderDialog::cb_newFolder(Widget, XtPointer closure, XtPointer)
{
    XFE_PrefsMailFolderDialog *theDialog =
        (XFE_PrefsMailFolderDialog *)closure;

    theDialog->newFolder();
}

void XFE_PrefsMailFolderDialog::newFolder() {

    XFE_FolderPromptDialog *fpd =
        new XFE_FolderPromptDialog(this->getBaseWidget(),
                                   "newFolderDialog",
                                   NULL,
                                   this);
    
    MSG_FolderInfo *folder=m_folderDropdown->getSelectedFolder();
    MSG_FolderInfo *newFolder = fpd->prompt(folder);

    if (newFolder) {
        m_folderDropdown->resyncDropdown();
        setFolder(newFolder);
    }
    delete fpd;

}

MWContext *XFE_PrefsMailFolderDialog::getContext()
{
    return m_context;
}


void XFE_PrefsMailFolderDialog::initPage()
{
    Widget kids[10];
    int i=0;

    Arg av[10];
    Cardinal ac;
    
    XP_ASSERT(m_chrome);
    
    ac=0;
    Widget form = XmCreateForm(m_chrome, "form", av, ac);
    m_instruct_label =
        kids[i++] = XmCreateLabelGadget(form, "instructions",av,ac);
    

    XtVaSetValues(m_instruct_label,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);

    ac=0;
    kids[i++] = m_server_toggle =
        XmCreateToggleButtonGadget(form, "folderOnServer", av, ac);
    kids[i++] = m_folder_toggle =
        XmCreateToggleButtonGadget(form, "specificFolder", av, ac);
    
    m_serverDropdown =
        new XFE_FolderDropdown(this,form, // only show servers 
                               TRUE, FALSE, FALSE, FALSE);
    Widget server_dropdown =
        kids[i++] = m_serverDropdown->getBaseWidget();
                              
    m_folderDropdown =
        new XFE_FolderDropdown(this,form, // only select folders 
                               FALSE, FALSE, FALSE); 
    Widget folder_dropdown =
        kids[i++] = m_folderDropdown->getBaseWidget();
    
    kids[i++] = m_new_folder_button =
        XmCreatePushButtonGadget(form, "newFolder", av, ac);

    int max_height1=XfeVaGetTallestWidget(m_server_toggle,
                                          server_dropdown,
                                          NULL);
    
    int max_height2=XfeVaGetTallestWidget(m_folder_toggle,
                                          folder_dropdown,
                                          m_new_folder_button,
                                          NULL);

    XtVaSetValues(m_server_toggle,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_instruct_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  XmNindicatorType, XmONE_OF_MANY,
                  NULL);

    m_serverDropdown->setPopupServer(FALSE);
    XtVaSetValues(server_dropdown,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_server_toggle,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_server_toggle,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);

    XtVaSetValues(m_folder_toggle,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_server_toggle,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  XmNindicatorType, XmONE_OF_MANY,
                  NULL);

    m_folderDropdown->setPopupServer(FALSE);
    XtVaSetValues(folder_dropdown,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_folder_toggle,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_folder_toggle,
                  XmNrightWidget, XmATTACH_NONE,
                  XmNbottomWidget, XmATTACH_NONE,
                  NULL);
    
    XtVaSetValues(m_new_folder_button,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_folder_toggle,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, folder_dropdown,
                  XmNrightWidget, XmATTACH_FORM,
                  XmNbottomWidget, XmATTACH_NONE,
                  NULL);

    XtManageChild(form);
    XtManageChild(m_instruct_label);
    XtManageChildren(kids, i);
    
    // callbacks
    XtAddCallback(m_chrome, XmNokCallback, cb_ok, this);
    XtAddCallback(m_chrome, XmNcancelCallback, cb_cancel, this);
    XtAddCallback(m_new_folder_button, XmNactivateCallback, cb_newFolder,this);
    XtAddCallback(m_folder_toggle,XmNvalueChangedCallback,cb_folderClick,this);
    XtAddCallback(m_server_toggle,XmNvalueChangedCallback,cb_serverClick,this);

}


MSG_FolderInfo *XFE_PrefsMailFolderDialog::getFolder()
{
    if (XmToggleButtonGetState(m_folder_toggle)) {
        return m_folderDropdown->getSelectedFolder();
    }

    // we have to get the new MSG_FolderInfo * for the
    // l10n'd folder name
    if (XmToggleButtonGetState(m_server_toggle)) {

        MSG_FolderInfo *server=m_serverDropdown->getSelectedFolder();

        // get the URL for the server
        URL_Struct *folderUrl=MSG_ConstructUrlForFolder(NULL, server);
        int urllen=XP_STRLEN(folderUrl->address) + 1 +
                   XP_STRLEN(m_l10n_name);
        char *url = new char[urllen];

        // construct the new URL
        XP_STRCPY(url, folderUrl->address);
        XP_STRCAT(url, "/");
        XP_STRCAT(url, m_l10n_name);

        // get the MSG_FolderInfo for the folder
        MSG_FolderInfo *folder=
            (MSG_FolderInfo *)m_master->FindMailFolder(url, TRUE);
        XP_FREE(url);
        NET_FreeURLStruct(folderUrl);
        
        return folder;
    }
    return NULL;
}

void XFE_PrefsMailFolderDialog::setFolder(MSG_FolderInfo *folder, int l10n_name)
{

    MSG_FolderLine folderLine;
    m_retVal=folder;
    
    MSG_FolderInfo *server=GetHostFolderInfo(folder);
    if (!server) server=MSG_GetLocalMailTree(m_master);

    MSG_GetFolderLineById(m_master,folder,&folderLine);
    if (l10n_name) {
        if (m_l10n_name) XP_FREE(m_l10n_name);
        m_l10n_name = XP_STRDUP(XP_GetString(l10n_name));
    }
    if (!m_l10n_name) m_l10n_name = XP_STRDUP(XP_GetString(MK_MSG_SENT_L10N_NAME));

    // set instructions
    char *char_format = XP_GetString(XFE_CHOOSE_FOLDER_INSTRUCT);
    char *char_string = PR_smprintf(char_format, m_l10n_name);
    XmString xmstr = XmStringCreateLocalized(char_string);
    XtVaSetValues(m_instruct_label,
                  XmNlabelString, xmstr,
                  NULL);
    XmStringFree(xmstr);
    XP_FREE(char_string);

    char_format = XP_GetString(XFE_FOLDER_ON_FORMAT);
    char_string = PR_smprintf(char_format, m_l10n_name, "");
    xmstr = XmStringCreateLocalized(char_string);
    XtVaSetValues(m_server_toggle,
                  XmNlabelString, xmstr,
                  NULL);
    XmStringFree(xmstr);
    XP_FREE(char_string);

    
    m_serverDropdown->selectFolder(server);
    m_folderDropdown->selectFolder(folder);
    if (folder && !XP_STRCMP(m_l10n_name, folderLine.name)) {
        XmToggleButtonSetState(m_server_toggle, True, True);
        // because the first time niether is set, and thus
        // the callback to un-set m_folder_toggle isn't called
        m_folderDropdown->setSensitive(False);
    } else {
        XmToggleButtonSetState(m_folder_toggle, True, True);
        // because the first time niether is set, and thus
        // the callback to un-set m_server_toggle isn't called
        m_serverDropdown->setSensitive(False);
    }
}

void XFE_PrefsMailFolderDialog::cb_folderClick(Widget,
                                            XtPointer closure,
                                            XtPointer callData)
{
    XFE_PrefsMailFolderDialog *theDialog=(XFE_PrefsMailFolderDialog *)closure;
    XmToggleButtonCallbackStruct *cbs =
        (XmToggleButtonCallbackStruct *)callData;
    theDialog->folderToggle(cbs);
}

void XFE_PrefsMailFolderDialog::cb_serverClick(Widget,
                                            XtPointer closure,
                                            XtPointer callData)
{
    XFE_PrefsMailFolderDialog *theDialog=(XFE_PrefsMailFolderDialog *)closure;
    XmToggleButtonCallbackStruct *cbs =
        (XmToggleButtonCallbackStruct *)callData;
    theDialog->serverToggle(cbs);
}

    hide();
void XFE_PrefsMailFolderDialog::folderToggle(XmToggleButtonCallbackStruct *cbs)
{
    XP_ASSERT(cbs);
    m_folderDropdown->setSensitive(cbs->set);
    if (cbs->set)
        XmToggleButtonSetState(m_server_toggle, False, True);

}

void XFE_PrefsMailFolderDialog::serverToggle(XmToggleButtonCallbackStruct *cbs)
{
    XP_ASSERT(cbs);
    m_serverDropdown->setSensitive(cbs->set);
    if (cbs->set)
        XmToggleButtonSetState(m_folder_toggle, False, True);
}

MSG_FolderInfo *XFE_PrefsMailFolderDialog::prompt()
{
    m_doneWithLoop=False;
    show();

    while (!m_doneWithLoop)
        fe_EventLoop();

    hide();
    return m_retVal;
}
