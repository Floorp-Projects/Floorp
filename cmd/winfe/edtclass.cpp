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

//edtclass.cpp

#include "stdafx.h"
#include "edtclass.h"
#include "edprops.h"//for wfe call
#include "edt.h"
//#include "edtable.h"
#include "xp_help.h"
#include "nethelp.h"

//CWFEWRAPPER

void
CWFEWrapper::GetLayoutViewSize(long *pWidth, long *pHeight)
{
    XP_ASSERT(m_mwcontext);
    wfe_GetLayoutViewSize(m_mwcontext,pWidth,pHeight);
}

void
CWFEWrapper::WinHelp(char * charp)
{
    NetHelp(charp);
}

char *
CWFEWrapper::szLoadString(UINT uint)
{
    return ::szLoadString(uint);
}




//CEDTWRAPPER

ED_ElementType 
CEDTWrapper::GetCurrentElementType()
{
    XP_ASSERT(m_mwcontext);
    return EDT_GetCurrentElementType(m_mwcontext);
}

char * 
CEDTWrapper::GetUnknownTagData()
{
    XP_ASSERT(m_mwcontext);
    return EDT_GetUnknownTagData(m_mwcontext);
}

ED_TagValidateResult 
CEDTWrapper::ValidateTag(char *pData, XP_Bool bNoBrackets )
{
    return EDT_ValidateTag(pData,bNoBrackets);
}

void 
CEDTWrapper::BeginBatchChanges()
{
    XP_ASSERT(m_mwcontext);
    EDT_BeginBatchChanges(m_mwcontext);
}

void 
CEDTWrapper::EndBatchChanges()
{
    XP_ASSERT(m_mwcontext);
    EDT_EndBatchChanges(m_mwcontext);
}

void 
CEDTWrapper::InsertUnknownTag( char* pUnknownTagData )
{
    XP_ASSERT(m_mwcontext);
    EDT_InsertUnknownTag(m_mwcontext,pUnknownTagData);
}

void 
CEDTWrapper::SetUnknownTagData( char* pUnknownTagData )
{
    XP_ASSERT(m_mwcontext);
    EDT_SetUnknownTagData(m_mwcontext,pUnknownTagData);
}



