/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsISupports.h"

#ifndef __nsIDeviceContextPS_h
#define __nsIDeviceContextPS_h

/* {35efd8b6-13cc-11d3-9d3a-006008948010} */
#define NS_IDEVICECONTEXTPS_IID \
  {0x35efd8b6, 0x13cc, 0x11d3, \
    {0x9d, 0x3a, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10}}

class nsIDeviceContextPS : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDEVICECONTEXTPS_IID);
  
  NS_IMETHOD SetSpec(nsIDeviceContextSpec *aSpec) = 0;

  NS_IMETHOD InitDeviceContextPS(nsIDeviceContext *aCreatingDeviceContext,
                                 nsIDeviceContext *aPrinterContext) = 0;
};


#endif
