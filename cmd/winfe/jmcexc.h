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

#ifndef __MFCJMCException_H
#define __MFCJMCException_H

#ifdef XP_WIN16
typedef unsigned int * PUINT;
#endif

class CJMCException : public CException
{
	DECLARE_DYNAMIC(CJMCException)
private:
    JMCException *m_pJMC;
public:
    CJMCException(JMCException *& pJMC);
    ~CJMCException();

public:
    JMCException *GetJMCException();
	virtual BOOL GetErrorMessage(LPTSTR lpszError, UINT nMaxError, PUINT pnHelpContext = NULL);
    virtual int ReportError(UINT nType = MB_OK, UINT nMessageID = 0);
};

#define EXCEPTION(ptr, stk) CJMCException stk(&(ptr))

#endif  //  __MFCJMCException_H
