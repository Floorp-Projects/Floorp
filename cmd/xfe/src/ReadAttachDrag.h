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
   ReadAttachDrag.h -- class definitions for message attachment drag source
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */

#ifndef _READ_ATTACH_DRAG_H
#define _READ_ATTACH_DRAG_H

class XFE_ReadAttachPanel;
class XFE_ReadAttachDrag;

#undef Bool
#include "mozilla.h"
#include "xfe.h"
#include "net.h"
#include "DragDrop.h"
#include "ReadAttachPanel.h"


class XFE_ReadAttachDrag : public XFE_DragNetscape
{
public:
    XFE_ReadAttachDrag(Widget,XFE_ReadAttachPanel*);
    ~XFE_ReadAttachDrag();
    void lockFrame();
    void unlockFrame();
protected:
    XFE_ReadAttachPanel *_attachPanel;
    char *_dragDataURL;
    char *_dragDataName;
    char *_tmpDirectory;
    char *_tmpFileName;

    // utilities
    void cleanupDataFiles();
    
    // methods for derived classes to override
    virtual int dragStart(int,int);
    virtual void targets();
    virtual void operations();
    virtual char *getTargetData(Atom);
    virtual void dragComplete();
private:
};

#endif // _READ_ATTACH_DRAG_H
