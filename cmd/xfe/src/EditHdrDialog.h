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
  EditHdrDialog.h -- add or remove arbitrary mail headers.
  Created: Akkana Peck <akkana@netscape.com>, 19-Nov-97.
 */



#ifndef _xfe_edithdrdialog_h
#define _xfe_edithdrdialog_h

#include "xp_core.h"
#include "ntypes.h"

#include "Dialog.h"

class XFE_Frame;

/* changes the delete response to UNMAP so we can keep the
   dialog around after it's popped down, and returns True
   if Ok was pressed, and False otherwise (either cancel or
   the WM close button was pressed. 

   This method calls fe_eventLoop so we can return with the
   user's response from here. 

   Since we UNMAP instead of DESTROY, you need to delete the
   instance after you're through with it.
  */
class XFE_EditHdrDialog: public XFE_Dialog
{
public:
    XFE_EditHdrDialog(Widget parent, char* name, MWContext* context);

    virtual ~XFE_EditHdrDialog();

    // returns the header that was selected, or NULL for CANCEL.
    // The string returned from this routine should be freed with XtFree();
    char* post();
  
private:
    MWContext* m_contextData;

    XP_Bool m_doneWithLoop;
    XP_Bool m_retVal;

    Widget m_list;
    Widget m_editButton;
    Widget m_deleteButton;

    char* promptHeader(char*);

    void ok();
    void cancel();
    void button(Widget);
    void selection(XmListCallbackStruct*);

    static void ok_cb(Widget, XtPointer, XtPointer);
    static void cancel_cb(Widget, XtPointer, XtPointer);
    static void butn_cb(Widget, XtPointer, XtPointer);
    static void select_cb(Widget, XtPointer, XtPointer);
};

#endif /* _xfe_edithdrdialog_h */
