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
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*
   PrefsPageNServer.cpp - News Server preference pane
   Created: Alec Flett <alecf@netscape.com>
 */

#include "rosetta.h"
#include "felocale.h"
#include "prefapi.h"
#include "msgcom.h"
#include "xpgetstr.h"
#include "Xfe/Geometry.h"
#include "PrefsPageNServer.h"
#include "PrefsDialogNServer.h"

extern int MK_MSG_REMOVE_HOST_CONFIRM;
extern int XFE_NEWSSERVER_DEFAULT;

// #include "xfe.h"

// these are the preferences I'm using:
#define NEWS_DIRECTORY "news.directory"
#define NEWS_MAX_ARTICLES "news.max_articles"
#define NEWS_NOTIFY "news.notify.on"

XFE_PrefsPageNServer::XFE_PrefsPageNServer(XFE_PrefsDialog *dialog)
    : XFE_PrefsPage(dialog),
      m_newsHosts(0),
      m_newsDialog(0)
{
}
XFE_PrefsPageNServer::~XFE_PrefsPageNServer()
{
    if (m_newsDialog) delete m_newsDialog;
}

void
XFE_PrefsPageNServer::create()
{
    Arg av[5];
    int ac;

    Widget form;

    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;

    m_wPage = form =
        XmCreateForm(m_wPageForm, "mailnewsNServer", av,ac);
    XtManageChild(form);

    Widget server_frame = createServerListFrame(form, NULL);
    /* Widget local_frame = */ createLocalFrame(form, server_frame);

    setCreated(TRUE);
}

Widget
XFE_PrefsPageNServer::createServerListFrame(Widget parent,
                                            Widget attachTo)
{
    Widget kids[7];
    int i=0;
    Arg av[10];
    int ac;
    Widget frame;
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
    frame =
        XmCreateFrame(parent,"serverFrame",av,ac);
    
    Widget form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNfractionBase, 3); ac++;
    form =
        XmCreateForm(frame,"serverForm",av,ac);

    Widget label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label =
        XmCreateLabelGadget(frame, "serverLabel",av,ac);

    
    // we need this crap because XmLists are stupid
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

    // create the widgets
    i=0;
    Widget add_button;
    Widget edit_button;
    Widget delete_button;
    Widget default_button;
    
    m_server_list =
        XmCreateScrolledList(form, "serverList", av, ac);

    kids[i++] = add_button =
        XmCreatePushButtonGadget(form, "serverAddButton", NULL, 0);
    kids[i++] = edit_button =
        XmCreatePushButtonGadget(form, "serverEditButton", NULL, 0);
    kids[i++] = delete_button =
        XmCreatePushButtonGadget(form, "serverDeleteButton", NULL, 0);
    kids[i++] = default_button =
        XmCreatePushButtonGadget(form, "serverDefaultButton", NULL, 0);

    // place the widgets
    XtVaSetValues(XtParent(m_server_list),
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_POSITION,
                  XmNrightPosition, 2, // 2/3 of the width
                  XmNbottomAttachment, XmATTACH_FORM,
                  NULL);
    XtVaSetValues(add_button,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, XtParent(m_server_list),
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(edit_button,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, add_button,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, XtParent(m_server_list),
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(delete_button,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, edit_button,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, XtParent(m_server_list),
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(default_button,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, delete_button,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, XtParent(m_server_list),
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  NULL);

    XtAddCallback(add_button, XmNactivateCallback, cb_addServer, this);
    XtAddCallback(edit_button, XmNactivateCallback, cb_editServer, this);
    XtAddCallback(delete_button, XmNactivateCallback, cb_deleteServer, this);
    XtAddCallback(default_button, XmNactivateCallback, cb_setDefault, this);
    
    XtManageChild(m_server_list);
    XtManageChildren(kids,i);
    XtManageChild(form);
    XtManageChild(label);
    XtManageChild(frame);

    // set default doesn't work yet.
    XtSetSensitive(default_button, False);
    
    return frame;
}    


Widget
XFE_PrefsPageNServer::createLocalFrame(Widget parent,
                                       Widget attachTo)
{
    Widget kids[7];
    int i=0;
    Arg av[10];
    int ac;
    Widget frame;
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
    frame =
        XmCreateFrame(parent,"localFrame",av,ac);
  
    Widget form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    form =
        XmCreateForm(frame,"localForm",av,ac);

    Widget label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label =
        XmCreateLabelGadget(frame, "localLabel",av,ac);

    Widget dir_label;
    Widget messages_label;

    
    kids[i++] = dir_label =
        XmCreateLabelGadget(form, "dirLabel", NULL,0);
    kids[i++] = m_directory_text =
        fe_CreateText(form, "dirText", NULL, 0);
    kids[i++] = m_directory_button =
        XmCreatePushButtonGadget(form,"chooseButton", NULL, 0);
    kids[i++] = m_size_limit_toggle =
        XmCreateToggleButtonGadget(form, "sizeLimitToggle", NULL, 0);
    kids[i++] = m_size_limit_text =
        fe_CreateText(form, "sizeLimitText", NULL, 0);
    kids[i++] = messages_label =
        XmCreateLabelGadget(form, "sizeLimit2", NULL, 0);

    int max_height1 = XfeVaGetTallestWidget(m_directory_text,
                                            m_directory_button,
                                            NULL);
    
    int max_height2 = XfeVaGetTallestWidget(m_size_limit_toggle,
                                            m_size_limit_text,
                                            messages_label,
                                            NULL);

    XtVaSetValues(dir_label,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(m_directory_text,
                  XmNheight, max_height1,
                  XmNcolumns, 40,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, dir_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(m_directory_button,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_NONE,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_directory_text,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNbottomWidget, m_directory_text,
                  NULL);
    
    XtVaSetValues(m_size_limit_toggle,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_directory_text,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(m_size_limit_text,
                  XmNheight, max_height2,
                  XmNcolumns, 4,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_size_limit_toggle,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_size_limit_toggle,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(messages_label,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_size_limit_text,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_size_limit_text,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);

    XtAddCallback(m_directory_button,XmNactivateCallback,cb_chooseDir, this);
    
    XtManageChildren(kids,i);
    XtManageChild(form);
    XtManageChild(label);
    XtManageChild(frame);
    return frame;
}    

void
XFE_PrefsPageNServer::init()
{


    char *charval;
    XP_Bool boolval;
    XP_Bool locked;
    int32 intval;

    // news directory
    PREF_CopyCharPref(NEWS_DIRECTORY,&charval);
    fe_SetTextField(m_directory_text, charval);
    XP_FREE(charval);
    locked=PREF_PrefIsLocked(NEWS_DIRECTORY);
    XtVaSetValues(m_directory_text,
                  XmNsensitive, !locked,
                  NULL);
    XtVaSetValues(m_directory_button,
                  XmNsensitive, !locked,
                  NULL);

    // max messages ask
    PREF_GetBoolPref(NEWS_NOTIFY, &boolval);
    XmToggleButtonSetState(m_size_limit_toggle, boolval, False);
    locked = PREF_PrefIsLocked(NEWS_NOTIFY);
    XtVaSetValues(m_size_limit_toggle,
                  XmNsensitive, !locked,
                  NULL);


    // max messages
    PREF_GetIntPref(NEWS_MAX_ARTICLES, &intval);
    charval=PR_smprintf("%d",intval);
    fe_SetTextField(m_size_limit_text, charval);
    XP_FREE(charval);
    locked = PREF_PrefIsLocked(NEWS_MAX_ARTICLES);
    XtVaSetValues(m_size_limit_text,
                  XmNsensitive, !locked,
                  NULL);


    refreshList();
    
    m_newsDialog=new XFE_PrefsNServerDialog(getPrefsDialog()->getBaseWidget());
    m_newsDialog->create();

    setInitialized(TRUE);
}

void XFE_PrefsPageNServer::refreshList()
{
    // now fill up the server list

    MSG_Master *master=fe_getMNMaster();
    
    int32 count;
    XmString *xmstrHosts;
    
    count=MSG_GetNewsHosts(master, NULL, 0);
    
    m_newsHosts = new MSG_NewsHost*[count];
    xmstrHosts = new XmString[count];
    MSG_GetNewsHosts(master, m_newsHosts, count);
    
    MSG_NewsHost *defaultHost = MSG_GetDefaultNewsHost(master);
    
    int i;
    for (i=0; i<count; i++) {
        MSG_Host *host     = MSG_GetMSGHostFromNewsHost(m_newsHosts[i]);
        char     *hostname = (char *)MSG_GetHostUIName(host);
        char     *hostString;

        // need to append (default) to default hostname
        if (m_newsHosts[i]==defaultHost) {
            int len = XP_STRLEN(hostname) +
                XP_STRLEN(XP_GetString(XFE_NEWSSERVER_DEFAULT));
            hostString = new char[len];
            
            XP_STRCPY(hostString,hostname);
            XP_STRCAT(hostString,XP_GetString(XFE_NEWSSERVER_DEFAULT));
            
        } else 
            hostString = XP_STRDUP(hostname);
        
        xmstrHosts[i] =
            XmStringCreateLocalized(hostString);
        
        XP_FREE(hostString);
    }

    XtVaSetValues(m_server_list,
                  XmNitemCount, count,
                  XmNitems, xmstrHosts,
                  NULL);

    for (i=0; i<count; i++)
        XmStringFree(xmstrHosts[i]);
    delete xmstrHosts;
    
}

MSG_Host *
XFE_PrefsPageNServer::getSelected() {

    int *position_list=NULL;
    int position_count;
    Boolean selected;

    XP_ASSERT(m_server_list);
    if (!m_server_list) return NULL;
    
    selected = XmListGetSelectedPos(m_server_list,
                                    &position_list,
                                    &position_count);
    if (!selected) return NULL;
    if (!position_list) return NULL;
    if (position_count<=0) return NULL;
    
    MSG_Host* newsHost =
        MSG_GetMSGHostFromNewsHost(m_newsHosts[position_list[0]-1]);

    return newsHost;

}

void
XFE_PrefsPageNServer::saveServer() {

    XP_ASSERT(m_newsDialog);
    if (!m_newsDialog) return;
    char    *server_name = m_newsDialog->getServerName();
    int32   port         = m_newsDialog->getPort();
    HG13181
    XP_Bool password     = m_newsDialog->getPassword();
    

    XP_FREE(server_name);
}

void
XFE_PrefsPageNServer::save()
{
    char *charval;
    XP_Bool boolval;
    int32 intval;

    charval = fe_GetTextField(m_directory_text);
    if (charval) {
        PREF_SetCharPref(NEWS_DIRECTORY, charval);
        XP_FREE(charval);
    }

    charval = fe_GetTextField(m_size_limit_text);
    if (charval) {
        intval = XP_ATOI(charval);
        PREF_SetIntPref(NEWS_MAX_ARTICLES, intval);
        XP_FREE(charval);
    }

    boolval = XmToggleButtonGetState(m_size_limit_toggle);
    PREF_SetBoolPref(NEWS_NOTIFY, boolval);
    
}

void
XFE_PrefsPageNServer::install()
{

}

Boolean
XFE_PrefsPageNServer::verify()
{
    return TRUE;
}

void
XFE_PrefsPageNServer::cb_addServer(Widget, XtPointer closure, XtPointer)
{
    ((XFE_PrefsPageNServer*)closure)->addServer();
}

void
XFE_PrefsPageNServer::cb_editServer(Widget, XtPointer closure, XtPointer)
{
    ((XFE_PrefsPageNServer*)closure)->editServer();
}

void
XFE_PrefsPageNServer::cb_deleteServer(Widget, XtPointer closure, XtPointer)
{
    ((XFE_PrefsPageNServer*)closure)->deleteServer();
}

void
XFE_PrefsPageNServer::cb_setDefault(Widget, XtPointer closure, XtPointer)
{
    ((XFE_PrefsPageNServer*)closure)->setDefault();
}

void
XFE_PrefsPageNServer::cb_serverSelected(Widget, XtPointer closure, XtPointer)
{
    ((XFE_PrefsPageNServer*)closure)->serverSelected();
}

void
XFE_PrefsPageNServer::cb_chooseDir(Widget, XtPointer closure, XtPointer)
{
    ((XFE_PrefsPageNServer*)closure)->chooseDir();
}


void
XFE_PrefsPageNServer::addServer()
{
    XP_ASSERT(m_newsDialog);
    if (!m_newsDialog) return;

    if (!m_newsDialog->prompt()) return;

    MSG_Master *master=fe_getMNMaster();

    MSG_CreateNewsHost(master,
                       m_newsDialog->getServerName(),
                       HG14871,
                       m_newsDialog->getPort());
    refreshList();
}

void
XFE_PrefsPageNServer::editServer()
{

    MSG_Host *newsHost = getSelected();
    if (!newsHost) return;
    
    const char *server_name = MSG_GetHostName(newsHost);
    int32 port = MSG_GetHostPort(newsHost);
    XP_Bool xxx = HG19861;
    XP_Bool password = FALSE;   // what's the API here?

    m_newsDialog->init(server_name, port, xxx, password);
    if (m_newsDialog->prompt())
        saveServer();
    
    refreshList();
}

void
XFE_PrefsPageNServer::deleteServer()
{
    MSG_Host *host = getSelected();
    if (!host) return;

    const char *confirm_message;
    confirm_message = PR_smprintf(XP_GetString(MK_MSG_REMOVE_HOST_CONFIRM),
                                  MSG_GetHostUIName(host));

    if (!XFE_Confirm(getContext(), confirm_message))
        return;
                     
    MSG_NewsHost *newsHost=MSG_GetNewsHostFromMSGHost(host);
    XP_ASSERT(newsHost);
    if (!newsHost) return;

    MSG_Master *master=fe_getMNMaster();

    MSG_DeleteNewsHost(master, newsHost);
    refreshList();
}

void
XFE_PrefsPageNServer::setDefault()
{
    MSG_Host *host = getSelected();
    if (!host) return;

}

void
XFE_PrefsPageNServer::serverSelected()
{
    
}

void
XFE_PrefsPageNServer::chooseDir()
{
    
}

