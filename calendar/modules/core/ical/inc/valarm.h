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
 * valarm.h
 * John Sun
 * 7/22/98 10:34:34 AM
 */

#ifndef __VALARM_H_
#define __VALARM_H_

#include <unistring.h>
#include "datetime.h"
#include "ptrarray.h"
#include "jlog.h"
#include "icalcomp.h"
#include "attendee.h"
#include "keyword.h"
#include "nscalcoreicalexp.h"

class NS_CAL_CORE_ICAL VAlarm: public ICalComponent
{
public:
    /** an enumeration of the ACTION parameter */
    /* default is audio */
    enum ACTION
    {
            ACTION_AUDIO = 0,
            ACTION_DISPLAY = 1,
            ACTION_EMAIL = 2,
            ACTION_PROCEDURE = 3,
            ACTION_INVALID = -1
    };

private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    JLog * m_Log;

    /* -- properties in ALL action types -- */
    /*ICalProperty * m_Action;*/
    ACTION m_Action;

    /* as a duration */
    ICalProperty * m_Trigger;

    /* as a datetime */
    DateTime m_TriggerDateTime;

    /* duration and repeat are optional */
    ICalProperty * m_Duration;
    ICalProperty * m_Repeat;

    JulianPtrArray * m_XTokensVctr;
    
    /* -- end ALL action types properties -- */


    /* used in more that one action type but not all */
    JulianPtrArray * m_AttachVctr; 
    ICalProperty * m_Description;

    /* audio props */
    /* always ONLY one attachment */
    /* NO description */

    /* display props */
    /* NO attachments */
    /* One description */

    /* email props */
    /* any number of attachments */
    /* One description */
    /* at least one attendee */
    JulianPtrArray * m_AttendeesVctr; 
    ICalProperty * m_Summary;

    /* procedure props */
    /* always ONLY one attachment */
    /* Zero or One description */

    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    /**
     * Creates NLS TimeZone from start month,day,week,time,until.  
     * TODO: possible bug, may need to call everytime before calling getNLSTimeZone().
     */
    void selfCheck();
    
   /**
     * store the data, depending on property name, property value
     * parameter names, parameter values, and the current line.
     *
     * @param       strLine         current line to process
     * @param       propName        name of property
     * @param       propVal         value of property
     * @param       parameters      property's parameters
     * @param       vTimeZones      vector of timezones
     * @return      TRUE if line was handled, FALSE otherwise
     */
    void storeData(UnicodeString & strLine, 
        UnicodeString & propName, UnicodeString & propVal,
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);

    /**
     * Helper method called by updateComponent to actually replace
     * the property data-members with updatedComponent's data-members.
     * @param           VTimeZone * updatedComponent
     *
     * @return          virtual void 
     */
    void updateComponentHelper(VAlarm * updatedComponent);
public:
    typedef void (VAlarm::*SetOp) (UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);

    /* Clients should not call these methods even though they are not public */

    void storeAction(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);
    void storeAttach(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);
    void storeAttendee(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);
    void storeDescription(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);
    void storeDuration(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);
    void storeRepeat(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);
    void storeSummary(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);
    void storeTrigger(UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones);
    
    void ApplyStoreOp(void (VAlarm::*op) (UnicodeString & strLine, 
        UnicodeString & propVal, JulianPtrArray * parameters, JulianPtrArray * vTimeZones), 
        UnicodeString & strLine, UnicodeString & propVal, 
        JulianPtrArray * parameters, JulianPtrArray * vTimeZones)
    {
        (this->*op)(strLine, propVal, parameters, vTimeZones);
    }
private:

    

#if 0
    VAlarm();
#endif
protected:

    VAlarm(VAlarm & that);

public:
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    VAlarm(JLog * initLog = 0);

    virtual ~VAlarm();

    /*  -- Start of ICALComponent interface -- */
 
    /**
     * The parse method is the standard interface for ICalComponent
     * subclasses to parse from an ICalReader object 
     * and from the ITIP method type (i.e. PUBLISH, REQUEST, CANCEL, etc.).
     * The method type can be set to \"\" if there is no method to be loaded.
     * Also accepts a vector of VTimeZone objects to apply to
     * the DateTime properties of the component.
     * 4-2-98:  Added bIgnoreBeginError.  If desired to start parsing in Component level,
     * it is useful to ignore first "BEGIN:".   Setting bIgnoreBeginError to TRUE allows for
     * this.
     *     
     * @param   brFile          ICalReader to load from
     * @param   sType           name of component (VTIMEZONE)
     * @param   parseStatus     return parse error status string (normally return "OK")
     * @param   vTimeZones      vector of VTimeZones to apply to datetimes
     * @param   bIgnoreBeginError   TRUE = ignore first "BEGIN:VTIMEZONE", FALSE otherwise
     * @param   encoding            the encoding of the stream,  default is 7bit.
     *
     * @return  parse error status string (parseStatus)
     */
    virtual UnicodeString & parse(ICalReader * brFile, UnicodeString & sType, 
        UnicodeString & parseStatus, JulianPtrArray * vTimeZones = 0,
        t_bool bIgnoreBeginError = FALSE,
        nsCalUtility::MimeEncoding encoding = nsCalUtility::MimeEncoding_7bit);
    
    /**
     * Returns a clone of this object
     * @param           initLog     log file to write errors to
     *
     * @return          clone of this object
     */
    virtual ICalComponent * clone(JLog * initLog);

    /**
     * Return TRUE if component is valid, FALSE otherwise
     *
     * @return          TRUE if is valid, FALSE otherwise
     */
    virtual t_bool isValid();
    
    /**
     * Print all contents of ICalComponent to iCal export format.
     *
     * @return          string containing iCal export format of ICalComponent
     */
    virtual UnicodeString toICALString();
    
    /**
     * Print all contents of ICalComponent to iCal export format, depending
     * on attendee name, attendee delegated-to, and where to include recurrence-id or not
     *
     * @param           method          method name (REQUEST,PUBLISH, etc.)
     * @param           name            attendee name to filter with
     * @param           isRecurring     TRUE = no recurrenceid, FALSE = include recurrenceid
     *
     * @return          string containing iCal export format of ICalComponent
     */
    virtual UnicodeString toICALString(UnicodeString method, UnicodeString name,
        t_bool isRecurring);

     /**
     * Print all contents of ICalComponent to human-readable string.
     *
     * @return          string containing human-readable format of ICalComponent
     */
    virtual UnicodeString toString();
    
    /**
     * Depending on character passed in, returns a string that represents
     * the ICAL export string of that property that the character represents, if 
     * other paramaters not null, then print out information is filtered in several ways
     * Attendee print out can be filtered so that only attendee with certain name is printed
     * also, can print out the relevant attendees in a delegation message
     * (ie. owner, delegate-to, delegate-from) 
     *
     * @param       c                   char of what property to print
     * @param       sFilterAttendee     name of attendee to print
     * @param       bDelegateRequest    TRUE if a delegate request, FALSE if not
     * @return                          ICAL export format of that property
     */
    virtual UnicodeString formatChar(t_int32 c, UnicodeString sFilterAttendee,
        t_bool delegateRequest = FALSE);

    /**
     * convert a character to the content of a property in string 
     * human-readable format
     * @param           c           a character represents a property
     * @param           dateFmt     for formatting datetimes
     *
     * @return          property in human-readable string
     */
    virtual UnicodeString toStringChar(t_int32 c, UnicodeString & dateFmt);

    /**
     * Returns the ICAL_COMPONENT enumeration value of this ICalComponent.
     * Each ICalComponent subclass must return a unique ICAL_COMPONENT value.
     *
     * @return          ICAL_COMPONENT value of this component
     */
    virtual ICAL_COMPONENT GetType() const { return ICAL_COMPONENT_VALARM; }

    /**
     * Update the private property data-members with updatedComponent's 
     * property data-members.
     * Usually, overwriting data-members should only occur if updatedComponent
     * is more recent than the current component.
     * Return TRUE if component was changed, FALSE otherwise
     * @param           ICalComponent * updatedComponent
     *
     * @return          virtual t_bool
     */
    virtual t_bool updateComponent(ICalComponent * updatedComponent);

    /*  -- End of ICALComponent interface -- */

    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/
    VAlarm::ACTION getAction() const { return m_Action; }
    void setAction(VAlarm::ACTION action) { m_Action = action; }

    /* nsCalDuration */
    nsCalDuration getDuration() const;
    void setDuration(nsCalDuration s, JulianPtrArray * parameters = 0);
    ICalProperty * getDurationProperty() const { return m_Duration; }

    /* Repeat */
    t_int32 getRepeat() const;
    void setRepeat(t_int32 i, JulianPtrArray * parameters = 0);
    ICalProperty * getRepeatProperty() const { return m_Repeat; }

    /* Trigger */
    /* can be date-time or duration */
    nsCalDuration getTriggerAsDuration() const;
    void setTriggerAsDuration(nsCalDuration s, JulianPtrArray * parameters = 0);
    void setTriggerAsDateTime(DateTime s);
    ICalProperty * getTriggerProperty() const { return m_Trigger; }
    DateTime getTriggerAsDateTime(DateTime startTime) const;

    /* Description */
    UnicodeString getDescription() const;
    void setDescription(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getDescriptionProperty() const { return m_Description; }

    /* SUMMARY */
    UnicodeString getSummary() const;
    void setSummary(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getSummaryProperty() const { return m_Summary; }

    /* ATTACH */
    void addAttach(UnicodeString s, JulianPtrArray * parameters = 0);
    void addAttachProperty(ICalProperty * prop);      
    JulianPtrArray * getAttach() const { return m_AttachVctr; }

    /* XTOKENS: NOTE: a vector of strings, not a vector of ICalProperties */
    void addXTokens(UnicodeString s);
    JulianPtrArray * getXTokens() const { return m_XTokensVctr; }

    /* ATTENDEES */
    void addAttendee(Attendee * a); 
    JulianPtrArray * getAttendees() const { return m_AttendeesVctr ; }
    
    /* JLOG */
    JLog * getLog() const { return m_Log; }

    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
    /*----------------------------- 
    ** STATIC METHODS 
    **---------------------------*/
    
     /**
     * Converts string to ACTION enumeration.  Returns AUDIO if error. 
     * @param           sAction      action string
     *
     * @return          ACTION enumeration value of sAction
     */
    static VAlarm::ACTION stringToAction(UnicodeString & sAction);

    /**
     * Converts ACTION to string.
     * @param           action      action value
     * @param           out         output action string
     *
     * @return          output action string
     */
    static UnicodeString & actionToString(VAlarm::ACTION action, UnicodeString & out);

    /*----------------------------- 
    ** OVERLOADED OPERATORS 
    **---------------------------*/ 
};

#endif /* __VALARM_H_ */

