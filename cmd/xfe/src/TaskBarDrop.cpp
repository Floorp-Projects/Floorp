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
   TaskBarDrop.cpp -- class definition for the task bar drop class
   Created: Alastair Gourlay <sgidev@netscape.com>, 1-Jan-96.
 */



#include "TaskBarDrop.h"
#include "Command.h"
#include "BrowserFrame.h"
#ifdef MOZ_MAIL_NEWS
#include "ComposeFrame.h"
#include "ComposeAttachFolderView.h"
#endif
#ifdef EDITOR
#include "EditorFrame.h"
#endif
#include <Xfe/Xfe.h>
#include "xfe2_extern.h"

#ifdef DEBUG_sgidev
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

//
// XFE_TaskBarDrop class
//

// constructor

XFE_TaskBarDrop::XFE_TaskBarDrop(Widget parent,const char *command)
    : XFE_DropNetscape(parent)
{
    _command=command;

	// Configure the drop site
	Arg			xargs[1];
	Cardinal	n = 0;
	
	XtSetArg(xargs[n],XmNanimationStyle,	XmDRAG_UNDER_NONE);		n++;
	
	update(xargs,n);
}

XFE_TaskBarDrop::~XFE_TaskBarDrop()
{
}

void XFE_TaskBarDrop::targets()
{
    _numTargets=2;
    _targets=new Atom[_numTargets];

    _targets[0]=_XA_NETSCAPE_URL;
    _targets[1]=XA_STRING;

    acceptFileTargets();
}

void XFE_TaskBarDrop::operations()
{
    // always copy - move/link irrelevant
    _operations=(unsigned int)XmDROP_COPY;
}

/* virtual */ void
XFE_TaskBarDrop::dragIn()
{
    XtVaSetValues(_widget,XmNraised,True,NULL);
}

/* virtual */ void
XFE_TaskBarDrop::dragOut()
{
    XtVaSetValues(_widget,XmNraised,False,NULL);
}


int XFE_TaskBarDrop::processTargets(Atom *targets,const char **data,int numItems)
{
    XDEBUG(printf("XFE_TaskBarDrop::processTargets()\n"));
    
    if (!targets || !data || numItems==0)
        return FALSE;

    int i;

    int urlCount=0;
    
    // count # of items in list (need to expand multi-item NetscapeURL data)
    for (i=0;i<numItems;i++) {
        if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
            continue;
        if (targets[i]==_XA_FILE_NAME) urlCount++;
        else if (targets[i]==XA_STRING) urlCount++;
        else if (targets[i]==_XA_NETSCAPE_URL) {
            XFE_URLDesktopType urlData(data[i]);
            urlCount+=urlData.numItems();
        }
    }

    char **urlList=new char*[urlCount];
    
    // create list of urls for dropped documents

    int j;
    for (i=0,j=0;i<numItems;i++) {
        if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
            continue;

        XDEBUG(printf("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(_widget),targets[i]),data[i]));

        if (targets[i]==_XA_FILE_NAME)
            urlList[j++]=XP_STRDUP(data[i]);
        else if (targets[i]==XA_STRING)
            urlList[j++]=XP_STRDUP(data[i]);
        else if (targets[i]==_XA_NETSCAPE_URL) {
            XFE_URLDesktopType urlData(data[i]);
            for (int k=0;k<urlData.numItems();k++)
                urlList[j++]=XP_STRDUP(urlData.url(k));
        }
    }

    // execute command on url list

    // Browser or Editor drop - open each document in a new window
    if (_command==xfeCmdOpenOrBringUpBrowser
#ifdef EDITOR
	|| _command==xfeCmdOpenEditor
#endif
       ) {
        for (i=0;i<urlCount;i++)
            openDocument(urlList[i]);
    }

#ifdef MOZ_MAIL_NEWS
    // Mail or News drop - open one compose window with documents attached
    if (_command==xfeCmdOpenInboxAndGetNewMessages ||
        _command==xfeCmdOpenNewsgroups) {
        openComposeWindow((const char**)urlList,urlCount);
    }
#endif
    
    // free url list
    for (j=0;j<urlCount;j++)
        if (urlList[j]) XP_FREE((void*)urlList[j]);
    
    delete urlList;
    
    return TRUE;
}

// process drop on browser or editor task icons

void XFE_TaskBarDrop::openDocument(const char *url)
{
    if (!url) return;

    URL_Struct *urlStruct = NET_CreateURLStruct (url,NET_DONT_RELOAD);

    if (!urlStruct)
        return;

#ifdef MOZ_MAIL_NEWS
    // Don't open a window if this is not a document URL
    if (!MSG_RequiresBrowserWindow(urlStruct->address)) {
        MWContext *context=XP_GetNonGridContext(fe_all_MWContexts->context);
        if (context)
            fe_GetURL(context,urlStruct, FALSE);
        else
            NET_FreeURLStruct(urlStruct);
        return;
    }
#endif

    // Open a new window with this URL
    if (_command==xfeCmdOpenOrBringUpBrowser) {
        fe_showBrowser(FE_GetToplevelWidget(),
                       NULL,
                       NULL,
                       urlStruct);
    }
#ifdef EDITOR
    else if (_command==xfeCmdOpenEditor) {
        fe_showEditor(XfeAncestorFindApplicationShell(_widget),
                      NULL,
                      NULL,
                      urlStruct);
    }
#endif
}

#ifdef MOZ_MAIL_NEWS
// process drop on mail or news taskbar icons

void XFE_TaskBarDrop::openComposeWindow(const char **urlList,int urlCount)
{
    MWContext *context=XP_GetNonGridContext(fe_all_MWContexts->context);
    if (!context)
        return;

    MSG_Pane* pane = fe_showCompose(XtParent(CONTEXT_WIDGET(context)),
                                    NULL,
                                    context,
                                    NULL,
                                    NULL,
                                    MSG_DEFAULT,
                                    (_command==xfeCmdOpenNewsgroups ? True : False)
                                    );

    // add urlList as attachments
    MSG_AttachmentData *attachments=new struct MSG_AttachmentData[urlCount+1];
    int i;
    int numAttachments=0;

    for (i=0;i<urlCount;i++) {
        if (XFE_ComposeAttachFolderView::validateAttachment(_widget,urlList[i])) {
            struct MSG_AttachmentData m = { 0 };
            m.url=XP_STRDUP(urlList[i]);
            m.desired_type=NULL;
            attachments[numAttachments++]=m;
        }
    }

    if (numAttachments>0) {
        attachments[numAttachments].url=NULL; // terminate list
        MSG_SetAttachmentList(pane,attachments);
    }
    
    // delete list
    for (i=0;i<numAttachments;i++) {
        if (attachments[i].url)
            XP_FREE((void*)attachments[i].url);
    }
    delete attachments;
}
#endif  // MOZ_MAIL_NEWS
