/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDurationProperty_h___
#define nsDurationProperty_h___

#include "nsICalProperty.h"
#include "nsICalStandardProperty.h"
#include "jdefines.h"
#include "prprty.h"
#include "sprprty.h"
#include "nscalcoreicalexp.h"

/* 
 * XPCOM wrapper around non-XPCOM julian classes DurationProperty
 * TODO: XXX:  XPCOM julian classes and remove them 
 */
class NS_CAL_CORE_ICAL nsDurationProperty : public nsICalProperty,
                                           public nsICalStandardProperty,
                                           public nsICalICalendarParserObject
{
public:
  nsDurationProperty();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  /* nsICalICalendarParserObject */
  NS_IMETHOD SetParameter(nsString & aKey, nsString & aValue);

  /* nsICalProperty */
  NS_IMETHOD SetParameters(JulianPtrArray * parameters) = 0;
  NS_IMETHOD_(void *) GetValue() const = 0;
  NS_IMETHOD SetValue(void * value) const = 0;
  NS_IMETHOD_(nsICalProperty *) Clone() = 0;
  NS_IMETHOD_(PRBool) IsValid() = 0;
  NS_IMETHOD_(nsString &) ToString(nsString & out) = 0;
  NS_IMETHOD_(nsString &) ToICALString(nsString & out) = 0;
  NS_IMETHOD_(nsString &) ToICALString(nsString & sProp, nsString & out) = 0;

  /* nsICalStandardProperty */
  NS_IMETHOD AddParameter(nsICalParameter * parameter) = 0;
  NS_IMETHOD_(nsString &) toExportString(nsString & out) = 0;
  NS_IMETHOD Parse(nsString & in);

protected:
  ~nsCalendar();

private:
  DurationProperty * mDurationProperty;

};

#endif









