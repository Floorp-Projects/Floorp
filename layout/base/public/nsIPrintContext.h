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
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 */
#ifndef nsIPrintContext_h___
#define nsIPrintContext_h___

#include "nsISupports.h"
#include "nscore.h"


#define NS_IPRINTCONTEXT_IID   \
{ 0xa05e0e40, 0xe1bf, 0x11d4, \
  { 0xa8, 0x5d, 0x0, 0x10, 0x5a, 0x18, 0x34, 0x19 } }


// An interface for presentation printing contexts 
class nsIPrintContext : public nsISupports {

public:
NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRINTCONTEXT_IID)



};


// Factory method to create a "paginated" presentation context for
// printing
extern NS_EXPORT nsresult
  NS_NewPrintContext(nsIPrintContext** aInstancePtrResult);

#endif
