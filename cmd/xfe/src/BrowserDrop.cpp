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
   BrowserDrop.cpp -- class definition for the browser drop class
   Created: Alastair Gourlay <sgidev@netscape.com>, 1-Jan-96.
 */



#include "BrowserDrop.h"

#ifdef DEBUG_sgidev
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

//
// XFE_BrowserDrop class
//

// constructor

XFE_BrowserDrop::XFE_BrowserDrop(Widget parent,XFE_BrowserFrame *browserFrame)
    : XFE_DropNetscape(parent)
{
    _browserFrame=browserFrame;
    _jsList=NULL;
    _sameDragSource=False;
}

XFE_BrowserDrop::~XFE_BrowserDrop()
{
    if (_jsList)
        cleanupJsList();
}

void XFE_BrowserDrop::targets()
{
    _numTargets=2;
    _targets=new Atom[_numTargets];

    _targets[0]=_XA_NETSCAPE_URL;
    _targets[1]=XA_STRING;

    acceptFileTargets();
}

void XFE_BrowserDrop::operations()
{
    // always copy - move/link irrelevant
    _operations=(unsigned int)XmDROP_COPY;
}

// wrapper for JavaScript event callback
void XFE_BrowserDrop::JsDropEventCb(MWContext*,LO_Element*,int32,void* closure, ETEventStatus status)
{
    if (closure) {
        XFE_BrowserDrop *bd=(XFE_BrowserDrop*)closure;
        bd->jsDropEventCb(status);
    }
}

int XFE_BrowserDrop::processTargets(Atom *targets,const char **data,int numItems)
{
    int i;

    XDEBUG(printf("XFE_BrowserDrop::processTargets()\n"));

    _sameDragSource=False;
    
    if (!targets || !data || numItems==0)
        return FALSE;
    
    // build list of dropped URLs for event JS dragdrop handler
    
    // count # of items in list (need to expand multi-item NetscapeURL data)
    int urlCount=0;
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

    _jsList=new char*[urlCount+1];
    int listSize=0;
    
    for (i=0;i<numItems;i++) {
        if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
            continue;

        XDEBUG(printf("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(_widget),targets[i]),data[i]));

        if (targets[i]==_XA_FILE_NAME) {
            char *address=(char*)XP_ALLOC(5+strlen(data[i])+1); // "file:" + filename + \0
            sprintf(address,"file:%s",data[i]);
            _jsList[listSize++]=address;
        }
        
        if (targets[i]==_XA_NETSCAPE_URL) {
            XFE_URLDesktopType urlData(data[i]);
            for (int j=0;j<urlData.numItems();j++) {
                _jsList[listSize++]=XP_STRDUP(urlData.url(j));
            }
        }
        if (targets[i]==XA_STRING) {
                _jsList[listSize++]=XP_STRDUP(data[i]);
        }
    }
    _jsList[listSize]=NULL;

    // bail if there are no valid URL's
    if (listSize==0) {
        cleanupJsList();
        return FALSE;
    }
    
    // detect drag from the same browser window
    if (XFE_DragBase::_activeDragShell) {
        Widget shell=_widget;
        while (!XtIsShell(shell)) shell=XtParent(shell);
        if (shell==XFE_DragBase::_activeDragShell) {
            _sameDragSource=True;
        }
    }
    
    // build JavaScript DRAGDROP event struct

    MWContext *context=_browserFrame->getContext();
    if (context) {
        JSEvent *jsEvent = XP_NEW_ZAP(JSEvent);
        jsEvent->type = EVENT_DRAGDROP;
        jsEvent->x=_dropEventX;
        jsEvent->y=_dropEventY;
        jsEvent->docx=_dropEventX + CONTEXT_DATA(context)->document_x;
        jsEvent->docy=_dropEventY + CONTEXT_DATA(context)->document_y;
        Position rootX=0;Position rootY=0;
        XtTranslateCoords(_widget,_dropEventX,_dropEventY,&rootX,&rootY);
        jsEvent->screenx=rootX;
        jsEvent->screeny=rootY;
        jsEvent->which=0; // Motif drag and drop doesn't give us the Button events
        jsEvent->modifiers=0; // Motif drag and drop doesn't give us the Button events
        jsEvent->data=_jsList;
        jsEvent->dataSize=listSize;
        
        ET_SendEvent(context, NULL, jsEvent, JsDropEventCb, (void*)this);
    }

    // URL's will be loaded by event handler, if approved by page's JavaScript handler.

    return TRUE;
}

void XFE_BrowserDrop::jsDropEventCb(int status)
{
    XDEBUG(printf("XFE_BrowserDrop::jsDropEventCb(%d)\n",status));

    // Load URLs if no event handler present, or if event handler said ok.
    // Note: We ignore the drop if it originated from the same browser window.
    // (We let JavaScript have it, but if it's not interested then the default UI
    // is to disallow it.)
    if (_jsList && status==0 && !_sameDragSource) {
        // open dropped URLs in browser
        int i=0;
        while (_jsList[i]) {
            loadURL(_jsList[i], i!=0);
            i++;
        }
    }

    // free the drop data
    cleanupJsList();
    
    return;
}


// defined in mozilla.c - cancels timer which loads homepage after sploosh screen
extern "C" void plonk_cancel();

void XFE_BrowserDrop::loadURL(const char *url,int newFrame)
{
    if (!url)
        return;

    // cancel initial sploosh-screen loader timeout
    plonk_cancel();
    
    URL_Struct *urlStruct = NET_CreateURLStruct (url,NET_DONT_RELOAD);
    if (!newFrame
#ifdef MOZ_MAIL_NEWS
        || !MSG_RequiresBrowserWindow(urlStruct->address)
#endif
	) {
        _browserFrame->getURL(urlStruct);
        fe_UserActivity(_browserFrame->getContext());
    }
    else {
        XFE_BrowserFrame *newFrame=new XFE_BrowserFrame(XtParent(_browserFrame->getBaseWidget()), _browserFrame, NULL);
        newFrame->show();
        newFrame->getURL(urlStruct);
        fe_UserActivity(newFrame->getContext());
    }
}

void XFE_BrowserDrop::cleanupJsList()
{
    if (_jsList) {
        int i=0;
        while (_jsList[i])
            XP_FREE(_jsList[i++]);
        delete _jsList;
        _jsList=NULL;
    }
}
