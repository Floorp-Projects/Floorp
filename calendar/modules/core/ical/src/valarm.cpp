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
 * valarm.cpp
 * John Sun
 * 7/22/98 11:15:46 AM
 */

#include "stdafx.h"
#include "jdefines.h"

#include "icalredr.h"
#include "valarm.h"
#include "unistrto.h"
#include "keyword.h"
#include "prprtyfy.h"

#include "functbl.h"
//---------------------------------------------------------------------

// private never use
#if 0
VAlarm::VAlarm()
{
    PR_ASSERT(FALSE);
}
#endif
//---------------------------------------------------------------------

VAlarm::VAlarm(JLog * initLog) 
: m_Trigger(0), m_Duration(0), m_Repeat(0), 
  m_XTokensVctr(0),
  m_AttachVctr(0), m_Description(0), m_AttendeesVctr(0),
  m_Summary(0),
  m_Log(initLog)
{
    m_Action = ACTION_INVALID;
    m_TriggerDateTime.setTime(-1);
}

//---------------------------------------------------------------------

VAlarm::VAlarm(VAlarm & that) 
: m_Trigger(0), m_Duration(0), m_Repeat(0), 
  m_XTokensVctr(0),
  m_AttachVctr(0), m_Description(0), m_AttendeesVctr(0),
  m_Summary(0)
{
    m_Action = that.m_Action;
    m_TriggerDateTime = that.m_TriggerDateTime;

    if (that.m_Trigger != 0)
    {
        m_Trigger = that.m_Trigger->clone(m_Log);
    }
    if (that.m_Duration != 0)
    {
        m_Duration = that.m_Duration->clone(m_Log);
    }
    if (that.m_Repeat != 0)
    {
        m_Repeat = that.m_Repeat->clone(m_Log);
    }
    if (that.m_XTokensVctr != 0)
    {
        m_XTokensVctr = new JulianPtrArray(); PR_ASSERT(m_XTokensVctr != 0);
        if (m_XTokensVctr != 0)
        {
            ICalProperty::CloneUnicodeStringVector(that.m_XTokensVctr, m_XTokensVctr);
        }
    }
    if (that.m_AttachVctr != 0)
    {
        m_AttachVctr = new JulianPtrArray(); PR_ASSERT(m_AttachVctr != 0);
        if (m_AttachVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_AttachVctr, m_AttachVctr, m_Log);
        }
    }
    if (that.m_Description != 0) 
    { 
        m_Description = that.m_Description->clone(m_Log); 
    }
    if (that.m_AttendeesVctr != 0)
    {
        m_AttendeesVctr = new JulianPtrArray(); PR_ASSERT(m_AttendeesVctr != 0);
        if (m_AttendeesVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_AttendeesVctr, m_AttendeesVctr, m_Log);
        }
    }
    if (that.m_Summary != 0) 
    { 
        m_Summary = that.m_Summary->clone(m_Log); 
    }
}

//---------------------------------------------------------------------
ICalComponent *
VAlarm::clone(JLog * initLog)
{
    m_Log = initLog;
    return new VAlarm(*this);
}
//---------------------------------------------------------------------

VAlarm::~VAlarm()
{
    // trigger, duration, repeat
    if (m_Trigger != 0) 
    { 
        delete m_Trigger; m_Trigger = 0; 
    }
    if (m_Duration != 0) 
    { 
        delete m_Duration; m_Duration = 0; 
    }
    if (m_Repeat != 0) 
    { 
        delete m_Repeat; m_Repeat = 0; 
    }
    // x-tokens
    if (m_XTokensVctr != 0) 
    { 
        ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
    // attach, description, attendees
    if (m_AttachVctr != 0) 
    { 
        ICalProperty::deleteICalPropertyVector(m_AttachVctr);
        delete m_AttachVctr; m_AttachVctr = 0; 
    }
    if (m_Description != 0) 
    { 
        delete m_Description; m_Description = 0; 
    }
    if (m_AttendeesVctr != 0) 
    { 
        ICalProperty::deleteICalPropertyVector(m_AttendeesVctr);
        delete m_AttendeesVctr; m_AttendeesVctr = 0; 
    }
    if (m_Summary != 0) 
    { 
        delete m_Summary; m_Summary = 0; 
    }
}
    
//---------------------------------------------------------------------

UnicodeString &
VAlarm::parse(ICalReader * brFile, UnicodeString & sType, 
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
        // ran out of memory, return invalid VAlarm
        return parseStatus;
    }
    ErrorCode status = ZERO_ERROR;
    
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
             (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVALARM) == 0))
         {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

            break;
         }
         else if (
                ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                    (   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
                        (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
                        (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) ||
                        (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) || 
                        (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) || 
                        (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0) || 
                        (ICalProperty::IsXToken(propVal)))
                  ) ||
                  ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) &&
                  ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) || 
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) || 
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) || 
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) || 
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0) || 
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) || 
                  (ICalProperty::IsXToken(propVal)) ||
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVALARM) == 0)))) 
         {
             // END:VEVENT, VTODO, VCALENDAR, VJOURNAL, VFREEBUSY, VTIMEZONE, x-token
             // BEGIN:VEVENT, VTODO, VJOURNAL, VFREEBUSY, VTIMEZONE, VCALENDAR, VALARM, xtoken
             // abrupt end
             if (m_Log) m_Log->logError(
                 JulianLogErrorMessage::Instance()->ms_iAbruptEndOfParsing, 
                 JulianKeyword::Instance()->ms_sVALARM, strLine, 300);
             ICalProperty::deleteICalParameterVector(parameters);
             parameters->RemoveAll();

             parseStatus = strLine;
             break;            
         }
         else 
         {
             storeData(strLine, propName, propVal, parameters, vTimeZones);
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
void VAlarm::storeAction(UnicodeString & strLine, UnicodeString & propVal, 
                         JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    t_int32 i;
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVALARM, strLine, 100);
    }
    i = stringToAction(propVal);
    if (i < 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidPropertyValue, 
            JulianKeyword::Instance()->ms_sVALARM,
            JulianKeyword::Instance()->ms_sACTION, propVal, 200);
    }
    else 
    {
        if (getAction() >= 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVALARM,
                JulianKeyword::Instance()->ms_sACTION, propVal, 100);
        }
        setAction((VAlarm::ACTION) i);
    }
}
//---------------------------------------------------------------------

void VAlarm::storeAttach(UnicodeString & strLine, UnicodeString & propVal, 
                         JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    t_bool bParamValid;
    bParamValid = ICalProperty::CheckParamsWithValueRangeCheck(parameters, 
        JulianAtomRange::Instance()->ms_asEncodingValueFMTTypeParamRange,
        JulianAtomRange::Instance()->ms_asEncodingValueFMTTypeParamRangeSize, 
        JulianAtomRange::Instance()->ms_asBinaryURIValueRange,
        JulianAtomRange::Instance()->ms_asBinaryURIValueRangeSize);        
    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVALARM, strLine, 100);
    }
    addAttach(propVal, parameters);
}
//---------------------------------------------------------------------

void VAlarm::storeAttendee(UnicodeString & strLine, UnicodeString & propVal, 
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
                JulianLogErrorMessage::Instance()->ms_iInvalidAttendee, 200);
            delete attendee; attendee = 0;
        }
        else
        {
            addAttendee(attendee);
        }
    }
}
//---------------------------------------------------------------------
void VAlarm::storeDescription(UnicodeString & strLine, UnicodeString & propVal, 
                              JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // check parameters
    t_bool bParamValid;
    bParamValid = ICalProperty::CheckParams(parameters, 
        JulianAtomRange::Instance()->ms_asAltrepLanguageParamRange,
        JulianAtomRange::Instance()->ms_asAltrepLanguageParamRangeSize);
    
    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVALARM, strLine, 100);
    }

    // check for duplicates
    if (getDescriptionProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVALARM, 
            JulianKeyword::Instance()->ms_sDESCRIPTION, 100);
    }

    setDescription(propVal, parameters);
}
//---------------------------------------------------------------------
void VAlarm::storeDuration(UnicodeString & strLine, UnicodeString & propVal, 
                           JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVALARM, strLine, 100);
    }

    if (getDurationProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVALARM, 
            JulianKeyword::Instance()->ms_sDURATION, 100);
    }
    Julian_Duration d(propVal);
    setDuration(d, parameters);
}
//---------------------------------------------------------------------
void VAlarm::storeRepeat(UnicodeString & strLine, UnicodeString & propVal, 
                         JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    t_bool bParseError = FALSE;
    t_int32 i;

    // no parameters
    if (parameters->GetSize() > 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVALARM, strLine, 100);
    }

    char * pcc = propVal.toCString("");
    PR_ASSERT(pcc != 0);
    i = JulianUtility::atot_int32(pcc, bParseError, propVal.size());
    delete [] pcc; pcc = 0;

    if (getRepeatProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVALARM, 
            JulianKeyword::Instance()->ms_sREPEAT, 100);
    }  
    if (!bParseError)
    {
        setRepeat(i, parameters);
    }
    else
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidNumberFormat, 
            JulianKeyword::Instance()->ms_sVALARM, 
            JulianKeyword::Instance()->ms_sREPEAT, propVal, 200);
    }
}
//---------------------------------------------------------------------
void VAlarm::storeSummary(UnicodeString & strLine, UnicodeString & propVal, 
                          JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // check parameters 
    t_bool bParamValid;
    bParamValid = ICalProperty::CheckParams(parameters, 
        JulianAtomRange::Instance()->ms_asLanguageParamRange,
        JulianAtomRange::Instance()->ms_asLanguageParamRangeSize);
    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVALARM, strLine, 100);
    }

    if (getSummaryProperty() != 0)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVALARM, 
            JulianKeyword::Instance()->ms_sSUMMARY, 100);
    }
    setSummary(propVal, parameters);
}

//---------------------------------------------------------------------
void VAlarm::storeTrigger(UnicodeString & strLine, UnicodeString & propVal, 
                          JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
{
    // check parameters 
    t_bool bParamValid;
    bParamValid = ICalProperty::CheckParams(parameters, 
        JulianAtomRange::Instance()->ms_asRelatedValueParamRange,
        JulianAtomRange::Instance()->ms_asRelatedValueParamRangeSize);
    if (!bParamValid)
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
            JulianKeyword::Instance()->ms_sVALARM, strLine, 100);
    }
    // if propVal is a datetime, store trigger as a datetime, otherwise store is as a duration
    if (getDurationProperty() != 0 || m_TriggerDateTime.isValid())
    {
           if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVALARM, 
                JulianKeyword::Instance()->ms_sDURATION, 100);
    }

    DateTime dt(propVal);
    if (dt.isValid())
    {
        setTriggerAsDateTime(dt);
    }
    else
    {
        Julian_Duration d(propVal);
        setTriggerAsDuration(d, parameters);            
    }
}
    

//---------------------------------------------------------------------
void 
VAlarm::storeData(UnicodeString & strLine, UnicodeString & propName,
                  UnicodeString & propVal, JulianPtrArray * parameters,
                  JulianPtrArray * vTimeZones)
{
    t_int32 hashCode = propName.hashCode();
    t_int32 i;
    for (i = 0; 0 != (JulianFunctionTable::Instance()->alarmStoreTable[i]).op; i++)
    {
        if ((JulianFunctionTable::Instance()->alarmStoreTable[i]).hashCode == hashCode)
        {
            ApplyStoreOp(((JulianFunctionTable::Instance())->alarmStoreTable[i]).op, 
                strLine, propVal, parameters, vTimeZones);
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
            JulianKeyword::Instance()->ms_sVALARM, propName, 200);
    }
}

//---------------------------------------------------------------------
void VAlarm::selfCheck()
{
    UnicodeString u;
   
    switch(m_Action)
    {
    case ACTION_AUDIO:
        break;
    case ACTION_DISPLAY:
        
        // if display prop, make sure description there.
        //   if not add a default description
        
        if (getDescription().size() == 0)
        {
            // TODO: make this localized;
            //u = JulianLogErrorMessage::Instance()->ms_iDefaultAlarmDescriptionString;
            u = "DEFAULT";
            setDescription(u);
        }
        break;
    case ACTION_EMAIL:
     
        // if email prop, make sure description and summary
        //   if not add a default description and summary
        
        if (getDescription().size() == 0)
        {
            // TODO: make this localized;
            //u = JulianLogErrorMessage::Instance()->ms_iDefaultAlarmDescriptionString;
            u = "DEFAULT";
            setDescription(u);
        }
        if (getSummary().size() == 0)
        {
             // TODO: make this localized;
            //u = JulianLogErrorMessage::Instance()->ms_iDefaultAlarmSummaryString;
            u = "DEFAULT";
            setSummary(u);
        }
        break;
    case ACTION_PROCEDURE:
        break;
    default:
        break;
    }
}
//---------------------------------------------------------------------
t_bool VAlarm::isValid()
{

    // action must be set to valid value
    if (m_Action == ACTION_INVALID)
        return FALSE;

    // trigger must be valid duration or datetime.
    if (m_Trigger == 0 && !m_TriggerDateTime.isValid())
        return FALSE;

    switch (m_Action)
    {
    case ACTION_AUDIO:
        // if action == audio: must ONLY one attach if it has an attach
        if (getAttach() != 0 && getAttach()->GetSize() != 1)
            return FALSE;

        // MAKE SURE certain properties are empty. may remove later
        if (getDescription().size() > 0)
            return FALSE;
        if (getSummary().size() > 0)
            return FALSE;
        if (getAttendees() != 0)
            return FALSE;
        break;
    case ACTION_DISPLAY:
        // if action == display: must have description
        if (getDescription().size() == 0)
            return FALSE;

        // MAKE SURE certain properties are empty. may remove later
        if (getSummary().size() > 0)
            return FALSE;
        if (getAttendees() != 0)
            return FALSE;
        if (getAttach() != 0)
            return FALSE;
        break;
    case ACTION_EMAIL:
        // if action == email: must have description, summary, at least ONE attendee
        if (getDescription().size() == 0 || getSummary().size() == 0 ||
            getAttendees() == 0 || getAttendees()->GetSize() == 0)
            return FALSE;
        break;
    case ACTION_PROCEDURE:
        // if action == procedure: must have ONLY one attach
        if (getAttach() == 0 || getAttach()->GetSize() != 1)
            return FALSE;

        // MAKE SURE certain properties are empty. may remove later
        if (getSummary().size() > 0)
            return FALSE;
        if (getAttendees() != 0)
            return FALSE;
        break;
    default:
        return FALSE;
    }

    return TRUE;
}
//---------------------------------------------------------------------
UnicodeString VAlarm::toICALString()
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVALARM;
    return ICalComponent::format(u, JulianFormatString::Instance()->ms_sVAlarmAllMessage, "");
}
//---------------------------------------------------------------------
UnicodeString VAlarm::toICALString(UnicodeString method,
                                   UnicodeString name,
                                   t_bool isRecurring)
{
    // NOTE: remove later avoid warnings
    if (isRecurring) {}
    return toICALString();
}
//---------------------------------------------------------------------
UnicodeString VAlarm::toString()
{
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_VAlarmStrDefaultFmt);
}
//---------------------------------------------------------------------
UnicodeString VAlarm::formatChar(t_int32 c, UnicodeString sFilterAttendee,
                                 t_bool delegateRequest)
{
    UnicodeString s, sResult;
    
    // NOTE: remove here is get rid of compiler warnings
    if (sFilterAttendee.size() > 0 || delegateRequest)
    {
    }

    switch (c)
    {
    case ms_cAction:
         // print method enum
         sResult += JulianKeyword::Instance()->ms_sACTION; 
         sResult += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
         sResult += actionToString(getAction(), s); 
         sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
         return sResult;
    case ms_cAttendees:
        s = JulianKeyword::Instance()->ms_sATTENDEE;
        return ICalProperty::propertyVectorToICALString(s, getAttendees(), sResult);    
    case ms_cAttach:
        s = JulianKeyword::Instance()->ms_sATTACH;
        return ICalProperty::propertyVectorToICALString(s, getAttach(), sResult);    
    case ms_cDescription:
        s = JulianKeyword::Instance()->ms_sDESCRIPTION;
          return ICalProperty::propertyToICALString(s, getDescriptionProperty(), sResult);
    case ms_cDuration:
        s = JulianKeyword::Instance()->ms_sDURATION;
        return ICalProperty::propertyToICALString(s, getDurationProperty(), sResult);
    case ms_cRepeat:
        s = JulianKeyword::Instance()->ms_sREPEAT;
        return ICalProperty::propertyToICALString(s, getRepeatProperty(), sResult);    
    case ms_cTrigger:
        s = JulianKeyword::Instance()->ms_sTRIGGER;
        if (m_Trigger != 0)
        {
            return ICalProperty::propertyToICALString(s, getTriggerProperty(), sResult);    
        }
        else
        {
            s += JulianKeyword::Instance()->ms_sSEMICOLON_SYMBOL;
            s += JulianKeyword::Instance()->ms_sVALUE;
            s += '=';
            s += JulianKeyword::Instance()->ms_sDATETIME;
            s += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
            s += m_TriggerDateTime.toISO8601();
            s += JulianKeyword::Instance()->ms_sLINEBREAK;
            return s;
        }
    case ms_cSummary:
        s = JulianKeyword::Instance()->ms_sSUMMARY;
        return ICalProperty::propertyToICALString(s, getSummaryProperty(), sResult);
    case ms_cXTokens: 
        return ICalProperty::vectorToICALString(getXTokens(), sResult);
    default:
        return "";
    }
}
//---------------------------------------------------------------------
UnicodeString VAlarm::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    UnicodeString s;
    switch (c)
    {
    case ms_cAction:
        return actionToString(getAction(), s); 
    case ms_cAttendees:
        return ICalProperty::propertyVectorToString(getAttendees(), dateFmt, s);
    case ms_cAttach:
        return ICalProperty::propertyVectorToString(getAttach(), dateFmt, s);
    case ms_cDescription:
        return ICalProperty::propertyToString(getDescriptionProperty(), dateFmt, s);
    case ms_cDuration:
        return ICalProperty::propertyToString(getDurationProperty(), dateFmt, s); 
    case ms_cRepeat:
        return ICalProperty::propertyToString(getRepeatProperty(), dateFmt, s); 
    case ms_cTrigger:
        if (m_Trigger != 0)
        {
            return ICalProperty::propertyToString(getTriggerProperty(), dateFmt, s); 
        }
        else
        {
            return m_TriggerDateTime.toString();
        }
    case ms_cSummary:
        return ICalProperty::propertyToString(getSummaryProperty(), dateFmt, s); 
    case ms_cXTokens:
    default:
        return "";
    }
}
//---------------------------------------------------------------------

void 
VAlarm::updateComponentHelper(VAlarm * updatedComponent)
{
    // todo: finish
}

//---------------------------------------------------------------------

t_bool
VAlarm::updateComponent(ICalComponent * updatedComponent)
{
    if (updatedComponent != 0)
    {
        ICAL_COMPONENT ucType = updatedComponent->GetType();

        // only call updateComponentHelper if it's a VAlarm and
        // it is an exact matching Name (STANDARD or DAYLIGHT)
        // basically always overwrite
        if (ucType == ICAL_COMPONENT_VALARM)
        {
            // todo: finish
        }
    }
    return FALSE;
}

//---------------------------------------------------------------------

VAlarm::ACTION 
VAlarm::stringToAction(UnicodeString & action)
{
    t_int32 hashCode = action.hashCode();

    if (JulianKeyword::Instance()->ms_ATOM_AUDIO == hashCode) return ACTION_AUDIO;
    else if (JulianKeyword::Instance()->ms_ATOM_DISPLAY == hashCode) return ACTION_DISPLAY;
    else if (JulianKeyword::Instance()->ms_ATOM_EMAIL == hashCode) return ACTION_EMAIL;
    else if (JulianKeyword::Instance()->ms_ATOM_PROCEDURE == hashCode) return ACTION_PROCEDURE;
    else return ACTION_INVALID;
    // ??? is AUDIO to be default, or should I have invalid state?
}

//---------------------------------------------------------------------

UnicodeString & VAlarm::actionToString(VAlarm::ACTION action, UnicodeString & out)
{
    switch(action)
    {
    case ACTION_AUDIO: out = JulianKeyword::Instance()->ms_sAUDIO; break;
    case ACTION_DISPLAY: out = JulianKeyword::Instance()->ms_sDISPLAY; break;
    case ACTION_EMAIL: out = JulianKeyword::Instance()->ms_sEMAIL; break;
    case ACTION_PROCEDURE: out = JulianKeyword::Instance()->ms_sPROCEDURE; break;
    default:
        out = "";
        break;
    }
    return out;
}
//---------------------------------------------------------------------
//---------------------------------------------------------------------
///Julian_Duration
void VAlarm::setDuration(Julian_Duration s, JulianPtrArray * parameters)
{ 
    if (m_Duration == 0)
        m_Duration = ICalPropertyFactory::Make(ICalProperty::DURATION, 
                                            (void *) &s, parameters);
    else
    {
        m_Duration->setValue((void *) &s);
        m_Duration->setParameters(parameters);
    }
}
Julian_Duration VAlarm::getDuration() const 
{
    Julian_Duration d; d.set(-1,-1,-1,-1,-1,-1);
    if (m_Duration == 0)
        return d; // return 0;
    else
    {
        d = *((Julian_Duration *) m_Duration->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
///Trigger
void VAlarm::setTriggerAsDuration(Julian_Duration s, JulianPtrArray * parameters)
{ 
    if (m_Trigger == 0)
        m_Trigger = ICalPropertyFactory::Make(ICalProperty::DURATION, 
                                            (void *) &s, parameters);
    else
    {
        m_Trigger->setValue((void *) &s);
        m_Trigger->setParameters(parameters);
    }
}
Julian_Duration VAlarm::getTriggerAsDuration() const 
{
    Julian_Duration d; d.set(-1,-1,-1,-1,-1,-1);
    if (m_Trigger == 0)
        return d; // return 0;
    else
    {
        d = *((Julian_Duration *) m_Trigger->getValue());
        return d;
    }
}
DateTime VAlarm::getTriggerAsDateTime(DateTime start) const
{
    DateTime d;
    if (m_Trigger != 0)
    {
        Julian_Duration du = getTriggerAsDuration();
        start.add(du);
        d = start;
    }
    else
        d = m_TriggerDateTime;
    return d;
}
void VAlarm::setTriggerAsDateTime(DateTime s)
{
    m_TriggerDateTime = s;
    // clear trigger duration if it exists
    if (m_Trigger != 0)
    {
        delete m_Trigger; m_Trigger = 0;
    }
}
//---------------------------------------------------------------------
//Repeat
void VAlarm::setRepeat(t_int32 i, JulianPtrArray * parameters)
{ 
#if 1
    if (m_Repeat == 0)
        m_Repeat = ICalPropertyFactory::Make(ICalProperty::INTEGER, 
                                            (void *) &i, parameters);
    else
    {
        m_Repeat->setValue((void *) &i);
        m_Repeat->setParameters(parameters);
    }
#else
    ICalComponent::setIntegerValue(((ICalProperty **) &m_Repeat), i, parameters);
#endif
}
t_int32 VAlarm::getRepeat() const 
{
#if 1
    t_int32 i;
    if (m_Repeat == 0)
        return -1;
    else
    {
        i = *((t_int32 *) m_Repeat->getValue());
        return i;
    }
#else
    t_int32 i = -1;
    ICalComponent::getIntegerValue(((ICalProperty **) &m_Repeat), i);
    return i;
#endif

}
//---------------------------------------------------------------------
//Description
void VAlarm::setDescription(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);
#if 1
    if (m_Description == 0)
        m_Description = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Description->setValue((void *) &s);
        m_Description->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_Description), s, parameters);
#endif
}
UnicodeString VAlarm::getDescription() const 
{
#if 1
    if (m_Description == 0)
        return "";
    else
    {
        UnicodeString u;
        u = *((UnicodeString *) m_Description->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_Description), us);
    return us;
#endif
}
//---------------------------------------------------------------------
//Summary
void VAlarm::setSummary(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);
#if 1
    if (m_Summary == 0)
        m_Summary = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Summary->setValue((void *) &s);
        m_Summary->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_Summary), s, parameters);
#endif
}
UnicodeString VAlarm::getSummary() const 
{
#if 1
    if (m_Summary == 0)
        return "";
    else
    {
        UnicodeString u;
        u = *((UnicodeString *) m_Summary->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_Summary), us);
    return us;
#endif
}
//---------------------------------------------------------------------
// attach
void VAlarm::addAttach(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addAttachProperty(prop);
}
void VAlarm::addAttachProperty(ICalProperty * prop)      
{ 
    if (m_AttachVctr == 0)
        m_AttachVctr = new JulianPtrArray(); 
    PR_ASSERT(m_AttachVctr != 0);
    if (m_AttachVctr != 0)
    {
        m_AttachVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// attendees
void VAlarm::addAttendee(Attendee * a)      
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
// XTOKENS
void VAlarm::addXTokens(UnicodeString s)         
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
