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

// jlog.cpp
// John Sun
// 10:52 AM March 2 1998

#include "stdafx.h"
#include "jdefines.h"
#include <unistring.h>
#include "jlog.h"
#include "jlogerr.h"
#include "jlogvctr.h"

//---------------------------------------------------------------------

const t_int32 JLog::m_DEFAULT_LEVEL = 200;
const char * JLog::m_DEFAULT_FILENAME = "JLog.txt";

//---------------------------------------------------------------------
t_bool JLog::addErrorToVector(JulianLogError * error)
{
    t_bool status = FALSE;
    if (m_CurrentEventLogVctr == 0)
    {
        addEventErrorEntry();
    }
    if (m_CurrentEventLogVctr != 0)
    {
        if (error->getPriority() >= m_Level)
        {
            m_CurrentEventLogVctr->AddError(error);
            status = TRUE;
        }
    }
    return status;
}
//---------------------------------------------------------------------

JLog::JLog(t_int32 initLevel) :
 m_Level(initLevel),
 m_FileName(0),
 m_WriteToString(TRUE),
 m_ErrorVctr(0),
 m_CurrentEventLogVctr(0),
 m_Stream(0)
{
}
 
//---------------------------------------------------------------------

JLog::JLog(const char * initFileName, t_int32 initLevel) :
 m_Level(initLevel),
 m_FileName(initFileName),
 m_WriteToString(FALSE),
 m_ErrorVctr(0),
 m_CurrentEventLogVctr(0),
 m_Stream(0)
{
    m_Stream = fopen(m_FileName, "w");
    assert(m_Stream != 0);
}

//---------------------------------------------------------------------

JLog::~JLog()
{
    if (m_ErrorVctr != 0)
    {
        JulianLogErrorVector * evtLogVctr = 0;
        t_int32 i = 0;
        for (i = m_ErrorVctr->GetSize() - 1; i >= 0; i--)
        {
            evtLogVctr = (JulianLogErrorVector *) m_ErrorVctr->GetAt(i);
            delete evtLogVctr;
        }
        delete m_ErrorVctr; m_ErrorVctr = 0;
    }
    if (m_Stream != 0)
        fclose(m_Stream);
}
//---------------------------------------------------------------------

void JLog::addEventErrorEntry()
{
    if (m_ErrorVctr == 0)
    {
        m_ErrorVctr = new JulianPtrArray();
    }
    if (m_ErrorVctr != 0)
    {
        JulianLogErrorVector * evtVctr = 0;
        evtVctr = new JulianLogErrorVector();
        if (evtVctr != 0)
        {
            m_ErrorVctr->Add(evtVctr);
            m_CurrentEventLogVctr = evtVctr;
        }
    }
}

//---------------------------------------------------------------------

void JLog::setCurrentEventLogValidity(t_bool b)
{
    if (m_CurrentEventLogVctr != 0)
    {
        m_CurrentEventLogVctr->SetValid(b);
    }
}

//---------------------------------------------------------------------

void JLog::setCurrentEventLogComponentType(JulianLogErrorVector::ECompType iComponentType)
{
    if (m_CurrentEventLogVctr != 0)
    {
        m_CurrentEventLogVctr->SetComponentType(iComponentType);
    }
}
//---------------------------------------------------------------------
#if 0
void JLog::setUIDRecurrenceID(UnicodeString & uid, UnicodeString & rid)
{
    if (m_CurrentEventLogVctr != 0)
    {
        m_CurrentEventLogVctr->SetUID(uid);
        m_CurrentEventLogVctr->SetRecurrenceID(rid);
    }
}
#else
void JLog::setUID(UnicodeString & uid)
{
    if (m_CurrentEventLogVctr != 0)
    {
        m_CurrentEventLogVctr->SetUID(uid);
    }
}
#endif
//---------------------------------------------------------------------

JulianPtrArray * 
JLog::getEventErrorLog(t_int32 index) const
{
    if (index < 0)
        return 0;
    else if (m_ErrorVctr == 0)
        return 0;
    else if (m_ErrorVctr->GetSize() < index)
        return 0;
    else
        return ((JulianPtrArray *) m_ErrorVctr->GetAt(index));
}

//---------------------------------------------------------------------
#if 0
JulianLogIterator * 
JLog::createIterator(JLog * aLog, JulianLogErrorVector::ECompType iComponentType, t_bool bValid)
{
    if (aLog->GetErrorVector() == 0)
        return 0;
    else
        return JulianLogIterator::createIterator(aLog->GetErrorVector(), iComponentType, bValid);
}
#endif
//---------------------------------------------------------------------
void JLog::logError(const t_int32 errorID, t_int32 level)
{
    UnicodeString u, v, w;
    logError(errorID, u, v, w, level);
}
 //---------------------------------------------------------------------
   
void JLog::logError(const t_int32 errorID, 
                    UnicodeString & comment, t_int32 level)
{
    UnicodeString u, v;
    logError(errorID, comment, u, v, level);
}
//---------------------------------------------------------------------

void JLog::logError(const t_int32 errorID,
                    UnicodeString & propName, UnicodeString & propValue,
                    t_int32 level)
{
    UnicodeString u;
    logError(errorID, propName, propValue, u, level);
}

//---------------------------------------------------------------------

void JLog::logError(const t_int32 errorID,
                    UnicodeString & propName, 
                    UnicodeString & paramName, 
                    UnicodeString & paramValue,
                    t_int32 level)
{
    JulianLogError * error = 0;
    UnicodeString shortReturnStatus = "3.0";
    UnicodeString offendingData;

    offendingData += propName; offendingData += ':'; offendingData += paramName;
    offendingData += ':'; offendingData += paramValue;
    error = new JulianLogError(errorID, shortReturnStatus, offendingData, level);
    if (error != 0)
    {
        t_bool bAdded;
        bAdded = addErrorToVector(error);
        if (!bAdded)
        {
            delete error; error = 0;
        }
    }
}

//---------------------------------------------------------------------
