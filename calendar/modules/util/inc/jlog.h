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
 * jlog.h
 * John Sun
 * 3/2/98 10:46:48 AM
 */

#ifndef __JLOG_H_
#define __JLOG_H_

#include <unistring.h> 
#include "ptrarray.h"
#include "jlogvctr.h"
#include "jlogerr.h"
#include "nscalutilexp.h"

/** 
 *  The JLog class defines a log file to write to while parsing
 *  iCal objects.  All iCal objects should take a JLog object during
 *  creation.  
 *  The log currently writes to a file or to a JulianString in memory.
 *  To write to a file, the logString or log method is called, passing
 *  in the strings to write to the log, and a priority level of the 
 *  message.  If the priority level of the message is greater than
 *  the current log's internal priority level, then the message is 
 *  written.  
 *  Here is the current priority levels:
 *  Setting to Defaults = 0
 *  Minor parse error: (i.e. Illegal duplicate properties, parameters) = 100
 *  Invalid Attendee, Freebusy, property name, property value = 200.
 *  Invalid Number format = 200.
 *  Abrupt end of parsing iCal object = 300.
 *  Invalid Event, Todo, Journal, VFreebusy, VTimeZone = 300.
 *  6-22-98.
 *  Changed from writing from one string to a list of in-memory strings.
 */
class NS_CAL_UTIL JLog
{
private:

    /** 
     * Default log level
     */
    static const t_int32 m_DEFAULT_LEVEL;

    /**
     * Default log filename.
     */
    static const char * m_DEFAULT_FILENAME;

    /* data members */

    /** current log filename */
    const char * m_FileName;

    /**
     * vector of vectors (each subvector contains vector of errors)
     */
    JulianPtrArray * m_ErrorVctr;

    /**
     * current vector of errors that is currently being written to.
     */
    nsCalLogErrorVector * m_CurrentEventLogVctr;

    /** TRUE = writeToString, FALSE = writeToFile, immutable */
    t_bool m_WriteToString;

    /** current log level */
    t_int32 m_Level;

    /** ptr to File */
    FILE * m_Stream;

    /* if successful addition return TRUE, else FALSE */
    t_bool addErrorToVector(nsCalLogError * error);

public:

    /**
     * Constructor defines new log file.  Using this constructor
     * will write messages to the private string data-member.
     * @param           t_int32 initLevel = m_DEFAULT_LEVEL
     */
    JLog(t_int32 initLevel = m_DEFAULT_LEVEL);

    /**
     * Constructor defines new log file.  Using this constructor
     * will write message to a file.  If no initFileName,
     * defaults to default filename,  If no level, defaults
     * to default priority level
     * @param           initFilename    initial filename
     * @param           initLevel       initial priority level
     */
    JLog(const char * initFilename, 
        t_int32 initLevel = m_DEFAULT_LEVEL);
    
    /**
     * Destructor.
     */
    ~JLog();

    /**
     * Sets priority level.
     * @param           level   new priority level
     *
     * @return          void 
     */
    void SetLevel(t_int32 level) { m_Level = level; }

    /**
     * Gets priority level.
     *
     * @return          current priority level
     */
    t_int32 GetLevel() const { return m_Level; }


    /**
     * Add a new vector of errors to error log.  
     * This amounts to adding a new event log to the error log.
     */
    void addEventErrorEntry();


    /**
     * Set current event log vector to indicate whether this is a valid or
     * invalid event.
     * @param           t_bool b
     *
     * @return          void 
     */
    void setCurrentEventLogValidity(t_bool b);

    /**
     * Set current event log vector's component type.
       Valid types to pass in are
       ECompType_VEVENT      = VEvent, 
       ECompType_VTODO       = VTodo, 
       ECompType_VFREEBUSY   = VJournal, 
       ECompType_VTIMEZONE   = VTimeZone,  
       ECompType_VFREEBUSY   = VFreebusy,
       ECompType_NSCALENDAR  = NSCalendar,
       ECompType_XCOMPONENT  = X-Token Components
       For example to setCurrentEventLogComponentType to VEVENT errors,
       do the following:

        setCurrentEventLogComponentType(ECompType_VEVENT);

       to set current event log component type to NSCALENDAR errors
       do the following:

        setCurrentEventLogComponentType(ECompType_NSCALENDAR);


     * @param           ComponentType
     */
    void setCurrentEventLogComponentType(nsCalLogErrorVector::ECompType iComponentType);

#if 0
    void setUIDRecurrenceID(UnicodeString & uid, UnicodeString & rid);
#else
    void setUID(UnicodeString & uid);
#endif
    /**
     * Returns the vector of errors for that event index.  Returns 0 for
     * invalid index.
     */
    JulianPtrArray * getEventErrorLog(t_int32 index) const;

    /**
     * Return the vector of log errors.
     *
     * @return          JulianPtrArray * 
     */
    JulianPtrArray * GetErrorVector() const { return m_ErrorVctr; }

#if 0
    /**
     * @see setCurrentEventLogComponentType 
     * To create iterator on valid VEVENT errors only do following:
     *      createIterator(logPtr, ECompType_VEVENT, TRUE)
     * To create iterator on invalid VEVENT errors only do following:
     *      createIterator(logPtr, ECompType_VEVENT, FALSE)
     * To create iterator on valid VFREEBUSY errors only do following:
     *      createIterator(logPtr, ECompType_VFREEBUSY, FALSE)
     * To create iterator on NSCALENDAR errors only do following:
     *      createIterator(logPtr, ECompType_NSCALENDAR, FALSE)
     * To create iterator on X-TOKEN COMPONENT errors only do following:
     *      createIterator(logPtr, ECompType_XCOMPONENT, FALSE)
     * 
     */
    static nsCalLogIterator * createIterator(JLog * aLog, nsCalLogErrorVector::ECompType iComponentType, t_bool bValid = TRUE);
#endif

    void logError(const t_int32 errorID, t_int32 level = m_DEFAULT_LEVEL);
    void logError(const t_int32 errorID, UnicodeString & comment, 
        t_int32 level = m_DEFAULT_LEVEL);
    void logError(const t_int32 errorID, UnicodeString & propName, 
        UnicodeString & propValue, t_int32 level = m_DEFAULT_LEVEL);
    void logError(const t_int32 errorID, UnicodeString & propName, 
        UnicodeString & paramName, UnicodeString & paramValue, 
        t_int32 level = m_DEFAULT_LEVEL);

};

#endif /* __JLOG_H_ */


