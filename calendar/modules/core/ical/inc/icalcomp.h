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

/* 
 * icalcomp.h
 * John Sun
 * 2/5/98 2:57:16 PM
 */

#include <unistring.h>
#include "ptrarray.h"
#include "icalredr.h"
#include "prprty.h"
#include "jutility.h"
#include "datetime.h"
#include "jlog.h"
#include "nscalcoreicalexp.h"

#ifndef __ICALCOMPONENT_H_
#define __ICALCOMPONENT_H_

/**
 *  ICalComponent defines a Calendar Component in iCal.  A calendar
 *  component can contain iCalendar properties as well as
 *  other iCalendar components.  
 *  Examples of iCalendar components include VEvent, VTodo,
 *  VJournal, VFreebusy, VTimeZone, and VAlarms.
 *  TZPart, althought not an iCal component, subclasses from
 *  ICalComponent.
 */
class NS_CAL_CORE_ICAL ICalComponent
{
protected:
    
    /**
     * Constructor.  Hide from clients except subclasses.
     */
    ICalComponent();

public:

    /**
     * An enumeration of Calendar Components.  TZPart is not
     * actually a Calendar component.  However, it is useful to add
     * it here because it implements this class' interface.
     */
    enum ICAL_COMPONENT
    {
      ICAL_COMPONENT_VEVENT = 0, 
      ICAL_COMPONENT_VTODO = 1, 
      ICAL_COMPONENT_VJOURNAL = 2,
      ICAL_COMPONENT_VFREEBUSY = 3,
      ICAL_COMPONENT_VTIMEZONE = 4,
      ICAL_COMPONENT_VALARM = 5,
      ICAL_COMPONENT_TZPART = 6
    };


    /**
     * Converts ICAL_COMPONENT to string. If bad ic, then return "".
     * @param           ICAL_COMPONENT ic
     *
     * @return          output name of component 
     */
    static UnicodeString componentToString(ICAL_COMPONENT ic);

    static ICAL_COMPONENT stringToComponent(UnicodeString & s, t_bool & error);

    /**
     * Destructor.
     *
     * @return          virtual 
     */
    virtual ~ICalComponent();


    /*  -- Start of ICALComponent interface (PURE VIRTUAL METHODS)-- */
    
    /**
     * The parse method is the standard interface for ICalComponent
     * subclasses to parse from an ICalReader object 
     * and from the ITIP method type (i.e. PUBLISH, REQUEST, CANCEL, etc.).
     * The method type can be set to \"\" if there is no method to be loaded.
     * Also accepts a vector of VTimeZone objects to apply to
     * the DateTime properties of the component.
     * TODO: maybe instead of passing a UnicodeString for method, why not a enum METHOD
     * 4-2-98:  Added bIgnoreBeginError.  If desired to start parsing in Component level,
     * it is useful to ignore first "BEGIN:".   Setting bIgnoreBeginError to TRUE allows for
     * this.
     *     
     * @param   brFile              ICalReader to load from
     * @param   sMethod             method of component (i.e. PUBLISH, REQUEST, REPLY)
     * @param   parseStatus         return parse error status string (normally return "OK")
     * @param   vTimeZones          vector of VTimeZones to apply to datetimes
     * @param   bIgnoreBeginError   TRUE = ignore first "BEGIN:VEVENT", FALSE otherwise
     * @param   encoding            the encoding of the stream,  default is 7bit.
     *
     * @return  parse error status string (parseStatus)
     */
    virtual UnicodeString & parse(ICalReader * brFile, UnicodeString & sMethod, 
        UnicodeString & parseStatus, JulianPtrArray * vTimeZones = 0,
        t_bool bIgnoreBeginError = FALSE, nsCalUtility::MimeEncoding encoding =
        nsCalUtility::MimeEncoding_7bit)
    {
        PR_ASSERT(FALSE);
        if (brFile != NULL && vTimeZones != NULL 
            && sMethod.size() > 0 && parseStatus.size() > 0 && bIgnoreBeginError)
        {}
        return parseStatus;
    }

    /**
     * Returns a clone of this object
     * @param           initLog     log file to write errors to
     *
     * @return          clone of this object
     */
    virtual ICalComponent * clone(JLog * initLog) { PR_ASSERT(FALSE); if (initLog) {} return 0; }
                                                                                               
    /**
     * Return TRUE if component is valid, FALSE otherwise
     *
     * @return          TRUE if is valid, FALSE otherwise
     */
    virtual t_bool isValid() { PR_ASSERT(FALSE); return FALSE; }

    /**
     * Print all contents of ICalComponent to iCal export format.
     *
     * @return          string containing iCal export format of ICalComponent
     */
    virtual UnicodeString toICALString() { PR_ASSERT(FALSE); return "";}

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
        t_bool isRecurring) 
    {
        PR_ASSERT(FALSE);
        if (method.size() == 0 && name.size() == 0 && isRecurring)
        {}
        return "";
    }
    
    /**
     * Print all contents of ICalComponent to human-readable string.
     *
     * @return          string containing human-readable format of ICalComponent
     */
    virtual UnicodeString toString() {PR_ASSERT(FALSE); return "";}

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
    virtual UnicodeString formatChar(t_int32 c, 
        UnicodeString sFilterAttendee, 
        t_bool delegateRequest = FALSE) 
    {
        PR_ASSERT(FALSE);   
#if 0
        if (c == 8 || sFilterAttendee.size() == 0 || delegateRequest) {} 
#endif   
        return "";
    }

    /**
     * convert a character to the content of a property in string 
     * human-readable format
     * @param           c           a character represents a property
     * @param           dateFmt     for formatting datetimes
     *
     * @return          property in human-readable string
     */
    virtual UnicodeString toStringChar(t_int32 c, UnicodeString & dateFmt) 
    {
        PR_ASSERT(FALSE);

#if 0 /* fix HPUX build busgage - this is dead code anyway - alecf 4/30/98 */
        if (c == 8) {}
#endif 
        return "";
#if 0
        if (dateFmt.size() > 0) {}
#endif
    }

    /**
     * Returns the ICAL_COMPONENT enumeration value of this ICalComponent.
     * Each ICalComponent subclass must return a unique ICAL_COMPONENT value.
     *
     * @return          ICAL_COMPONENT value of this component
     */
    virtual ICAL_COMPONENT GetType() const { PR_ASSERT(FALSE); return ICAL_COMPONENT_VEVENT; }

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
    virtual t_bool updateComponent(ICalComponent * updatedComponent)
    {
        PR_ASSERT(FALSE); 
        return FALSE;
    }

    /*  -- End of ICALComponent PURE VIRTUAL interface -- */


    /*-------------------------------------------------------------------*/
    /* NOT PURE
     * TRYING TO MERGE SIMILARITIES IN VFREEBUSY AND TIMEBASEDEVENT TOGETHER
     */

    /**
     * Check if this component matches a UID and a sequence number.
     * TimeBasedEvent and VFreebusy should override this method.
     * @param           sUID        uid to look for
     * @param           iSeqNo      sequence number to look for
     *
     * @return          TRUE if match, FALSE otherwise
     */
    virtual t_bool MatchUID_seqNO(UnicodeString sUID, t_int32 iSeqNo = -1)
    {
        /* NOTE: remove later, just to get rid of compiler warnings */
        if (sUID.size() > 0 && iSeqNo > 0) {}
        return FALSE;
    }

    /**
     * Return the UID of this component.  TimeBasedEvent and VFreebusy
     * should override this method.
     *
     * @return          the UID of this component.
     */
    virtual UnicodeString getUID() const { return ""; }

    /**
     * Return the sequence number of this component.  
     * TimeBasedEvent and VFreebusy should override this method.
     *
     * @return          the sequence number of this component.
     */
    virtual t_int32 getSequence() const { return -1; }

    /*---------------------------------------------------------------------*/

    /*  -- End of ICALComponent interface -- */


    /**
     * Given a component name, a component format string,  print the component in
     * following manner with properties specified by the format string.  It does
     * this by calling abstract formatChar method.
     * (i.e. sComponentName = VEVENT)
     *
     * BEGIN:VEVENT
     * DTSTART:19980331T112233Z (properties dependent on format string)
     * ...
     * END:VEVENT
     *
     * Optionally, the caller may pass in an sFilterAttendee and delegateRequest args.
     * If sFilterAttendee != "", then filter on sFilterAttendee, meaning that
     * only the attendee with the name sFilterAttendee should be printed.  If the
     * delegateRequest is set to TRUE, 
     * also print attendees who are delegates or related-to the delegates.
     * @param           sComponentName          name of ICalComponent
     * @param           strFmt                  property format string
     * @param           sFilterAttendee         attendee to filter on
     * @param           delegateRequest         whether to print delegates
     *
     * @return          iCal export string of component
     */
    UnicodeString format(UnicodeString & sComponentName, UnicodeString & strFmt, 
        UnicodeString sFilterAttendee, t_bool delegateRequest = FALSE);

    /**
     * Given a format string, prints the component to human-readable format
     * with properties specified by format string.  It does this by calling
     * abstract toStringChar method.
     * @param           strFmt          property format string
     *
     * @return          human-readable string of component
     */
    UnicodeString toStringFmt(UnicodeString & strFmt); 

    /* STATIC METHODS */
   
    /**
     * Returns the keyletter representing that property 
     * For example, if propertyName = "DTSTART", outLetter would return 'B'
     * and return boolean would be TRUE.
     * return FALSE if property not found.
     * @param           UnicodeString & propertyName
     * @param           t_int32 & outLetter
     *
     * @return          static t_bool 
     */
    static t_bool propertyNameToKeyLetter(UnicodeString & propertyName,
        t_int32 & outLetter);


    /**
     * Given a ptr to an array of properties strings (char *) and the count
     * create the iCal formatting string for those properties and return
     * it in out.
     * If ppsPropList is null or iPropCount is 0, then
     * return the string of all properties and alarms.
     * @param           char ** ppsPropList
     * @param           t_int32 iPropCount
     * @param           UnicodeString & out
     *
     * @return          static UnicodeString 
     */
    static UnicodeString & makeFormatString(char ** ppsPropList, t_int32 iPropCount,
        UnicodeString & out);

    /**
     * Cleanup vector of UnicodeString objects.   Delete each UnicodeString
     * in vector.
     * @param           strings     vector of elements to delete from
     */
    static void deleteUnicodeStringVector(JulianPtrArray * stringVector);
    
    /**
     * Cleanup vector of ICalComponent objects.  Delete each ICalComponent
     * in vector
     * @param           strings     vector of elements to delete from
     */
    static void deleteICalComponentVector(JulianPtrArray * componentVector);

    /**
     * Clones each ICalComponent element in the toClone vector and put clone in
     * the out vector
     * @param           JulianPtrArray * out
     * @param           JulianPtrArray * toClone
     *
     * @return          static void 
     */
    static void cloneICalComponentVector(JulianPtrArray * out, JulianPtrArray * toClone);

#if 0
    /**
     * Sets the ICalProperty in dateTimePropertyPtr to have 
     * a value on inVal and paramter value on inParameters
     * @param           ICalProperty ** dateTimePropertyPtr
     * @param           DateTime inVal
     * @param           JulianPtrArray * inParameters
     *
     * @return          static void 
     */
    static void setDateTimeValue(ICalProperty ** dateTimePropertyPtr,
        DateTime inVal, JulianPtrArray * inParameters);

    /**
     * Return the value of the dateTimePropertyPtr in outVal 
     * @param           ICalProperty ** dateTimePropertyPtr
     * @param           DateTime & outVal
     *
     * @return          static void 
     */
    static void getDateTimeValue(ICalProperty ** dateTimePropertyPtr, 
        DateTime & outVal);

     /**
     * Sets the ICalProperty in stringPropertyPtr to have 
     * a value on inVal and paramter value on inParameters
     * @param           ICalProperty ** stringPropertyPtr
     * @param           UnicodeString inVal
     * @param           JulianPtrArray * inParameters
     *
     * @return          static void 
     */
    static void setStringValue(ICalProperty ** stringPropertyPtr,
        UnicodeString inVal, JulianPtrArray * inParameters);

    /**
     * Return the value of the stringPropertyPtr in outVal 
     * @param           ICalProperty ** stringPropertyPtr
     * @param           UnicodeString & outVal
     *
     * @return          static void 
     */
    static void getStringValue(ICalProperty ** stringPropertyPtr, 
        UnicodeString & outVal);

     /**
     * Sets the ICalProperty in integerPropertyPtr to have 
     * a value on inVal and paramter value on inParameters
     * @param           ICalProperty ** integerTimePropertyPtr
     * @param           t_int32 inVal
     * @param           JulianPtrArray * inParameters
     *
     * @return          static void 
     */
    static void setIntegerValue(ICalProperty ** integerPropertyPtr,
        t_int32 inVal, JulianPtrArray * inParameters);

    /**
     * Return the value of the integerPropertyPtr in outVal 
     * @param           ICalProperty ** integerPropertyPtr
     * @param           t_int32 & outVal
     *
     * @return          static void 
     */
    static void getIntegerValue(ICalProperty ** integerPropertyPtr, 
        t_int32 & outVal);

#endif
protected:
    /* replace current vector with contents in replaceVctr */
    /* don't overwrite if replaceVctr is empty and bForceOverwriteOnEmpty is FALSE */

    /**
     * Replace the ICalProperty contents of propertyVctrPtr with the ICalProperty 
     * elements in replaceVctr.  This means delete all ICalProperty elements in
     * (*propertyVctrPtr), then clone each element in replaceVctr and add to
     * (*propertyVctrPtr).  If however replaceVctr is empty, don't delete current
     * elements unless bForceOverwriteOnEmpty flag is ON. (see below)
     * If bAddInsteadOfOverwrite is TRUE, don't delete all ICalProperty elements, 
     * just add all elements in replaceVctr to propertyVctrPtr
     * If bForceOverwriteOnEmpty is TRUE, delete all ICalProperty elements even if 
     * replaceVctr is empty or NULL.
     * @param           JulianPtrArray ** propertyVctrPtr
     * @param           JulianPtrArray * replaceVctr
     * @param           t_bool bAddInsteadOfOverwrite = FALSE
     * @param           t_bool bForceOverwriteOnEmpty = FALSE
     *
     * @return          static void 
     */
    static void internalSetPropertyVctr(JulianPtrArray ** propertyVctrPtr,
        JulianPtrArray * replaceVctr, t_bool bAddInsteadOfOverwrite = FALSE,
        t_bool bForceOverwriteOnEmpty = FALSE);

    /**
     * Replace the XToken contents of xTokensVctrPtr with the XToken 
     * elements in replaceVctr.  This means delete all XToken elements in
     * (*xTokensVctrPtr), then clone each element in replaceVctr and add to
     * (*xTokensVctrPtr).  If however replaceVctr is empty, don't delete current
     * elements unless bForceOverwriteOnEmpty flag is ON. (see below)
     * If bAddInsteadOfOverwrite is TRUE, don't delete all XToken elements, 
     * just add all elements in replaceVctr to xTokensVctrPtr
     * If bForceOverwriteOnEmpty is TRUE, delete all XToken elements even if 
     * replaceVctr is empty or NULL.
     * @param           JulianPtrArray ** xTokensVctrPtr
     * @param           JulianPtrArray * replaceVctr
     * @param           t_bool bAddInsteadOfOverwrite = FALSE
     * @param           t_bool bForceOverwriteOnEmpty = FALSE
     *
     * @return          static void 
     */
    static void internalSetXTokensVctr(JulianPtrArray ** xTokensVctrPtr,
        JulianPtrArray * replaceVctr, t_bool bAddInsteadOfOverwrite = FALSE,
        t_bool bForceOverwriteOnEmpty = FALSE);


    /**
     * Replace the ICalProperty in (*propertyPtr) to with replaceProp.
     * This means delete the current ICalProperty and cloning replaceProp
     * and setting (*propertyPtr) to clone.
     * If replaceProp == NULL, don't delete current ICalProperty.
     * However, if bForceOverwriteOnEmpty is TRUE, delete current ICalProperty
     * even if replaceProp is NULL.
     * @param           ICalProperty ** propertyPtr
     * @param           ICalProperty * replaceProp
     * @param           t_bool bForceOverwriteOnEmpty = FALSE
     *
     * @return          static void 
     */
    static void internalSetProperty(ICalProperty ** propertyPtr,
        ICalProperty * replaceProp, t_bool bForceOverwriteOnEmpty = FALSE);

    
      
};

#endif /* __ICALCOMPONENT_H_ */


