/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsICalendar_h___
#define nsICalendar_h___

#include "nsISupports.h"
//#include "nsString.h"
#include "unistring.h"
#include "ptrarray.h"
#include "nscal.h"
#include "nsICalProperty.h"
//#include "nsICalStringProperty.h"
#include "nsICalVEvent.h"

//1cc84170-6912-11d2-943d-006008268c31
#define NS_ICALENDAR_IID \
{ 0x1cc84170, 0x6912, 0x11d2, \
{ 0x94, 0x3d, 0x00, 0x60, 0x08, 0x26, 0x8c, 0x31 } }

// todo: xpcom UnicodeString, JulianPtrArray or use something else (nsIVector, nsString)

class nsICalendar : public nsISupports
{
public:

  NS_IMETHOD Init() = 0;
  
  /*
  NS_IMETHOD Export(const char * filename, t_bool & status) = 0;
  NS_IMETHOD_(nsICalendar *) Clone() = 0;
  */

  NS_IMETHOD_(UnicodeString) GetCalScale() const = 0;
  NS_IMETHOD SetCalScale(UnicodeString s, JulianPtrArray * parameters) = 0;
  //NS_IMETHOD SetCalScaleProperty(nsICalStringProperty * property) = 0;
  //NS_IMETHOD_(nsICalProperty *) GetCalScaleProperty() const = 0;

  NS_IMETHOD_(UnicodeString) GetVersion() const = 0;
  NS_IMETHOD SetVersion(UnicodeString s, JulianPtrArray * parameters) = 0;
  NS_IMETHOD SetVersionProperty(nsICalProperty * property) = 0;
  //NS_IMETHOD_(nsICalProperty *) GetVersionProperty() const = 0;

  NS_IMETHOD_(UnicodeString) GetProdid() const = 0;
  NS_IMETHOD SetProdid(UnicodeString s, JulianPtrArray * parameters) = 0;
  //NS_IMETHOD_(nsICalProperty *) GetProdidProperty() const = 0;

  NS_IMETHOD SetMethod(PRInt32 i) = 0;
  NS_IMETHOD_(PRInt32) GetMethod() const = 0;

  NS_IMETHOD AddEvent(nsICalVEvent * event) = 0;

  NS_IMETHOD_(NSCalendar *) GetNSCalendar() const = 0;
  /*
  NS_IMETHOD AddXTokens(UnicodeString s) = 0;
  NS_IMETHOD_(JulianPtrArray *) GetXTokens() const = 0;

  NS_IMETHOD SetEventsLastUpdatedFromServer(nsIDateTime d) = 0;
  NS_IMETHOD_(nsIDateTime) GetEventsLastUpdatedFromServer() = 0;

  NS_IMETHOD_(nsIDateTime) GetEventsSpanStart() = 0;
  NS_IMETHOD_(nsIDateTime) GetEventsSpanEnd() = 0;

  // todo: xpcom JLog&& ICalComponent
  //NS_IMETHOD_(nsICalLog*) GetLog() = 0;
  //NS_IMETHOD_(JulianPtrArray *) GetLogVector(nsICalICalendarComponent * ic);
  //NS_IMETHOD_(JulianPtrArray *) GetLogVector();

  NS_IMETHOD_(nsString &) GetCurl() const = 0;
  NS_IMETHOD SetCurl(const char* ps) = 0;
  NS_IMETHOD SetCurl(const nsString& s) = 0;

  NS_IMETHOD_(UnicodeString) ToString() = 0;
  NS_IMETHOD_(UnicodeString &) CreateCalendarHeader(UnicodeString & sResult);
  NS_IMETHOD_(UnicodeString) ToICALString() = 0;
  NS_IMETHOD_(UnicodeString) ToFilteredICALString(UnicodeString componentPattern) = 0;

  NS_IMETHOD_(nsICalVFreebusy *) CreateVFreebusy(nsIDateTime start, nsIDateTime end) = 0;
  NS_IMETHOD CalculateVFreebusy(nsICalVFreebusy * toFillIn) = 0;

  NS_IMETHOD_(nsICalVFreebusy *) GetVFreebusy(UnicodeString sUID, PRInt32 iSeqNo) = 0;
  NS_IMETHOD_(nsICalVFreebusy *) GetEvent(UnicodeString sUID, PRInt32 iSeqNo) = 0;
  NS_IMETHOD_(nsICalVFreebusy *) GetTodo(UnicodeString sUID, PRInt32 iSeqNo) = 0;
  NS_IMETHOD_(nsICalVFreebusy *) GetJournal(UnicodeString sUID, PRInt32 iSeqNo) = 0;

  NS_IMETHOD AddEvent(nsICalICalendarComponent * i) = 0;
  NS_IMETHOD AddTodo(nsICalICalendarComponent * i) = 0;
  NS_IMETHOD AddJournal(nsICalICalendarComponent * i) = 0;
  NS_IMETHOD AddVFreebusy(nsICalICalendarComponent * i) = 0;
  NS_IMETHOD AddTimeZone(nsICalICalendarComponent * i) = 0;

  NS_IMETHOD AddEventList(JulianPtrArray * v) = 0;

  NS_IMETHOD_(JulianPtrArray *) GetEvents() const = 0;
  
  NS_IMETHOD_(JulianPtrArray *) ChangeEventsOwnership() = 0;

  NS_IMETHOD_(JulianPtrArray *) GetTodos() const = 0;
  NS_IMETHOD_(JulianPtrArray *) GetJournals() const = 0;
  NS_IMETHOD_(JulianPtrArray *) GetVFreebusies() const = 0;
  NS_IMETHOD_(JulianPtrArray *) GetTimeZones() const = 0;
  
  NS_IMETHOD GetUniqueUIDs(JulianPtrArray * retUID, PRInt32 iCalComponentType);
  
  NS_IMETHOD GetEvents(JulianPtrArray * out, UnicodeString & sUID) const = 0;
  NS_IMETHOD GetTodos(JulianPtrArray * out, UnicodeString & sUID) const = 0;
  NS_IMETHOD GetJournals(JulianPtrArray * out, UnicodeString & sUID) const = 0;
  
  NS_IMETHOD GetEventsByComponentID(JulianPtrArray * out, UnicodeString & sUID,
                                    UnicodeString & sRecurrenceID, 
                                    UnicodeString & sModifier) const = 0;
  NS_IMETHOD GetEventsByRange(JulianPtrArray * out, 
                              nsIDateTime start, nsIDateTime end) const = 0;

  NS_IMETHOD SortComponentsByDTStart(PRInt32 iCalComponentType);
  NS_IMETHOD SortComponentsByUID(PRInt32 iCalComponentType);

  NS_IMETHOD UpdateEventsRange(nsICalVEvent * v) = 0;
  NS_IMETHOD UpdateEventsRange(nsIDateTime ds, nsIDateTime de) = 0;
  */

};
#endif




