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
#include "stdafx.h"

#include "dialog.h"

/*----------------------------------------------------------------------**
** Hack alert.                                                          **
** The following code is needed so that backend libraries can perform   **
** a small set of operations before we are initialized.                 **
** In specific, the fortezza security lib may call FE_PromptPassword    **
** via the context function table before we ever have finished creating **
** a window.                                                            **
**----------------------------------------------------------------------*/

char *sux_PromptPassword(MWContext *pContext, const char *pMessage)	{
    char *pRetval = NULL;
    
    char *pWinMessage = FE_Windowsify(pMessage);
    if(pWinMessage)  {
        CDialogPASS dlgPass;
        theApp.m_splash.SafeHide();
        
        pRetval = dlgPass.DoModal(pWinMessage);
        
        XP_FREE(pWinMessage);
    }
    
    return(pRetval);
}

MWContext *FE_GetInitContext(void)  {
    static MWContext *pCX = NULL;
    static _ContextFuncs sFux;
    static MWContext sCX;
    
    //  First time through, init context.
    //  Note, it should only set up the information
    //      needed to get the back end libraries by.
    //  It should not be added to the global context
    //      list or be routed through any of the CAbstractCX
    //      derived classes.  ETC.
    if(pCX == NULL) {
        memset(&sCX, 0, sizeof(sCX));
        memset(&sFux, 0, sizeof(sFux));

        //  Set up required functions.
        sFux.PromptPassword = sux_PromptPassword;
        
        //  Fill in the context.
        sCX.funcs = &sFux;
        pCX = &sCX;
    }
    
    return(pCX);
}

