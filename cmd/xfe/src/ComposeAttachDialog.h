/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   ComposeAttachDialog.h -- class definitions for compose attachment dialogs
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



#ifndef _COMPOSE_ATTACH_DIALOG_H
#define _COMPOSE_ATTACH_DIALOG_H

// Classes in this file:
//      XFE_ComposeAttachLocationDialog
//      XFE_ComposeAttachFileDialog
//

#include "Component.h"
#include <Xm/Xm.h>
#include "ComposeAttachFolderView.h"

class XFE_ComposeAttachFolderView;

//
// XFE_ComposeAttachLocationDialog - enter URL attachment
//

class XFE_ComposeAttachLocationDialog : public XFE_Component {
public:
    XFE_ComposeAttachLocationDialog(XFE_ComposeAttachFolderView*);
    virtual ~XFE_ComposeAttachLocationDialog();

    // widget interfaces
    virtual void createWidgets(Widget);
    operator Widget() { return getBaseWidget(); };
    void show();
protected:
    // Motif information
    Widget _parent;
    Widget _dialog;
    Widget _locationText;
    
    // panel contents    
    XFE_ComposeAttachFolderView *_attachFolder;
    
    // internal callback stubs
    static void OkCb(Widget,XtPointer,XtPointer);
    static void ClearCb(Widget,XtPointer,XtPointer);
    static void CancelCb(Widget,XtPointer,XtPointer);

    // internal callback methods
    void okCb();
    void clearCb();
    void cancelCb();
private:
};

//
// XFE_ComposeAttachFileDialog - select file attachment
//

class XFE_ComposeAttachFileDialog : public XFE_Component {
public:
    XFE_ComposeAttachFileDialog(XFE_ComposeAttachFolderView*);
    virtual ~XFE_ComposeAttachFileDialog();

    // widget interfaces
    virtual void createWidgets(Widget);
    operator Widget() { return getBaseWidget(); };
    void show();
protected:
    // Motif information
    Widget _parent;
    Widget _dialog;
    Widget _attachEncodingMenu;
    Widget _attachBinaryButton;
    
    // panel contents    
    XFE_ComposeAttachFolderView *_attachFolder;
    
    // internal callback stubs
    static void OkCb(Widget,XtPointer,XtPointer);
    static void CancelCb(Widget,XtPointer,XtPointer);
    static void SetFileAttachBinaryCb(Widget,XtPointer,XtPointer);

    // internal callback methods
    void okCb(const char*);
    void cancelCb();
    void setFileAttachBinaryCb(XtPointer);
private:
};


#endif // _COMPOSE_ATTACH_DIALOG_H
