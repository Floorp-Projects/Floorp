/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 */

#include "nsISupports.h"
#include "nsIDeviceContext.h"

#ifndef __nsIDeviceContextXp_h
#define __nsIDeviceContextXp_h

/* {35efd8b6-13cc-11d3-9d3a-006008948010} */
#define NS_IDEVICECONTEXTXP_IID \
  {0x35efd8b6, 0x13cc, 0x11d3, \
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}}

class nsXPrintContext;

class nsIDeviceContextXp : public DeviceContextImpl
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDEVICECONTEXTXP_IID);
  
  NS_IMETHOD SetSpec(nsIDeviceContextSpec *aSpec) = 0;

  NS_IMETHOD InitDeviceContextXP(nsIDeviceContext *aCreatingDeviceContext,
                                 nsIDeviceContext *aPrinterContext) = 0;

  NS_IMETHOD GetPrintContext(nsXPrintContext*& aContext) = 0;
};


#endif
