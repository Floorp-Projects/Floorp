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
   EditorDrop.h -- class definition for the editor drop class
   Created: Alastair Gourlay <sgidev@netscape.com>, 1-Jan-96.
 */



#ifndef _EDITOR_DROP_H
#define _EDITOR_DROP_H

class XFE_EditorDrop;
class XFE_EditorView;

#include "EditorView.h"
#include "DragDrop.h"

//
// XFE_EditorDrop class
//

class XFE_EditorDrop : public XFE_DropNetscape
{
public:
    XFE_EditorDrop(Widget,XFE_EditorView*);
    virtual ~XFE_EditorDrop();
protected:
    XFE_EditorView *_editorView;

    void targets();
    void operations();
    Atom acceptDrop(unsigned int,Atom*,unsigned int);
    int processTargets(Atom*,const char**,int);

    int insertLink(const char*);
    int insertFile(const char*);
    int insertText(const char*);
private:
};

#endif // _EDITOR_DROP_H
