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
   ReadAttachPanel.h -- class definitions for read window attachment panel
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */

#ifndef _READ_ATTACH_PANEL_H
#define _READ_ATTACH_PANEL_H

// Classes in this file:
//      XFE_ReadAttachPanel
//      XFE_ReadDragAttach (nyi)
//

#include "AttachPanel.h"
#include "ReadAttachDrag.h"

//
// XFE_ReadAttachPanel
//

class XFE_ReadAttachPanel : public XFE_AttachPanel {
public:
    XFE_ReadAttachPanel(MWContext*);
    ~XFE_ReadAttachPanel();
    void createWidgets(Widget);

    // geometry
    void setPaneHeight(int);
    int getPaneHeight();
    
    // attachment list
    void addAttachments(MSG_Pane*,MSG_AttachmentData*);
    void removeAllAttachments();
protected:
    Widget _open;
    Widget _save;
    Widget _properties;

    Widget _propertiesDialog;
    Widget _nameLabel;
    Widget _nameValue;
    Widget _typeLabel;
    Widget _typeValue;
    Widget _encLabel;
    Widget _encValue;
    Widget _descLabel;
    Widget _descValue;

    XFE_ReadAttachDrag *_attachDrag;

    MSG_Pane *_pane;
    MSG_AttachmentData *_attachments;
    
    // internal methods
    virtual void updateSelectionUI();
    void displayAttachmentProperties(int);
    
    // internal callback stubs
    static void OpenCb(Widget,XtPointer,XtPointer);
    static void SaveCb(Widget,XtPointer,XtPointer);
    static void PropertiesCb(Widget,XtPointer,XtPointer);

    // internal callback methods
    void doubleClickCb(int);
    void openCb();
    void saveCb();
    void propertiesCb();
private:
};

#endif // _READ_ATTACH_PANEL_H

