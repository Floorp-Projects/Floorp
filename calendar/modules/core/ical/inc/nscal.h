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
 * nscal.h
 * John Sun
 * 2/18/98 1:35:06 PM
 */
/*
 * Updated 9/24/98  sman
 * We need to be able to append a list of events:  AddEventList
 */

#ifndef __NSCALENDAR_H_
#define __NSCALENDAR_H_

#include <unistring.h>
#include "ptrarray.h"
#include "prprty.h"
#include "icalredr.h"
#include "icalcomp.h"
#include "vevent.h"
#include "vtodo.h"
#include "vjournal.h"
#include "vfrbsy.h"
#include "jutility.h"
#include "julnstr.h"
#include "nscalcoreicalexp.h"

/**
 *  NSCalendar encapsulates a iCalendar calendar object.  An NSCalendar
 *  may contain Calendar Components such as VEvent, VTodo, VJournal,
 *  VFreebusy and VTimeZones.  It also contains the METHOD
 *  PRODID, VERSION, and CALSCALE properties.    (NAME, SOURCE removed 4-28-98).
 */
class NS_CAL_CORE_ICAL NSCalendar
{
private:
#if 0
   
  
    /**
     * Default constructor. Hide from clients. 
     */
    NSCalendar();
#endif
public:
    /*static const UnicodeString ms_asMETHODS[];*/

    /**
     *  An enumeration of ITIP methods.
     */
    enum METHOD{METHOD_PUBLISH = 0, METHOD_REQUEST = 1, 
        METHOD_REPLY = 2, METHOD_CANCEL = 3, METHOD_REFRESH = 4, 
        METHOD_COUNTER = 5, METHOD_DECLINECOUNTER = 6, METHOD_ADD = 7,
        METHOD_LENGTH= 8, METHOD_INVALID = -1};

    /**
     * Converts ITIP method in string to the METHOD enumeration. 
     * @param           UnicodeString & method
     *
     * @return          static NSCalendar::METHOD 
     */
    static NSCalendar::METHOD stringToMethod(UnicodeString & method);

    /**
     * Converts METHOD to string.   A method not publish through add
     * returns "". (i.e. METHOD_LENGTH, METHOD_INVALID).
     * @param           NSCalendar::METHOD method
     * @param           UnicodeString & out
     *
     * @return          static UnicodeString 
     */
    static UnicodeString & methodToString(NSCalendar::METHOD method, UnicodeString & out);
    
    enum JOURNAL_METHOD{
        JOURNAL_METHOD_PUBLISH = METHOD_PUBLISH, 
        JOURNAL_METHOD_CANCEL= METHOD_CANCEL, 
        JOURNAL_METHOD_REFRESH = METHOD_REFRESH, 
        JOURNAL_METHOD_LENGTH = 3, JOURNAL_METHOD_INVALID = -1
    };

    enum FREEBUSY_METHOD{
        
        FREEBUSY_METHOD_PUBLISH = METHOD_PUBLISH, 
        FREEBUSY_METHOD_REQUEST = METHOD_REQUEST, 
        FREEBUSY_METHOD_REPLY = METHOD_REPLY, 
        FREEBUSY_METHOD_LENGTH = 3, FREEBUSY_METHOD_INVALID = -1
    };

    enum SPECIAL_METHOD{
        SPECIAL_METHOD_DELEGATE = 0, 
        SPECIAL_METHOD_LENGTH = 1, SPECIAL_METHOD_INVALID = -1
    };
        
    enum METHOD_PROPS{PROPS_METHOD = 0, PROPS_JOURNAL_METHOD = 1, 
        PROPS_FREEBUSY_METHOD = 2, PROPS_SPECIAL_METHOD = 3};

    enum CALSCALE{CALSCALE_GREGORIAN = 0,
        CALSCALE_LENGTH = 1, CALSCALE_INVALID = -1};

    /* --  METHODS -- */

    /**
     * Constructor.  Creates NSCalendar with initial logfile initLog.
     * @param           JLog * initLog = 0
     */
    NSCalendar(JLog * initLog = 0);
    /*NSCalendar(User u);*/

    /**
     * Copy constructor 
     * @param           NSCalendar & that
     */
    NSCalendar(NSCalendar & that);

    /**
     * Destructor 
     */
    ~NSCalendar();

    /**
     * Export this calendar to a file with filename.  If error occurred,
     * return false in status boolean.
     * TODO: test it
     */
    void export(const char * filename, t_bool & status);

    /**
     * clone this NSCalendar
     * TODO: test it
     */
    NSCalendar * clone(JLog * initLog);

    /* CALSCALE */
    UnicodeString getCalScale() const;
    void setCalScale(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getCalScaleProperty() const { return m_CalScale; }

    /* VERSION */
    UnicodeString getVersion() const;
    void setVersion(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getVersionProperty() const { return m_Version; }

    /* PRODID */
    UnicodeString getProdid() const;
    void setProdid(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getProdidProperty() const { return m_Prodid; }

#if 0
    /* SOURCE */
    UnicodeString getSource() const;
    void setSource(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getSourceProperty() const { return m_Source; }

    /* NAME */
    UnicodeString getName() const;
    void setName(UnicodeString s, JulianPtrArray * parameters = 0);
    ICalProperty * getNameProperty() const { return m_Name; }
#endif

    /* METHOD: NOTE: Method is not an ICalProperty, but saved as an NSCalendar::METHOD */
    void setMethod(NSCalendar::METHOD i) { m_iMethod = i; }
    NSCalendar::METHOD getMethod() const { return (NSCalendar::METHOD) m_iMethod; }

    /* XTOKENS: NOTE: a vector of strings, not a vector of ICalProperties */
    void addXTokens(UnicodeString s);
    JulianPtrArray * getXTokens() const { return m_XTokensVctr; }

    /** getter and setter for when events were last updated from server */
    void setEventsLastUpdatedFromServer(DateTime d) { m_EventsLastUpdatedFromServer = d; }
    DateTime getEventsLastUpdatedFromServer() { return m_EventsLastUpdatedFromServer; }

    /** getter for when events span */
    DateTime getEventsSpanStart() { return m_EventsSpanStart; }
    DateTime getEventsSpanEnd() { return m_EventsSpanEnd; }


    /**
     * Given an ICalReader object, parse the stream to populate this NSCalendar.
     * In the process of parsing, the NSCalendar may create subcomponents
     * such as VEvent, VTodo, VJournal, VTimeZone, and VFreebusy.
     * If stream came from a file, store filename (NOTE: may remove later).
     *
     * @param           brFile      ICalReader stream object to parse from
     * @param           fileName    filename of stream if stream is from a file
     * @param           encoding    the encoding of the stream,  default is 7bit.
     *
     */
    void parse(ICalReader * brFile, UnicodeString & fileName, 
        nsCalUtility::MimeEncoding encoding = nsCalUtility::MimeEncoding_7bit);
 
    /**
     * Returns the error log.
     *
     * @return          JLog* 
     */
    JLog* getLog() const { return m_Log; };

    /**
     * Return the vector of JulianLogErrors on this ICalComponent:
     * Return 0 if ic is not in this calendar.
     * Return 0 if ic has no error vector.
     * Do not deallocate returned vector.  Returns a ptr to vector, not a new vector.
     * @param           ICalComponent * ic
     *
     * @return          JulianPtrArray * 
     */
    JulianPtrArray * getLogVector(ICalComponent * ic);

    /**
     * Return the vector of JulianLogErrors on this calendar.
     * Return 0 if this calendar has no error vector.
     * Do not deallocate returned vector.  Returns a ptr to vector, not a new vector.
     */
    JulianPtrArray * getLogVector();

    /**
     * Returns the curl associated with this nsCalendar. The
     * curl points to the calendar store where components in
     * this nsCalendar are to be stored. It is required
     * for an nsCalendar to have an associated curl if any
     * of its components will be persisted in a calendar store.
     *
     * @return  a JulianString containing the curl 
     */
    JulianString getCurl() const {return m_sCurl;}

    /**
     * Set the default curl for this nsCalendar. 
     */
    void setCurl(const char* ps) {m_sCurl = ps;}

    /**
     * Set the default curl for this nsCalendar. 
     */
    void setCurl(const JulianString& s) {m_sCurl = s;}

    /**
     * Returns calendar in a human-readable format to a string.
     *
     * @return          UnicodeString 
     */
    UnicodeString toString();

    /**
     * Returns the header information of this calendar in a string reference.  The header
     * information is defined to be the calendar properties such as
     * method, version, prodid, calscale, name, and source.
     * @param           UnicodeString & sResult
     *
     * @return          UnicodeString & 
     */
    UnicodeString & createCalendarHeader(UnicodeString & sResult);

    /**
     * Returns calendar in ICAL format to a string.
     *
     * @return          UnicodeString 
     */
    UnicodeString toICALString();
   
    /**
     * Returns calendar in ICAL format to a string with the component pattern
     * specified.
     *
     * @return          UnicodeString 
     */
    UnicodeString toFilteredICALString(UnicodeString componentPattern);

    /**
     * Create a VFreebusy object with the correct freebusy periods
     * depending on the current VEvent vector.  The start and end time
     * of the VFreebusy will defined from the start and end parameters.
     * Clients must delete returned object.
     * @param           start       start time of VFreebusy
     * @param           end         end time of VFreebusy
     *
     * @return          new VFreebusy representing freebusy time from [start-end]
     */
    VFreebusy * createVFreebusy(DateTime start, DateTime end);
    
    /**
     * Fill in a VFreebusy object with the correct freebusy periods
     * depending on the current VEvent vector.  The start and end time
     * of the VFreebusy will defined from the DTSTART and DTEND values
     * of the VFreebusy.    
     * @param           VFreebusy * toFillIn
     */
    void calculateVFreebusy(VFreebusy * toFillIn);

    /**
     * Return the first VFreebusy with a specified UID and sequence no.  
     * If sequence number set to -1, just search on UID.
     * @param           sUID        target UID to look for
     * @param           iSeqNo      target seqNo to look for
     *
     * @return          first VFreebusy matching uid, sequence number.
     */
    VFreebusy * getVFreebusy(UnicodeString sUID, t_int32 iSeqNo = -1);
    /*VFreebusy * getVFreebusy(JulianPtrArray * out, DateTime start, DateTime end);*/
    
    /**
     * Return the first VEvent with a specified UID and sequence no.  
     * If sequence number set to -1, just search on UID.
     * @param           sUID        target UID to look for
     * @param           iSeqNo      target seqNo to look for
     *
     * @return          first VEvent matching uid, sequence number.
     */
    VEvent * getEvent(UnicodeString sUID, t_int32 iSeqNo = -1);      

    /**
     * Return the first VTodo with a specified UID and sequence no.  
     * If sequence number set to -1, just search on UID.
     * @param           sUID        target UID to look for
     * @param           iSeqNo      target seqNo to look for
     *
     * @return          first VTodo matching uid, sequence number.
     */
    VTodo * getTodo(UnicodeString sUID, t_int32 iSeqNo = -1);      

    /**
     * Return the first VJournal with a specified UID and sequence no.  
     * If sequence number set to -1, just search on UID.
     * @param           sUID        target UID to look for
     * @param           iSeqNo      target seqNo to look for
     *
     * @return          first VJournal matching uid, sequence number.
     */
    VJournal * getJournal(UnicodeString sUID, t_int32 iSeqNo = -1);      

    /**
     * Add the ICalComponent to the vector of Events.  
     * NOTE: doesn't add clone of v, but actually ptr to v, so don't deallocate
     * this may change later
     * NOTE: changed from VEvent * v to ICalComponent * v.
     * @param           v       VEvent to add
     */
    void addEvent(ICalComponent * v);
  
    /**
     * Append the supplied vector of events to this NSCalendar's
     * event list. Note that it does not clone the events. It simply
     * adds the pointer to the events to the event list.
     * @param  pEventList  the list of events to add...
     */
    void addEventList(JulianPtrArray *pEventList);

    /**
     * Add the ICalComponent to the vector of Todos.  
     * NOTE: doesn't add clone of v, but actually ptr to v, so don't deallocate
     * this may change later
     * NOTE: changed from VTodo * v to ICalComponent * v.
     * @param           v       VTodo to add
     */
    void addTodo(ICalComponent * v);

    /**
     * Add the ICalComponent to the vector of Journals.  
     * NOTE: doesn't add clone of v, but actually ptr to v, so don't deallocate
     * this may change later
     * NOTE: changed from VJournal * v to ICalComponent * v.
     * @param           v       VJournal to add
     */
    void addJournal(ICalComponent * v);

    /**
     * Add the ICalComponent to the vector of VFreebusy.  
     * NOTE: doesn't add clone of v, but actually ptr to v, so don't deallocate
     * this may change later
     * NOTE: changed from VFreebusy * v to ICalComponent * v.
     * @param           v       VFreebusy to add
     */
    void addVFreebusy(ICalComponent * v);

    /**
     * Add the ICalComponent to the vector of VTimeZone.  
     * NOTE: doesn't add clone of v, but actually ptr to v, so don't deallocate
     * this may change later
     * NOTE: changed from VTimezone * v to ICalComponent * v.
     * @param           v       VTimeZone to add
     */
    void addTimeZone(ICalComponent * v);

    /**
     * Return ptr to vector of events
     * @return          ptr to vector of events
     */
    JulianPtrArray * getEvents() const { return m_VEventVctr; }
    
    /**
     *  XXX: Temp hack.
     *  This is used to remove the ownership of the vector of events
     *  from this calendar to nobody.  It does this by returning
     *  the pointer to m_VEventVctr, then setting the internal value
     *  of m_VEventVctr to 0.  Thus when this NSCalendar is destroyed
     *  the vector of events is not.
     *  The caller now owns the vector of events and must delete it
     *  and its VEvent contents.
     *  This method should be called only when moving events from
     *  one calendar to another.  
     *
     * @return          the pointer to the vector of events
     */
    JulianPtrArray * changeEventsOwnership();

    /**
     * Return ptr to vector of vfreebusies
     * @return          ptr to vector of vfreebusies
     */
    JulianPtrArray * getVFreebusy() const { return m_VFreebusyVctr; }
    
    /**
     * Return ptr to vector of todos
     * @return          ptr to vector of todos
     */
    JulianPtrArray * getTodos() const { return m_VTodoVctr; }
    
    /**
     * Return ptr to vector of journals
     * @return          ptr to vector of journals
     */
    JulianPtrArray * getJournals() const { return m_VJournalVctr; }

    /**
     * Return ptr to vector of timezones
     * @return          ptr to vector of timezones
     */
    JulianPtrArray * getTimeZones() const { return m_VTimeZoneVctr; }

    /**
     * Given a vector of components, and the type of the component, prints 
     * each component in vector to its default human-readable string value
     * Each string is appended to resulting string and is returned in out.
     * For debugging uses.
     * @param           components  vector of components to print   
     * @param           sType       type of component in string
     * @param           out         resulting output string
     *
     * @return          output string (out)
     */
    static UnicodeString & debugPrintComponentVector(JulianPtrArray * components, 
        const UnicodeString sType, UnicodeString & out);    

    /**
     * Fills is the vector retUID with a list of unique uids for this component type
     * client must delete contents of retUID.
     * @param           fillin
     *
     * @return          void 
     */
    void getUniqueUIDs(JulianPtrArray * retUID, ICalComponent::ICAL_COMPONENT type);

    /**
     * Fills in out with VEvents with UID equal to sUID, client must
     * delete contents of events
     * @param           XPPtrArray * out
     * @param           UnicodeString & sUID
     *
     * @return          void 
     */
    void getEvents(JulianPtrArray * out, UnicodeString & sUID);


    /**
     * Fetch events by (uid, rec-id, modifier).
     * set rec-id to "" if no rec-id.  set modifier to "" if no
     * modifier.
     *
     * @param           JulianPtrArray * out
     * @param           UnicodeString & sUID
     * @param           UnicodeString & sRecurrenceID
     * @param           UnicodeString & sModifier
     *
     * @return          void 
     */
    void getEventsByComponentID(JulianPtrArray * out, UnicodeString & sUID,
        UnicodeString & sRecurrenceID, UnicodeString & sModifier);

    /**
     * Get events by date range, adds ptr to out, not clones of ptrs.
     */
    void getEventsByRange(JulianPtrArray * out, DateTime start, DateTime end);

    /**
     * Fills in out with VTodos with UID equal to sUID, client must
     * delete contents of events
     * @param           XPPtrArray * out
     * @param           UnicodeString & sUID
     *
     * @return          void 
     */
    void getTodos(JulianPtrArray * out, UnicodeString & sUID);

    /**
     * Fills in out with VJournals with UID equal to sUID, client must
     * delete contents of events
     * @param           XPPtrArray * out
     * @param           UnicodeString & sUID
     *
     * @return          void 
     */
    void getJournals(JulianPtrArray * out, UnicodeString & sUID);

    /**
     * Sorts vector of components by UID, 
     * passing in type to decide which vector to sort
     * @param           type    component type of vector (VEVENT, VTODO, etc.)
     */
    void sortComponentsByUID(ICalComponent::ICAL_COMPONENT type);

    /**
     * Sorts vector of components by DTSTART value, 
     * passing in type to decide which vector to sort
     * @param           type    component type of vector (VEVENT, VTODO, etc.)
     */
    void sortComponentsByDTStart(ICalComponent::ICAL_COMPONENT type);

protected:

    /**
     * Populates property data members.  Does this by taking contents of 
     * property value and storing in appropriate property, with 
     * parameters also being stored.  Basically a big switch statement
     * on the property name.  
     * @param           strLine     current parsed line
     * @param           propName    property name 
     * @param           propVal     property value
     * @param           parameters  parameters of property
     *
     * @return          TRUE  (TODO: may remove later)
     */
    t_bool storeData(UnicodeString & strLine, UnicodeString & propName,
        UnicodeString & propVal, JulianPtrArray * parameters);
    
    /*void selfCheck();*/

    /**
     * Helper method that expands the VEvent, VTodo, and VJournal
     * vectors.
     *
     * @return          void 
     */
    void expandAllComponents();

private:
    


    /**
     * Add component with type to correct vector.  For example,
     * if type == ICAL_COMPONENT_VEVENT, then add to event vector.
     * Return TRUE if component was updated with component already in store,
     * otherwise return FALSE if component had to be added to store.
     * @param           ic      component to add
     * @param           type    type of component
     * @return          TRUE if component was updated, FALSE if component was added
     */
    t_bool addComponentWithType(ICalComponent * ic, 
        ICalComponent::ICAL_COMPONENT type);

#if 0
    /**
     * Adds a vector of components with type to correct vector.
     * Make sure all elements in components vector have 
     * component type equal to type.
     * @param           components  vector of components
     * @param           type        type of all components in vector
     */
    void addComponentsWithType(JulianPtrArray * components,
        ICalComponent::ICAL_COMPONENT type);
#endif

    /**
     * Expand each component in vector of TimeBasedEvents
     * with component type = type.
     * @param           v       vector of TimeBasedEvents
     * @param           type    type of all components in vector
     */
    void expandTBEVector(JulianPtrArray * v, 
        ICalComponent::ICAL_COMPONENT type);


    /**
     * Expand a TimeBasedEvent with component type = type
     * @param           e       TimeBasedEvent to expand
     * @param           type    component type of TimeBasedEvent
     */
    void expandComponent(TimeBasedEvent * e, 
        ICalComponent::ICAL_COMPONENT type);
        
    /**
     * Helper method sorts ICalComponents by UID.
     * Only applicable for VEvent, VTodo, VJournal, VFreebusy
     * vectors.
     * @param           components  vector of components to sort
     * @param           type        type of all components in vector
     */
    static void sortComponentsByUIDHelper(JulianPtrArray * components, 
        ICalComponent::ICAL_COMPONENT type);

    /**
     * Helper method sorts ICalComponents by DTSTART.
     * Only applicable for VEvent, VTodo, VJournal, VFreebusy
     * vectors.
     * @param           components  vector of components to sort
     * @param           type        type of all components in vector
     */
    static void sortComponentsByDTStartHelper(JulianPtrArray * components, 
        ICalComponent::ICAL_COMPONENT type);

    /**
     * Fills in retUID vector with all unique UIDs found in components vector.
     * Does this by sorting components vector by UID, then marching down.
     * vector. O(nlogn + n).  
     * @param           retUID      fill in with all unique UIDs
     * @param           components  vector of components to look through
     * @param           type        type of all components in vector.
     */
    static void getUniqueUIDsHelper(JulianPtrArray * retUID, 
        JulianPtrArray * components, ICalComponent::ICAL_COMPONENT type);
#if 0
    /**
     * Fills in out vector with ICalComponents with UID = uid and
     * with GetType() == type.
     * @param           out     fill in with components with uid, type match 
     * @param           uid     target UID
     * @param           type    target component type
     */
    void getComponentsWithType(JulianPtrArray * out, UnicodeString & uid,
        ICalComponent::ICAL_COMPONENT type);
#endif
     /**
     * Fetch components by (uid, rec-id, modifier).
     * set rec-id to "" if no rec-id.  set modifier to "" if no
     * modifier.
     *
     * @param           JulianPtrArray * out
     * @param           UnicodeString & sUID
     * @param           UnicodeString & sRecurrenceID
     * @param           UnicodeString & sModifier
     *
     * @return          void 
     */
    void getTBEWithTypeByComponentID(JulianPtrArray * out, UnicodeString & sUID,
        UnicodeString & sRecurrenceID, UnicodeString & sModifier, 
        ICalComponent::ICAL_COMPONENT type);

     /**
     * Fills in out with TimeBasedEvents with the following
     * if sRecurrenceID.isValid()
     * {
     *    if sModifier == NONE, just get components that match (uid, recid)
     *    else if sModifier = THISANDPRIOR, get components that match (uid, recid, THISANDPRIOR)
     *    else if sModifier = THISANDFUTURE, get components that match (uid, recid, THISANDFUTURE)
     *
     * }
     * else
     *   call getComponents(out, components, sUID);
     *
     * @param           JulianPtrArray * out
     * @param           JulianPtrArray * component
     * @param           UnicodeString & sUID
     * @param           UnicodeString & sRecurrenceID
     * @param           UnicodeString & sModifier
     *
     * @return          void 
     */
    void getTBEByComponentID(JulianPtrArray * out, 
        JulianPtrArray * component, UnicodeString & sUID,
        UnicodeString & sRecurrenceID, UnicodeString & sModifier);

    /**
     * Fills in out with ICalComponents with UID = sUID.
     * NOTE: clients NOT responsible for deleting components (passing actual ptr)
     * @param           out         fill in with components with uid = sUID
     * @param           components  vector of components to look through
     * @param           sUID        target UID
     */
    static void getComponents(JulianPtrArray * out, JulianPtrArray * components, 
        UnicodeString & sUID);

    /**
     *  return the ICalComponent that matches the uid and sequence #
     */

    /**
     * Return the ICalComponent that matches the UID and sequence number.
     * If sequence number is -1, then just match on UID.
     * @param           vTBE        vector of components to look through
     * @param           sUID        target UID
     * @param           iSeqNo      target sequence number
     *
     * @return          first ICalComponent matching UID, iSeqNo.
     */
    static ICalComponent * getComponent(JulianPtrArray * vTBE, 
        UnicodeString & sUID, t_int32 iSeqNo = -1);


    /**
     * Prints each component in components vector to ICAL export string
     * and append result to out.  Resulting output string contains
     * all component's iCal export string.
     * @param           components      vector of components to print in ICAL
     * @param           out             output string
     *
     * @return          output string (out)
     */
    static UnicodeString & printComponentVector(JulianPtrArray * components, 
        UnicodeString & out);

    /**
     * Prints each component in components vector to ICAL export string
     * to file f.  Resulting output string contains all component's iCal export string.
     * @param           components      vector of components to print in ICAL
     * @param           f               FILE to print to
     * @return          TRUE if written OK, FALSE otherwise.
     */
    static t_bool printComponentVectorToFile(JulianPtrArray * components, FILE * f);

public:
     /**
     * Prints each component in components vector to ICAL export string
     * and append result to out.  Resulting output string contains
     * events with filtered properties depending on strFmt.
     * all component's iCal export string.
     * @param           components      vector of components to print in ICAL
     * @param           strFmt          component formatting string to apply
     * @param           out             output string
     *
     * @return          output string (out)
     */
    static UnicodeString & printFilteredComponentVector(JulianPtrArray * components, 
        UnicodeString & strFmt, UnicodeString & out);
private:
    /**
     * Helper method.  Fills in Freebusy object f with periods representing
     * free and busy time periods of this calendar from start to end time.
     * Calculates periods by looking through event vector and using
     * events with TRANSP = OPAQUE.
     * @param           f       Freebusy object to fill in with periods
     * @param           start   start of freebusy period 
     * @param           end     end of freebusy period
     */
    void createVFreebusyHelper(Freebusy * f, DateTime start, DateTime end);

    /**
     *  update the m_EventsSpanStart and m_EventsSpanEnd members
     *  taking into account new VEvent.
     */ 
    void updateEventsRange(VEvent * v);


    /* -- private data members -- */

    /* parsing flags */
    static t_bool m_bLoadMultipleCalendars;
    static t_bool m_bSmartLoading;

    /* vector of calendar components */
    JulianPtrArray * m_VJournalVctr;
    JulianPtrArray * m_VEventVctr;
    JulianPtrArray * m_VTodoVctr;
    JulianPtrArray * m_VTimeZoneVctr;
    JulianPtrArray * m_VFreebusyVctr;
    
    /* vector of x-tokens */
    JulianPtrArray * m_XTokensVctr;

    /* NSCalendar properties */
    ICalProperty * m_CalScale;
    ICalProperty * m_Version;
    ICalProperty * m_Prodid;
#if 0
    ICalProperty * m_Source;
    ICalProperty * m_Name;
#endif
    /*ICalProperty * m_Method;
    UnicodeString & m_Method;*/
    
    /** method is stored as int */
    t_int32 m_iMethod;

    /* log file pointer */
    JLog * m_Log;


    /** 
     * the timestamp of the last time these events were updated 
     */
    DateTime m_EventsLastUpdatedFromServer;

    /**
     * the start of end time range of events.  
     */
    DateTime m_EventsSpanStart;
    DateTime m_EventsSpanEnd;
   
    JulianString m_sCurl;   /* cal url to calendar store for this calendar */
};

#endif /* __NSCALENDAR_H_ */


