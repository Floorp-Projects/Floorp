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
 * 
 *
 */
#ifndef nsIPrintPreviewContext_h___
#define nsIPrintPreviewContext_h___

#include "nsISupports.h"
#include "nscore.h"

class nsIPrintSettings;

#define NS_IPRINTPREVIEWCONTEXT_IID   \
{ 0xdfd92dd, 0x19ff, 0x4e62, \
  { 0x83, 0x16, 0x8e, 0x44, 0x3, 0x1a, 0x19, 0x62 } }


// An interface for presentation printing contexts 
class nsIPrintPreviewContext : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRINTPREVIEWCONTEXT_IID)

  // Set and Get nsIPrintSettings
  NS_IMETHOD SetPrintSettings(nsIPrintSettings* aPS) = 0;
  NS_IMETHOD GetPrintSettings(nsIPrintSettings** aPS) = 0;

  // Enables the turning on and off of scaling
  NS_IMETHOD SetScalingOfTwips(PRBool aOn) = 0;
};


// Factory method to create a "paginated" presentation context for
// printing
nsresult
NS_NewPrintPreviewContext(nsIPrintPreviewContext** aInstancePtrResult);

#endif
