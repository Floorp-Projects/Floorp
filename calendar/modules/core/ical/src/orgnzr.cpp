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

// orgnzr.cpp
// John Sun
// 4/7/98 11:28:26 AM

#include "stdafx.h"
#include "jdefines.h"

#include "orgnzr.h"
#include "jutility.h"
#include "keyword.h"
#include "uri.h"
//---------------------------------------------------------------------

const t_int32 nsCalOrganizer::ms_cnsCalOrganizerName          = 'N';
const t_int32 nsCalOrganizer::ms_cnsCalOrganizerDir           = 'l'; // 'el'
const t_int32 nsCalOrganizer::ms_cnsCalOrganizerSentBy        = 's';
const t_int32 nsCalOrganizer::ms_cnsCalOrganizerCN            = 'C';
const t_int32 nsCalOrganizer::ms_cnsCalOrganizerLanguage      = 'm';
const t_int32 nsCalOrganizer::ms_cnsCalOrganizerDisplayName   = 'z';

//---------------------------------------------------------------------

nsCalOrganizer::nsCalOrganizer(JLog * initLog)
: m_Log(initLog)
{
}

//---------------------------------------------------------------------

nsCalOrganizer::nsCalOrganizer(nsCalOrganizer & that)
{
    m_CN = that.m_CN;
    m_Language = that.m_Language;
    m_SentBy = that.m_SentBy;
    m_Dir = that.m_Dir;

    m_Name = that.m_Name;
}

//---------------------------------------------------------------------

nsCalOrganizer::~nsCalOrganizer()
{
}

//---------------------------------------------------------------------

ICalProperty * 
nsCalOrganizer::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return (ICalProperty *) new nsCalOrganizer(*this);
}

//---------------------------------------------------------------------

void nsCalOrganizer::parse(UnicodeString & propVal, 
                      JulianPtrArray * parameters)
{
    t_int32 i;
    ICalParameter * param;
    UnicodeString pName, pVal;
    if (propVal.size() == 0)
    {
        return;
    }
    setName(propVal);
    if (parameters != 0)
    {
        for (i = 0; i < parameters->GetSize(); i++)
        {
            param = (ICalParameter *) parameters->GetAt(i);
           
            setParam(param->getParameterName(pName), 
                param->getParameterValue(pVal));
        }   
    }
}

//---------------------------------------------------------------------

void nsCalOrganizer::setParam(UnicodeString & paramName, 
                         UnicodeString & paramVal)
{
    ErrorCode status = ZERO_ERROR;    
    
    //if (FALSE) TRACE("(%s, %s)\r\n", paramName.toCString(""), paramVal.toCString(""));
    if (paramName.size() == 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidParameterName, 
            JulianKeyword::Instance()->ms_sORGANIZER, paramName, 200);
    }
    else
    {
        t_int32 hashCode = paramName.hashCode();

        if (JulianKeyword::Instance()->ms_ATOM_CN == hashCode)
        {
            if (getCN().size() != 0)
            {
                 if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iDuplicatedParameter,
                        JulianKeyword::Instance()->ms_sORGANIZER, paramName, 100);
            }
            setCN(paramVal);
        }
        else if (JulianKeyword::Instance()->ms_ATOM_LANGUAGE == hashCode)
        {
            if (getLanguage().size() != 0)
            {
                 if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iDuplicatedParameter,
                        JulianKeyword::Instance()->ms_sORGANIZER, paramName, 100);
            }
            setLanguage(paramVal);
        }
        else if (JulianKeyword::Instance()->ms_ATOM_SENTBY == hashCode)
        {
            if (getSentBy().size() != 0)
            {
                 if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iDuplicatedParameter,
                        JulianKeyword::Instance()->ms_sORGANIZER, paramName, 100);
            }
            nsCalUtility::stripDoubleQuotes(paramVal);  // double quote property
            setSentBy(paramVal);
        }
        else if (JulianKeyword::Instance()->ms_ATOM_DIR == hashCode)
        {
            if (getDir().size() != 0)
            {
                 if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iDuplicatedParameter,
                        JulianKeyword::Instance()->ms_sORGANIZER, paramName, 100);
            }
            nsCalUtility::stripDoubleQuotes(paramVal);  // double quote property
            setDir(paramVal);
        }
        else if (ICalProperty::IsXToken(paramName))
        {
            if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iXTokenParamIgnored,
                        JulianKeyword::Instance()->ms_sORGANIZER, paramName, 100);
        }
        else 
        {
            if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iInvalidParameterName,
                        JulianKeyword::Instance()->ms_sORGANIZER, paramName, 200);
        }
    }
}

//---------------------------------------------------------------------

UnicodeString & nsCalOrganizer::toICALString(UnicodeString & out)
{
    UnicodeString u;
    return toICALString(u, out);
}

//---------------------------------------------------------------------

UnicodeString & nsCalOrganizer::toICALString(UnicodeString & sProp,
                                        UnicodeString & out)
{
    out = "";
    out += sProp;
    UnicodeString u;
    if (m_CN.size() > 0)
    {
        out += ';'; out += JulianKeyword::Instance()->ms_sCN; 
        out += '='; out += m_CN;
    }
    if (m_Language.size() > 0)
    {
        out += ';'; out += JulianKeyword::Instance()->ms_sLANGUAGE; 
        out += '='; out += m_Language;
    }
    if (m_SentBy.size() > 0)
    {
        // double quote sent-by, dir
        u = m_SentBy;
        u = nsCalUtility::addDoubleQuotes(u);
        out += ';'; out += JulianKeyword::Instance()->ms_sSENTBY; 
        out += '='; out += u;
    }
    if (m_Dir.size() > 0)
    {
        // double quote sent-by, dir
        u = m_Dir;
        u = nsCalUtility::addDoubleQuotes(u);
        out += ';'; out += JulianKeyword::Instance()->ms_sDIR; 
        out += '='; out += u;
    }
    out += ':';
    out += m_Name;
    out += JulianKeyword::Instance()->ms_sLINEBREAK;
    return out;
}

//---------------------------------------------------------------------

UnicodeString & nsCalOrganizer::toString(UnicodeString & out)
{
    out = toString(JulianFormatString::Instance()->ms_OrganizerStrDefaultFmt, out);
    return out;
}

//---------------------------------------------------------------------

UnicodeString & nsCalOrganizer::toString(UnicodeString & strFmt,
                                    UnicodeString & out)
{
    if (strFmt.size() == 0 && 
        JulianFormatString::Instance()->ms_OrganizerStrDefaultFmt.size() > 0)
    {
        // if empty string, use default
        return toString(out);
    }

    UnicodeString into;
    t_int32 i,j;    
    //if (FALSE) TRACE("strFmt = %s\r\n", strFmt.toCString(""));
    out = "";
    for ( i = 0; i < strFmt.size(); )
    {
	
        // NOTE: changed from % to ^ for attendee
        ///
	    ///  If there's a special formatting character,
	    ///  handle it.  Otherwise, just emit the supplied
	    ///  character.
	    ///

        j = strFmt.indexOf('^', i);
        if ( -1 != j)
        {
	        if (j > i)
            {
	            out += strFmt.extractBetween(i,j,into);
            }
            i = j + 1;
            if ( strFmt.size() > i)
            {
                out += toStringChar(strFmt[(TextOffset) i]); 
                i++;
            }
	    }
	    else
        {
          out += strFmt.extractBetween(i, strFmt.size(),into);
          break;
        }
    }
    return out;
}

//---------------------------------------------------------------------

UnicodeString nsCalOrganizer::toStringChar(t_int32 c)
{
    switch(c)
    {
    case ms_cnsCalOrganizerName:
        return getName();
    case ms_cnsCalOrganizerDir:
        return m_Dir;
    case ms_cnsCalOrganizerSentBy:
        return m_SentBy;
    case ms_cnsCalOrganizerCN:
        return m_CN;
    case ms_cnsCalOrganizerLanguage:
        return m_Language;
    case ms_cnsCalOrganizerDisplayName:
        // return CN is CN != "", else
        // return the nsCalOrganizerName after the ':'
        if (m_CN.size() > 0)
            return m_CN;
        else
        {
            UnicodeString u;
            t_int32 i = m_Name.indexOf(':');
            if (i >= 0)
            {
                u = getName().extractBetween(i + 1, getName().size(), u);
                return u;
            }
            else 
                return "";
        }
     default:
	    return "";
    }
}

//---------------------------------------------------------------------

t_bool nsCalOrganizer::isValid()
{
    /*
    UnicodeString mailto;
    if (getName().size() < 7)
        return FALSE;
    // change to URL, must have "MAILTO:" in front
    mailto = getName().extractBetween(0, 7, mailto);
    if (mailto.compareIgnoreCase(JulianKeyword::Instance()->ms_sMAILTO_COLON) != 0)
        return FALSE;
    else
        return TRUE;
        */
    return URI::IsValidURI(m_Name);
}

//---------------------------------------------------------------------

void *
nsCalOrganizer::getValue() const
{
    return (void *) &m_Name;
}

//---------------------------------------------------------------------

void
nsCalOrganizer::setValue(void * value)
{
    PR_ASSERT(value != 0);
    if (value != 0)
    {
        m_Name = *((UnicodeString *) value);
    }
}

//---------------------------------------------------------------------

void nsCalOrganizer::setParameters(JulianPtrArray * parameters)
{
    t_int32 i;
    ICalParameter * param;
    UnicodeString pName, pVal;
    if (parameters != 0)
    {
        for (i = 0; i < parameters->GetSize(); i++)
        {
            param = (ICalParameter *) parameters->GetAt(i);
            setParam(param->getParameterName(pName), 
                param->getParameterValue(pVal));
        }   
    }
}

//---------------------------------------------------------------------

void nsCalOrganizer::setName(UnicodeString sName) 
{ 
    m_Name = sName; 
}

//---------------------------------------------------------------------
