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
   LocationDrag.h -- class definitions for browser location drag source
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



#ifndef _LOCATION_DRAG_H
#define _LOCATION_DRAG_H

#undef Bool
#include "mozilla.h"
#include "xfe.h"
#include "net.h"
#include "DragDrop.h"

class XFE_LocationDrag : public XFE_DragNetscape
{
public:
    XFE_LocationDrag(Widget);
    ~XFE_LocationDrag();
    void setDragData(URL_Struct*);
protected:
    char *_dragDataURL;

    // methods for derived classes to override
    virtual int dragStart(int,int);
    virtual void targets();
    virtual void operations();
    virtual char *getTargetData(Atom);
    virtual void dragComplete();
private:
};

#endif // _LOCATION_DRAG_H
