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

IMPLEMENT_DYNAMIC(CJMCException, CException)

CJMCException::CJMCException(JMCException *& pJMC)    {
    m_pJMC = pJMC;

    //  Clear this out, we're managing it now.
    pJMC = NULL;

    //  What in the hell are you thinking?
    XP_ASSERT(m_pJMC);
}

CJMCException::~CJMCException() {
    JMCException_Destroy(m_pJMC);
    m_pJMC = NULL;
}

JMCException *CJMCException::GetJMCException()  {
    return(m_pJMC);
}

BOOL CJMCException::GetErrorMessage(LPTSTR pError, UINT nSize, PUINT nHelpID)   {
    if(nSize < 32)  {
        return(FALSE);
    }

    //  fill in a simple error message until this can get better.
    sprintf(pError, "JMC exception %d", JMCException_GetErrorCode(m_pJMC));
    return(TRUE);
}

int CJMCException::ReportError(UINT nType, UINT nMessageID) {
    CString Message;
    UINT nHelpID = 0;

    char aBuffer[512];
    if(!GetErrorMessage(aBuffer, 512, &nHelpID))   {
        if(!Message.LoadString(nMessageID))  {
            Message = "Generic JMC exception";
        }
    }
    else    {
        Message = aBuffer;
    }

    return(AfxMessageBox(Message, nType, nHelpID));
}
