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

#ifndef nsCalendar_h___
#define nsCalendar_h___

#include "jdefines.h"
#include "nsICalendar.h"
#include "nsICalICalendarParserObject.h"
#include "nsICalICalendarContainer.h"
#include "nscal.h"
#include "nscalcoreicalexp.h"

class NS_CAL_CORE_ICAL nsCalendar : public nsICalendar, 
                                    public nsICalICalendarContainer,
                                    public nsICalICalendarParserObject
{
public:
  nsCalendar();

  NS_DECL_ISUPPORTS

  /* nsICalICalendarParserObject */
  NS_IMETHOD Init();
  NS_IMETHOD SetParameter(nsString & aKey, nsString & aValue);

  /* nsICalendar */
  NS_IMETHOD_(UnicodeString) GetCalScale() const;
  NS_IMETHOD SetCalScale(UnicodeString s, JulianPtrArray * parameters = 0);
  NS_IMETHOD SetCalScaleProperty(nsICalProperty * property);
  //NS_IMETHOD_(nsICalProperty *) GetCalScaleProperty() const;

  NS_IMETHOD_(UnicodeString) GetVersion() const;
  NS_IMETHOD SetVersion(UnicodeString s, JulianPtrArray * parameters = 0);
  NS_IMETHOD SetVersionProperty(nsICalProperty * property);
  //NS_IMETHOD_(nsICalProperty *) GetVersionProperty() const;

  NS_IMETHOD_(UnicodeString) GetProdid() const;
  NS_IMETHOD SetProdid(UnicodeString s, JulianPtrArray * parameters = 0);
  NS_IMETHOD SetProdidProperty(nsICalProperty * property);
  //NS_IMETHOD_(nsICalProperty *) GetProdidProperty() const;

  NS_IMETHOD_(PRInt32) GetMethod() const;
  NS_IMETHOD SetMethod(PRInt32 i);
  NS_IMETHOD SetMethodProperty(nsICalProperty * property);

  NS_IMETHOD AddEvent(nsICalVEvent * event);

  NS_IMETHOD_(PRBool) StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop, 
                                    JulianPtrArray * vTimeZones);
 
  NS_IMETHOD_(NSCalendar *) GetNSCalendar() const;
protected:
  ~nsCalendar();

private:
  NSCalendar * mCalendar;
};

#endif /* nsCalendar_h___ */











