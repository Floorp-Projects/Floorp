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
   BrowserDrop.h -- class definition for the browser drop class
   Created: Alastair Gourlay <sgidev@netscape.com>, 1-Jan-96.
 */



#ifndef _BROWSER_DROP_H
#define _BROWSER_DROP_H

class XFE_BrowserDrop;
class XFE_BrowserFrame;

#include "BrowserFrame.h"
#include "DragDrop.h"
#include <libevent.h>

//
// XFE_BrowserDrop class
//

class XFE_BrowserDrop : public XFE_DropNetscape
{
public:
    XFE_BrowserDrop(Widget,XFE_BrowserFrame*);
    ~XFE_BrowserDrop();
protected:
    XFE_BrowserFrame *_browserFrame;
    char **_jsList;
    int _sameDragSource;
    
    void targets();
    void operations();
    int processTargets(Atom*,const char**,int);
    void jsDropEventCb(int);
    void loadURL(const char*,int);
    void cleanupJsList();

    static void JsDropEventCb(MWContext*,LO_Element*,int32,void*,ETEventStatus);
    
private:
};

#endif // _BROWSER_DROP_H
