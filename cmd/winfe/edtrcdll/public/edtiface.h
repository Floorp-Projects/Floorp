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

//interfaces for EDT_CODE
#ifndef _EDTIFACE_H
#define _EDTIFACE_H

#include "lo_ele.h"
#include "edttypes.h"

#ifndef rc_inferface
#define     rc_interface class
#endif


rc_interface IWFEInterface
{
public:
    virtual void GetLayoutViewSize(long * pWidth, long * pHeight)=0;
    virtual void WinHelp(char *)=0;
    virtual char *szLoadString(UINT)=0;
};


rc_interface IEDTInterface
{
public:
    virtual ED_ElementType GetCurrentElementType()=0;
    virtual char * GetUnknownTagData()=0;
    virtual ED_TagValidateResult ValidateTag( char *pData, XP_Bool bNoBrackets )=0;
    virtual void BeginBatchChanges()=0;
    virtual void EndBatchChanges()=0;
    virtual void InsertUnknownTag( char* pUnknownTagData )=0;
    virtual void SetUnknownTagData(char *pUnknownTagData )=0;
};

#endif //_EDTIFACE_H

