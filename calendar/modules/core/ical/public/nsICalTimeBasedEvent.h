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

#ifndef nsICalTimeBasedEvent_h___
#define nsICalTimeBasedEvent_h___

#include "nsISupports.h"
#include "nsICalProperty.h"
#include "ptrarray.h"
#include "unistring.h"

//208f8720-6912-11d2-943d-006008268c31
#define NS_ICALTIMEBASEDEVENT_IID \
{ 0x208f8720, 0x6912, 0x11d2, \
{ 0x94, 0x3d, 0x00, 0x60, 0x08, 0x26, 0x8c, 0x31 } }

class nsICalTimeBasedEvent : public nsISupports 
{
public: 
  NS_IMETHOD Init() = 0;

  // for now.
  /*
  NS_IMETHOD_(nsIDateTime *) GetDTStart() const = 0;
  NS_IMETHOD SetDTStart(nsIDateTime * d, JulianPtrArray * parameters) = 0;
  NS_IMETHOD SetDTStartProperty(nsICalDateTimeProperty * property) = 0;
  */

  NS_IMETHOD_(PRInt32) GetSequence() const = 0;
  NS_IMETHOD SetSequence(PRInt32 i, JulianPtrArray * parameters) = 0;
  NS_IMETHOD SetSequenceProperty(nsICalProperty * property) = 0;


  NS_IMETHOD SelfCheck() = 0;
};

#endif




