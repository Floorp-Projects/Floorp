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

#ifndef nsCalDateTimeProperty_h___
#define nsCalDateTimeProperty_h___

#include "jdefines.h"
#include "nsICalProperty.h"
#include "nsICalICalendarParserObject.h"
#include "nsCalStandardProperty.h"
#include "jdefines.h"
#include "dprprty.h"
#include "nscalcoreicalexp.h"

/* 
 * XPCOM wrapper around non-XPCOM julian classes DateTimeProperty
 * TODO: XXX:  XPCOM julian classes and remove them 
 */
class NS_CAL_CORE_ICAL nsCalDateTimeProperty : public nsCalStandardProperty
{
public:
  nsCalDateTimeProperty();


  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  /* nsICalProperty */
  NS_IMETHOD_(void *) GetValue() const;
  NS_IMETHOD SetValue(void * value);
  NS_IMETHOD_(nsICalProperty *) Clone();
  NS_IMETHOD_(PRBool) IsValid();
  NS_IMETHOD_(nsString &) ToString(nsString & out);
  NS_IMETHOD_(nsString &) ToICALString(nsString & out);
  NS_IMETHOD_(nsString &) ToICALString(nsString & sProp, nsString & out);

  /* nsICalStandardProperty */
  NS_IMETHOD_(nsString &) toExportString(nsString & out);

protected:
  ~nsCalDateTimeProperty();

};

#endif









