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
   ReadAttachDrag.cpp -- class definitions for message attachment drag source
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */

// classes:
//      XFE_ReadAttachDrag
//

#include <stdio.h>
#include <Xm/Xm.h>
#include "ReadAttachDrag.h"
#include "DesktopTypes.h"

extern void fe_SaveSynchronousURL(MWContext*,URL_Struct*,const char*);

// constructor

XFE_ReadAttachDrag::XFE_ReadAttachDrag(Widget w,XFE_ReadAttachPanel *attachPanel) : XFE_DragNetscape(w)
{
    _attachPanel=attachPanel;
    _dragDataURL=NULL;
    _dragDataName=NULL;
    _tmpDirectory=NULL;
    _tmpFileName=NULL;
}

// destructor

XFE_ReadAttachDrag::~XFE_ReadAttachDrag()
{
}


// decide if this drag is interesting

int XFE_ReadAttachDrag::dragStart(int,int)
{
    if (!_dragWidget || !XmIsPushButton(_dragWidget))
        return FALSE;

    // If drag widget is an AttachPanelItem, extract the URL
    XtPointer userData;
    XtVaGetValues(_dragWidget,XmNuserData,&userData,NULL);
    XFE_AttachPanelItem *item=(XFE_AttachPanelItem*)userData;

    if (item && item->data()) {
        _attachPanel->selectItem(item);
        _dragDataURL=XP_STRDUP(item->data());
        if (item->dataLabel())
            _dragDataName=XP_STRDUP(item->dataLabel());
        else
            _dragDataName=XP_STRDUP("noname");
            
        setDragIconForType(item->dataType());
        return TRUE;
    }

    return FALSE;
}

// specify types for this particular drag

void XFE_ReadAttachDrag::targets()
{
    _numTargets=2;
    _targets=new Atom[_numTargets];

    _targets[0]=_XA_NETSCAPE_URL;
    _targets[1]=XA_STRING;

    setFileTarget(_XA_FILE_NAME);
}

// specify operations for this particular drag

void XFE_ReadAttachDrag::operations()
{
    _operations=XmDROP_COPY;
}

// provide data for requested target from targets() list

char *XFE_ReadAttachDrag::getTargetData(Atom target)
{
    // WARNING - data *must* be allocated with Xt malloc API, or Xt
    // will spring a leak!

    if (!_dragDataURL || !_dragDataName)
        return NULL;
    
    if (target==_XA_NETSCAPE_URL) {
        // translate drag data to NetscapeURL format
        XFE_URLDesktopType urlData;

        urlData.createItemList(1);
        urlData.url(0,_dragDataURL);
        return (char*) XtNewString(urlData.getString());
    }

    if (target==_XA_FILE_NAME) {
        // save url as appropriately named file in a tmp
        // directory.

        if ((_tmpDirectory=XFE_DesktopType::createTmpDirectory())==NULL)
            return NULL;

        _tmpFileName=new char[strlen(_tmpDirectory)+1+strlen(_dragDataName)+1];
        sprintf(_tmpFileName,"%s/%s",_tmpDirectory,_dragDataName);
        URL_Struct *urlStruct=NET_CreateURLStruct(_dragDataURL,NET_DONT_RELOAD);
        if (urlStruct) {
            lockFrame();
            fe_SaveSynchronousURL(_attachPanel->context(),urlStruct,_tmpFileName);
            unlockFrame();
        }

        // return file name        
        return (char*) XtNewString(_tmpFileName);
    }

    if (target==XA_STRING) {
        // return the URL
        return (char*) XtNewString(_dragDataURL);
    }

    return NULL;
}

    
void XFE_ReadAttachDrag::dragComplete()
{
    if (_dragDataURL) {
        XP_FREE(_dragDataURL);
        _dragDataURL=NULL;
    }

    if (_dragDataName) {
        XP_FREE(_dragDataName);
        _dragDataName=NULL;
    }

    // if we created tmp files, delete them.
    cleanupDataFiles();    
}


// provide safe locking when saving to tmp file. Need to
// prevent clicking in thread window or message from interrupting
// save. Ideally fe_SaveSynchronousURL() would use its own
// context and not be interruptable.

void XFE_ReadAttachDrag::lockFrame()
{
    Widget s=_widget;
    while (s && !XtIsShell(s)) {
        s=XtParent(s);
    }
    XtSetSensitive(s,FALSE);

    CONTEXT_DATA (_attachPanel->context())->clicking_blocked = True;
    fe_SetCursor (_attachPanel->context(), False);
}

void XFE_ReadAttachDrag::unlockFrame()
{
    Widget s=_widget;
    while (s && !XtIsShell(s)) {
        s=XtParent(s);
    }
    XtSetSensitive(s,TRUE);
    
    CONTEXT_DATA (_attachPanel->context())->clicking_blocked = False;
    fe_SetCursor (_attachPanel->context(), False);
}

// remove tmp files and directory
// make sure that we only remove files that we created in /tmp/nsdndXXXXXX/

void XFE_ReadAttachDrag::cleanupDataFiles()
{

    if (_tmpFileName) {
        // delete tmp file
        if (_tmpDirectory &&
            strncmp(_tmpFileName,_tmpDirectory,strlen(_tmpDirectory))==0) {
            unlink(_tmpFileName);
        }
        delete _tmpFileName;
        _tmpFileName=NULL;
    }

    if (_tmpDirectory) {
        // delete tmp directory
        rmdir(_tmpDirectory);
        free((void*)_tmpDirectory);
        _tmpDirectory=NULL;
    }
}
