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

#ifndef nsICiter_h__
#define nsICiter_h__

#include "nsISupports.h"

class nsString;

#define NS_ICITER_IID \
{ /* a6cf9102-15b3-11d2-932e-00805f8add32 */ \
0xa6cf9102, 0x15b3, 0x11d2, \
{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

/** Handle plaintext citations, as in mail quoting.
  * Used by the editor but not dependant on it.
  */
class nsICiter  : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICITER_IID; return iid; }

  NS_IMETHOD GetCiteString(const nsString& aInString, nsString& aOutString) = 0;

  NS_IMETHOD StripCites(const nsString& aInString, nsString& aOutString) = 0;

  NS_IMETHOD Rewrap(const nsString& aInString,
                    PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                    nsString& aOutString) = 0;
};

#endif //nsICiter_h__

