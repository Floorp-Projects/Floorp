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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsIHTMLTableColElement__h_
#define _nsIHTMLTableColElement__h_


#include "nsISupports.h"

// XXX ask Peter if this can go away
#define NS_IHTMLTABLECOLELEMENT_IID \
{ 0xcc60f140,  0x4bf4, 0x11d2, \
 { 0x8F, 0x40, 0x00, 0x60, 0x08, 0x15, 0x9B, 0x0C } } 

class nsIHTMLTableColElement : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTMLTABLECOLELEMENT_IID)
  
  /** @return the number of columns this column object represents.  Always >= 1 */
  NS_IMETHOD GetSpanValue (PRInt32* aSpan) = 0;

};


#endif
