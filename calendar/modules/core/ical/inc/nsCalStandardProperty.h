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

#ifndef nsCalStandardProperty_h___
#define nsCalStandardProperty_h___

#include "nsCalProperty.h"
#include "nsICalStandardProperty.h"

#include "sdprprty.h"

class NS_CAL_CORE_ICAL nsCalStandardProperty : public nsCalProperty,
                                               public nsICalStandardProperty
{
public:
  nsCalStandardProperty();
  ~nsCalStandardProperty();

public:

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  /* nsICalICalendarParserObject */
  NS_IMETHOD SetParameter(nsString & aKey, nsString & aValue);

  /* nsICalStandardProperty */
  NS_IMETHOD_(JulianPtrArray *) GetParameters();
  NS_IMETHOD SetParameters(JulianPtrArray * parameters);
  NS_IMETHOD AddParameter(nsString & aKey, nsString & aValue);
  NS_IMETHOD AddParameter(ICalParameter * parameter);
  NS_IMETHOD Parse(nsString & in);


};
#endif




