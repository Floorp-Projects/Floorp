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
 * duration.h
 * John Sun
 * 1/28/98 5:40:22 PM
 */

#ifndef __DURATION_H_
#define __DURATION_H_

#include <ptypes.h>
#include <unistring.h>
#include "nscalutilexp.h"

/**
 * The nsCalDuration class implements the "duration" data type.
 * The "duration" data type is used to identify properties that contain
 * a duration of time.
 * For example:
 *                                                  
 *   P10Y3M15DT5H30M20S
 *                                                  
 * The nsCalDuration class models the duration datatype specified in the iCalendar document.
 * Assertion: if week value is not equal is zero, then all other values must be zero
 *
 */
class NS_CAL_UTIL nsCalDuration 
{
private:

    /*-----------------------------
    ** MEMBERS
    **---------------------------*/

    /** duration year value */ 
    t_int32 m_iYear;
    /** duration month value */ 
    t_int32 m_iMonth;
    /** duration day value */ 
    t_int32 m_iDay;
    /** duration hour value */ 
    t_int32 m_iHour;
    /** duration minute value */ 
    t_int32 m_iMinute;
    /** duration second value */ 
    t_int32 m_iSecond;

    /** duration week value - NOTE
     * if week != 0 then ALL other members = 0 */ 
    t_int32 m_iWeek;

    /** if duration is negative */
    t_bool m_NegativeDuration;

    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
   
    /**
     * sets values to an invalid duration.  This
     * internally sets all values to -1.
     */
    void setInvalidDuration();

    /**
     * parse an iCal nsCalDuration string and populate the data members
     *
     * @param   us  the iCal Duraton string
     */
    void parse(UnicodeString & us);

public:
      
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    
    /**
     * default constrctor, set duration to 5 min.
     */
    nsCalDuration();
    
    /**
     * Constructs a nsCalDuration object using the grammar
     * defined in the iCalendar spec.
     *
     * @param s String to parse
     */
    nsCalDuration(UnicodeString & us);
     
    /**
     * Constructs a copy of a nsCalDuration object.
     *
     * @param   d   nsCalDuration to copy
     */
    nsCalDuration(nsCalDuration & aDuration);
    
    /**
     * Constructs a nsCalDuration object with a single value set.
     * Always yields a positive Duration
     *
     * @param type the field to set (uses <code>Recurrence</code> type constants)
     * @param value value to assign the field
     */
    nsCalDuration(t_int32 type, t_int32 value);
    
    /**
     * Constructs a nsCalDuration object with a single value set.
     * Always yields a positive Duration
     * Setting the week != 0 automatically ignores other param values and sets them to 0.
     * Setting any param (year,month,day,hour,min,sec) < 0 automatically creates an invalid Duration.
     *
     * @param   year    initial year value
     * @param   month   initial month value
     * @param   day     initial day value
     * @param   hour    initial hour value
     * @param   min     initial min value
     * @param   sec     intiial sec value
     * @param   week    intiial week value
     * @param           isNegativeDuration  TRUE if duration is negative, FALSE otherwise
     */
    nsCalDuration(t_int32 year, t_int32 month, t_int32 day, t_int32 hour, t_int32 min,
        t_int32 sec, t_int32 week, t_bool bNegativeDuration = FALSE);
    /** 
     * Destructor.
     */
    ~nsCalDuration();

    /*-----------------------------
    ** GETTERS and SETTERS
    **---------------------------*/
  
#if 0
    /**
     * Set year value. 
     * @param           i   new year value
     */
    void setYear(t_int32 i) { m_iYear = i; }
    
    /**
     * Set year value. 
     * @param           i   new year value
     */
    void setMonth(t_int32 i) { m_iMonth = i; }
    
    /**
     * Set month value. 
     * @param           i   new month value
     */
    void setDay(t_int32 i) { m_iDay = i; }
    
    /**
     * Set day value. 
     * @param           i   new day value
     */
    void setHour(t_int32 i) { m_iHour = i; }
    
    /**
     * Set minute value. 
     * @param           i   new minute value
     */
    void setMinute(t_int32 i) { m_iMinute = i; }
    
    /**
     * Set second value. 
     * @param           i   new second value
     */
    void setSecond(t_int32 i) { m_iSecond = i; }

    /**
     * Set week value. 
     * @param           i   new week value
     */
    void setWeek(t_int32 i) { m_iWeek = i; }
#endif


    /**
     * Sets all duration member values except week. 
     * Sets week value to zero.
     * Integer parameters must be POSITIVE.
     * Setting any param (year,month,day,hour,min,sec) < 0 automatically creates an invalid Duration.
     * If you are trying to set a negative duration, set bNegativeDuration to TRUE.
     * @param           year    new year value
     * @param           month   new month value
     * @param           day     new day value
     * @param           hour    new hour value
     * @param           min     new minute value
     * @param           sec     new second value
     * @param           isNegativeDuration  TRUE if duration is negative, FALSE otherwise
     */
    void set(t_int32 year, t_int32 month, t_int32 day, t_int32 hour,
        t_int32 min, t_int32 sec, t_bool bNegativeDuration = FALSE);

    /**
     * Sets all duration member values except week.  
     * Setting week < 0 automatically creates an invalid Duration.
     * Sets other values to zero.
     * Integer parameters must be POSITIVE.
     * If you are trying to set a negative duration, set bNegativeDuration to TRUE.
     * @param           week    new week value.
     * @param           isNegativeDuration  TRUE if duration is negative, FALSE otherwise
     */
    void set(t_int32 week, t_bool bNegativeDuration = FALSE);

    /**
     * Return if TRUE, duration is negative, FALSE otherwise
     *
     * @return          TRUE is duration is negative, FALSE otherwise
     */
    t_bool isNegativeDuration() const { return m_NegativeDuration; }

    /**
     * Returns current year value.
     * Takes negative state into account.
     *
     * @return          current year value 
     */
    t_int32 getYear() const;
    
    /**
     * Returns current month value
     * Takes negative state into account.
     *
     * @return          current month value 
     */
    t_int32 getMonth() const;
    
    /**
     * Returns current day value
     * Takes negative state into account.
     *
     * @return          current day value 
     */
    t_int32 getDay() const;
    
    /**
     * Returns current hour value
     * Takes negative state into account.
     *
     * @return          current hour value 
     */
    t_int32 getHour() const;
    
    /**
     * Returns current minute value
     * Takes negative state into account.
     *
     * @return          current minute value 
     */
    t_int32 getMinute() const;
    
    /**
     * Returns current second value
     * Takes negative state into account.
     *
     * @return          current second value 
     */
    t_int32 getSecond() const;
    
    /**
     * Returns current week value
     * Takes negative state into account.
     *
     * @return          current week value 
     */
    t_int32 getWeek() const;

    /*-----------------------------
    ** UTILITIES
    **---------------------------*/

    /**
     * Sets duration members by parsing string input.
     * @param           s   string to parse
     */
    void setDurationString(UnicodeString & s);

    /**
     * Comparision method. 
     * @param           d   nsCalDuration to compare to.
     *
     * @return  -1 if this is shorter, 0 if equal, 1 if longer 
     *           length of duration
     */
    t_int32 compareTo(const nsCalDuration &d);

    /**
     * Normalizes the current duration.  This means rounding off
     * values.  For example, a duration with values 13 months,
     * is normalized to 1 year, 1 month.
     */
    void normalize();

    /**
     * Flattens year to 12 months, then a month to 30 days.  Not very accurate.
     * Used so when I want to print out to iCal, doesn't print months and years,
     * but days.
     */
    void unnormalize();

    /**
     * Returns TRUE if duration lasts no length of time, otherwise
     * returns FALSE
     *
     * @return          TRUE if duration takes no time, FALSE otherwise
     */
    t_bool isZeroLength();

    /**
     * returns this nsCalDuration object to a UnicodeString
     * 
     * @return  a UnicodeString representing the human-readable format of this nsCalDuration
     */
    UnicodeString toString();

    /**
     * returns this nsCalDuration object to a UnicodeString
     * 
     * @return  a UnicodeString representing the iCal format of this nsCalDuration
     */
    UnicodeString toICALString();

    /**
     *  Check whether this duration is valid.  A invalid duration will
     *  have a value of -1 for at least one data member.
     *
     *  @return TRUE if duration is valid, FALSE otherwise
     */
    t_bool isValid();

    /*-----------------------------
    ** OVERLOADED OPERATORS
    **---------------------------*/

    /**
     * assignment operator
     *
     * @param   d       nsCalDuration to copy
     * @return          a copy of that nsCalDuration
     */
    const nsCalDuration &operator=(const nsCalDuration & d); 

    /**
     * (==) equality operator
     *
     * @param   d       nsCalDuration to copy
     * @return          TRUE if this is equal to d, otherwise FALSE
     */
    t_bool operator==(const nsCalDuration & that); 

    /**
     * (!=) in-equality operator
     *
     * @param   d       nsCalDuration to copy
     * @return          TRUE if this NOT equal to d, otherwise FALSE
     */
    t_bool operator!=(const nsCalDuration & that); 
     
    /*-- TODO - make normalize work so that >, < will work correctly */

    /**
     * (>) greater than operator
     *
     * @param   d       nsCalDuration to copy
     * @return          TRUE if this is longer duration than d, otherwise FALSE
     */
    t_bool operator>(const nsCalDuration & that); 

    /**
     * (<) less than operator
     *
     * @param   d       nsCalDuration to copy
     * @return          TRUE if this is shorter duration than d, otherwise FALSE
     */
    t_bool operator<(const nsCalDuration & that); 
};

#endif /* __DURATION_H_ */

