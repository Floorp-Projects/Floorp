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
   FolderPromptDialog.h -- "New Folder" dialog
   Created: Akkana Peck <akkana@netscape.com>, 11-Dec-97.
 */



#ifndef _xfe_FolderPromptDialog_h
#define _xfe_FolderPromptDialog_h

#include "xp_core.h"
#include "Dialog.h"

class XFE_Frame;
class XFE_FolderDropdown;
class MSG_FolderInfo;

class XFE_FolderPromptDialog: public XFE_Dialog
{
public:
    XFE_FolderPromptDialog(Widget parent,
                           char *name,
                           XFE_Frame *frame,
                           XFE_Component* toplevel);

    virtual ~XFE_FolderPromptDialog();

	/* changes the delete response to UNMAP so we can keep the
	   dialog around after it's popped down, and returns True
	   if Ok was pressed, and False otherwise (either cancel or
	   the WM close button was pressed. 

	   This method calls fe_eventLoop so we can return with the
	   user's response from here. 

	   since we UNMAP instead of DESTROY, you need to delete the
	   instance after you're through with it.*/

    MSG_FolderInfo* prompt(MSG_FolderInfo*);

private:
    XFE_Frame *m_frame;
    XFE_FolderDropdown *m_folderDropDown;
    Widget m_text;

    XP_Bool m_doneWithLoop;
    MSG_FolderInfo* m_retVal;

    void ok();
    void cancel();

    static void ok_cb(Widget, XtPointer, XtPointer);
    static void cancel_cb(Widget, XtPointer, XtPointer);
};

#endif /* _xfe_FolderPromptDialog_h */
