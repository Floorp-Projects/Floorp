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
   AdvSearchDialog.h -- dialog for specifying options to message search
   Created: Akkana Peck <akkana@netscape.com>, 21-Oct-97.
 */



#ifndef _xfe_AdvSearchDialog_h
#define _xfe_AdvSearchDialog_h

#include "xp_core.h"
#include "Dialog.h"

class XFE_Frame;

class XFE_AdvSearchDialog: public XFE_Dialog
{
public:
    XFE_AdvSearchDialog(Widget parent,
                        char *name,
                        XFE_Frame *frame);

    virtual ~XFE_AdvSearchDialog();

	/* changes the delete response to UNMAP so we can keep the
	   dialog around after it's popped down, and returns True
	   if Ok was pressed, and False otherwise (either cancel or
	   the WM close button was pressed. 

	   This method calls fe_eventLoop so we can return with the
	   user's response from here. 

	   since we UNMAP instead of DESTROY, you need to delete the
	   instance after you're through with it.*/
    XP_Bool post();

private:
    XFE_Frame *m_frame;

    XP_Bool m_doneWithLoop;
    XP_Bool m_retVal;

    Widget m_subfolderToggle;
    Widget m_searchLocalToggle;
    Widget m_searchServerToggle;

    void ok();
    void cancel();
    void toggle();

    static void ok_cb(Widget, XtPointer, XtPointer);
    static void cancel_cb(Widget, XtPointer, XtPointer);
    static void toggle_cb(Widget, XtPointer, XtPointer);
};

#endif /* _xfe_AdvSearchDialog_h */
