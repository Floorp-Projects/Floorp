/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* -*- Mode: C++; tab-width: 4 -*-
   TaskBar.h -- class definition for taskbar.
   Created: Stephen Lamm <slamm@netscape.com>, 19-Nov-96.
 */

#ifndef _xfe_taskbar_manager_h
#define _xfe_taskbar_manager_h

#include "xp_core.h"
#include "Component.h"

#include "ntypes.h"  // For MWContext.
#include "msgcom.h"  // Need for BIFF stuff.

#include <X11/Intrinsic.h>

class XFE_TaskBar;

class XFE_TaskBarManager : public XFE_Component
{
public:
    XFE_TaskBarManager(MWContext *context, Pixel bg);
    
    void Register(XFE_TaskBar *taskBar);
    void Unregister(XFE_TaskBar *taskBar);

    void DoDock();
    void DoFloat(Widget w);
    void RaiseFloat();

    XP_Bool IsTaskBarFloating() { return m_isFloat; }

    MSG_BIFF_STATE GetBiffState() { return m_biffState; }

    // use NotificationCenter to call updateBiffState()  ??
    //   (through a new static callback)
    void UpdateBiffState(MSG_BIFF_STATE biffState);

private:
    MSG_BIFF_STATE m_biffState;
    Boolean m_isFloat;
    XP_List *m_taskBarList;
    Widget m_floatForm;
    XFE_TaskBar *m_floatTaskBar;

    Widget createFloatPalette(MWContext *context);

    static void destroy_cb(Widget, XtPointer, XtPointer);
};


#endif /* _xfe_taskbar_manager__h */
