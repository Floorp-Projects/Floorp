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

// recid.cpp
// John Sun
// 3/20/98 5:18:34 PM

#include "stdafx.h"
#include "jdefines.h"
#include "recid.h"
#include "keyword.h"
#include "jlog.h"
//---------------------------------------------------------------------

// private never use
#if 0
nsCalRecurrenceID::nsCalRecurrenceID()
{
    PR_ASSERT(FALSE);
}
#endif
//---------------------------------------------------------------------

nsCalRecurrenceID::nsCalRecurrenceID(JLog * initLog)
: m_DateTime(-1), m_Range(nsCalRecurrenceID::RANGE_NONE),
  m_Log(initLog)
{
    //PR_ASSERT(initLog != 0);
}

//---------------------------------------------------------------------

nsCalRecurrenceID::nsCalRecurrenceID(nsCalRecurrenceID & that)
: m_Range(that.m_Range)
{
    m_DateTime = that.m_DateTime;
}

//---------------------------------------------------------------------

nsCalRecurrenceID::nsCalRecurrenceID(DateTime datetime,
                                       JLog * initLog,
                                       RANGE range)

: m_Range(range), m_Log(initLog)
{
    m_DateTime = datetime;
    //PR_ASSERT(initLog != 0);
}

//---------------------------------------------------------------------

nsCalRecurrenceID::~nsCalRecurrenceID() {}

//---------------------------------------------------------------------

ICalProperty * nsCalRecurrenceID::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new nsCalRecurrenceID(*this);
}

//---------------------------------------------------------------------

void nsCalRecurrenceID::setParameters(JulianPtrArray * parameters)
{
    t_int32 i;
    ICalParameter * ip;
    UnicodeString pName;
    UnicodeString pVal;
    if (parameters != 0)
    {
        for (i = 0; i < parameters->GetSize(); i++)
        {
            ip = (ICalParameter *) parameters->GetAt(i);
            setParam(ip->getParameterName(pName), ip->getParameterValue(pVal));
        }
    }
}

//---------------------------------------------------------------------

void nsCalRecurrenceID::setParam(UnicodeString & paramName, UnicodeString & paramVal)
{
    t_int32 i;
    if (paramName.size() == 0)
    {
        if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iInvalidParameterName, 
            JulianKeyword::Instance()->ms_sRECURRENCEID, paramName, 200);
    }
    else
    {
        t_int32 hashCode = paramName.hashCode();

        if (JulianKeyword::Instance()->ms_ATOM_RANGE == hashCode)
        {
            i = nsCalRecurrenceID::stringToRange(paramVal);
            if (i < 0)
            {
                if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iInvalidParameterValue, 
                    JulianKeyword::Instance()->ms_sRECURRENCEID, paramName, paramVal, 200);
            }
            else 
            {
                if (getRange() >= 0)
                {
                    if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iDuplicatedParameter, 
                        JulianKeyword::Instance()->ms_sRECURRENCEID, paramName, 100);
                }
                setRange((nsCalRecurrenceID::RANGE) i);
            }        
        } 
        else if (ICalProperty::IsXToken(paramName))
        {
            if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iXTokenParamIgnored,
                        JulianKeyword::Instance()->ms_sRECURRENCEID, paramName, 100);
        }
        else 
        {
            // NOTE: what about optional parameters?? THERE ARE NONE.
            if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iInvalidParameterName,
                        JulianKeyword::Instance()->ms_sRECURRENCEID, paramName, 200);
        }
    }
}

//---------------------------------------------------------------------

t_bool nsCalRecurrenceID::isValid()
{
    return m_DateTime.isValid();
}

//---------------------------------------------------------------------

UnicodeString &
nsCalRecurrenceID::toString(UnicodeString & strFmt, UnicodeString & out)
{
    // NOTE: Remove later 
    if (strFmt.size() > 0) {}

    return toString(out);
}

//---------------------------------------------------------------------

void 
nsCalRecurrenceID::setValue(void * value)
{
    PR_ASSERT(value != 0);
    if (value != 0)
    {
        setDateTime(*((DateTime *) value));
    }
}

//---------------------------------------------------------------------

void *
nsCalRecurrenceID::getValue() const
{
    return (void *) &m_DateTime;
}

//---------------------------------------------------------------------

UnicodeString & 
nsCalRecurrenceID::toString(UnicodeString & out)
{
    UnicodeString u;
    out = "";
    out += JulianKeyword::Instance()->ms_sRANGE; 
    out += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
    out += rangeToString(m_Range, u);
    out += JulianKeyword::Instance()->ms_sDATE; 
    out += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
    out += m_DateTime.toISO8601();
    return out;
}

//---------------------------------------------------------------------

UnicodeString &
nsCalRecurrenceID::toICALString(UnicodeString & sProp, UnicodeString & out)
{
    out = "";
    out += sProp;
    if (m_Range != nsCalRecurrenceID::RANGE_NONE)
    {
        UnicodeString u;
        out += ';';
        out += JulianKeyword::Instance()->ms_sRANGE; 
        out += '=';
        out += rangeToString(m_Range, u);
    }
    out += ':';
    out += m_DateTime.toISO8601();
    out += JulianKeyword::Instance()->ms_sLINEBREAK;
    return out;
}

//---------------------------------------------------------------------

UnicodeString &
nsCalRecurrenceID::toICALString(UnicodeString & out)
{
    UnicodeString u;
    return toICALString(u, out);
}

//---------------------------------------------------------------------

nsCalRecurrenceID::RANGE 
nsCalRecurrenceID::stringToRange(UnicodeString & sRange)
{
    t_int32 hashCode = sRange.hashCode();

    if (JulianKeyword::Instance()->ms_ATOM_THISANDPRIOR == hashCode) 
        return RANGE_THISANDPRIOR;
    else if (JulianKeyword::Instance()->ms_ATOM_THISANDFUTURE == hashCode) 
        return RANGE_THISANDFUTURE;
    else return RANGE_NONE;
}

//---------------------------------------------------------------------

UnicodeString & 
nsCalRecurrenceID::rangeToString(nsCalRecurrenceID::RANGE range, 
                                  UnicodeString & out)
{
    out = "";
    switch(range)
    {
    case RANGE_THISANDPRIOR: out = JulianKeyword::Instance()->ms_sTHISANDPRIOR; break;
    case RANGE_THISANDFUTURE: out = JulianKeyword::Instance()->ms_sTHISANDFUTURE; break;
    default:
        // NONE case
        out = "";
    }
    return out;
}

//---------------------------------------------------------------------

