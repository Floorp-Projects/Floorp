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

#ifndef _EDTCLASS_H
#define _EDTCLASS_H

#include "edtrcdll\edtiface.h"

class CWFEWrapper :public IWFEInterface
{
    MWContext *m_mwcontext;
public:
    CWFEWrapper(){}
    void setMWContext(MWContext *pmw){m_mwcontext=pmw;}
    MWContext *getMWContext(){return m_mwcontext;}
public:
    virtual void GetLayoutViewSize(long * pWidth, long * pHeight);
    virtual void WinHelp(char *);
    virtual char *szLoadString(UINT);
};


class CEDTWrapper :public IEDTInterface
{
    MWContext *m_mwcontext;
public:
    CEDTWrapper(){}
    void setMWContext(MWContext *pmw){m_mwcontext=pmw;}
    MWContext *getMWContext(){return m_mwcontext;}
public:
    virtual ED_ElementType GetCurrentElementType();
    virtual char * GetUnknownTagData();
    virtual ED_TagValidateResult ValidateTag( char *pData, XP_Bool bNoBrackets );
    virtual void BeginBatchChanges();
    virtual void EndBatchChanges();
    virtual void InsertUnknownTag( char* pUnknownTagData );
    virtual void SetUnknownTagData(char *pUnknownTagData );
};

#endif //_EDTCLASS_H


