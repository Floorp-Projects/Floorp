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

// tzpart.cpp
// John Sun
// 10:04 AM February 24 1998

#include "stdafx.h"
#include "jdefines.h"

#include "tzpart.h"
#include "prprtyfy.h"
#include "rcrrence.h"
#include "jlog.h"
#include "keyword.h"

#include "functbl.h"
//---------------------------------------------------------------------

// private never use
#if 0
TZPart::TZPart()
{
    PR_ASSERT(FALSE);
}
#endif
//---------------------------------------------------------------------

TZPart::TZPart(JLog * initLog) 
: m_CommentVctr(0), m_DTStart(0), m_TZNameVctr(0), m_TZOffsetTo(0),
  m_TZOffsetFrom(0), m_RDate(0), m_RRule(0),
  m_StartMonth(-1), m_StartDay(-1), m_StartWeek(0), m_StartTime(0),
  m_XTokensVctr(0),
  m_iStartYear(0), m_iStartMonth(0), m_iStartDay(0), m_iStartHour(0), m_iStartMinute(0), m_iStartSecond(0),
  m_iRDateYear(0), m_iRDateMonth(0), m_iRDateDay(0), m_iRDateHour(0), m_iRDateMinute(0), m_iRDateSecond(0),
  m_Log(initLog)
{
    
    m_Until.setTime(-1);
}

//---------------------------------------------------------------------

TZPart::TZPart(TZPart & that) 
: m_CommentVctr(0), m_DTStart(0), m_TZNameVctr(0), m_TZOffsetTo(0),
  m_TZOffsetFrom(0), m_RDate(0), m_RRule(0), 
  m_iStartYear(0), m_iStartMonth(0), m_iStartDay(0), m_iStartHour(0), m_iStartMinute(0), m_iStartSecond(0),
  m_iRDateYear(0), m_iRDateMonth(0), m_iRDateDay(0), m_iRDateHour(0), m_iRDateMinute(0), m_iRDateSecond(0),
  m_XTokensVctr(0)
{
    m_StartMonth = that.m_StartMonth;
    m_StartDay = that.m_StartDay;
    m_StartWeek = that.m_StartWeek;
    m_StartTime = that.m_StartTime;
    m_Until = that.m_Until;
    m_Name = that.m_Name;

    m_iStartYear = that.m_iStartYear;
    m_iStartMonth = that.m_iStartMonth;
    m_iStartDay = that.m_iStartDay;
    m_iStartHour = that.m_iStartHour;
    m_iStartMinute = that.m_iStartMinute;
    m_iStartSecond = that.m_iStartSecond;

    m_iRDateYear = that.m_iRDateYear;
    m_iRDateMonth = that.m_iRDateMonth;
    m_iRDateDay = that.m_iRDateDay;
    m_iRDateHour = that.m_iRDateHour;
    m_iRDateMinute = that.m_iRDateMinute;
    m_iRDateSecond = that.m_iRDateSecond;

    if (that.m_CommentVctr != 0)
    {
        m_CommentVctr = new JulianPtrArray(); PR_ASSERT(m_CommentVctr != 0);
        if (m_CommentVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_CommentVctr, m_CommentVctr, m_Log);
        }
    }

    if (that.m_DTStart != 0) { m_DTStart = that.m_DTStart->clone(m_Log); }

    if (that.m_TZNameVctr != 0)
    {
        m_TZNameVctr = new JulianPtrArray(); PR_ASSERT(m_TZNameVctr != 0);
        if (m_TZNameVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_TZNameVctr, m_TZNameVctr, m_Log);
        }
    }
    if (that.m_TZOffsetTo != 0) 
    { 
        m_TZOffsetTo = that.m_TZOffsetTo->clone(m_Log); 
    }
    if (that.m_TZOffsetFrom != 0) 
    { 
        m_TZOffsetFrom = that.m_TZOffsetFrom->clone(m_Log); 
    }
    if (that.m_RDate != 0) 
    { 
        m_RDate = that.m_RDate->clone(m_Log); 
    }
    if (that.m_RRule != 0) 
    { 
        m_RRule = that.m_RRule->clone(m_Log); 
    }
    if (that.m_XTokensVctr != 0)
    {
        m_XTokensVctr = new JulianPtrArray(); PR_ASSERT(m_XTokensVctr != 0);
        if (m_XTokensVctr != 0)
        {
            ICalProperty::CloneUnicodeStringVector(that.m_XTokensVctr, m_XTokensVctr);
        }
    }
}
//---------------------------------------------------------------------
ICalComponent *
TZPart::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new TZPart(*this);
}
//---------------------------------------------------------------------

TZPart::~TZPart()
{
    if (m_CommentVctr != 0) 
    {
        ICalProperty::deleteICalPropertyVector(m_CommentVctr);
        delete m_CommentVctr; m_CommentVctr = 0;
    }
    if (m_TZNameVctr != 0) 
    {
        ICalProperty::deleteICalPropertyVector(m_TZNameVctr);
        delete m_TZNameVctr; m_TZNameVctr = 0;
    }      
    if (m_DTStart != 0) 
    { 
        delete m_DTStart; m_DTStart = 0; 
    }
    if (m_RDate != 0) 
    { 
        delete m_RDate; m_RDate = 0; 
    }
    if (m_RRule != 0) 
    { 
        delete m_RRule; m_RRule = 0; 
    }
    if (m_TZOffsetTo != 0) 
    { 
        delete m_TZOffsetTo; m_TZOffsetTo = 0; 
    }
    if (m_TZOffsetFrom != 0) 
    { 
        delete m_TZOffsetFrom; m_TZOffsetFrom = 0; 
    }
    if (m_XTokensVctr != 0) 
    { 
        ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
}

//---------------------------------------------------------------------
UnicodeString &
TZPart::parse(ICalReader * brFile, UnicodeString & sType, 
              UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
              t_bool bIgnoreBeginError,
              JulianUtility::MimeEncoding encoding)
{
    UnicodeString strLine, propName, propVal;
    JulianPtrArray * parameters = new JulianPtrArray();
    parseStatus = JulianKeyword::Instance()->ms_sOK;

    PR_ASSERT(parameters != 0 && brFile != 0);
    if (parameters == 0 || brFile == 0)
    {
        // ran out of memory, return invalid tzpart
        return parseStatus;
    }
    ErrorCode status = ZERO_ERROR;
    
    setName(sType);

    // NOTE: Remove later to remove a stupid compile warning
    if (vTimeZones && bIgnoreBeginError){}

    while (TRUE)
    {
        PR_ASSERT(brFile != 0);
        brFile->readFullLine(strLine, status);
        ICalProperty::Trim(strLine);

        if (FAILURE(status) && strLine.size() == 0)
            break;
        
        ICalProperty::parsePropertyLine(strLine, propName, propVal, parameters);
      
        if (strLine.size() == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            continue;
        }
        // END:STANDARD OR END:DAYLIGHT (sType = daylight or standard)
         else if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                  (propVal.compareIgnoreCase(sType) == 0))
         {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

            break;
         }
         else if (
                 ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                  (
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) ||
                  (ICalProperty::IsXToken(propVal))
                  ))
                  ||
                 ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) &&
                  (
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sDAYLIGHT) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sSTANDARD) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0) ||
                  (ICalProperty::IsXToken(propVal))
                  )))
         {
             // abrupt end             
             // BEGIN:DAYLIGHT or BEGIN:STANDARD
             // BEGIN:VEVENT, VTODO, VJOURNAL, VTIMEZONE, VFREEBUSY, xtoken
             // END:VTIMEZONE, END:VCALENDAR
             if (m_Log) m_Log->logError(
                 JulianLogErrorMessage::Instance()->ms_iAbruptEndOfParsing, 
                 JulianKeyword::Instance()->ms_sTZPART, strLine, 300);
             ICalProperty::deleteICalParameterVector(parameters);
             parameters->RemoveAll();

             parseStatus = strLine;
             break;            
         }
         else 
         {
             storeData(strLine, propName, propVal, parameters);
             ICalProperty::deleteICalParameterVector(parameters);
             parameters->RemoveAll();
         }
    }
    ICalProperty::deleteICalParameterVector(parameters);
    parameters->RemoveAll();
    delete parameters; parameters = 0;

    selfCheck();
    return parseStatus;
}

//---------------------------------------------------------------------
void TZPart::storeData(UnicodeString & strLine, UnicodeString & propName,
                       UnicodeString & propVal, JulianPtrArray * parameters)
{
    t_int32 hashCode = propName.hashCode();
    t_int32 i;
    for (i = 0; 0 != (JulianFunctionTable::Instance()->tzStoreTable[i]).op; i++)
    {
        if ((JulianFunctionTable::Instance()->tzStoreTable[i]).hashCode == hashCode)
        {
            ApplyStoreOp(((JulianFunctionTable::Instance())->tzStoreTable[i]).op, 
                strLine, propVal, parameters, 0);
            return;
        }
    }
    if (ICalProperty::IsXToken(propName))
    {
        addXTokens(strLine);
    }
    else
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidPropertyName, 
            JulianKeyword::Instance()->ms_sTZPART, propName, 200);
    }
}
//---------------------------------------------------------------------
void TZPart::storeComment(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // check parameters
    t_bool bParamValid = ICalProperty::CheckParams(parameters, 
        JulianAtomRange::Instance()->ms_asAltrepLanguageParamRange,
        JulianAtomRange::Instance()->ms_asAltrepLanguageParamRangeSize);
    
    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
    }

    addComment(propVal, parameters);
}
void TZPart::storeTZName(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // check parameters 
    t_bool bParamValid = ICalProperty::CheckParams(parameters, 
        JulianAtomRange::Instance()->ms_asLanguageParamRange,
        JulianAtomRange::Instance()->ms_asLanguageParamRangeSize);
    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
    }

    addTZName(propVal, parameters);
}
void TZPart::storeDTStart(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // assert in local time
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
    }

    if (getDTStartProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sTZPART, 
            JulianKeyword::Instance()->ms_sDTSTART, 100);
    }
    DateTime d(propVal);
    if (DateTime::IsParseable(propVal, m_iStartYear, m_iStartMonth, m_iStartDay, 
        m_iStartHour, m_iStartMinute, m_iStartSecond))
    {
        m_StartTime = (t_int32) ((m_iStartHour * 3600 + m_iStartMinute * 60) * 1000); // set start time in millis
    }
    setDTStart(d, parameters);
}
void TZPart::storeRDate(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // assert in local time
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
    }

    if (getRDateProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sTZPART, 
            JulianKeyword::Instance()->ms_sRDATE, 100);
    }
    // DONE:?TODO: Finish
    DateTime d(propVal);
    DateTime::IsParseable(propVal, m_iRDateYear, m_iRDateMonth, m_iRDateDay, 
        m_iRDateHour, m_iRDateMinute, m_iRDateSecond);
    setRDate(d, parameters);
}
void TZPart::storeRRule(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
 // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
    }

    if (getRRuleProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sTZPART, 
            JulianKeyword::Instance()->ms_sRRULE, 100);
    }
    setRRule(propVal, parameters); 
}
void TZPart::storeTZOffsetTo(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
    }

    if (getTZOffsetToProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sTZPART, 
            JulianKeyword::Instance()->ms_sTZOFFSETTO, 100);
    }
    setTZOffsetTo(propVal, parameters); 
}
void TZPart::storeTZOffsetFrom(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
    }

    if (getTZOffsetFromProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sTZPART, 
            JulianKeyword::Instance()->ms_sTZOFFSETFROM, 100);
    }
    setTZOffsetFrom(propVal, parameters);
}
//---------------------------------------------------------------------
void TZPart::selfCheck()
{
    //if (FALSE) TRACE("starttime = %d, startmonth = %d, startday = %d, startweek = %d\r\n", m_StartTime, m_StartMonth, m_StartDay, m_StartWeek);
    // TODO: finish
    //parseRule();
}
//---------------------------------------------------------------------
t_bool TZPart::isValid()
{
    UnicodeString u;

    // TODO: log that if not parseable TZOFFSETTO:, TZOFFSETFROM:
    // required to have valid dtstart, tzoffsetTo, tzoffsetFrom
    if (!getDTStart().isValid())
    {
        if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingStartingTime, 
                JulianKeyword::Instance()->ms_sTZPART, u, 300);    
        return FALSE;
    }
    u = getTZOffsetTo();
    if (!DateTime::IsParseableUTCOffset(u))
        return FALSE;

    u = getTZOffsetFrom();
    if (!DateTime::IsParseableUTCOffset(u))
        return FALSE;
    
    return TRUE;
}

//---------------------------------------------------------------------

UnicodeString TZPart::toICALString()
{
    return ICalComponent::format(m_Name, JulianFormatString::Instance()->ms_sTZPartAllMessage, "");
}

//---------------------------------------------------------------------

UnicodeString TZPart::toICALString(UnicodeString method, 
                                   UnicodeString name,
                                   t_bool isRecurring)
{
    // NOTE: remove later avoid warnings
    if (isRecurring) {}
    return toICALString();
}

//---------------------------------------------------------------------
UnicodeString TZPart::formatChar(t_int32 c, UnicodeString sFilterAttendee,
                                 t_bool delegateRequest)
{
    UnicodeString s, sResult;
    // NOTE: remove here is get rid of compiler warnings
    if (sFilterAttendee.size() > 0 || delegateRequest)
    {
    }
    
    switch (c)
    {
    case ms_cComment:
        s = JulianKeyword::Instance()->ms_sCOMMENT;
        return ICalProperty::propertyVectorToICALString(s, getComment(), sResult); 
    case ms_cTZName:
        s = JulianKeyword::Instance()->ms_sTZNAME;
        return ICalProperty::propertyVectorToICALString(s, getTZName(), sResult); 
    case ms_cDTStart:
#if 0
        s = JulianKeyword::Instance()->ms_sDTSTART;
        return ICalProperty::propertyToICALString(s, getDTStartProperty(), sResult); 
#else 
        if (getDTStartProperty() != 0)
        {
            sResult += JulianKeyword::Instance()->ms_sDTSTART;
            sResult += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
            DateTime::ToISO8601String(m_iStartYear, m_iStartMonth, m_iStartDay, 
                m_iStartHour, m_iStartMinute, m_iStartSecond, s);
            sResult += s;
            sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
        }
        return sResult;
#endif
    case ms_cRDate:
#if 0
        s = JulianKeyword::Instance()->ms_sRDATE;
        return ICalProperty::propertyToICALString(s, getRDateProperty(), sResult); 
#else
        if (getRDateProperty() != 0)
        {
            sResult += JulianKeyword::Instance()->ms_sRDATE;
            sResult += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
            DateTime::ToISO8601String(m_iRDateYear, m_iRDateMonth, m_iRDateDay, 
                m_iRDateHour, m_iRDateMinute, m_iRDateSecond, s);
            sResult += s;
            sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
        }
        return sResult;
#endif
    case ms_cRRule:
        s = JulianKeyword::Instance()->ms_sRRULE;
        return ICalProperty::propertyToICALString(s, getRRuleProperty(), sResult); 
    case ms_cTZOffsetTo:
        s = JulianKeyword::Instance()->ms_sTZOFFSETTO;
        return ICalProperty::propertyToICALString(s, getTZOffsetToProperty(), sResult); 
    case ms_cTZOffsetFrom:
        s = JulianKeyword::Instance()->ms_sTZOFFSETFROM;
        return ICalProperty::propertyToICALString(s, getTZOffsetFromProperty(), sResult); 
    case ms_cXTokens: 
        return ICalProperty::vectorToICALString(getXTokens(), sResult);
    default:
        return "";
    }
}

//---------------------------------------------------------------------
UnicodeString TZPart::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    UnicodeString s;
    switch (c)
    {
    case ms_cComment:
        return ICalProperty::propertyVectorToString(getComment(), s); 
    case ms_cTZName:
        return ICalProperty::propertyVectorToString(getTZName(), s); 
    case ms_cDTStart:
        return ICalProperty::propertyToString(getDTStartProperty(), dateFmt, s); 
    case ms_cRDate:
        return ICalProperty::propertyToString(getRDateProperty(), dateFmt, s); 
    case ms_cRRule:
        return ICalProperty::propertyToString(getRRuleProperty(), dateFmt, s); 
    case ms_cTZOffsetTo:
        return ICalProperty::propertyToString(getTZOffsetToProperty(), dateFmt, s); 
    case ms_cTZOffsetFrom:
        return ICalProperty::propertyToString(getTZOffsetFromProperty(), dateFmt, s); 
    default:
        return "";
    }
}
//---------------------------------------------------------------------

UnicodeString TZPart::toString()
{    
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_TZPartStrDefaultFmt);
}

//---------------------------------------------------------------------
void TZPart::setName(UnicodeString s) 
{ 
    m_Name = s; 
}
//---------------------------------------------------------------------
#if 0
t_bool TZPart::parseRule()
{
    if (getRDate().isValid())
    {
        parseRDate();
        return TRUE;
    }
    else
    {
        return parseRRule();
    }
}
#endif
//---------------------------------------------------------------------
void TZPart::parseRDate()
{
    PR_ASSERT(getRDate().isValid());
    if (getRDate().isValid())
    {
        DateTime d;
        d = getRDate();

        m_StartMonth = d.get(Calendar::MONTH);
        m_StartWeek = d.get(Calendar::DAY_OF_WEEK_IN_MONTH);
        m_StartDay = d.get(Calendar::DAY_OF_WEEK);   
    }
}
//---------------------------------------------------------------------
t_bool TZPart::parseRRule()
{
    t_int32 i;
    UnicodeString pN, pV, s, strLine;
    ICalParameter * ip;
    t_bool bParseError = FALSE;
    ErrorCode status = ZERO_ERROR;

    // TODO: insert assertion that Recurrence::isValidRRule(getRRule());
    strLine = getRRule();
    //if (FALSE) TRACE("line (size = %d) = ---%s---\r\n", strLine.size(), strLine.toCString(""));   
      
    // manipulating string so I can use the parsePropertyLine method
    strLine += ':'; //JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
    strLine.insert(0, JulianKeyword::Instance()->ms_sRRULE_WITH_SEMICOLON);
    //if (FALSE) TRACE("line (size = %d) = ---%s---\r\n", strLine.size(), strLine.toCString(""));   

    JulianPtrArray * parameters = new JulianPtrArray();
    PR_ASSERT(parameters != 0);
    if (parameters == 0)
    {
        return FALSE;
    }

    ICalProperty::parsePropertyLine(strLine, pN, pV, parameters);
    //if (FALSE) TRACE("pN = --%s--, pV = --%s--, paramSize = %d\r\n", pN.toCString(""), pV.toCString(""), parameters->GetSize());
      
    for (i = 0; i < parameters->GetSize(); i++)
    {
        ip = (ICalParameter *) parameters->GetAt(i);
   
        pN = ip->getParameterName(pN);
        pV = ip->getParameterValue(pV);
        //if (FALSE) TRACE("pN = --%s--, pV = --%s--, paramSize = %d\r\n", pN.toCString(""), pV.toCString(""), parameters->GetSize());
       
        if (pN.compareIgnoreCase(JulianKeyword::Instance()->ms_sBYMONTH) == 0)
        {
            // since month is 0-based, must subtract 1
            char * pVcc = pV.toCString("");
            PR_ASSERT(pVcc != 0);
            m_StartMonth = JulianUtility::atot_int32(pVcc, bParseError, pV.size()) - 1;
            delete [] pVcc; pVcc = 0;
        }
        else if (pN.compareIgnoreCase(JulianKeyword::Instance()->ms_sBYDAY) == 0)
        {
            if (pV.size() == 3)
            {
                // positive week
                char * pVcc = pV.toCString("");
                PR_ASSERT(pVcc != 0);
                m_StartWeek = JulianUtility::atot_int32(pVcc, bParseError, 1);
                delete [] pVcc; pVcc = 0;
                m_StartDay = Recurrence::stringToDay(pV.extract(1, 2, s), bParseError);
            }
            else
            {
                PR_ASSERT(pV.size() == 4);  // note: assumption is pV.size() == 4
                
                char * pVcc = pV.toCString("");
                PR_ASSERT(pVcc != 0);
                m_StartWeek = JulianUtility::atot_int32(pVcc + 1, bParseError, 1); // (always len = 1)
                delete [] pVcc; pVcc = 0;
                m_StartDay = Recurrence::stringToDay(pV.extract(2, 2, s), bParseError); // SU, MO, TU, etc. (always len = 2)

                if (pV[(TextOffset) 0] == '-')
                    m_StartWeek = 0 - m_StartWeek;
                else if (pV[(TextOffset) 0] == '+') { }
                else { bParseError = TRUE; }
                
                // ugly way to handle sign handling
                //s = pV.extract(0, 1, s);
                //if (s == "-") m_StartWeek = - m_StartWeek;
                //else if (s == "+") {}
                //else { bParseError = TRUE; }
            }
        }
        else if (pN.compareIgnoreCase(JulianKeyword::Instance()->ms_sUNTIL) == 0)
        {
            m_Until.setTimeString(pV);
        }
    }
    ICalProperty::deleteICalParameterVector(parameters);
    delete parameters; parameters = 0; 
    // if assertion failed return FALSE.
    if(!(m_StartMonth != -1 && m_StartDay != -1 && m_StartWeek != 0))
    {
        return FALSE;
    }
    if (bParseError || FAILURE(status))
    {
        // BAD RRULE was parsed
        m_StartMonth = -1;
        m_StartDay = -1;
        m_StartWeek = 0;
        return FALSE;
    }
    return TRUE;
}
//---------------------------------------------------------------------
float TZPart::UTCOffsetToFloat(UnicodeString & utcOffset)
{
    t_int32 hour = 0, minute = 0;
    if (DateTime::IsParseableUTCOffset(utcOffset, hour, minute))
    {
        return (float) (hour + (minute / 60));
    }
    PR_ASSERT(FALSE); // all calls should return from above return statement 
    return 0;
}

//---------------------------------------------------------------------

void 
TZPart::updateComponentHelper(TZPart * updatedComponent)
{
    // no need: TZName
    ICalComponent::internalSetProperty(&m_DTStart, updatedComponent->m_DTStart);
    ICalComponent::internalSetProperty(&m_RDate, updatedComponent->m_RDate);
    ICalComponent::internalSetProperty(&m_RRule, updatedComponent->m_RRule);
    ICalComponent::internalSetProperty(&m_TZOffsetTo, updatedComponent->m_TZOffsetTo);
    ICalComponent::internalSetProperty(&m_TZOffsetFrom, updatedComponent->m_TZOffsetFrom);
    ICalComponent::internalSetXTokensVctr(&m_XTokensVctr, updatedComponent->m_XTokensVctr);
}

//---------------------------------------------------------------------

t_bool
TZPart::updateComponent(ICalComponent * updatedComponent)
{
    if (updatedComponent != 0)
    {
        ICAL_COMPONENT ucType = updatedComponent->GetType();

        // only call updateComponentHelper if it's a TZPart and
        // it is an exact matching Name (STANDARD or DAYLIGHT)
        // basically always overwrite
        if (ucType == ICAL_COMPONENT_TZPART)
        {
            // should be a safe cast with check above.
            TZPart * uctzp = (TZPart *) updatedComponent;

            // only if TZID's match and are not empty
            if (uctzp->getName().size() > 0 && getName().size() > 0)
            {
                if (uctzp->getName() == getName())
                {
                    updateComponentHelper(uctzp);
                    selfCheck();
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}
//---------------------------------------------------------------------

//Comment
void TZPart::addComment(UnicodeString s, JulianPtrArray * parameters)
{
    //ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
    //        new UnicodeString(s), 0);

    ICalProperty * prop = 
            ICalPropertyFactory::Make(ICalProperty::TEXT, 
            (void *) &s, parameters);
    
    addCommentProperty(prop);
}
void TZPart::addCommentProperty(ICalProperty * prop)
{
    if (m_CommentVctr == 0)
        m_CommentVctr = new JulianPtrArray();    
    PR_ASSERT(m_CommentVctr != 0);
    if (m_CommentVctr != 0)
    {
        m_CommentVctr->Add(prop);
    }
}

//---------------------------------------------------------------------
//TZName
void TZPart::addTZName(UnicodeString s, JulianPtrArray * parameters)
{
    //ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
    //        new UnicodeString(s), 0);

    ICalProperty * prop = 
            ICalPropertyFactory::Make(ICalProperty::TEXT, 
            (void *) &s, parameters);

    addTZNameProperty(prop);
}
void TZPart::addTZNameProperty(ICalProperty * prop)
{
    if (m_TZNameVctr == 0)
        m_TZNameVctr = new JulianPtrArray();    
    PR_ASSERT(m_TZNameVctr != 0);
    if (m_TZNameVctr != 0)
    {
       m_TZNameVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// DTStart
void TZPart::setDTStart(DateTime s, JulianPtrArray * parameters)
{ 
#if 1
    if (m_DTStart == 0)
        m_DTStart = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_DTStart->setValue((void *) &s);
        m_DTStart->setParameters(parameters);
    }
#else
    ICalComponent::setDateTimeValue(((ICalProperty **) &m_DTStart), s, parameters);
#endif
}

DateTime TZPart::getDTStart() const
{
#if 1
    DateTime d(-1);
    if (m_DTStart == 0)
        return d; //return 0;
    else
    {
        d = *((DateTime *) m_DTStart->getValue());
        return d;
    }
#else
    DateTime d(-1);
    ICalComponent::getDateTimeValue(((ICalProperty **) &m_DTStart), d);
    return d;
#endif
}
//---------------------------------------------------------------------
// RDate
// TODO: become a vector
void TZPart::setRDate(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_RDate == 0)
        m_RDate = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_RDate->setValue((void *) &s);
        m_RDate->setParameters(parameters);
    }
    parseRDate();
}

DateTime TZPart::getRDate() const
{
    DateTime d(-1);
    if (m_RDate == 0)
        return d; //return 0;
    else
    {
        d = *((DateTime *) m_RDate->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
//RRule
// TODO: become a vector
void TZPart::setRRule(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_RRule == 0)
        m_RRule = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_RRule->setValue((void *) &s);
        m_RRule->setParameters(parameters);
    }
    parseRRule();
}
UnicodeString TZPart::getRRule() const 
{
    UnicodeString u;
    if (m_RRule == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_RRule->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//TZOffsetTo
void TZPart::setTZOffsetTo(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_TZOffsetTo == 0)
        m_TZOffsetTo = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_TZOffsetTo->setValue((void *) &s);
        m_TZOffsetTo->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_TZOffsetTo), s, parameters);
#endif
}
UnicodeString TZPart::getTZOffsetTo() const 
{
#if 1
    UnicodeString u;
    if (m_TZOffsetTo == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_TZOffsetTo->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_TZOffsetTo), us);
    return us;
#endif
}
//---------------------------------------------------------------------
//TZOffsetFrom
void TZPart::setTZOffsetFrom(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_TZOffsetFrom == 0)
        m_TZOffsetFrom = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_TZOffsetFrom->setValue((void *) &s);
        m_TZOffsetFrom->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_TZOffsetFrom), s, parameters);
#endif
}
UnicodeString TZPart::getTZOffsetFrom() const 
{
#if 1
    UnicodeString u;
    if (m_TZOffsetFrom == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_TZOffsetFrom->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_TZOffsetFrom), us);
    return us;
#endif
}
//---------------------------------------------------------------------
// XTOKENS
void TZPart::addXTokens(UnicodeString s)         
{
    if (m_XTokensVctr == 0)
        m_XTokensVctr = new JulianPtrArray(); 
    PR_ASSERT(m_XTokensVctr != 0);
    if (m_XTokensVctr != 0)
    {
        m_XTokensVctr->Add(new UnicodeString(s));
    }
}
//---------------------------------------------------------------------



