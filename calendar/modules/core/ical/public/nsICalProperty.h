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

#ifndef nsICalProperty_h___
#define nsICalProperty_h___

#include "nsISupports.h"
#include "nsString.h"
#include "ptrarray.h"
#include "prprty.h"

//1d950490-6912-11d2-943d-006008268c31
#define NS_ICALPROPERTY_IID \
{ 0x1d950490, 0x6912, 0x11d2, \
 { 0x94, 0x3d, 0x00, 0x60, 0x08, 0x26, 0x8c, 0x31 } }

// todo: xpcom UnicodeString, JulianPtrArray or use something else (nsIVector, nsString)

class nsICalProperty : public nsISupports
{
public:

  NS_IMETHOD Init() = 0;
  
  NS_IMETHOD SetParameters(JulianPtrArray * parameters) = 0;
  NS_IMETHOD_(JulianPtrArray *) GetParameters() = 0;
  NS_IMETHOD_(void *) GetValue() const = 0;
  NS_IMETHOD SetValue(void * value) = 0;
  NS_IMETHOD_(nsICalProperty *) Clone() = 0;
  NS_IMETHOD_(PRBool) IsValid() = 0;
  NS_IMETHOD_(nsString &) ToString(nsString & out) = 0;
  NS_IMETHOD_(nsString &) ToICALString(nsString & out) = 0;
  NS_IMETHOD_(nsString &) ToICALString(nsString & sProp, nsString & out) = 0;

  NS_IMETHOD_(ICalProperty *) GetICalProperty() const = 0;
};
#endif








