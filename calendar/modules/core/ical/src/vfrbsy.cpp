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

// vfrbsy.cpp
// John Sun
// 4:46 PM Febuary 19 1998

#include "stdafx.h"
#include "jdefines.h"

#include "vfrbsy.h"
#include "prprty.h"
#include "prprtyfy.h"
#include "freebusy.h"
#include "jutility.h"
#include "jlog.h"
#include "vtimezne.h"
#include "keyword.h"
#include "orgnzr.h"

#include "functbl.h"
//---------------------------------------------------------------------
// private never use
#if 0
VFreebusy::VFreebusy()
{
    PR_ASSERT(FALSE);
}
#endif
  
//---------------------------------------------------------------------

void VFreebusy::selfCheck()
{
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

    if (getFreebusy() != 0 && getDTStart().isValid() && getDTEnd().isValid())
        normalize();
    // if sequence is null, set to 0.
    if (getSequence() == -1)
        setSequence(0);
}

//---------------------------------------------------------------------

void VFreebusy::combineSameFreebusies(JulianPtrArray * freebusies)
{
    t_int32 i = 0;
    t_int32 j = 0;
    Freebusy * f1 = 0;
    Freebusy * f2 = 0;
    
    if (freebusies != 0 && freebusies->GetSize() >= 2)
    {
        // merge similar freebusy
        for (i = 0; i < freebusies->GetSize() - 1; i++)
        {
            f1 = (Freebusy *) freebusies->GetAt(i);
            for (j = i + 1; j < freebusies->GetSize() ; j++)
            {
                f2 = (Freebusy *) freebusies->GetAt(j);
                if (Freebusy::HasSameParams(f1, f2))
                {
                    // add all periods in f1 into f2
                    // remove f1 from freebusies. delete f1
                    f1->addAllPeriods(f2->getPeriod());
                    delete f2; f2 = 0;
                    freebusies->RemoveAt(j);
                    j--;
                }
            }
        }
    }
}

//---------------------------------------------------------------------
#if 0
void VFreebusy::DEBUG_printFreebusyVector(const char * message, JulianPtrArray * freebusies)
{
#if TESTING_ITIPRIG
    if (TRUE) TRACE(message);
#endif

    UnicodeString sResult;
    if (freebusies != 0)
    {
        t_int32 i;
        UnicodeString s;
        for (i = 0; i < freebusies->GetSize(); i++)
        {   
            s = ((Freebusy *) freebusies->GetAt(i))->toICALString(s);
            sResult += ICalProperty::multiLineFormat(s);
        }
    }

#if TESTING_ITIPRIG
    if (TRUE) TRACE("%s\r\n", sResult.toCString(""));    
#endif
}
#endif
//---------------------------------------------------------------------

void VFreebusy::mergeSinglePeriodFreebusies()
{
    sortFreebusy();   

    // 1) combine same params freebusy
    combineSameFreebusies(getFreebusy());
    //DEBUG_printFreebusyVector("after combining\r\n", getFreebusy());

    // 2) normalize each freebusy (compress overlapping periods)
    compressFreebusies(getFreebusy());
    //DEBUG_printFreebusyVector("after compressing\r\n", getFreebusy());

    // 3) Unzip freebusies
    unzipFreebusies();
    //DEBUG_printFreebusyVector("after unzipping\r\n", getFreebusy());
}

//---------------------------------------------------------------------

void VFreebusy::compressFreebusies(JulianPtrArray * freebusies)
{
    if (freebusies != 0)
    {
        t_int32 i;
        Freebusy * f1 = 0;
        for (i = 0; i < freebusies->GetSize(); i++)
        {
            f1 = (Freebusy *) freebusies->GetAt(i);
            f1->normalizePeriods();
        }
    }
}
//---------------------------------------------------------------------

void VFreebusy::unzipFreebusies()
{
    JulianPtrArray * freebusies = getFreebusy();
    if (freebusies != 0)
    {
        t_int32 oldSize = freebusies->GetSize();
        t_int32 i = 0;
        t_int32 j = 0;
        Freebusy * f1 = 0;
        Freebusy * f2 = 0;
        
        for (i = oldSize - 1; i >= 0; i--)
        {
            f1 = (Freebusy *) freebusies->GetAt(i);
            if (f1->getPeriod() != 0)
            {   
                // foreach period in f1, create a new freebusy for it,
                // add to freebusy vector
                for (j = 0; j < f1->getPeriod()->GetSize(); j++)
                {   
                    f2 = new Freebusy(m_Log);
                    PR_ASSERT(f2 != 0);
                    if (f2 != 0)
                    {
                        f2->setType(f1->getType());
#if 0
                        f2->setStatus(f1->getStatus());
#endif
                        f2->addPeriod((Period *) f1->getPeriod()->GetAt(j));
                        addFreebusy(f2);
                    }
                }
            }
            // now delete f1
            delete f1; f1 = 0;
            freebusies->RemoveAt(i);
        }
    }
}

//---------------------------------------------------------------------
void VFreebusy::addExtraFreePeriodFreebusies(JulianPtrArray * freebusies)
{
    if (freebusies != 0)
    {
        Freebusy * fb;
        Period *p;
        t_int32 i = 0;
        t_int32 j = 0;
        t_int32 oldSize = freebusies->GetSize();
        if (getDTStart().isValid() && getDTEnd().isValid())
        {
            sortFreebusy();
            DateTime startTime, nextTime, endTime;
            startTime = getDTStart();
            nextTime = startTime;

            for (i = 0; i < oldSize; i++)
            {
                fb = (Freebusy *) freebusies->GetAt(i);

                PR_ASSERT(fb != 0 && fb->getPeriod()->GetSize() == 1);
                
                p = (Period *) fb->getPeriod()->GetAt(0);

                nextTime = p->getEndingTime(nextTime);
                endTime = p->getStart();
    
                if (startTime < endTime)
                {
                    // add a new freebusy period
                    fb = new Freebusy(m_Log); PR_ASSERT(fb != 0);
                    fb->setType(Freebusy::FB_TYPE_FREE);
#if 0
                    fb->setStatus(Freebusy::FB_STATUS_TENTATIVE); // for dbg
#endif
                    p = new Period();
                    p->setStart(startTime);
                    p->setEnd(endTime);
                    fb->addPeriod(p); // ownership of p not taken, must delete
                    delete p; p = 0;
                    freebusies->Add(fb); // fb ownership taken
                }
                startTime = nextTime;
            }
            endTime = getDTEnd();
            if (nextTime < endTime)
            {
                // add final period 
                // add a new freebusy period
                fb = new Freebusy(m_Log); PR_ASSERT(fb != 0);
                fb->setType(Freebusy::FB_TYPE_FREE);
#if 0
                fb->setStatus(Freebusy::FB_STATUS_TENTATIVE); // for dbg
#endif
                p = new Period();
                p->setStart(nextTime);
                p->setEnd(endTime);
                fb->addPeriod(p); // ownership of p not taken, must delete
                delete p; p = 0;
                freebusies->Add(fb); // fb ownership taken
            }
            sortFreebusy();
        }
    }
}

//---------------------------------------------------------------------

void VFreebusy::normalize()
{
    // 1) Can mergeSinglePeriodFreebusies first time
    mergeSinglePeriodFreebusies();

    // 2) I will now add FREE freebusy periods to those periods not already covered.
    addExtraFreePeriodFreebusies(getFreebusy());    
    //DEBUG_printFreebusyVector("----- after make free 4)\r\n", v);

    // 3) Combine consective free periods.  This is OPTIONAL.
    mergeSinglePeriodFreebusies();
    //DEBUG_printFreebusyVector("----- after merge-single-period freebusies 5)\r\n", v);

    sortFreebusy();
}
//---------------------------------------------------------------------

void VFreebusy::sortFreebusy()
{
    if (m_FreebusyVctr != 0)
        m_FreebusyVctr->QuickSort(Freebusy::CompareFreebusy);
}

//---------------------------------------------------------------------

void VFreebusy::stamp()
{
    DateTime d;
    setDTStamp(d);
}

//---------------------------------------------------------------------

VFreebusy::VFreebusy(JLog * initLog)
: m_AttendeesVctr(0), m_CommentVctr(0), 
    /*m_Created(0), */
    /*m_Duration(0),*/
    m_TempDuration(0),
  m_DTEnd(0), m_DTStart(0), m_DTStamp(0), m_FreebusyVctr(0), 
  m_LastModified(0), 
  m_Organizer(0),
  m_RequestStatusVctr(0), 
  //m_RelatedToVctr(0), 
  m_Sequence(0), m_UID(0),
  m_URL(0),
  //m_URLVctr(0), 
  m_XTokensVctr(0),
  m_ContactVctr(0),
  m_Log(initLog)
{
    //PR_ASSERT(initLog != 0);
}

//---------------------------------------------------------------------

VFreebusy::VFreebusy(VFreebusy & that)
: m_AttendeesVctr(0), m_CommentVctr(0), 
    /*m_Created(0),*/
    //m_Duration(0), 
    m_TempDuration(0), 
  m_DTEnd(0), m_DTStart(0), m_DTStamp(0), m_FreebusyVctr(0), 
  m_LastModified(0), 
  m_Organizer(0),
  m_RequestStatusVctr(0), 
  //m_RelatedToVctr(0), 
  m_Sequence(0), m_UID(0),
  m_URL(0),
  //m_URLVctr(0), 
  m_ContactVctr(0),
  m_XTokensVctr(0)
{
    if (that.m_AttendeesVctr != 0)
    {
        m_AttendeesVctr = new JulianPtrArray(); PR_ASSERT(m_AttendeesVctr != 0);
        if (m_AttendeesVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_AttendeesVctr, m_AttendeesVctr, m_Log);
        }
    }
    if (that.m_CommentVctr != 0)
    {
        m_CommentVctr = new JulianPtrArray(); PR_ASSERT(m_CommentVctr != 0);
        if (m_CommentVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_CommentVctr, m_CommentVctr, m_Log);
        }
    }
    if (that.m_ContactVctr != 0)
    {
        m_ContactVctr = new JulianPtrArray(); PR_ASSERT(m_ContactVctr != 0);
        if (m_ContactVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_ContactVctr, m_ContactVctr, m_Log);
        }
    }
    /*
    if (that.m_Created != 0) 
    { 
        m_Created = that.m_Created->clone(m_Log); 
    }
    */
    /*
    if (that.m_Duration != 0) 
    { 
        m_Duration = that.m_Duration->clone(m_Log); 
    }
    */
    if (that.m_DTEnd != 0) 
    { 
        m_DTEnd = that.m_DTEnd->clone(m_Log); 
    }
    if (that.m_DTStart != 0) 
    {
        m_DTStart = that.m_DTStart->clone(m_Log); 
    }
    if (that.m_DTStamp != 0) 
    { 
        m_DTStamp = that.m_DTStamp->clone(m_Log); 
    }
    if (that.m_FreebusyVctr != 0)
    {
        m_FreebusyVctr = new JulianPtrArray(); PR_ASSERT(m_FreebusyVctr != 0);
        if (m_FreebusyVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_FreebusyVctr, m_FreebusyVctr, m_Log);
        }
    }
    if (that.m_LastModified != 0) 
    { 
        m_LastModified = that.m_LastModified->clone(m_Log); 
    }
    if (that.m_Organizer != 0) 
    { 
        m_Organizer = that.m_Organizer->clone(m_Log); 
    }
    if (that.m_RequestStatusVctr != 0) 
    { 
        //m_RequestStatus = that.m_RequestStatus->clone(m_Log); 
        m_RequestStatusVctr = new JulianPtrArray(); PR_ASSERT(m_RequestStatusVctr != 0);
        if (m_RequestStatusVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_RequestStatusVctr, m_RequestStatusVctr, m_Log);
        }
    }
    if (that.m_Sequence != 0) 
    { 
        m_Sequence = that.m_Sequence->clone(m_Log); 
    }
    if (that.m_UID != 0) 
    { 
        m_UID = that.m_UID->clone(m_Log); 
    }
    if (that.m_URL != 0) 
    { 
        m_URL = that.m_URL->clone(m_Log); 
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
VFreebusy::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new VFreebusy(*this);
}

//---------------------------------------------------------------------
VFreebusy::~VFreebusy()
{
    // datetime properties
    /*
    if (m_Created != 0) 
    { 
        delete m_Created; m_Created = 0; 
    }
    */
    if (m_TempDuration != 0)
    {
        delete m_TempDuration; m_TempDuration = 0;
    }
    if (m_DTStart != 0) 
    { 
        delete m_DTStart; m_DTStart = 0; 
    }
    if (m_DTStamp != 0) 
    { 
        delete m_DTStamp; m_DTStamp = 0; 
    }
    if (m_LastModified != 0) 
    { 
        delete m_LastModified; m_LastModified = 0; 
    }
    if (m_DTEnd != 0) 
    { 
        delete m_DTEnd; m_DTEnd = 0; 
    }
    /*
    if (m_Duration != 0) 
    { 
        delete m_Duration; m_Duration = 0; 
    }
    */
    // string properties
    if (m_RequestStatusVctr != 0) 
    { 
        //delete m_RequestStatus; m_RequestStatus = 0; 
        ICalProperty::deleteICalPropertyVector(m_RequestStatusVctr);
        delete m_RequestStatusVctr; m_RequestStatusVctr = 0; 
    }
    if (m_Organizer != 0) 
    { 
        delete m_Organizer; m_Organizer = 0; 
    }
    if (m_UID != 0) 
    { 
        delete m_UID; m_UID = 0; 
    }
    if (m_URL != 0) 
    { 
        delete m_URL; m_URL = 0; 
    }

    // integer properties
    if (m_Sequence != 0) 
    { 
        delete m_Sequence; m_Sequence = 0; 
    }

    // attendees
    if (m_AttendeesVctr != 0) { 
        ICalProperty::deleteICalPropertyVector(m_AttendeesVctr);
        delete m_AttendeesVctr; m_AttendeesVctr = 0; 
    }
    // freebusy
    if (m_FreebusyVctr != 0) { 
        ICalProperty::deleteICalPropertyVector(m_FreebusyVctr);
        delete m_FreebusyVctr; m_FreebusyVctr = 0; 
    }

    // string vectors
    if (m_CommentVctr != 0) 
    { 
        ICalProperty::deleteICalPropertyVector(m_CommentVctr);
        delete m_CommentVctr; m_CommentVctr = 0; 
    }
    //if (m_RelatedToVctr != 0) { ICalProperty::deleteICalPropertyVector(m_RelatedToVctr);
    //    delete m_RelatedToVctr; m_RelatedToVctr = 0; }
    //if (m_URLVctr != 0) { ICalProperty::deleteICalPropertyVector(m_URLVctr);
    //    delete m_URLVctr; m_URLVctr = 0; }

    if (m_ContactVctr != 0) 
    { 
        ICalProperty::deleteICalPropertyVector(m_ContactVctr);
        delete m_ContactVctr; m_ContactVctr = 0; 
    }
    if (m_XTokensVctr != 0) { ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
}
//---------------------------------------------------------------------
UnicodeString & 
VFreebusy::parse(ICalReader * brFile, UnicodeString & method, 
                 UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
                 t_bool bIgnoreBeginError, nsCalUtility::MimeEncoding encoding)
{
    parseStatus = JulianKeyword::Instance()->ms_sOK;
    UnicodeString strLine, propName, propVal;

    JulianPtrArray * parameters = new JulianPtrArray();
    PR_ASSERT(parameters != 0 && brFile != 0);
    if (parameters == 0 || brFile == 0)
    {
        // ran out of memory, return invalid VFreebusy
        return parseStatus;
    }
    
    ErrorCode status = ZERO_ERROR;

    setMethod(method);

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
        else if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
            (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0))
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            break;
        }
        else if (
            ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) &&
             ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) && !bIgnoreBeginError )||
             ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) ||
              (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
              (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
              (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) ||
              (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0) ||
              (ICalProperty::IsXToken(propVal)))
            ) ||
            ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                   ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0) ||
                   (ICalProperty::IsXToken(propVal)))
            )
            ) //else if
        {
            // break on 
            // END:VCALENDAR, VEVENT, VTODO, VJOURNAL, VTIMEZONE, x-token
            // BEGIN:VEVENT, VTODO, VJOURNAL, VFREEBUSY, VTIMEZONE, VCALENDAR, x-token
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

            parseStatus = strLine;
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iAbruptEndOfParsing, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 300);
            break;
        }
        else
        {
//#ifdef TIMING
            //clock_t start, end;
            //start = clock();
//#endif
            storeData(strLine, propName, propVal, parameters, vTimeZones);
//#ifdef TIMING
            //end = clock();
            //double d = end - start;
            //if (FALSE) TRACE("storeData on %s took %f ms.\r\n", propName.toCString(""), d);
//#endif
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

        }
    } 
    ICalProperty::deleteICalParameterVector(parameters);
    parameters->RemoveAll();
    delete parameters; parameters = 0;

    selfCheck(); // sorts freebusy vector as well
    return parseStatus;
}
//---------------------------------------------------------------------
t_bool
VFreebusy::storeData(UnicodeString & strLine, UnicodeString & propName,
                     UnicodeString & propVal, JulianPtrArray * parameters,
                     JulianPtrArray * vTimeZones)
{
    t_int32 hashCode = propName.hashCode();
    t_int32 i;
    for (i = 0; 0 != (JulianFunctionTable::Instance()->vfStoreTable[i]).op; i++)
    {
        if ((JulianFunctionTable::Instance()->vfStoreTable[i]).hashCode == hashCode)
        {
            ApplyStoreOp(((JulianFunctionTable::Instance())->vfStoreTable[i]).op, 
                strLine, propVal, parameters, vTimeZones);
            return TRUE;
        }
    }
    if (ICalProperty::IsXToken(propName))
    {
        addXTokens(strLine);
        return TRUE;
    }
    else
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidPropertyName, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 200);
        UnicodeString u;
        u = JulianLogErrorMessage::Instance()->ms_sRS202;
        u += '.'; u += ' ';
        u += strLine;
        //setRequestStatus(JulianLogErrorMessage::Instance()->ms_iRS202); 
        addRequestStatus(u);
        return FALSE;
    }
    return FALSE;
}
//---------------------------------------------------------------------
void VFreebusy::storeAttendees(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    Attendee * attendee = new Attendee(GetType(), m_Log);
    PR_ASSERT(attendee != 0);
    if (attendee != 0)
    {
        attendee->parse(propVal, parameters);
        if (!attendee->isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidAttendee, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propVal, 200);
            UnicodeString u;
            u = JulianLogErrorMessage::Instance()->ms_sRS202;
            u += '.'; u += ' ';
            u += strLine;
            //setRequestStatus(JulianLogErrorMessage::Instance()->ms_iRS202); 
            addRequestStatus(u);
            delete attendee; attendee = 0;
        }
        else
        {
            addAttendee(attendee);
        }
    }
}
void VFreebusy::storeComment(UnicodeString & strLine, UnicodeString & propVal, 
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
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }
    addComment(propVal, parameters);
}
void VFreebusy::storeContact(UnicodeString & strLine, UnicodeString & propVal, 
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
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }
    addContact(propVal, parameters);
}
/*
void VFreebusy::storeCreated(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }

    if (getCreatedProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sCREATED, 100);
    }
    DateTime d;
    d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

    setCreated(d , parameters);   
}
*/
void VFreebusy::storeDuration(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }

    if (m_TempDuration != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
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
void VFreebusy::storeDTEnd(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters (MUST BE IN UTC)
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }

    // DTEND must be in UTC
    if (getDTEndProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sDTEND, 100);
    }
    DateTime d;
    d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

    setDTEnd(d, parameters);
}
void VFreebusy::storeDTStart(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters (MUST BE IN UTC)
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }

    // DTSTART must be in UTC
    if (getDTStartProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sDTSTART, 100);
    }
    DateTime d;
    d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

    setDTStart(d, parameters);
}
void VFreebusy::storeDTStamp(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters (MUST BE IN UTC)
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }

    if (getDTStampProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sDTSTAMP, 100);
    }
    DateTime d;
    d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

    setDTStamp(d, parameters);   
}
void VFreebusy::storeFreebusy(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // Freebusy must be in UTC
    Freebusy * freeb = new Freebusy(m_Log);
    PR_ASSERT(freeb != 0);
    if (freeb != 0)
    {
        freeb->parse(propVal, parameters, vTimeZones);
        if (!freeb->isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidFreebusy, 200);
            UnicodeString u;
            u = JulianLogErrorMessage::Instance()->ms_sRS202;
            u += '.'; u += ' ';
            u += strLine;
            //setRequestStatus(JulianLogErrorMessage::Instance()->ms_iRS202); 
            addRequestStatus(u);
            delete freeb; freeb = 0;
        }
        else
        {
            addFreebusy(freeb);
        }
    }
}
void VFreebusy::storeLastModified(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters (MUST BE IN UTC)
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }

    if (getLastModified().getTime() >= 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sLASTMODIFIED, 100);
    }
    DateTime d;
    d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

    setLastModified(d, parameters);   
}
void VFreebusy::storeOrganizer(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // check parameters 
    t_bool bParamValid = ICalProperty::CheckParams(parameters, 
        JulianAtomRange::Instance()->ms_asSentByParamRange,
        JulianAtomRange::Instance()->ms_asSentByParamRangeSize);
    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }

    if (getOrganizerProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sORGANIZER, 100);
    }
    setOrganizer(propVal, parameters);
}
void VFreebusy::storeRequestStatus(UnicodeString & strLine, UnicodeString & propVal, 
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
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }
#if 0
    /* Don't print duplicated property on Request Status */
    if (getRequestStatusProperty() != 0)
    {
        if (m_Log) m_Log->logError(
        JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
        JulianKeyword::Instance()->ms_sVFREEBUSY, 
        JulianKeyword::Instance()->ms_sREQUESTSTATUS, 100);
    }
    
#endif
    addRequestStatus(propVal, parameters);
}
void VFreebusy::storeSequence(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
 t_bool bParseError = FALSE;
    t_int32 i;
    
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }

    char * pcc = propVal.toCString("");
    PR_ASSERT(pcc != 0);
    i = nsCalUtility::atot_int32(pcc, bParseError, propVal.size());
    delete [] pcc; pcc = 0;
    if (getSequenceProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sSEQUENCE, 100);
    }
    if (!bParseError)
    {
        setSequence(i, parameters);
    }
    else
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidNumberFormat, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sSEQUENCE, propVal, 200);
    }
}
void VFreebusy::storeUID(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }
    if (getUIDProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sUID, 100);
    }
    setUID(propVal, parameters);
}
void VFreebusy::storeURL(UnicodeString & strLine, UnicodeString & propVal, 
    JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
    }
     if (getURLProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, 
            JulianKeyword::Instance()->ms_sURL, 100);
    }
    setURL(propVal, parameters);
    //addURL(propVal, parameters);
}
//---------------------------------------------------------------------
t_bool VFreebusy::isValid()
{
    DateTime dS, dE;
    dS = getDTStart();
    dE = getDTEnd();

    // TODO: handle bad method names

    if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sPUBLISH) == 0)
    {
        // publish must have dtstamp
        if (!getDTStamp().isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingStartingTime, 300);
            return FALSE;
        }

        // MUST have organizer
        if (getOrganizer().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingOrganizer, 300);
            return FALSE;
        }
    }
    else if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0)
    {
        // publish must have dtstamp
        if (!getDTStamp().isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingDTStamp, 300);
            return FALSE;
        }
        // TODO: must have requested attendee address
        if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
        {
            if (m_Log) m_Log->logError(
                    JulianLogErrorMessage::Instance()->ms_iMissingAttendees, 300);
            return FALSE;
        }

        // must have organizer
        if (getOrganizer().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingOrganizer, 300);
            return FALSE;
        }

        // must have start, end
        if (!dS.isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingStartingTime, 300);
            return FALSE;
        }
        if (!dE.isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingEndingTime, 300);
            return FALSE;
        }
        if (dS.afterDateTime(dE))
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iEndBeforeStartTime, 300);
            return FALSE;
        }
    }
    else if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREPLY) == 0)
    {
        // both request and reply must have valid dtstamp and a UID
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

        // TODO: must have recipient replying address
        if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
        {
            if (m_Log) m_Log->logError(
                    JulianLogErrorMessage::Instance()->ms_iMissingAttendees, 300);
            return FALSE;
        }
        // must have originator' organizer address
        if (getOrganizer().size() == 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingOrganizer, 300);
            return FALSE;
        }
        // must have start, end
        if (!dS.isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingStartingTime, 300);
            return FALSE;
        }
        if (!dE.isValid())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iMissingEndingTime, 300);
            return FALSE;
        }
        if (dS.afterDateTime(dE))
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iEndBeforeStartTime, 300);
            return FALSE;
        }
    }

    /*
    // both request and reply must have valid dtstamp and a UID
        if ((!getDTStamp().isValid()) || (getUID().size() == 0))
            return FALSE;
    
    // sequence must be >= 0
    if (getSequence() < 0)
        return FALSE;

    // if a reply
    if (dS.isValid() || dE.isValid())
    {
        // ds and de must be valid and dS cannot be after dE
        if (!dS.isValid() || !dE.isValid() || dS.afterDateTime(dE))
            return FALSE;
    }
    */

    return TRUE;
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::toICALString()
{
    return formatHelper(JulianFormatString::Instance()->ms_sVFreebusyAllMessage, "");
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::toICALString(UnicodeString sMethod, 
                                      UnicodeString sName,
                                      t_bool isRecurring)
{
    // NOTE: Remove later, to avoid warnings
    if (isRecurring) {}

    if (sMethod.compareIgnoreCase(JulianKeyword::Instance()->ms_sPUBLISH) == 0)
        return formatHelper(JulianFormatString::Instance()->ms_sVFreebusyPublishMessage, "");
    else if (sMethod.compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0)
        return formatHelper(JulianFormatString::Instance()->ms_sVFreebusyRequestMessage, "");
    else if (sMethod.compareIgnoreCase(JulianKeyword::Instance()->ms_sREPLY) == 0)
        return formatHelper(JulianFormatString::Instance()->ms_sVFreebusyReplyMessage, sName);
    else
        return toICALString();
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::toString()
{
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_VFreebusyStrDefaultFmt);
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::formatHelper(UnicodeString & strFmt, UnicodeString sFilterAttendee)
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVFREEBUSY;
    return ICalComponent::format(u, strFmt, sFilterAttendee);
}
//---------------------------------------------------------------------
UnicodeString 
VFreebusy::formatChar(t_int32 c, UnicodeString sFilterAttendee,
                                t_bool delegateRequest)
{
    UnicodeString sResult, s;
    t_int32 i;
    JulianPtrArray * v;

    // NOTE: remove later, kept in to remove compiler warning
    if (delegateRequest)
    {
    }

    switch (c)
    {    
    case ms_cAttendees:    
        {    
            v = getAttendees();
            if (v != 0)
            {
                if (sFilterAttendee.size() > 0) 
                {
                    Attendee * a = getAttendee(sFilterAttendee);
                    if (a != 0)
                        s = a->toICALString(s);
                    else
                        s = "";
                    sResult += ICalProperty::multiLineFormat(s);
                }
                else 
                {
                    for (i=0; i< v->GetSize(); i++) 
                    {
                        s = ((Attendee * ) v->GetAt(i))->toICALString(s);
                        sResult += ICalProperty::multiLineFormat(s);
                    }
                }    
            }
            return sResult;    
        }
    case ms_cFreebusy:
        s = JulianKeyword::Instance()->ms_sFREEBUSY;
        return ICalProperty::propertyVectorToICALString(s, getFreebusy(), sResult);
    case ms_cComment: 
        s = JulianKeyword::Instance()->ms_sCOMMENT;
        return ICalProperty::propertyVectorToICALString(s, getComment(), sResult);     
    case ms_cContact: 
        s = JulianKeyword::Instance()->ms_sCONTACT;
        return ICalProperty::propertyVectorToICALString(s, getContact(), sResult);
        /*
    case ms_cCreated: 
        s = JulianKeyword::Instance()->ms_sCREATED;
        return ICalProperty::propertyToICALString(s, getCreatedProperty(), sResult);  
        */
    case ms_cDTEnd:
        s = JulianKeyword::Instance()->ms_sDTEND;
        return ICalProperty::propertyToICALString(s, getDTEndProperty(), sResult);
    case ms_cDuration:
        s = JulianKeyword::Instance()->ms_sDURATION;
        s += ':';
        s += getDuration().toICALString();
        s += JulianKeyword::Instance()->ms_sLINEBREAK;
        return s;
    case ms_cDTStart: 
        s = JulianKeyword::Instance()->ms_sDTSTART;
        return ICalProperty::propertyToICALString(s, getDTStartProperty(), sResult);  
    case ms_cDTStamp:
        s = JulianKeyword::Instance()->ms_sDTSTAMP;
        return ICalProperty::propertyToICALString(s, getDTStampProperty(), sResult);  
    //case ms_cRelatedTo:
    //    s = JulianKeyword::Instance()->ms_sRELATEDTO;
    //    return ICalProperty::propertyVectorToICALString(s, getRelatedTo(), sResult);
    case ms_cOrganizer:
        s = JulianKeyword::Instance()->ms_sORGANIZER;
        return ICalProperty::propertyToICALString(s, getOrganizerProperty(), sResult);
    case ms_cRequestStatus:
        s = JulianKeyword::Instance()->ms_sREQUESTSTATUS;
        //return ICalProperty::propertyToICALString(s, getRequestStatusProperty(), sResult);
        return ICalProperty::propertyVectorToICALString(s, getRequestStatus(), sResult);
    case ms_cSequence:
        s = JulianKeyword::Instance()->ms_sSEQUENCE;
        return ICalProperty::propertyToICALString(s, getSequenceProperty(), sResult);
    case ms_cUID:
        s = JulianKeyword::Instance()->ms_sUID;
        return ICalProperty::propertyToICALString(s, getUIDProperty(), sResult);
    case ms_cURL: 
        s = JulianKeyword::Instance()->ms_sURL;
        //return ICalProperty::propertyVectorToICALString(s, getURL(), sResult);
        return ICalProperty::propertyToICALString(s, getURLProperty(), sResult);
    case ms_cXTokens: 
        return ICalProperty::vectorToICALString(getXTokens(), sResult);
    default:
        {
          //DebugMsg.Instance().println(0,"No such member in VFreebusy");
          //sResult = ParserUtil.multiLineFormat(sResult);
          return "";
        }   
    }
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
   
    UnicodeString sResult;

    switch(c)
    {
    case ms_cAttendees:
        return ICalProperty::propertyVectorToString(getAttendees(), dateFmt, sResult);
        /*
    case ms_cCreated:
        return ICalProperty::propertyToString(getCreatedProperty(), dateFmt, sResult);
        */
    case ms_cContact:
        return ICalProperty::propertyVectorToString(getContact(), dateFmt, sResult);
    case ms_cDuration:
        return getDuration().toString();
        //return ICalProperty::propertyToString(getDurationProperty(), dateFmt, sResult);
    case ms_cDTEnd:
        return ICalProperty::propertyToString(getDTEndProperty(), dateFmt, sResult);
    case ms_cDTStart:
        return ICalProperty::propertyToString(getDTStartProperty(), dateFmt, sResult); 
    case ms_cDTStamp:
        return ICalProperty::propertyToString(getDTStampProperty(), dateFmt, sResult); 
    case ms_cLastModified:
        return ICalProperty::propertyToString(getLastModifiedProperty(), dateFmt, sResult); 
    case ms_cOrganizer:
        return ICalProperty::propertyToString(getOrganizerProperty(), dateFmt, sResult); 
    case ms_cRequestStatus:
        //return ICalProperty::propertyToString(getRequestStatusProperty(), dateFmt, sResult); 
        return ICalProperty::propertyVectorToString(getRequestStatus(), dateFmt, sResult); 
    case ms_cSequence:
        return ICalProperty::propertyToString(getSequenceProperty(), dateFmt, sResult); 
    case ms_cUID:
        return ICalProperty::propertyToString(getUIDProperty(), dateFmt, sResult);
    case ms_cURL:
        return ICalProperty::propertyToString(getURLProperty(), dateFmt, sResult);
    case ms_cFreebusy:
        return ICalProperty::propertyVectorToString(getFreebusy(), dateFmt, sResult);
    default:
        {
            return "";
        }   
    }
}
//---------------------------------------------------------------------
t_bool VFreebusy::MatchUID_seqNO(UnicodeString sUID, t_int32 iSeqNo)
{
    //t_int32 seq = getSequence();
    //UnicodeString uid = getUID();

    if (getSequence() == iSeqNo && getUID().compareIgnoreCase(sUID) == 0)
      return TRUE;
    else
      return FALSE;
}

//---------------------------------------------------------------------
#if 0
void
VFreebusy::setAttendeeStatus(UnicodeString & sAttendeeFilter,
                             Attendee::STATUS status,
                             JulianPtrArray * delegatedTo)
{
    Attendee * a;
    a = getAttendee(sAttendeeFilter);
    if (a == 0)
    {
        // add a new attendee to this event (a partycrasher)
        a = Attendee::getDefault(GetType());
        PR_ASSERT(a != 0);
        if (a != 0)
        {
            a->setName(sAttendeeFilter);
            addAttendee(a);
        }
    }
    PR_ASSERT(a != 0);
    if (a != 0)
    {
        a->setStatus(status);
        if (status == Attendee::STATUS_DELEGATED)
        {
            if (delegatedTo != 0)
            {
                t_int32 i;
                UnicodeString u;
                // for each new attendee, set name, role = req_part, delfrom = me,
                // rsvp = TRUE, expect = request, status = needs-action.
                for (i = 0; i < delegatedTo->GetSize(); i++)
                {
                    u = *((UnicodeString *) delegatedTo->GetAt(i));
                    a->addDelegatedTo(u);

                    Attendee * delegate = Attendee::getDefault(GetType(), m_Log);
                    PR_ASSERT(delegate != 0);
                    if (delegate != 0)
                    {
                        delegate->setName(u);
                        delegate->setRole(Attendee::ROLE_REQ_PARTICIPANT);
                        delegate->addDelegatedFrom(sAttendeeFilter);
                        delegate->setRSVP(Attendee::RSVP_TRUE);
                        delegate->setExpect(Attendee::EXPECT_REQUEST);
                        delegate->setStatus(Attendee::STATUS_NEEDSACTION);
                        addAttendee(delegate);
                    }
                }
            }
        }
    }
}
#endif
//---------------------------------------------------------------------

Attendee *
VFreebusy::getAttendee(UnicodeString sAttendee)
{
    return Attendee::getAttendee(getAttendees(), sAttendee);
}

//---------------------------------------------------------------------

int
VFreebusy::CompareVFreebusyByUID(const void * a,
                                  const void * b)
{
    PR_ASSERT(a != 0 && b != 0);
    VFreebusy * ta = *(VFreebusy **) a;
    VFreebusy * tb = *(VFreebusy **) b;
    
    return (int) ta->getUID().compare(tb->getUID());
}

//---------------------------------------------------------------------

int
VFreebusy::CompareVFreebusyByDTStart(const void * a,
                                      const void * b)
{
    PR_ASSERT(a != 0 && b != 0);
    VFreebusy * ta = *(VFreebusy **) a;
    VFreebusy * tb = *(VFreebusy **) b;

    DateTime da, db;
    da = ta->getDTStart();
    db = tb->getDTStart();

    return (int) (da.compareTo(db));
}

//---------------------------------------------------------------------

void VFreebusy::removeAllFreebusy()
{
    if (m_FreebusyVctr != 0) 
    { 
        ICalProperty::deleteICalPropertyVector(m_FreebusyVctr);
        delete m_FreebusyVctr; m_FreebusyVctr = 0; 
    }
}
//---------------------------------------------------------------------
#if 0
t_bool
VFreebusy::isMoreRecent(VFreebusy * component)
{
    // component is not NULL
    PR_ASSERT(component != 0);
    
    if (component != 0)
    {
        t_bool bHasSeq;
        t_bool bHasDTStamp;
     
        t_int32 iSeq, jSeq;
        DateTime idts, jdts;

        iSeq = getSequence();
        idts = getDTStamp();

        jSeq = component->getSequence();
        jdts = component->getDTStamp();

        // both components must have sequence and DTSTAMP
        bHasSeq = (iSeq >= 0 && jSeq >= 0);
        bHasDTStamp = (idts.isValid() && jdts.isValid());

        PR_ASSERT(bHasSeq);
        PR_ASSERT(bHasDTStamp);

        // if iSeq > jSeq return TRUE
        // else if iSeq < jSeq return FALSE
        // else if iSeq == jSeq
        // {
        // else if jdts is after idts return FALSE
        // else if jdts is before idts return TRUE
        // else (dts is equal to idts) return TRUE
        // }

        if (component != 0 && bHasSeq && bHasDTStamp)
        {
            if (iSeq > jSeq)
            {
                return TRUE;
            }
            else if (iSeq < jSeq)
            {
                return FALSE;
            }   
            else
            {
                return !(jdts.afterDateTime(idts));
            }
        }
    }
    // assert failed
    return FALSE;
}
#endif
//---------------------------------------------------------------------

void 
VFreebusy::updateComponentHelper(VFreebusy * updatedComponent)
{
    // no created, last-modified, UID
    DateTime d;
    ICalComponent::internalSetPropertyVctr(&m_AttendeesVctr, updatedComponent->getAttendees());
    ICalComponent::internalSetPropertyVctr(&m_CommentVctr, updatedComponent->getComment());
    ICalComponent::internalSetPropertyVctr(&m_ContactVctr, updatedComponent->getContact());
    ICalComponent::internalSetPropertyVctr(&m_FreebusyVctr, updatedComponent->getFreebusy());
    //ICalComponent::internalSetPropertyVctr(&m_RelatedToVctr, updatedComponent->getRelatedTo());
    //ICalComponent::internalSetPropertyVctr(&m_URLVctr, updatedComponent->getURL());
    ICalComponent::internalSetProperty(&m_DTStart, updatedComponent->m_DTStart);
    ICalComponent::internalSetProperty(&m_DTEnd, updatedComponent->m_DTEnd);
    ICalComponent::internalSetProperty(&m_DTStamp, updatedComponent->m_DTStamp);
    ICalComponent::internalSetProperty(&m_Organizer, updatedComponent->m_Organizer);
    //ICalComponent::internalSetPropertyVctr(&m_RequestStatusVctr, updatedComponent->getRequestStatus());
    ICalComponent::internalSetProperty(&m_Sequence, updatedComponent->m_Sequence);
    ICalComponent::internalSetProperty(&m_URL, updatedComponent->m_URL);
    ICalComponent::internalSetXTokensVctr(&m_XTokensVctr, updatedComponent->m_XTokensVctr);

    ICalComponent::internalSetProperty(&m_LastModified, updatedComponent->m_LastModified);
    //setLastModified(d);
}

//---------------------------------------------------------------------

t_bool
VFreebusy::updateComponent(ICalComponent * updatedComponent)
{
    if (updatedComponent != 0)
    {
        ICAL_COMPONENT ucType = updatedComponent->GetType();

        // only call updateComponentHelper if it's a VFreebusy and
        // it is an exact matching UID and updatedComponent 
        // is more recent than this component
        if (ucType == ICAL_COMPONENT_VFREEBUSY)
        {
            // should be a safe cast with check above.
            VFreebusy * ucvf = (VFreebusy *) updatedComponent;

            // only if uid's match and are not empty
            if (ucvf->getUID().size() > 0 && getUID().size() > 0)
            {
                //if (ucvf->getUID() == getUID() && !isMoreRecent(ucvf))
                if (ucvf->getUID() == getUID())
                {
                    updateComponentHelper(ucvf);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}
//---------------------------------------------------------------------
// GETTERS AND SETTERS here
//---------------------------------------------------------------------
// attendee
void VFreebusy::addAttendee(Attendee * a)
{ 
    if (m_AttendeesVctr == 0)
        m_AttendeesVctr = new JulianPtrArray(); 
    PR_ASSERT(m_AttendeesVctr != 0);
    if (m_AttendeesVctr != 0)
    {
        m_AttendeesVctr->Add(a);
    }
}
//---------------------------------------------------------------------
// freebusy
void VFreebusy::addFreebusy(Freebusy * f)    
{ 
    if (m_FreebusyVctr == 0)
        m_FreebusyVctr = new JulianPtrArray(); 
    PR_ASSERT(m_FreebusyVctr != 0);
    if (m_FreebusyVctr != 0)
    {
        m_FreebusyVctr->Add(f);
    }
}
//---------------------------------------------------------------------
///DTEnd
void VFreebusy::setDTEnd(DateTime s, JulianPtrArray * parameters)
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
#else
    ICalComponent::setDateTimeValue(((ICalProperty **) &m_DTEnd), s, parameters);
#endif
}
DateTime VFreebusy::getDTEnd() const 
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
///nsCalDuration
void VFreebusy::setDuration(nsCalDuration s, JulianPtrArray * parameters)
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
nsCalDuration VFreebusy::getDuration() const 
{
    /*
    nsCalDuration d; d.set(-1,-1,-1,-1,-1,-1);
    if (m_Duration == 0)
        return d; // return 0;
    else
    {
        d = *((nsCalDuration *) m_Duration->getValue());
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
//LAST-MODIFIED
void VFreebusy::setLastModified(DateTime s, JulianPtrArray * parameters)
{ 
#if 1
    if (m_LastModified == 0)
        m_LastModified = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_LastModified->setValue((void *) &s);
        m_LastModified->setParameters(parameters);
    }
#else
    ICalComponent::setDateTimeValue(((ICalProperty **) &m_LastModified), s, parameters);
#endif
}

DateTime VFreebusy::getLastModified() const
{
#if 1
    DateTime d(-1);
    if (m_LastModified == 0)
        return d; // return 0;
    else
    {
        d = *((DateTime *) m_LastModified->getValue());
        return d;
    }
#else
    DateTime d(-1);
    ICalComponent::getDateTimeValue(((ICalProperty **) &m_LastModified), d);
    return d;
#endif
}
//---------------------------------------------------------------------
#if 0
//Created
void VFreebusy::setCreated(DateTime s, JulianPtrArray * parameters)
{ 
#if 1
    if (m_Created == 0)
        m_Created = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_Created->setValue((void *) &s);
        m_Created->setParameters(parameters);
    }
#else
    ICalComponent::setDateTimeValue(((ICalProperty **) &m_Created), s, parameters);
#endif
}
DateTime VFreebusy::getCreated() const
{
#if 1
    DateTime d(-1);
    if (m_Created == 0)
        return d;//return 0;
    else
    {
        d = *((DateTime *) m_Created->getValue());
        return d;
    }
#else
    DateTime d(-1);
    ICalComponent::getDateTimeValue(((ICalProperty **) &m_Created), d);
    return d;
#endif
}
#endif
//---------------------------------------------------------------------
//DTStamp
void VFreebusy::setDTStamp(DateTime s, JulianPtrArray * parameters)
{ 
#if 1
    if (m_DTStamp == 0)
        m_DTStamp = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_DTStamp->setValue((void *) &s);
        m_DTStamp->setParameters(parameters);
    }
#else
    ICalComponent::setDateTimeValue(((ICalProperty **) &m_DTStamp), s, parameters);
#endif
}
DateTime VFreebusy::getDTStamp() const
{
#if 1
    DateTime d(-1);
    if (m_DTStamp == 0)
        return d;//return 0;
    else
    {
        d = *((DateTime *) m_DTStamp->getValue());
        return d;
    }
#else
    DateTime d(-1);
    ICalComponent::getDateTimeValue(((ICalProperty **) &m_DTStamp), d);
    return d;
#endif
}
//---------------------------------------------------------------------
///DTStart
void VFreebusy::setDTStart(DateTime s, JulianPtrArray * parameters)
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

DateTime VFreebusy::getDTStart() const
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
//Organizer
void VFreebusy::setOrganizer(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);
    
    if (m_Organizer == 0)
    {
        //m_Organizer = ICalPropertyFactory::Make(ICalProperty::TEXT, 
        //                                    (void *) &s, parameters);

        m_Organizer = (ICalProperty *) new nsCalOrganizer(m_Log);
        PR_ASSERT(m_Organizer != 0);
        m_Organizer->setValue((void *) &s);
        m_Organizer->setParameters(parameters);        
    }
    else
    {
        m_Organizer->setValue((void *) &s);
        m_Organizer->setParameters(parameters);
    }
}
UnicodeString VFreebusy::getOrganizer() const 
{
    UnicodeString u;
    if (m_Organizer == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Organizer->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//RequestStatus
#if 0
void VFreebusy::setRequestStatus(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_RequestStatus == 0)
        m_RequestStatus = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_RequestStatus->setValue((void *) &s);
        m_RequestStatus->setParameters(parameters);
    }
}
UnicodeString VFreebusy::getRequestStatus() const 
{
    UnicodeString u;
    if (m_RequestStatus == 0)
        return "";
    else 
    {
        u = *((UnicodeString *) m_RequestStatus->getValue());
        return u;
    }
}
#endif
void VFreebusy::addRequestStatus(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addRequestStatusProperty(prop);
} 
void VFreebusy::addRequestStatusProperty(ICalProperty * prop)      
{ 
    if (m_RequestStatusVctr == 0)
        m_RequestStatusVctr = new JulianPtrArray(); 
    PR_ASSERT(m_RequestStatusVctr != 0);
    m_RequestStatusVctr->Add(prop);
}
//---------------------------------------------------------------------
//UID
void VFreebusy::setUID(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_UID == 0)
        m_UID = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_UID->setValue((void *) &s);
        m_UID->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_UID), s, parameters);
#endif
}
UnicodeString VFreebusy::getUID() const 
{
#if 1
    UnicodeString u;
    if (m_UID == 0)
        return "";
    else {
        u = *((UnicodeString *) m_UID->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_UID), us);
    return us;
#endif
}
//---------------------------------------------------------------------
//Sequence
void VFreebusy::setSequence(t_int32 i, JulianPtrArray * parameters)
{
#if 1
    if (m_Sequence == 0)
        m_Sequence = ICalPropertyFactory::Make(ICalProperty::INTEGER, 
                                            (void *) &i, parameters);
    else
    {
        m_Sequence->setValue((void *) &i);
        m_Sequence->setParameters(parameters);
    }
#else
    ICalComponent::setIntegerValue(((ICalProperty **) &m_Sequence), i, parameters);
#endif
}
t_int32 VFreebusy::getSequence() const 
{
#if 1
    t_int32 i;
    if (m_Sequence == 0)
        return -1;
    else
    {
        i = *((t_int32 *) m_Sequence->getValue());
        return i;
    }
#else
    t_int32 i = -1;
    ICalComponent::getIntegerValue(((ICalProperty **) &m_Sequence), i);
    return i;
#endif
}
//---------------------------------------------------------------------
//comment
void VFreebusy::addComment(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addCommentProperty(prop);
}
void VFreebusy::addCommentProperty(ICalProperty * prop)
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
// Contact
void VFreebusy::addContact(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *)&s, parameters);
    addContactProperty(prop);
}
void VFreebusy::addContactProperty(ICalProperty * prop)    
{ 
    if (m_ContactVctr == 0)
        m_ContactVctr = new JulianPtrArray(); 
    PR_ASSERT(m_ContactVctr != 0);
    if (m_ContactVctr != 0)
    {
        m_ContactVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// RelatedTo
//void VFreebusy::addRelatedTo(UnicodeString s, JulianPtrArray * parameters)
//{
//    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
//            (void *) &s, parameters);
//    addRelatedToProperty(prop);
//} 
//void VFreebusy::addRelatedToProperty(ICalProperty * prop)      
//{ 
//    if (m_RelatedToVctr == 0)
//        m_RelatedToVctr = new JulianPtrArray(); 
//    PR_ASSERT(m_RelatedToVctr != 0);
//    m_RelatedToVctr->Add(prop);
//}
//---------------------------------------------------------------------
// URL
//void VFreebusy::addURL(UnicodeString s, JulianPtrArray * parameters)
//{
//    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
//            (void *) &s, parameters);
//    addURLProperty(prop);
//}
//void VFreebusy::addURLProperty(ICalProperty * prop)    
//{ 
//    if (m_URLVctr == 0)
//        m_URLVctr = new JulianPtrArray(); 
//    PR_ASSERT(m_URLVctr != 0);
//    m_URLVctr->Add(prop);
//}
void VFreebusy::setURL(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_URL == 0)
        m_URL = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_URL->setValue((void *) &s);
        m_URL->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_URL), s, parameters);
#endif
}
UnicodeString VFreebusy::getURL() const 
{
#if 1
    UnicodeString u;
    if (m_URL == 0)
        return "";
    else {
        u = *((UnicodeString *) m_URL->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_URL), us);
    return us;
#endif
}
//---------------------------------------------------------------------
// XTOKENS
void VFreebusy::addXTokens(UnicodeString s)         
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

