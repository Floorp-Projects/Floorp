/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   TaskBarDrop.h -- class definition for the task bar drop class
   Created: Alastair Gourlay <sgidev@netscape.com>, 1-Jan-96.
 */



#ifndef _TASK_BAR_DROP_H
#define _TASK_BAR_DROP_H

#include <mozilla.h>
#include <xfe.h>
#include "DragDrop.h"

//
// XFE_TaskBarDrop class
//

class XFE_TaskBarDrop : public XFE_DropNetscape
{
public:
    XFE_TaskBarDrop(Widget,const char*);
    virtual ~XFE_TaskBarDrop();
protected:
    const char *_command;
    
    void targets();
    void operations();
    int processTargets(Atom*,const char**,int);
    void openDocument(const char*);
    void openComposeWindow(const char**,int);

    virtual void dragIn();
    virtual void dragOut();
private:
};

#endif // _TASK_BAR_DROP_H
