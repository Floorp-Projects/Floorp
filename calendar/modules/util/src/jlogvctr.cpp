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
 * jlogvctr.cpp
 * John Sun
 * 8/17/98 4:48:46 PM
 */

#include "stdafx.h"
#include "jdefines.h"
#include "jlogvctr.h"

//---------------------------------------------------------------------
nsCalLogErrorVector::nsCalLogErrorVector()
{
    m_ICalComponentType = ECompType_NSCALENDAR;
    m_ErrorVctr = 0;
    m_bValidEvent = TRUE;
}
//---------------------------------------------------------------------
nsCalLogErrorVector::nsCalLogErrorVector(ECompType iICalComponentType)
{
    m_ICalComponentType = iICalComponentType;
    m_ErrorVctr = 0;
    m_bValidEvent = TRUE;
}
//---------------------------------------------------------------------
nsCalLogErrorVector::~nsCalLogErrorVector()
{
    if (m_ErrorVctr != 0)
    {
        nsCalLogError::deleteNsCalLogErrorVector(m_ErrorVctr);   
        delete m_ErrorVctr; m_ErrorVctr = 0;
    }
}
//---------------------------------------------------------------------
void nsCalLogErrorVector::AddError(nsCalLogError * error)
{
    if (m_ErrorVctr == 0)
        m_ErrorVctr = new JulianPtrArray();
    if (m_ErrorVctr != 0)
        m_ErrorVctr->Add(error);
}
//---------------------------------------------------------------------
