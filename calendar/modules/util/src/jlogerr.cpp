/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the 'NPL'); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an 'AS IS' basis,
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
 * jlogerr.cpp
 * John Sun
 * 6/22/98 2:46:34 PM
 */
#include "stdafx.h"
#include "jdefines.h"
#include "jlogerr.h"

//---------------------------------------------------------------------
#if 0
nsCalLogError::nsCalLogError(UnicodeString & errorString, 
                               t_int32 errorPriority)
{
    m_ErrorString = errorString;
    m_Priority = errorPriority;
    //m_ID = errorID;
}
#endif
//---------------------------------------------------------------------
nsCalLogError::nsCalLogError(t_int32 errorID,
                               UnicodeString & shortReturnStatusCode,
                               UnicodeString & offendingData,
                               t_int32 errorPriority)
{
    m_ErrorID = errorID;
    m_ShortReturnStatusCode = shortReturnStatusCode;
    m_OffendingData = offendingData;
    m_Priority = errorPriority;
}
//---------------------------------------------------------------------
void nsCalLogError::deleteNsCalLogErrorVector(JulianPtrArray * errors)
{
    if (errors != 0)
    {
        t_int32 i;
        for (i = errors->GetSize() - 1; i >= 0; i--)
        {
            delete ((nsCalLogError *) errors->GetAt(i));
        }
    }
}
