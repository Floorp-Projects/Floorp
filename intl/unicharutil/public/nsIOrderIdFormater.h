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
#ifndef nsIOrderIdFormater_h__
#define nsIOrderIdFormater_h__


#include "nsISupports.h"
#include "nsString.h"
#include "nscore.h"

// {CCD4D372-CCDC-11d2-B3B1-00805F8A6670}
#define NS_IORDERIDFORMATER_IID \
{ 0xccd4d372, 0xccdc, 0x11d2, \
    { 0xb3, 0xb1, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } };



class nsIOrderIdFormater : public nsISupports {

public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IORDERIDFORMATER_IID)

  // Convert one Unicode character into upper case
  NS_IMETHOD ToString( PRUint32 aOrder, nsString& aResult) = 0;

};

#endif  /* nsIOrderIdFormater_h__ */
