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

// vevent.cpp
// John Sun
// 10:50 AM February 9 1998

#include "stdafx.h"
#include "jdefines.h"

#include "jutility.h"
#include "vevent.h"
#include "icalredr.h"
#include "prprtyfy.h"
#include "unistrto.h"
#include "jlog.h"
#include "vtimezne.h"
#include "keyword.h"

#include "period.h"
#include "datetime.h"

#include "functbl.h"
//---------------------------------------------------------------------
void VEvent::setDefaultFmt(UnicodeString s)
{
    JulianFormatString::Instance()->ms_VEventStrDefaultFmt = s;
}
//---------------------------------------------------------------------

// never use
#if 0
VEvent::VEvent()
{
    PR_ASSERT(FALSE);
}
#endif
//---------------------------------------------------------------------

VEvent::VEvent(JLog * initLog)
:   m_DTEnd(0), 
    m_TempDuration(0),
    m_GEO(0), m_Location(0), m_Priority(0), m_ResourcesVctr(0), 
    m_Transp(0),
    TimeBasedEvent(initLog)
{
}
//---------------------------------------------------------------------
VEvent::VEvent(VEvent & that)
: m_DTEnd(0), 
  m_TempDuration(0),
  m_GEO(0), m_Location(0), m_Priority(0), m_ResourcesVctr(0),
  m_Transp(0),
  TimeBasedEvent(that)
{
    m_origDTEnd = that.m_origDTEnd;
    m_origMyDTEnd = that.m_origMyDTEnd;

    if (that.m_DTEnd != 0) 
    { 
        m_DTEnd = that.m_DTEnd->clone(m_Log); 
    }
    //if (that.m_Duration != 0) { m_Duration = that.m_Duration->clone(m_Log); }
    if (that.m_GEO != 0) 
    { 
        m_GEO = that.m_GEO->clone(m_Log); 
    }
    if (that.m_Location != 0) 
    { 
        m_Location = that.m_Location->clone(m_Log); 
    }
    if (that.m_Priority != 0) 
    { 
        m_Priority = that.m_Priority->clone(m_Log); 
    }
    if (that.m_Transp != 0) 
    { 
        m_Transp = that.m_Transp->clone(m_Log); 
    }
    if (that.m_ResourcesVctr != 0)
    {
        m_ResourcesVctr = new JulianPtrArray(); PR_ASSERT(m_ResourcesVctr != 0);
        if (m_ResourcesVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_ResourcesVctr, m_ResourcesVctr, m_Log);
        }
    }
}
//---------------------------------------------------------------------
ICalComponent * 
VEvent::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new VEvent(*this);
}
//---------------------------------------------------------------------

VEvent::~VEvent()
{
    if (m_TempDuration != 0)
    {
        delete m_TempDuration; m_TempDuration = 0;
    }
    // properties
    if (m_DTEnd != 0) 
    { 
        delete m_DTEnd; m_DTEnd = 0; 
    }
    //if (m_Duration != 0) { delete m_Duration; m_Duration = 0; }
    if (m_GEO != 0) 
    { 
        delete m_GEO; m_GEO = 0; 
    }
    if (m_Location!= 0) 
    { 
        delete m_Location; m_Location = 0; 
    }
    if (m_Priority!= 0) 
    { 
        delete m_Priority; m_Priority = 0; 
    }
    if (m_Transp != 0) 
    { 
        delete m_Transp; m_Transp = 0; 
    }
    
    // vector of strings
    if (m_ResourcesVctr != 0) { 
        ICalProperty::deleteICalPropertyVector(m_ResourcesVctr);
        delete m_ResourcesVctr; m_ResourcesVctr = 0; 
    }
    //delete (TimeBasedEvent *) this;
}

//---------------------------------------------------------------------

UnicodeString &
VEvent::parse(ICalReader * brFile, UnicodeString & sMethod, 
              UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
              t_bool bIgnoreBeginError, nsCalUtility::MimeEncoding encoding) 
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVEVENT;
    return parseType(u, brFile, sMethod, parseStatus, vTimeZones, bIgnoreBeginError, encoding);
}

//---------------------------------------------------------------------
#if 0
t_int8 VEvent::compareTo(VEvent * ve)
{
    PR_ASSERT(ve != 0);
    // TODO;
    return -1;
}
#endif /* #if 0 */
//---------------------------------------------------------------------

Date VEvent::difference()
{
    if (getDTEnd().getTime() >= 0 && getDTStart().getTime() >= 0)
    {
        return (getDTEnd().getTime() - getDTStart().getTime());
    }
    else return 0;
}

//---------------------------------------------------------------------

void VEvent::selfCheck()
{

    TimeBasedEvent::selfCheck();

    // if no dtend, and there is a dtstart and duration, calculate dtend
    if (!getDTEnd().isValid())
    {
        // calculate DTEnd from duration and dtstart
        // TODO: remove memory leaks in m_TempDuration
        if (getDTStart().isValid() && m_TempDuration != 0 && m_TempDuration->isValid())
        {
            DateTime end;
            end = getDTStart();
            end.add(*m_TempDuration);
            setDTEnd(end);
            delete m_TempDuration; m_TempDuration = 0; // lifetime of variable is only
            // after load.
        }
        else if (!getDTStart().isValid())
        {
            if (m_TempDuration != 0)
                delete m_TempDuration; m_TempDuration = 0;
        }
    }

    // if is anniversary event, set transp to TRANSPARENT
    if (isAllDayEvent())
    {
        setTransp(JulianKeyword::Instance()->ms_sTRANSP);
        
        // if no dtend, set dtend to end of day of dtstart.
        if (!getDTEnd().isValid())
        {
            DateTime d;
            d = getDTStart();
            d.add(Calendar::DATE, 1);
            d.set(Calendar::HOUR_OF_DAY, 0);
            d.set(Calendar::MINUTE, 0);
            d.set(Calendar::SECOND, 0);
            setDTEnd(d);
        }

        // If DTEND is before DTSTART, set dtend = dtstart
        if (getDTEnd().getTime() < getDTStart().getTime())
        {
            DateTime d;
            d = getDTStart();
            setDTEnd(d);

            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iDTEndBeforeDTStart, 100);
        }
    }
    else 
    {
        if (!getDTEnd().isValid())
        {
            // dtend is same as dtstart
            DateTime d;
            d = getDTStart();
            if (d.isValid())
                setDTEnd(d);
        }

        // If DTEND is before DTSTART, set dtend = dtstart
        if (getDTEnd().getTime() < getDTStart().getTime())
        {
            DateTime d;
            d = getDTStart();
            setDTEnd(d);

            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iDTEndBeforeDTStart, 100);
        }
    }
    if (getStatus().size() > 0)
    {
        // check if I got a valid status
        UnicodeString u = getStatus();
        u.toUpper();
        ICalProperty::Trim(u);
        JAtom ua(u);
        if ((JulianKeyword::Instance()->ms_ATOM_CONFIRMED != ua) && 
            (JulianKeyword::Instance()->ms_ATOM_TENTATIVE != ua) && 
            (JulianKeyword::Instance()->ms_ATOM_CANCELLED != ua))
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidPropertyValue, 
                JulianKeyword::Instance()->ms_sVEVENT, 
                JulianKeyword::Instance()->ms_sSTATUS, u, 200);

            setStatus("");
        }
    }
}

//---------------------------------------------------------------------
void VEvent::storeDTEnd(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // check parameters
    t_bool bParamValid = ICalProperty::CheckParamsWithValueRangeCheck(parameters, 
        JulianAtomRange::Instance()->ms_asTZIDValueParamRange,
        JulianAtomRange::Instance()->ms_asTZIDValueParamRangeSize,
        JulianAtomRange::Instance()->ms_asDateDateTimeValueRange,
        JulianAtomRange::Instance()->ms_asDateDateTimeValueRangeSize);
    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
    }

    if (getDTEndProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVEVENT, 
            JulianKeyword::Instance()->ms_sDTEND, 100);
    }  
    UnicodeString u, out;

    u = JulianKeyword::Instance()->ms_sVALUE;
    out = ICalParameter::GetParameterFromVector(u, out, parameters);

    t_bool bIsDate = DateTime::IsParseableDate(propVal);

    if (bIsDate)
    {
        // if there is a VALUE=X parameter, make sure X is DATE
        if (out.size() != 0 && (JulianKeyword::Instance()->ms_ATOM_DATE != out.hashCode()))
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iPropertyValueTypeMismatch,
                JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
        }   
    }
    else
    {
        // if there is a VALUE=X parameter, make sure X is DATETIME
        if (out.size() != 0 && (JulianKeyword::Instance()->ms_ATOM_DATETIME != out.hashCode()))
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iPropertyValueTypeMismatch,
                JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
        }
    }

    DateTime d;
    d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);
    
    setDTEnd(d, parameters);
}     
void VEvent::storeDuration(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
    }

    if (m_TempDuration != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVEVENT, 
            JulianKeyword::Instance()->ms_sDURATION, 100);
    }
    //Duration d(propVal);
    if (m_TempDuration == 0)
    {
        m_TempDuration = new nsCalDuration(propVal);
        PR_ASSERT(m_TempDuration != 0);
    }
    else
        m_TempDuration->setDurationString(propVal);
}     
void VEvent::storeGEO(UnicodeString & strLine, UnicodeString & propVal,
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
    }

    if (getGEOProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVEVENT, 
            JulianKeyword::Instance()->ms_sGEO, 100);
    }
    setGEO(propVal, parameters);
}       
void VEvent::storeLocation(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    t_bool bParamValid = ICalProperty::CheckParams(parameters, 
        JulianAtomRange::Instance()->ms_asAltrepLanguageParamRange,
        JulianAtomRange::Instance()->ms_asAltrepLanguageParamRangeSize);

    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
    }

    if (getLocationProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVEVENT, 
            JulianKeyword::Instance()->ms_sLOCATION, 100);
    }
    setLocation(propVal, parameters);
}  
void VEvent::storePriority(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    t_bool bParseError = FALSE;
    t_int32 i;

    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
    }
    
    char * pcc = propVal.toCString("");
    PR_ASSERT(pcc != 0);
    i = nsCalUtility::atot_int32(pcc, bParseError, propVal.size());
    delete [] pcc; pcc = 0;
    if (getPriorityProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVEVENT, 
            JulianKeyword::Instance()->ms_sPRIORITY, 100);
    }
    if (!bParseError)
    {
        setPriority(i, parameters);
    }
    else
    {
        if (m_Log) m_Log->logError(JulianLogErrorMessage::Instance()->ms_iInvalidNumberFormat, 
            JulianKeyword::Instance()->ms_sVEVENT, 
            JulianKeyword::Instance()->ms_sPRIORITY, propVal, 200);
    }
}  
void VEvent::storeResources(UnicodeString & strLine, UnicodeString & propVal, 
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
            JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
    }

    addResourcesPropertyVector(propVal, parameters);
}
void VEvent::storeTransp(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
    }

    if (getTranspProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVEVENT, 
            JulianKeyword::Instance()->ms_sTRANSP, 100);
    }
    setTransp(propVal, parameters);
}       
//---------------------------------------------------------------------
t_bool 
VEvent::storeData(UnicodeString & strLine, UnicodeString & propName, 
                  UnicodeString & propVal, JulianPtrArray * parameters, 
                  JulianPtrArray * vTimeZones)
{
    if (TimeBasedEvent::storeData(strLine, propName, propVal, 
                                  parameters, vTimeZones))
        return TRUE;
    else
    {
        t_int32 hashCode = propName.hashCode();
        t_int32 i;
        for (i = 0; 0 != (JulianFunctionTable::Instance()->veStoreTable[i]).op; i++)
        {
            if ((JulianFunctionTable::Instance()->veStoreTable[i]).hashCode == hashCode)
            {
                ApplyStoreOp(((JulianFunctionTable::Instance())->veStoreTable[i]).op, 
                    strLine, propVal, parameters, vTimeZones);
                return TRUE;
            }
        }
        if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidPropertyName, 
                JulianKeyword::Instance()->ms_sVEVENT, propName, 200);
        UnicodeString u;
        u = JulianLogErrorMessage::Instance()->ms_sRS202;
        u += '.'; u += ' ';
        u += strLine;
        addRequestStatus(u);
        return FALSE;
    }
}

//---------------------------------------------------------------------
/// note: todo: crash proof, bulletproofed
void VEvent::populateDatesHelper(DateTime start, Date ldiff, 
                                 JulianPtrArray * vPeriods)
{
    Date diff = ldiff;
    Date diff2 = -1;

    if (vPeriods != 0)
    {
        // Check if period matches this date, if-so, set
        // difference to be length of period
        // TODO: if multiple periods match this date, choose period
        // with longest length

        t_int32 i;
        UnicodeString u;
        Period p;
        DateTime ps, pe;
        Date pl;
        for (i = 0; i < vPeriods->GetSize(); i++)
        {   
            u = *((UnicodeString *) vPeriods->GetAt(i));
            
            //if (FALSE) TRACE("populateDatesHelper(): u = %s\r\n", u.toCString(""));

            p.setPeriodString(u);
            PR_ASSERT(p.isValid());
            if (p.isValid())
            {
                ps = p.getStart();
                
                if (ps.equalsDateTime(start))
                {
                    pe = p.getEndingTime(pe);
                    pl = (pe.getTime() - ps.getTime());
                    //if (FALSE) TRACE("populateDatesHelper(): pl = %d\r\n", pl);

                    diff2 = ((diff2 > pl) ? diff2 : pl);
                }
            }
        }
    }
    if (diff2 != -1)
        diff = diff2;

    DateTime dEnd;
    dEnd.setTime(start.getTime() + diff);
    setDTEnd(dEnd);
    setMyOrigEnd(dEnd);

    DateTime origEnd;
    origEnd.setTime(getOrigStart().getTime() + ldiff);
    setOrigEnd(origEnd);
}

//---------------------------------------------------------------------

UnicodeString VEvent::formatHelper(UnicodeString & strFmt, 
                                   UnicodeString sFilterAttendee, 
                                   t_bool delegateRequest) 
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVEVENT;
    return ICalComponent::format(u, strFmt, sFilterAttendee, delegateRequest);
}

//---------------------------------------------------------------------

UnicodeString VEvent::toString()
{
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_VEventStrDefaultFmt);
}

//---------------------------------------------------------------------

UnicodeString VEvent::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    UnicodeString sResult;
    switch ( c )
    {
    case ms_cDuration:
        return getDuration().toString();
        //return ICalProperty::propertyToString(getDurationProperty(), dateFmt, sResult);
    case ms_cDTEnd:
        return ICalProperty::propertyToString(getDTEndProperty(), dateFmt, sResult);
    case ms_cGEO:
        return ICalProperty::propertyToString(getGEOProperty(), dateFmt, sResult);
    case ms_cLocation:
        return ICalProperty::propertyToString(getLocationProperty(), dateFmt, sResult);
    case ms_cTransp:
        return ICalProperty::propertyToString(getTranspProperty(), dateFmt, sResult);
    case ms_cPriority:
        return ICalProperty::propertyToString(getPriorityProperty(), dateFmt, sResult);
    case ms_cResources:
        return ICalProperty::propertyVectorToString(getResources(), dateFmt, sResult);
    default:
        return TimeBasedEvent::toStringChar(c, dateFmt);
    }
}

//---------------------------------------------------------------------

UnicodeString VEvent::formatChar(t_int32 c, UnicodeString sFilterAttendee, 
                                 t_bool delegateRequest) 
{
    UnicodeString s, sResult;
    switch (c) {
    case ms_cDTEnd:
        s = JulianKeyword::Instance()->ms_sDTEND;
        return ICalProperty::propertyToICALString(s, getDTEndProperty(), sResult);
    case ms_cDuration:
        s = JulianKeyword::Instance()->ms_sDURATION;
        s += ':';
        s += getDuration().toICALString();
        s += JulianKeyword::Instance()->ms_sLINEBREAK;
        return s;
        //return ICalProperty::propertyToICALString(s, getDurationProperty(), sResult);
    case ms_cLocation: 
        s = JulianKeyword::Instance()->ms_sLOCATION;
        return ICalProperty::propertyToICALString(s, getLocationProperty(), sResult);
    case ms_cPriority: 
        s = JulianKeyword::Instance()->ms_sPRIORITY;
        return ICalProperty::propertyToICALString(s, getPriorityProperty(), sResult);    
    case ms_cResources:
        s = JulianKeyword::Instance()->ms_sRESOURCES;
        return ICalProperty::propertyVectorToICALString(s, getResources(), sResult);    
    case ms_cGEO: 
        s = JulianKeyword::Instance()->ms_sGEO;
        return ICalProperty::propertyToICALString(s, getGEOProperty(), sResult);     
    case ms_cTransp:
        s = JulianKeyword::Instance()->ms_sTRANSP;
        return ICalProperty::propertyToICALString(s, getTranspProperty(), sResult);
    default:
        {
            return TimeBasedEvent::formatChar(c, sFilterAttendee, delegateRequest);
        }
    }
}

//---------------------------------------------------------------------

t_bool VEvent::isValid()
{
    if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sPUBLISH) == 0) ||
        (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0) ||
        (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sADD) == 0))
    {
        // must have dtstart
        if ((!getDTStart().isValid()))
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingStartingTime, 300);
            return FALSE;
        }
        // If dtend exists, make sure it is not before dtstart
        if (getDTEnd().isValid() && getDTEnd() < getDTStart())
        { 
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iEndBeforeStartTime, 300);
            return FALSE;
        }
        // must have dtstamp, summary, uid
        if (!getDTStamp().isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingDTStamp, 300);
            return FALSE;
        }
        if (getSummary().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingSummary, 300);
            return FALSE;
        }
        if (getUID().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingUID, 300);
            return FALSE;
        }

        // TODO: must have organizer, comment out for now since CS&T doesn't have ORGANIZER
        /*
        if (getOrganizer().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingOrganizer, 300);
            return FALSE;
        }
        */
        // must have sequence >= 0
        if (getSequence() < 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingSeqNo, 300);
            return FALSE;
        }
        if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0)
        {
            if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
            {
                if (m_Log) m_Log->logError(
                    JulianLogErrorMessage::Instance()->ms_iMissingAttendees, 300);
                return FALSE;
            }
        }
    }
    else if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREPLY) == 0) ||
             (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sCANCEL) == 0) ||
             (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sDECLINECOUNTER) == 0))
    {
         // must have dtstamp, uid
        if (!getDTStamp().isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingDTStamp, 300);
            return FALSE;
        }
        if (getUID().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingUID, 300);
            return FALSE;
        }

        // must have organizer
        if (getOrganizer().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingOrganizer, 300);
            return FALSE;
        }
        // must have sequence >= 0
        if (getSequence() < 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingSeqNo, 300);
            return FALSE;
        }
        if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREPLY) == 0)
        {
            // TODO: attendees are required, commenting out for now
            if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
            {
                if (m_Log) m_Log->logError(
                    JulianLogErrorMessage::Instance()->ms_iMissingAttendees, 300);
                return FALSE;
            }
        }
    }
    else if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREFRESH) == 0))
    {
        // must have dtstamp, uid
        if (!getDTStamp().isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingDTStamp, 300);
            return FALSE;
        }
        if (getUID().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingUID, 300);
            return FALSE;
        }
        // TODO: attendees required?
    }
    else if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sCOUNTER) == 0))
    {
        // must have dtstamp, uid
        if (!getDTStamp().isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingDTStamp, 300);
            return FALSE;
        }

        if (getUID().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingUID, 300);
            return FALSE;
        }

        // must have sequence >= 0
        if (getSequence() < 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingSeqNo, 300);   
            return FALSE;
        }
    }
    
    // check super class isValid method
    return TimeBasedEvent::isValid();
}

//---------------------------------------------------------------------
#if 0
t_bool VEvent::isCriticalInfoSame()
{
    // TODO finish
    return FALSE;
}
#endif /* #if 0 */
//---------------------------------------------------------------------

UnicodeString VEvent::cancelMessage() 
{
    UnicodeString s = JulianKeyword::Instance()->ms_sCANCELLED;
    setStatus(s);
    return formatHelper(JulianFormatString::Instance()->ms_sVEventCancelMessage, "");
}

//---------------------------------------------------------------------

UnicodeString VEvent::requestMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventRequestMessage, "");
}

//---------------------------------------------------------------------

UnicodeString VEvent::requestRecurMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventRecurRequestMessage, "");
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::counterMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventCounterMessage, "");
}

//---------------------------------------------------------------------
  
UnicodeString VEvent::declineCounterMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventDeclineCounterMessage, "");
}

//---------------------------------------------------------------------

UnicodeString VEvent::addMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventAddMessage, "");
}

//---------------------------------------------------------------------
  
UnicodeString VEvent::refreshMessage(UnicodeString sAttendeeFilter) 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventRefreshMessage, sAttendeeFilter);
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::allMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventAllPropertiesMessage, "");
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::replyMessage(UnicodeString sAttendeeFilter) 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventReplyMessage, sAttendeeFilter);
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::publishMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventPublishMessage, "");
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::publishRecurMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventRecurPublishMessage, "");
}

//---------------------------------------------------------------------
void 
VEvent::updateComponentHelper(TimeBasedEvent * updatedComponent)
{
    //TODO: handle duration, origDTEnd, origMyDTEnd.
    TimeBasedEvent::updateComponentHelper(updatedComponent);
    if (updatedComponent->GetType() == GetType())
    {
        ICalComponent::internalSetPropertyVctr(&m_ResourcesVctr, ((VEvent *) updatedComponent)->getResources());
        ICalComponent::internalSetProperty(&m_DTEnd, ((VEvent *) updatedComponent)->m_DTEnd);
        ICalComponent::internalSetProperty(&m_GEO, ((VEvent *) updatedComponent)->m_GEO);
        ICalComponent::internalSetProperty(&m_Location, ((VEvent *) updatedComponent)->m_Location);
        ICalComponent::internalSetProperty(&m_Priority, ((VEvent *) updatedComponent)->m_Priority);
        ICalComponent::internalSetProperty(&m_Transp, ((VEvent *) updatedComponent)->m_Transp);
    }
}
//---------------------------------------------------------------------
///DTEnd
void VEvent::setDTEnd(DateTime s, JulianPtrArray * parameters)
{ 
#if 1
    if (m_DTEnd == 0)
        m_DTEnd = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_DTEnd->setValue((void *) &s);
        m_DTEnd->setParameters(parameters);
    }
    //validateDTEnd();
#else
    ICalComponent::setDateTimeValue(((ICalProperty **) &m_DTEnd), s, parameters);
#endif
}
DateTime VEvent::getDTEnd() const 
{
#if 1
    DateTime d(-1);
    if (m_DTEnd == 0)
        return d; //return 0;
    else
    {
        d = *((DateTime *) m_DTEnd->getValue());
        return d;
    }
#else
    DateTime d(-1);
    ICalComponent::getDateTimeValue(((ICalProperty **) &m_DTEnd), d);
    return d;
#endif
}
//---------------------------------------------------------------------
///Duration
void VEvent::setDuration(nsCalDuration s, JulianPtrArray * parameters)
{ 
    /*
    if (m_Duration == 0)
        m_Duration = ICalPropertyFactory::Make(ICalProperty::DURATION, 
                                            (void *) &s, parameters);
    else
    {
        m_Duration->setValue((void *) &s);
        m_Duration->setParameters(parameters);
    }
    //validateDuration();
    */
    // NOTE: remove later, to avoid compiler warnings
    if (parameters) 
    {
    }

    
    DateTime end;
    end = getDTStart();
    end.add(s);
    setDTEnd(end);
}
nsCalDuration VEvent::getDuration() const 
{
    /*
    Duration d; d.setMonth(-1);
    if (m_Duration == 0)
        return d; // return 0;
    else
    {
        d = *((Duration *) m_Duration->getValue());
        return d;
    }
    */
    nsCalDuration duration;
    DateTime start, end;
    start = getDTStart();
    end = getDTEnd();
    DateTime::getDuration(start,end,duration);
    return duration;
}
//---------------------------------------------------------------------
//GEO
void VEvent::setGEO(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_GEO == 0)
        m_GEO = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_GEO->setValue((void *) &s);
        m_GEO->setParameters(parameters);
    }
#else 
    ICalComponent::setStringValue(((ICalProperty **) &m_GEO), s, parameters);
#endif
}
UnicodeString VEvent::getGEO() const 
{
#if 1
    if (m_GEO == 0)
        return "";
    else
        return *((UnicodeString *) m_GEO->getValue());
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_GEO), us);
    return us;
#endif

}

//---------------------------------------------------------------------
//Location
void VEvent::setLocation(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Location == 0)
    {
        m_Location = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                           (void *) &s, parameters);
    }
    else
    {
        m_Location->setValue((void *) &s);
        m_Location->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_Location), s, parameters);
#endif
}
UnicodeString VEvent::getLocation() const  
{
#if 1
    if (m_Location == 0)
        return "";
    else
        return *((UnicodeString *) m_Location->getValue());
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_Location), us);
    return us;
#endif
}
//---------------------------------------------------------------------
//Priority
void VEvent::setPriority(t_int32 i, JulianPtrArray * parameters)
{
#if 1
    if (m_Priority == 0)
        m_Priority = ICalPropertyFactory::Make(ICalProperty::INTEGER, 
                                            (void *) &i, parameters);
    else
    {
        m_Priority->setValue((void *) &i);
        m_Priority->setParameters(parameters);
    }
#else
    ICalComponent::setIntegerValue(((ICalProperty **) &m_Priority), i, parameters);
#endif
}
t_int32 VEvent::getPriority() const 
{
#if 1
    if (m_Priority == 0)
        return -1;
    else
        return *((t_int32 *) m_Priority->getValue());
#else
    t_int32 i = -1;
    ICalComponent::getIntegerValue(((ICalProperty **) &m_Priority), i);
    return i;
#endif
}
//---------------------------------------------------------------------
//Transp
void VEvent::setTransp(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Transp == 0)
        m_Transp = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Transp->setValue((void *) &s);
        m_Transp->setParameters(parameters);
    }
#else
     ICalComponent::setStringValue(((ICalProperty **) &m_Transp), s, parameters);
#endif
}
UnicodeString VEvent::getTransp() const  
{
#if 1
    if (m_Transp == 0)
        return "";
    else
        return *((UnicodeString *) m_Transp->getValue());
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_Transp), us);
    return us;
#endif
}
//---------------------------------------------------------------------
// RESOURCES
void VEvent::addResources(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addResourcesProperty(prop);
}

//---------------------------------------------------------------------

void VEvent::addResourcesProperty(ICalProperty * prop)
{
    if (m_ResourcesVctr == 0)
        m_ResourcesVctr = new JulianPtrArray(); 
    PR_ASSERT(m_ResourcesVctr != 0);
    if (m_ResourcesVctr != 0)
    {
        m_ResourcesVctr->Add(prop);
    }
}

//---------------------------------------------------------------------

void
VEvent::addResourcesPropertyVector(UnicodeString & propVal,
                                   JulianPtrArray * parameters)
{
    //ICalProperty * ip;

    //ip = ICalPropertyFactory::Make(ICalProperty::TEXT, new UnicodeString(propVal),
    //        parameters);
    addResources(propVal, parameters);
    //addResourcesProperty(ip);
  

#if 0
    // TODO: see TimeBasedEvent::addCategoriesPropertyVector()

    ErrorCode status = ZERO_ERROR;
    UnicodeStringTokenizer * st;
    UnicodeString us; 
    UnicodeString sDelim = ",";

    st = new UnicodeStringTokenizer(propVal, sDelim);
    //ICalProperty * ip;
    while (st->hasMoreTokens())
    {
        us = st->nextToken(us, status);
        us.trim();
        //if (FALSE) TRACE("us = %s, status = %d", us.toCString(""), status);
        // TODO: if us is in Resources range then add, else, log and error
        addResourcesProperty(ICalPropertyFactory::Make(ICalProperty::TEXT,
            new UnicodeString(us), parameters));
    }
    delete st; st = 0;
#endif

}
//---------------------------------------------------------------------

